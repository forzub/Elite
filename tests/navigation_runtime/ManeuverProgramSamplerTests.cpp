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

void testActuatorSegmentIsSampledWithoutReallocation()
{
    Program program = baseProgram();
    program.actuatorSegmentCount = 1;
    auto& segment = program.actuatorSegments[0];
    segment.durationSeconds = 2.0;
    segment.rearMainEnabled = true;
    segment.rearMainThrottleStart01 = 0.20;
    segment.rearMainThrottleEnd01 = 0.80;
    segment.foreMainEnabled = false;
    segment.manoeuvreAccelerationStartMapMps2 =
        {0.0, 1.0, 0.0};
    segment.manoeuvreAccelerationEndMapMps2 =
        {0.0, 2.0, 0.0};
    segment.propulsionFeasible = true;

    const auto midpoint = Sampler::sample(program, 101.0);
    require(
        midpoint.status == Sampler::Status::Active,
        "actuator-program midpoint must remain active"
    );
    require(
        midpoint.hasActuatorCommand,
        "sampler lost explicit planner actuator command"
    );
    require(
        midpoint.actuatorSegmentIndex == 0,
        "sampler selected the wrong actuator interval"
    );
    requireNear(
        midpoint.rearMainThrottle01,
        0.50,
        1.0e-12,
        "rear-main throttle interpolation is wrong"
    );
    requireNear(
        midpoint.foreMainThrottle01,
        0.0,
        0.0,
        "sampler invented a fore main engine"
    );
    requireNear(
        midpoint.manoeuvreAccelerationMapMps2.y,
        1.50,
        1.0e-12,
        "manoeuvre/RCS command interpolation is wrong"
    );
    require(
        midpoint.propulsionFeasible,
        "sampler changed the planner feasibility witness"
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
        testActuatorSegmentIsSampledWithoutReallocation();
        testProgramBoundsClampWithoutCreatingNewTrajectory();
        testInvalidProgramFailsClosed();
        testProgramStorageIsStaticallyBounded();
        testStoragePageSelectionUsesNextPageStart();
        testFollowerCompletionUsesPageLocalElapsedTime();

        std::cout << "MANEUVER PROGRAM SAMPLER TESTS: PASS\n";
        std::cout << " - fixed-capacity AcceptedManeuverProgram\n";
        std::cout << " - exact proved feed-forward survives sampling\n";
        std::cout << " - sampler performs no target-velocity control solve\n";
        std::cout << " - planner actuator intervals are sampled directly\n";
        std::cout << " - invalid time domains fail closed\n";
        std::cout << " - storage pages share one canonical maneuver timeline\n";
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
