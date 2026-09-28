#include "src/game/navigation/AcceptedManeuverProgram.h"
#include "src/game/navigation/ManeuverProgramSampler.h"
#include "src/game/navigation/ManeuverProgramTimeline.h"
#include "src/game/navigation/TrajectoryFollower.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace
{

using Program = game::navigation::AcceptedManeuverProgram;
using Sampler = game::navigation::ManeuverProgramSampler;
using Timeline = game::navigation::ManeuverProgramTimeline;
using Follower = game::navigation::TrajectoryFollower;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void requireNear(
    double actual,
    double expected,
    double tolerance,
    const std::string& message
)
{
    if (std::abs(actual - expected) > tolerance)
        throw std::runtime_error(message);
}

Program baseProgram()
{
    Program program;
    program.valid = true;
    program.revision = 77;
    program.objectiveRevision = 12;
    program.family = Program::ManeuverFamily::LeadRotateMainBurn;
    program.acceptedAtUniverseTimeSeconds = 100.0;
    program.validUntilUniverseTimeSeconds = 103.0;
    program.sampleCount = 2;

    auto& a = program.samples[0];
    a.timeOffsetSeconds = 0.0;
    a.positionMapMeters = {0.0, 0.0, 0.0};
    a.velocityMapMetersPerSecond = {2.0, 0.0, 0.0};
    a.linearAccelerationFeedForwardMapMps2 = {4.0, 0.0, 0.0};
    a.forwardMap = {0.0, 0.0, -1.0};
    a.rightMap = {1.0, 0.0, 0.0};
    a.upMap = {0.0, 1.0, 0.0};
    a.angularVelocityMapRadPerSecond = {0.0, 0.0, 0.2};
    a.angularAccelerationFeedForwardMapRadPerSec2 = {0.0, 0.0, 0.5};

    auto& b = program.samples[1];
    b.timeOffsetSeconds = 2.0;
    b.positionMapMeters = {10.0, 4.0, 0.0};
    b.velocityMapMetersPerSecond = {8.0, 2.0, 0.0};
    b.linearAccelerationFeedForwardMapMps2 = {0.0, 2.0, 0.0};
    b.forwardMap = {1.0, 0.0, 0.0};
    b.rightMap = {0.0, 0.0, 1.0};
    b.upMap = {0.0, 1.0, 0.0};
    b.angularVelocityMapRadPerSecond = {0.0, 0.4, 0.0};
    b.angularAccelerationFeedForwardMapRadPerSec2 = {0.0, 1.5, 0.0};

    program.tracking.positionErrorMeters = 20.0;
    program.tracking.linearVelocityErrorMps = 10.0;
    program.tracking.forwardAngleErrorRad = 0.5;
    program.tracking.angularVelocityErrorRadPerSec = 1.0;
    program.tracking.linearFeedbackReserveMps2 = 2.0;
    program.tracking.angularFeedbackReserveRadPerSec2 = 0.5;
    return program;
}

void testExactAcceptedSamplePreservesFeedForward()
{
    const Program program = baseProgram();

    const auto result = Sampler::sample(program, 100.0);
    require(result.status == Sampler::Status::Active,
            "accepted program must be active at t0");
    require(result.lowerSampleIndex == 0 &&
            result.upperSampleIndex == 0,
            "t0 sample must not invent interpolation");
    requireNear(
        result.reference.linearAccelerationFeedForwardMapMps2.x,
        4.0,
        0.0,
        "sampler changed the exact proved linear feed-forward sample"
    );
    requireNear(
        result.reference.angularAccelerationFeedForwardMapRadPerSec2.z,
        0.5,
        0.0,
        "sampler changed the exact proved angular feed-forward sample"
    );
}

void testMidpointInterpolatesReferenceAndFeedForwardOnly()
{
    const Program program = baseProgram();

    const auto result = Sampler::sample(program, 101.0);
    require(result.status == Sampler::Status::Active,
            "mid-program sample must remain active");
    require(result.lowerSampleIndex == 0 &&
            result.upperSampleIndex == 1,
            "mid-program sample must identify the bounding control keys");
    requireNear(result.interpolation01, 0.5, 1.0e-12,
                "mid-program interpolation fraction is wrong");

    requireNear(result.reference.positionMapMeters.x, 5.0, 1.0e-12,
                "sampler changed reference position interpolation");
    requireNear(result.reference.positionMapMeters.y, 2.0, 1.0e-12,
                "sampler changed reference position interpolation");
    requireNear(result.reference.velocityMapMetersPerSecond.x, 5.0, 1.0e-12,
                "sampler changed reference velocity interpolation");
    requireNear(
        result.reference.linearAccelerationFeedForwardMapMps2.x,
        2.0,
        1.0e-12,
        "sampler must interpolate the proved feed-forward program rather than re-solve control"
    );
    requireNear(
        result.reference.linearAccelerationFeedForwardMapMps2.y,
        1.0,
        1.0e-12,
        "sampler lost a feed-forward component"
    );
    requireNear(
        result.reference.angularAccelerationFeedForwardMapRadPerSec2.y,
        0.75,
        1.0e-12,
        "sampler changed angular feed-forward interpolation"
    );
    requireNear(
        result.reference.angularAccelerationFeedForwardMapRadPerSec2.z,
        0.25,
        1.0e-12,
        "sampler changed angular feed-forward interpolation"
    );

    requireNear(glm::length(result.reference.forwardMap), 1.0, 1.0e-12,
                "sampled forward basis is not normalized");
    requireNear(glm::length(result.reference.rightMap), 1.0, 1.0e-12,
                "sampled right basis is not normalized");
    requireNear(glm::length(result.reference.upMap), 1.0, 1.0e-12,
                "sampled up basis is not normalized");
    requireNear(
        glm::dot(result.reference.forwardMap, result.reference.upMap),
        0.0,
        1.0e-12,
        "sampled body basis lost orthogonality"
    );
}

void testProgramBoundsClampWithoutCreatingNewTrajectory()
{
    const Program program = baseProgram();

    const auto before = Sampler::sample(program, 99.5);
    require(before.status == Sampler::Status::BeforeStart,
            "pre-start sampling must be explicit");
    requireNear(before.reference.positionMapMeters.x, 0.0, 0.0,
                "pre-start sampling must clamp to the accepted first sample");

    const auto after = Sampler::sample(program, 102.5);
    require(after.status == Sampler::Status::AfterEnd,
            "post-program sampling must be explicit");
    requireNear(after.reference.positionMapMeters.x, 10.0, 0.0,
                "post-program sampling must clamp to the accepted final sample");
    requireNear(
        after.reference.linearAccelerationFeedForwardMapMps2.y,
        2.0,
        0.0,
        "post-program sampling must not synthesize a new terminal command"
    );
}

void testInvalidProgramFailsClosed()
{
    Program program = baseProgram();
    program.samples[1].timeOffsetSeconds = 0.0;

    const auto duplicateTime = Sampler::sample(program, 100.0);
    require(duplicateTime.status == Sampler::Status::InvalidInput,
            "non-monotonic maneuver keys must fail closed");

    program = baseProgram();
    program.validUntilUniverseTimeSeconds = 101.0;

    const auto outsideValidity = Sampler::sample(program, 100.5);
    require(outsideValidity.status == Sampler::Status::InvalidInput,
            "program whose proved time domain exceeds validity must fail closed");
}

void testProgramStorageIsStaticallyBounded()
{
    static_assert(
        Program::kMaxSamples == 16,
        "accepted maneuver program capacity changed unexpectedly"
    );

    Program program = baseProgram();
    require(program.samples.size() == Program::kMaxSamples,
            "accepted maneuver program must use fixed-capacity storage");
}

void testStoragePageSelectionUsesNextPageStart()
{
    Program pages[2] = {baseProgram(), baseProgram()};
    pages[0].acceptedAtUniverseTimeSeconds = 50.0;
    pages[0].sequenceStartOffsetSeconds = 0.0;
    pages[0].samples[1].timeOffsetSeconds = 0.3;
    pages[0].validUntilUniverseTimeSeconds = 51.0;

    pages[1].revision = 78;
    pages[1].acceptedAtUniverseTimeSeconds = 50.0;
    pages[1].sequenceStartOffsetSeconds = 0.3;
    pages[1].samples[1].timeOffsetSeconds = 0.2;
    pages[1].validUntilUniverseTimeSeconds = 51.0;

    const auto secondWindow = Timeline::pageWindow(pages[1]);
    require(secondWindow.valid, "second storage-page window is invalid");

    const double immediatelyBeforeSecondPage = std::nextafter(
        secondWindow.startUniverseTimeSeconds,
        -std::numeric_limits<double>::infinity()
    );
    const auto before = Timeline::selectActivePage(
        pages,
        2,
        immediatelyBeforeSecondPage,
        0
    );
    require(
        before.status == Timeline::SelectionStatus::Active &&
        before.pageIndex == 0,
        "timeline selected a storage page before its own canonical start"
    );

    const auto atStart = Timeline::selectActivePage(
        pages,
        2,
        secondWindow.startUniverseTimeSeconds,
        0
    );
    require(
        atStart.status == Timeline::SelectionStatus::Active &&
        atStart.pageIndex == 1 &&
        atStart.pagesAdvanced == 1,
        "timeline did not advance exactly at the next storage-page start"
    );

    requireNear(
        Timeline::elapsedPageSeconds(
            pages[1],
            secondWindow.startUniverseTimeSeconds
        ),
        0.0,
        0.0,
        "page-local elapsed time did not share the maneuver epoch"
    );
}

void testStoragePageContinuityAtRealUniverseEpoch()
{
    Program pages[2] = {baseProgram(), baseProgram()};
    constexpr double epoch = 875000000.0;
    pages[0].acceptedAtUniverseTimeSeconds = epoch;
    pages[0].sequenceStartOffsetSeconds = 0.1;
    pages[0].samples[1].timeOffsetSeconds = 0.2;
    pages[0].validUntilUniverseTimeSeconds = epoch + 2.0;

    pages[1].acceptedAtUniverseTimeSeconds = epoch;
    pages[1].sequenceStartOffsetSeconds = 0.3;
    pages[1].samples[1].timeOffsetSeconds = 0.2;
    pages[1].validUntilUniverseTimeSeconds = epoch + 2.0;

    const auto firstWindow = Timeline::pageWindow(pages[0]);
    const auto secondWindow = Timeline::pageWindow(pages[1]);
    require(std::abs(secondWindow.startUniverseTimeSeconds -
                     firstWindow.endUniverseTimeSeconds) > 1.0e-9,
            "fixture must expose absolute-epoch rounding");

    const auto first = Timeline::selectActivePage(
        pages, 2, firstWindow.startUniverseTimeSeconds, 0
    );
    require(first.status == Timeline::SelectionStatus::Active &&
                first.pageIndex == 0,
            "large universe epoch rejected the first continuous page");

    const auto second = Timeline::selectActivePage(
        pages, 2, secondWindow.startUniverseTimeSeconds, 0
    );
    require(second.status == Timeline::SelectionStatus::Active &&
                second.pageIndex == 1,
            "large universe epoch blocked the next continuous page");

    pages[1].sequenceStartOffsetSeconds += 0.01;
    const auto gap = Timeline::selectActivePage(
        pages, 2, firstWindow.startUniverseTimeSeconds, 0
    );
    require(gap.status == Timeline::SelectionStatus::InvalidInput,
            "real page discontinuity must still be rejected");
}

void testSpatialSamplerFollowsVehicleInsteadOfNominalClock()
{
    Program program = baseProgram();
    program.referenceMode = Program::ReferenceMode::SpatialCorridor;

    // Nominal time is already beyond this page, but the craft is physically
    // only 25% along the accepted segment. Spatial mode must stay with the
    // craft instead of jumping to the terminal reference.
    const double segmentLength = std::sqrt(116.0);
    const glm::dvec3 oneMeterPerpendicular(
        -4.0 / segmentLength,
        10.0 / segmentLength,
        0.0
    );
    const auto result = Sampler::sampleSpatial(
        program,
        102.5,
        glm::dvec3(2.5, 1.0, 0.0) +
            oneMeterPerpendicular,
        0
    );

    require(result.status == Sampler::Status::Active,
            "spatial sampler became time-expired");
    require(result.spatialReference,
            "spatial sampler did not mark its reference mode");
    require(result.lowerSampleIndex == 0 &&
                result.upperSampleIndex == 1,
            "spatial sampler selected the wrong accepted segment");
    requireNear(
        result.interpolation01,
        0.25,
        1.0e-12,
        "spatial sampler followed nominal clock instead of vehicle progress"
    );
    requireNear(
        result.reference.positionMapMeters.x,
        2.5,
        1.0e-12,
        "spatial reference did not project onto accepted geometry"
    );
    requireNear(
        result.spatialDistanceMeters,
        1.0,
        1.0e-12,
        "spatial sampler cross-track distance is wrong"
    );
}

void testSpatialSamplerNeverJumpsBehindMonotonicCursor()
{
    Program program = baseProgram();
    program.referenceMode = Program::ReferenceMode::SpatialCorridor;
    program.sampleCount = 4;
    program.validUntilUniverseTimeSeconds = 110.0;

    for (std::size_t i = 0; i < 4; ++i)
    {
        auto& sample = program.samples[i];
        sample.timeOffsetSeconds = static_cast<double>(i);
        sample.positionMapMeters =
            {10.0 * static_cast<double>(i), 0.0, 0.0};
        sample.velocityMapMetersPerSecond = {10.0, 0.0, 0.0};
        sample.linearAccelerationFeedForwardMapMps2 =
            glm::dvec3(0.0);
        sample.forwardMap = {1.0, 0.0, 0.0};
        sample.rightMap = {0.0, 0.0, 1.0};
        sample.upMap = {0.0, 1.0, 0.0};
        sample.angularVelocityMapRadPerSecond = glm::dvec3(0.0);
        sample.angularAccelerationFeedForwardMapRadPerSec2 =
            glm::dvec3(0.0);
    }

    const auto result = Sampler::sampleSpatial(
        program,
        101.0,
        glm::dvec3(5.0, 0.0, 0.0),
        2
    );

    require(result.status == Sampler::Status::Active,
            "monotonic spatial cursor invalidated a valid program");
    require(result.lowerSampleIndex == 2,
            "spatial sampler jumped backwards behind its monotonic cursor");
    requireNear(
        result.reference.positionMapMeters.x,
        20.0,
        1.0e-12,
        "monotonic spatial cursor was ignored"
    );
}

void testSpatialPageSelectionUsesPhysicalProgressNotTime()
{
    Program pages[2] = {baseProgram(), baseProgram()};
    for (auto& page : pages)
    {
        page.referenceMode = Program::ReferenceMode::SpatialCorridor;
        page.acceptedAtUniverseTimeSeconds = 50.0;
        page.validUntilUniverseTimeSeconds = 1000.0;
        page.samples[0].timeOffsetSeconds = 0.0;
        page.samples[1].timeOffsetSeconds = 10.0;
        page.samples[0].velocityMapMetersPerSecond =
            {1.0, 0.0, 0.0};
        page.samples[1].velocityMapMetersPerSecond =
            {1.0, 0.0, 0.0};
    }

    pages[0].sequenceStartOffsetSeconds = 0.0;
    pages[0].samples[0].positionMapMeters = {0.0, 0.0, 0.0};
    pages[0].samples[1].positionMapMeters = {10.0, 0.0, 0.0};

    pages[1].revision = 78;
    pages[1].sequenceStartOffsetSeconds = 10.0;
    pages[1].samples[0].positionMapMeters = {10.0, 0.0, 0.0};
    pages[1].samples[1].positionMapMeters = {20.0, 0.0, 0.0};

    const auto stillFirst = Timeline::selectSpatialPage(
        pages,
        2,
        500.0,
        glm::dvec3(5.0, 0.0, 0.0),
        0
    );
    require(
        stillFirst.status == Timeline::SelectionStatus::Active &&
        stillFirst.pageIndex == 0,
        "nominal time advanced a spatial storage page ahead of the craft"
    );

    const auto crossed = Timeline::selectSpatialPage(
        pages,
        2,
        500.0,
        glm::dvec3(11.0, 0.0, 0.0),
        0
    );
    require(
        crossed.status == Timeline::SelectionStatus::Active &&
        crossed.pageIndex == 1 &&
        crossed.pagesAdvanced == 1,
        "spatial storage page did not advance after physical endpoint crossing"
    );
}

void testFollowerCompletionUsesPageLocalElapsedTime()
{
    Program page = baseProgram();
    page.acceptedAtUniverseTimeSeconds = 100.0;
    page.sequenceStartOffsetSeconds = 5.0;
    page.validUntilUniverseTimeSeconds = 108.0;
    page.completionTriggersReplan = true;
    page.samples[1].positionMapMeters =
        page.samples[0].positionMapMeters;
    page.samples[1].velocityMapMetersPerSecond =
        page.samples[0].velocityMapMetersPerSecond;
    page.samples[1].forwardMap = page.samples[0].forwardMap;
    page.samples[1].rightMap = page.samples[0].rightMap;
    page.samples[1].upMap = page.samples[0].upMap;
    page.samples[0].angularVelocityMapRadPerSecond = glm::dvec3(0.0);
    page.samples[1].angularVelocityMapRadPerSecond = glm::dvec3(0.0);

    Follower::AgentState agent;
    agent.positionMapMeters = page.samples[1].positionMapMeters;
    agent.velocityMapMetersPerSecond =
        page.samples[1].velocityMapMetersPerSecond;
    agent.forwardMap = page.samples[1].forwardMap;
    agent.rightMap = page.samples[1].rightMap;
    agent.upMap = page.samples[1].upMap;

    const game::navigation::ManeuverTrackingController::Policy policy;
    const auto beforeLocalEnd = Follower::follow(
        page,
        106.0,
        agent,
        policy
    );
    require(
        beforeLocalEnd.status == Follower::Status::Following,
        "Follower completed a later storage page using global maneuver age"
    );

    const auto atLocalEnd = Follower::follow(
        page,
        107.0,
        agent,
        policy
    );
    require(
        atLocalEnd.status == Follower::Status::Complete,
        "Follower did not complete at the page-local nominal end"
    );
}

} // namespace

int main()
{
    try
    {
        testExactAcceptedSamplePreservesFeedForward();
        testMidpointInterpolatesReferenceAndFeedForwardOnly();
        testProgramBoundsClampWithoutCreatingNewTrajectory();
        testInvalidProgramFailsClosed();
        testProgramStorageIsStaticallyBounded();
        testStoragePageSelectionUsesNextPageStart();
        testStoragePageContinuityAtRealUniverseEpoch();
        testSpatialSamplerFollowsVehicleInsteadOfNominalClock();
        testSpatialSamplerNeverJumpsBehindMonotonicCursor();
        testSpatialPageSelectionUsesPhysicalProgressNotTime();
        testFollowerCompletionUsesPageLocalElapsedTime();

        std::cout << "MANEUVER PROGRAM SAMPLER TESTS: PASS\n";
        std::cout << " - fixed-capacity AcceptedManeuverProgram\n";
        std::cout << " - exact proved feed-forward survives sampling\n";
        std::cout << " - sampler performs no target-velocity control solve\n";
        std::cout << " - velocity and full-attitude reference keys are sampled directly\n";
        std::cout << " - invalid time domains fail closed\n";
        std::cout << " - storage pages share one canonical maneuver timeline\n";
        std::cout << " - spatial corridors follow physical progress, not nominal time\n";
        std::cout << " - spatial storage pages advance only after endpoint crossing\n";
        std::cout << " - monotonic spatial cursor cannot jump backwards\n";
        std::cout << " - Follower completion uses page-local elapsed time\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "MANEUVER PROGRAM SAMPLER TESTS: FAIL: "
            << error.what() << "\n";
        return 1;
    }
}
