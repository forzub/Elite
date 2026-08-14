#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "src/game/client/ClientPresentationClock.h"
#include "src/game/client/SnapshotPresentationWindow.h"

namespace
{

struct TestSnapshot
{
    double timeSeconds = 0.0;
    double scalarPositionMeters = 0.0;
};

[[noreturn]] void fail(const std::string& message)
{
    std::cerr << "[FAIL] " << message << '\n';
    std::exit(1);
}

void require(bool condition, const std::string& message)
{
    if (!condition)
        fail(message);
}

void requireNear(
    double actual,
    double expected,
    double tolerance,
    const std::string& message
)
{
    if (std::abs(actual - expected) > tolerance)
    {
        fail(
            message +
            " actual=" + std::to_string(actual) +
            " expected=" + std::to_string(expected)
        );
    }
}

double snapshotTime(const TestSnapshot& snapshot)
{
    return snapshot.timeSeconds;
}

void testPresentationWindowSelectsOneSharedBracket()
{
    std::deque<TestSnapshot> snapshots {
        {1.00, 10.0},
        {1.06, 20.0},
        {1.12, 30.0},
        {1.18, 40.0},
    };

    const auto window =
        game::client::resolveSnapshotPresentationWindow(
            snapshots,
            1.135,
            snapshotTime
        );

    require(window.hasSnapshots, "window must see snapshot history");
    require(!window.clampedToOldest, "valid time must not clamp oldest");
    require(!window.clampedToNewest, "valid time must not clamp newest");
    require(window.hasInterpolationBracket, "valid time must have bracket");
    require(window.olderIndex == 2, "older index must be stable");
    require(window.newerIndex == 3, "newer index must be stable");
    requireNear(window.interpolationAlpha, 0.25, 1.0e-12, "shared alpha");
}

void testPresentationWindowClampsOnlyAtHistoryBoundary()
{
    std::deque<TestSnapshot> snapshots {
        {2.00, 0.0},
        {2.06, 0.0},
        {2.12, 0.0},
    };

    const auto before =
        game::client::resolveSnapshotPresentationWindow(
            snapshots,
            1.50,
            snapshotTime
        );
    require(before.clampedToOldest, "early render time must clamp oldest");
    require(!before.clampedToNewest, "early render time must not clamp newest");
    requireNear(before.renderTimeSeconds, 2.00, 1.0e-12, "oldest clamp value");

    const auto after =
        game::client::resolveSnapshotPresentationWindow(
            snapshots,
            3.00,
            snapshotTime
        );
    require(after.clampedToNewest, "late render time must clamp newest");
    require(!after.clampedToOldest, "late render time must not clamp oldest");
    requireNear(after.renderTimeSeconds, 2.12, 1.0e-12, "newest clamp value");
}

void testCapturedCadenceKeepsRemoteInterpolationAlive()
{
    game::client::ClientPresentationClock clock;
    std::deque<TestSnapshot> snapshots;

    constexpr double FrameDt = 1.0 / 80.0;
    constexpr double SnapshotInterval = 0.060;
    constexpr double OneWayLatency = 0.100;
    constexpr double SpeedMetersPerSecond = 180.0;

    double serverNow = 0.0;
    double nextPublishedServerTime = 0.0;
    std::size_t bracketFrames = 0;
    std::size_t checkedFrames = 0;
    std::size_t clampedNewestFrames = 0;
    double maxInterpolationErrorMeters = 0.0;
    double minAlpha = 1.0;
    double maxAlpha = 0.0;
    std::uint64_t starvationAtWarmup = 0;
    bool capturedWarmupStarvation = false;

    for (int frame = 0; frame < 16000; ++frame)
    {
        serverNow += FrameDt;

        while (nextPublishedServerTime + OneWayLatency <= serverNow)
        {
            snapshots.push_back({
                nextPublishedServerTime,
                nextPublishedServerTime * SpeedMetersPerSecond
            });
            nextPublishedServerTime += SnapshotInterval;
            while (snapshots.size() > 20)
                snapshots.pop_front();
        }

        const bool hasSnapshot = !snapshots.empty();
        const double newest =
            hasSnapshot ? snapshots.back().timeSeconds : 0.0;

        clock.update(FrameDt, serverNow, hasSnapshot, newest);

        if (!clock.ready() || snapshots.size() < 2 || serverNow < 2.0)
            continue;

        if (!capturedWarmupStarvation)
        {
            starvationAtWarmup = clock.starvationCount();
            capturedWarmupStarvation = true;
        }

        const auto window =
            game::client::resolveSnapshotPresentationWindow(
                snapshots,
                clock.renderTimeSeconds(),
                snapshotTime
            );

        ++checkedFrames;
        if (window.clampedToNewest)
            ++clampedNewestFrames;
        if (!window.hasInterpolationBracket)
            continue;

        ++bracketFrames;
        minAlpha = std::min(minAlpha, window.interpolationAlpha);
        maxAlpha = std::max(maxAlpha, window.interpolationAlpha);

        const auto& a = snapshots[window.olderIndex];
        const auto& b = snapshots[window.newerIndex];
        const double interpolated =
            a.scalarPositionMeters +
            (b.scalarPositionMeters - a.scalarPositionMeters) *
                window.interpolationAlpha;
        const double exact =
            window.renderTimeSeconds * SpeedMetersPerSecond;

        maxInterpolationErrorMeters =
            std::max(
                maxInterpolationErrorMeters,
                std::abs(interpolated - exact)
            );
    }

    require(checkedFrames > 10000, "steady cadence must produce long sample");
    require(clampedNewestFrames == 0, "steady presentation must never hold newest");
    require(bracketFrames == checkedFrames, "every steady frame must have bracket");
    require(minAlpha < 0.02, "alpha must visit the low end of the bracket");
    require(maxAlpha > 0.98, "alpha must visit the high end of the bracket");
    require(
        maxInterpolationErrorMeters < 1.0e-9,
        "linear remote interpolation must reproduce exact motion"
    );
    require(
        clock.starvationCount() == starvationAtWarmup,
        "steady mature cadence must not add starvation events"
    );
}

void testLargeClientStallRebuildsHistoryInsteadOfSnapshotHold()
{
    game::client::ClientPresentationClock clock;
    std::deque<TestSnapshot> snapshots;

    double estimatedServerNow = 0.0;
    double authoritativeServerNow = 0.0;
    double nextSnapshotTime = 0.0;

    constexpr double FrameDt = 1.0 / 80.0;
    constexpr double SnapshotInterval = 0.060;
    constexpr double OneWayLatency = 0.100;

    auto publishAvailable = [&]()
    {
        while (nextSnapshotTime + OneWayLatency <= authoritativeServerNow)
        {
            snapshots.push_back({nextSnapshotTime, 0.0});
            nextSnapshotTime += SnapshotInterval;
            while (snapshots.size() > 20)
                snapshots.pop_front();
        }
    };

    for (int frame = 0; frame < 240; ++frame)
    {
        estimatedServerNow += FrameDt;
        authoritativeServerNow += FrameDt;
        publishAvailable();
        clock.update(
            FrameDt,
            estimatedServerNow,
            !snapshots.empty(),
            snapshots.empty() ? 0.0 : snapshots.back().timeSeconds
        );
    }

    // Reproduce the synchronous-host failure we captured: wall/client time
    // jumps by 2.2 s, while capped server simulation advances only 0.25 s.
    estimatedServerNow += 2.2;
    authoritativeServerNow += 0.25;
    publishAvailable();
    clock.update(
        2.2,
        estimatedServerNow,
        true,
        snapshots.back().timeSeconds
    );

    require(clock.hardRebaseCount() >= 1, "large disagreement must hard-rebase once");

    std::size_t checkedFrames = 0;
    std::size_t clampedNewestFrames = 0;
    std::size_t missingBracketFrames = 0;
    std::uint64_t starvationAfterRecoveryWarmup = 0;
    bool capturedRecoveryStarvation = false;

    for (int frame = 0; frame < 1200; ++frame)
    {
        estimatedServerNow += FrameDt;
        authoritativeServerNow += FrameDt;
        publishAvailable();
        clock.update(
            FrameDt,
            estimatedServerNow,
            true,
            snapshots.back().timeSeconds
        );

        // Give the rebase two publication intervals to rebuild a bracket.
        if (frame < 10)
            continue;

        if (!capturedRecoveryStarvation)
        {
            starvationAfterRecoveryWarmup = clock.starvationCount();
            capturedRecoveryStarvation = true;
        }

        const auto window =
            game::client::resolveSnapshotPresentationWindow(
                snapshots,
                clock.renderTimeSeconds(),
                snapshotTime
            );

        ++checkedFrames;
        if (window.clampedToNewest)
            ++clampedNewestFrames;
        if (!window.hasInterpolationBracket)
            ++missingBracketFrames;
    }

    require(checkedFrames > 1000, "recovery trace must be long enough");
    require(clampedNewestFrames == 0, "recovered timeline must not hold newest");
    require(missingBracketFrames == 0, "recovered timeline must retain bracket");
    require(
        clock.starvationCount() == starvationAfterRecoveryWarmup,
        "recovered mature timeline must not add starvation events"
    );
}

void testRemoteStartupRebasesStalePreSnapshotPlayhead()
{
    game::client::ClientPresentationClock clock;

    constexpr double FrameDt = 1.0 / 80.0;
    constexpr double SnapshotInterval = 0.060;

    // Reproduce a real separate-process startup: the client spends some frames
    // without authoritative history while its provisional server-time estimate
    // describes an older epoch. The dedicated server is already several
    // seconds ahead by the time the first snapshot arrives.
    double estimatedServerNow = 70.0;
    for (int frame = 0; frame < 120; ++frame)
    {
        estimatedServerNow += FrameDt;
        clock.update(FrameDt, estimatedServerNow, false, 0.0);
        require(
            !clock.ready(),
            "presentation clock must remain provisional before first snapshot"
        );
    }

    std::deque<TestSnapshot> snapshots;
    double authoritativeServerNow = 78.0;
    double nextSnapshotTime = 76.86;
    while (nextSnapshotTime <= authoritativeServerNow + 1.0e-12)
    {
        snapshots.push_back({nextSnapshotTime, 0.0});
        nextSnapshotTime += SnapshotInterval;
    }

    clock.update(
        FrameDt,
        estimatedServerNow,
        true,
        snapshots.back().timeSeconds
    );

    require(clock.ready(), "first authoritative snapshot must arm presentation clock");

    auto firstWindow =
        game::client::resolveSnapshotPresentationWindow(
            snapshots,
            clock.renderTimeSeconds(),
            snapshotTime
        );

    require(
        !firstWindow.clampedToOldest,
        "remote startup must not leave presentation behind oldest snapshot"
    );
    require(
        !firstWindow.clampedToNewest,
        "remote startup must keep interpolation lead behind newest snapshot"
    );
    require(
        firstWindow.hasInterpolationBracket,
        "remote startup must enter representable snapshot history immediately"
    );

    double minAlpha = 1.0;
    double maxAlpha = 0.0;
    std::size_t checkedFrames = 0;
    std::size_t clampedOldestFrames = 0;

    for (int frame = 0; frame < 600; ++frame)
    {
        estimatedServerNow += FrameDt;
        authoritativeServerNow += FrameDt;

        while (nextSnapshotTime <= authoritativeServerNow + 1.0e-12)
        {
            snapshots.push_back({nextSnapshotTime, 0.0});
            nextSnapshotTime += SnapshotInterval;
            while (snapshots.size() > 20)
                snapshots.pop_front();
        }

        clock.update(
            FrameDt,
            estimatedServerNow,
            true,
            snapshots.back().timeSeconds
        );

        const auto window =
            game::client::resolveSnapshotPresentationWindow(
                snapshots,
                clock.renderTimeSeconds(),
                snapshotTime
            );

        ++checkedFrames;
        if (window.clampedToOldest)
            ++clampedOldestFrames;
        if (!window.hasInterpolationBracket)
            continue;

        minAlpha = std::min(minAlpha, window.interpolationAlpha);
        maxAlpha = std::max(maxAlpha, window.interpolationAlpha);
    }

    require(checkedFrames == 600, "remote startup trace must run completely");
    require(
        clampedOldestFrames == 0,
        "remote startup must never fall behind retained snapshot history"
    );
    require(minAlpha < 0.10, "remote startup alpha must visit low bracket values");
    require(maxAlpha > 0.90, "remote startup alpha must visit high bracket values");
}

void testFractionalPredictionTimingRemovesFixedTickStaircase()
{
    constexpr double FixedDt = 0.020;
    constexpr double RenderDt = 1.0 / 80.0;
    constexpr double SpeedMetersPerSecond = 420.0;

    double accumulator = 0.0;
    double fixedPosition = 0.0;
    double elapsed = 0.0;
    double maxFixedOnlyError = 0.0;
    double maxFractionalError = 0.0;

    for (int frame = 0; frame < 10000; ++frame)
    {
        elapsed += RenderDt;
        accumulator += RenderDt;

        while (accumulator >= FixedDt)
        {
            fixedPosition += SpeedMetersPerSecond * FixedDt;
            accumulator -= FixedDt;
        }

        const double fixedOnly = fixedPosition;
        const double fractional =
            fixedPosition + SpeedMetersPerSecond * accumulator;
        const double exact = SpeedMetersPerSecond * elapsed;

        maxFixedOnlyError =
            std::max(maxFixedOnlyError, std::abs(fixedOnly - exact));
        maxFractionalError =
            std::max(maxFractionalError, std::abs(fractional - exact));
    }

    require(
        maxFixedOnlyError > 5.0,
        "50 Hz fixed-only presentation must expose a multi-meter staircase at test speed"
    );
    require(
        maxFractionalError < 1.0e-8,
        "fractional render sampling must remove the fixed-tick staircase"
    );
}

using TestFunction = void (*)();
struct TestCase
{
    const char* name;
    TestFunction function;
};

} // namespace

int main()
{
    const std::vector<TestCase> tests {
        {"presentation window selects one shared bracket", testPresentationWindowSelectsOneSharedBracket},
        {"presentation window clamps only at history boundary", testPresentationWindowClampsOnlyAtHistoryBoundary},
        {"captured cadence keeps remote interpolation alive", testCapturedCadenceKeepsRemoteInterpolationAlive},
        {"large client stall rebuilds history", testLargeClientStallRebuildsHistoryInsteadOfSnapshotHold},
        {"remote startup rebases stale pre-snapshot playhead", testRemoteStartupRebasesStalePreSnapshotPlayhead},
        {"fractional prediction removes fixed tick staircase", testFractionalPredictionTimingRemovesFixedTickStaircase},
    };

    std::size_t passed = 0;
    for (const auto& test : tests)
    {
        test.function();
        ++passed;
        std::cout << "[PASS] " << test.name << '\n';
    }

    std::cout << passed << "/" << tests.size() << " tests passed\n";
    return 0;
}
