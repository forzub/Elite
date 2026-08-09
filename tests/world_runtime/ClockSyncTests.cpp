#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "src/game/client/ClientServerClock.h"
#include "src/game/client/ClientPresentationClock.h"
#include "src/game/client/ClientUniverseTimeline.h"
#include "src/game/server/ServerTimelineClock.h"
#include "src/world/time/UniverseClock.h"

namespace
{

void require(
    bool condition,
    const char* expression,
    const char* file,
    int line
)
{
    if (condition)
        return;

    std::ostringstream message;
    message << file << ':' << line
            << ": requirement failed: " << expression;
    throw std::runtime_error(message.str());
}

#define REQUIRE(expr) \
    require(static_cast<bool>(expr), #expr, __FILE__, __LINE__)

void requireNear(
    double actual,
    double expected,
    double tolerance,
    const char* label
)
{
    if (std::abs(actual - expected) <= tolerance)
        return;

    std::ostringstream message;
    message << label
            << ": expected " << expected
            << ", got " << actual
            << ", tolerance " << tolerance;
    throw std::runtime_error(message.str());
}

class DeterministicNoise
{
public:
    explicit DeterministicNoise(std::uint64_t seed)
        : m_state(seed ? seed : 1)
    {
    }

    double uniform01()
    {
        m_state = m_state * 6364136223846793005ULL + 1442695040888963407ULL;
        const std::uint64_t bits = m_state >> 11;
        return static_cast<double>(bits) /
            static_cast<double>(std::uint64_t{1} << 53);
    }

    double symmetric()
    {
        return uniform01() * 2.0 - 1.0;
    }

private:
    std::uint64_t m_state;
};

struct SyncEvent
{
    double receiveTrueSeconds = 0.0;
    double clientSendLocalSeconds = 0.0;
    double clientReceiveLocalSeconds = 0.0;
    double serverReceiveSeconds = 0.0;
};

struct SimulationConfig
{
    double durationSeconds = 180.0;
    double warmupSeconds = 12.0;
    double clientDriftPpm = 80.0;
    double syncIntervalSeconds = 0.50;
    double baseOneWayLatencySeconds = 0.050;
    double jitterAmplitudeSeconds = 0.015;
    double spikeProbability = 0.05;
    double spikeSeconds = 0.120;
    double serverTickSeconds = 0.020;
};

std::vector<SyncEvent> makeEvents(
    const SimulationConfig& config,
    std::uint64_t seed
)
{
    DeterministicNoise noise(seed);
    const double clientRate =
        1.0 + config.clientDriftPpm * 1.0e-6;

    std::vector<SyncEvent> events;

    for (double sendTrue = 0.0;
         sendTrue <= config.durationSeconds;
         sendTrue += config.syncIntervalSeconds)
    {
        auto networkDelay = [&]()
        {
            double delay =
                config.baseOneWayLatencySeconds +
                noise.symmetric() * config.jitterAmplitudeSeconds;

            if (noise.uniform01() < config.spikeProbability)
                delay += config.spikeSeconds;

            return std::max(0.001, delay);
        };

        const double outbound = networkDelay();
        const double inbound = networkDelay();
        const double requestArrival = sendTrue + outbound;

        // The authoritative server timeline advances on fixed simulation
        // ticks. A request is therefore stamped on the first completed tick
        // at or after its arrival.
        const double serverReceive =
            std::ceil(
                requestArrival / config.serverTickSeconds
            ) * config.serverTickSeconds;

        const double responseArrival = serverReceive + inbound;

        SyncEvent event;
        event.receiveTrueSeconds = responseArrival;
        event.clientSendLocalSeconds = sendTrue * clientRate;
        event.clientReceiveLocalSeconds = responseArrival * clientRate;
        event.serverReceiveSeconds = serverReceive;
        events.push_back(event);
    }

    return events;
}

struct Metrics
{
    double rmsErrorMs = 0.0;
    double maxErrorMs = 0.0;
    double maxSpeedErrorPercent = 0.0;
    double meanSpeedErrorPercent = 0.0;
    std::size_t samples = 0;
};

class LatestOffsetEstimator
{
public:
    void advance(double localDelta)
    {
        m_local += localDelta;
    }

    void add(const SyncEvent& event)
    {
        const double midpoint =
            0.5 * (
                event.clientSendLocalSeconds +
                event.clientReceiveLocalSeconds
            );
        m_offset = event.serverReceiveSeconds - midpoint;
        m_ready = true;
    }

    bool ready() const { return m_ready; }
    double time() const { return m_local + m_offset; }

private:
    double m_local = 0.0;
    double m_offset = 0.0;
    bool m_ready = false;
};

class EmaOffsetEstimator
{
public:
    void advance(double localDelta)
    {
        m_local += localDelta;
    }

    void add(const SyncEvent& event)
    {
        const double midpoint =
            0.5 * (
                event.clientSendLocalSeconds +
                event.clientReceiveLocalSeconds
            );
        const double observed =
            event.serverReceiveSeconds - midpoint;

        if (!m_ready)
            m_offset = observed;
        else
            m_offset += 0.12 * (observed - m_offset);

        m_ready = true;
    }

    bool ready() const { return m_ready; }
    double time() const { return m_local + m_offset; }

private:
    double m_local = 0.0;
    double m_offset = 0.0;
    bool m_ready = false;
};

class BoundedPhaseEstimator
{
public:
    void advance(double localDelta)
    {
        m_local += localDelta;

        if (!m_ready)
            return;

        const double target = m_local + m_targetOffset;
        const double error = target - m_time;
        const double correction =
            std::clamp(error / 8.0, -0.0025, 0.0025);
        m_time += localDelta * (1.0 + correction);
    }

    void add(const SyncEvent& event)
    {
        const double midpoint =
            0.5 * (
                event.clientSendLocalSeconds +
                event.clientReceiveLocalSeconds
            );
        const double observed =
            event.serverReceiveSeconds - midpoint;

        m_offsets.push_back(observed);
        while (m_offsets.size() > 16)
            m_offsets.pop_front();

        std::vector<double> sorted(m_offsets.begin(), m_offsets.end());
        std::sort(sorted.begin(), sorted.end());
        m_targetOffset = sorted[sorted.size() / 2];

        if (!m_ready && m_offsets.size() >= 6)
        {
            m_time = m_local + m_targetOffset;
            m_ready = true;
        }
    }

    bool ready() const { return m_ready; }
    double time() const { return m_time; }

private:
    std::deque<double> m_offsets;
    double m_local = 0.0;
    double m_targetOffset = 0.0;
    double m_time = 0.0;
    bool m_ready = false;
};

class ProductionEstimator
{
public:
    void advance(double localDelta)
    {
        m_clock.advance(localDelta);
    }

    void add(const SyncEvent& event)
    {
        m_clock.addSyncSample(
            event.clientSendLocalSeconds,
            event.clientReceiveLocalSeconds,
            event.serverReceiveSeconds
        );
    }

    bool ready() const { return m_clock.synchronized(); }
    double time() const { return m_clock.estimatedServerTimeSeconds(); }

    const game::client::ClientServerClock& clock() const
    {
        return m_clock;
    }

private:
    game::client::ClientServerClock m_clock;
};

template <typename Estimator>
Metrics runClockSimulation(
    Estimator& estimator,
    const SimulationConfig& config,
    std::uint64_t seed,
    bool variableFrameRate
)
{
    const auto events = makeEvents(config, seed);
    const double clientRate =
        1.0 + config.clientDriftPpm * 1.0e-6;

    std::size_t eventIndex = 0;
    double trueTime = 0.0;
    double errorSquares = 0.0;
    double speedErrorSum = 0.0;
    double previousEstimate = 0.0;
    double previousTrue = 0.0;
    bool havePrevious = false;

    Metrics result;
    std::size_t frameIndex = 0;

    while (trueTime < config.durationSeconds)
    {
        double trueDt = 1.0 / 60.0;

        if (variableFrameRate)
        {
            static constexpr double Pattern[] =
            {
                1.0 / 20.0,
                1.0 / 144.0,
                1.0 / 72.0,
                1.0 / 30.0,
                1.0 / 120.0,
                1.0 / 55.0,
            };
            trueDt = Pattern[frameIndex % (sizeof(Pattern) / sizeof(Pattern[0]))];
        }

        ++frameIndex;
        trueDt = std::min(trueDt, config.durationSeconds - trueTime);
        trueTime += trueDt;

        estimator.advance(trueDt * clientRate);

        while (eventIndex < events.size() &&
               events[eventIndex].receiveTrueSeconds <= trueTime)
        {
            estimator.add(events[eventIndex]);
            ++eventIndex;
        }

        if (!estimator.ready() || trueTime < config.warmupSeconds)
            continue;

        const double estimate = estimator.time();
        const double error = estimate - trueTime;

        errorSquares += error * error;
        result.maxErrorMs =
            std::max(result.maxErrorMs, std::abs(error) * 1000.0);

        if (havePrevious)
        {
            const double trueSpan = trueTime - previousTrue;
            const double estimateSpan = estimate - previousEstimate;

            if (trueSpan > 0.0)
            {
                const double speedError =
                    estimateSpan / trueSpan - 1.0;
                const double speedPercent = speedError * 100.0;

                result.maxSpeedErrorPercent =
                    std::max(
                        result.maxSpeedErrorPercent,
                        std::abs(speedPercent)
                    );
                speedErrorSum += std::abs(speedPercent);
            }
        }

        previousEstimate = estimate;
        previousTrue = trueTime;
        havePrevious = true;
        ++result.samples;
    }

    REQUIRE(result.samples > 0);
    result.rmsErrorMs =
        std::sqrt(errorSquares / static_cast<double>(result.samples)) * 1000.0;
    result.meanSpeedErrorPercent =
        result.samples > 1
            ? speedErrorSum / static_cast<double>(result.samples - 1)
            : 0.0;
    return result;
}

void printMetrics(const char* name, const Metrics& m)
{
    std::cout
        << std::left << std::setw(22) << name
        << " rms=" << std::setw(8) << std::fixed << std::setprecision(3)
        << m.rmsErrorMs << " ms"
        << " max=" << std::setw(8) << m.maxErrorMs << " ms"
        << " max-speed=" << std::setw(8) << m.maxSpeedErrorPercent << " %"
        << " mean-speed=" << m.meanSpeedErrorPercent << " %\n";
}

void testClockStrategiesUnderLatencyJitterAndDrift()
{
    SimulationConfig config;

    LatestOffsetEstimator latest;
    EmaOffsetEstimator ema;
    BoundedPhaseEstimator bounded;
    ProductionEstimator production;

    const Metrics latestMetrics =
        runClockSimulation(latest, config, 0x12345678ULL, true);
    const Metrics emaMetrics =
        runClockSimulation(ema, config, 0x12345678ULL, true);
    const Metrics boundedMetrics =
        runClockSimulation(bounded, config, 0x12345678ULL, true);
    const Metrics productionMetrics =
        runClockSimulation(production, config, 0x12345678ULL, true);

    std::cout << "\nClock strategy simulation:\n";
    printMetrics("latest midpoint", latestMetrics);
    printMetrics("EMA offset", emaMetrics);
    printMetrics("bounded phase", boundedMetrics);
    printMetrics("robust affine PLL", productionMetrics);

    // The production estimator is selected for smoothness first. Jitter must
    // never be converted into visible +/- tens-of-percent time-rate pulses.
    REQUIRE(productionMetrics.maxSpeedErrorPercent < 0.40);
    REQUIRE(productionMetrics.meanSpeedErrorPercent < 0.10);

    // It must also remain reasonably close to the authoritative timeline under
    // 80 ppm client drift, fixed-tick server timestamps and latency spikes.
    REQUIRE(productionMetrics.rmsErrorMs < 45.0);
    REQUIRE(productionMetrics.maxErrorMs < 60.0);

    // Direct/EMA timestamp chasing is intentionally retained here as a
    // regression benchmark: it should be substantially less smooth.
    REQUIRE(
        productionMetrics.maxSpeedErrorPercent <
        emaMetrics.maxSpeedErrorPercent
    );
    REQUIRE(
        productionMetrics.maxSpeedErrorPercent <
        latestMetrics.maxSpeedErrorPercent
    );
    REQUIRE(
        productionMetrics.maxSpeedErrorPercent <
        boundedMetrics.maxSpeedErrorPercent
    );
    REQUIRE(
        productionMetrics.rmsErrorMs <
        boundedMetrics.rmsErrorMs
    );
}

void testClockIsFrameRateIndependent()
{
    SimulationConfig config;

    ProductionEstimator fixedFrame;
    ProductionEstimator variableFrame;

    const Metrics fixed =
        runClockSimulation(fixedFrame, config, 0xCAFEBABEULL, false);
    const Metrics variable =
        runClockSimulation(variableFrame, config, 0xCAFEBABEULL, true);

    // Packet processing is quantized to render frames, so a 20 FPS client can
    // have a little more phase noise than a 60/144 FPS client. What must stay
    // frame-rate independent is the clock rate: neither case may convert that
    // scheduling noise into visible acceleration/deceleration.
    REQUIRE(fixed.rmsErrorMs < 45.0);
    REQUIRE(variable.rmsErrorMs < 45.0);
    REQUIRE(fixed.maxSpeedErrorPercent < 0.20);
    REQUIRE(variable.maxSpeedErrorPercent < 0.20);
}

void testServerTimelineNeverFreezesWithGameplay()
{
    game::server::ServerTimelineClock serverClock;

    constexpr double FixedStep = 0.020;
    for (int i = 0; i < 500; ++i)
        serverClock.advance(FixedStep);

    requireNear(
        serverClock.timeSeconds(),
        10.0,
        1.0e-12,
        "server timeline after 500 fixed steps"
    );

    // Invalid/negative deltas are ignored rather than making the authoritative
    // server clock non-monotonic.
    serverClock.advance(-1.0);
    serverClock.advance(std::numeric_limits<double>::quiet_NaN());
    requireNear(
        serverClock.timeSeconds(),
        10.0,
        1.0e-12,
        "server timeline monotonicity"
    );
}

void testAcceleratedUniverseRemainsAffineToServerTimeline()
{
    game::server::ServerTimelineClock serverClock;
    world::time::UniverseClock universeClock;

    universeClock.reset();
    const double universeStart = universeClock.timeSeconds();

    universeClock.setTimeScale(500.0);
    universeClock.setSimulationMode(true);

    constexpr double FixedStep = 0.020;
    constexpr int Steps = 250;

    for (int i = 0; i < Steps; ++i)
    {
        serverClock.advance(FixedStep);
        universeClock.update(FixedStep);
    }

    const double serverDelta = serverClock.timeSeconds();
    const double universeDelta =
        universeClock.timeSeconds() - universeStart;

    requireNear(serverDelta, 5.0, 1.0e-12, "server timeline delta");
    requireNear(
        universeDelta,
        serverDelta * 500.0,
        1.0e-6,
        "accelerated universe/server affine relation"
    );

    game::client::ClientUniverseTimeline clientTimeline;
    clientTimeline.synchronize(
        0.0,
        universeStart,
        500.0,
        77
    );

    requireNear(
        clientTimeline.timeAtServerTime(serverDelta),
        universeClock.timeSeconds(),
        1.0e-6,
        "client/server accelerated universe agreement"
    );
}

void testUniverseTimelineUsesServerTimeOnly()
{
    game::client::ClientUniverseTimeline timeline;

    timeline.synchronize(100.0, 1000.0, 4.0, 7);
    REQUIRE(timeline.synchronized());
    requireNear(timeline.timeAtServerTime(100.0), 1000.0, 1e-12, "anchor");
    requireNear(timeline.timeAtServerTime(100.25), 1001.0, 1e-12, "scaled time");

    // Same revision: a normal observation does not create a second clock.
    timeline.synchronize(100.50, 1002.0, 4.0, 7);
    requireNear(timeline.timeAtServerTime(100.75), 1003.0, 1e-12, "same revision");

    // Explicit timeline change is atomic.
    timeline.synchronize(200.0, 5000.0, 0.0, 8);
    requireNear(timeline.timeAtServerTime(999.0), 5000.0, 1e-12, "paused revision");
    REQUIRE(timeline.revision() == 8);
}

void testPresentationClockUsesBufferedSnapshotHistory()
{
    game::client::ClientPresentationClock clock;

    double trueServerTime = 0.0;
    double newestSnapshotTime = 0.0;
    double nextSnapshotTime = 0.0;
    double previousRenderTime = 0.0;
    bool havePrevious = false;

    constexpr double FrameDt = 1.0 / 120.0;
    constexpr double SnapshotInterval = 0.060;
    constexpr double OneWayLatency = 0.100;

    for (int frame = 0; frame < 2400; ++frame)
    {
        trueServerTime += FrameDt;

        // Deliver snapshots at 16.7 Hz after 100 ms one-way latency.
        while (nextSnapshotTime + OneWayLatency <= trueServerTime)
        {
            newestSnapshotTime = nextSnapshotTime;
            nextSnapshotTime += SnapshotInterval;
        }

        const bool hasSnapshot = trueServerTime >= OneWayLatency;
        clock.update(
            FrameDt,
            trueServerTime,
            hasSnapshot,
            newestSnapshotTime
        );

        if (!clock.ready() || !hasSnapshot || trueServerTime < 2.0)
            continue;

        const double render = clock.renderTimeSeconds();
        REQUIRE(render <= newestSnapshotTime + 1.0e-12);

        if (havePrevious)
        {
            REQUIRE(render + 1.0e-12 >= previousRenderTime);
            REQUIRE(render - previousRenderTime < 0.020);
        }

        previousRenderTime = render;
        havePrevious = true;
    }

    REQUIRE(clock.hardRebaseCount() == 0);
    REQUIRE(clock.starvationCount() == 0);

    // 100 ms transport age + 100 ms buffered interpolation is the nominal
    // 200 ms presentation delay in this deterministic trace.
    requireNear(
        trueServerTime - clock.renderTimeSeconds(),
        0.200,
        0.025,
        "buffered presentation delay"
    );
}

void testPresentationClockRecoversFromLargeEstimatorLead()
{
    game::client::ClientPresentationClock clock;

    // Stable history before a long synchronous-host frame.
    clock.update(0.016, 1.000, true, 0.900);
    const double before = clock.renderTimeSeconds();

    // The client wall clock advances by 2.2 s, while the capped authoritative
    // runner has advanced only 0.25 s. This reproduces the captured Hub Motion
    // Lab failure: estimated render time is now ~2 seconds beyond snapshot
    // history.
    clock.update(2.200, 3.200, true, 1.150);

    REQUIRE(clock.hardRebaseCount() >= 1);
    REQUIRE(clock.renderTimeSeconds() <= 1.150 + 1.0e-12);
    REQUIRE(clock.renderTimeSeconds() + 1.0e-12 >= before);
    REQUIRE(clock.snapshotLeadSeconds() >= 0.050);

    double newest = 1.150;
    double estimated = 3.200;
    double previous = clock.renderTimeSeconds();

    // Continue with a still-wrong estimated clock. Snapshot history itself
    // must keep the presentation playhead smooth instead of falling back to
    // latest-snapshot hold at 16.7 Hz.
    for (int frame = 0; frame < 600; ++frame)
    {
        constexpr double Dt = 1.0 / 120.0;
        estimated += Dt;

        if ((frame % 7) == 0)
            newest += 0.060;

        clock.update(Dt, estimated, true, newest);
        const double render = clock.renderTimeSeconds();

        REQUIRE(render + 1.0e-12 >= previous);
        REQUIRE(render <= newest + 1.0e-12);
        REQUIRE(render - previous < 0.020);
        previous = render;
    }

    REQUIRE(clock.starvationCount() == 0);
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
    const std::vector<TestCase> tests =
    {
        {
            "clock strategies under latency, jitter and drift",
            testClockStrategiesUnderLatencyJitterAndDrift
        },
        {
            "clock is frame-rate independent",
            testClockIsFrameRateIndependent
        },
        {
            "server timeline never freezes with gameplay",
            testServerTimelineNeverFreezesWithGameplay
        },
        {
            "accelerated universe stays affine to server time",
            testAcceleratedUniverseRemainsAffineToServerTimeline
        },
        {
            "universe timeline uses server time only",
            testUniverseTimelineUsesServerTimeOnly
        },
        {
            "presentation clock uses buffered snapshot history",
            testPresentationClockUsesBufferedSnapshotHistory
        },
        {
            "presentation clock recovers from estimator lead",
            testPresentationClockRecoversFromLargeEstimatorLead
        },
    };

    int failed = 0;

    for (const TestCase& test : tests)
    {
        try
        {
            test.function();
            std::cout << "[PASS] " << test.name << '\n';
        }
        catch (const std::exception& error)
        {
            ++failed;
            std::cerr << "[FAIL] " << test.name << "\n       "
                      << error.what() << '\n';
        }
    }

    std::cout << '\n'
              << (tests.size() - static_cast<std::size_t>(failed))
              << '/' << tests.size() << " tests passed\n";

    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
