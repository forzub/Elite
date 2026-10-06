#include "game/navigation/NavigationExecutionReplanPolicy.h"
#include "game/navigation/AcceptedManeuverProgram.h"
#include "game/navigation/TrajectoryFollower.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

#include <glm/glm.hpp>

namespace
{

using Replan = game::navigation::NavigationExecutionReplanPolicy;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

Replan::Query stableAutomatic()
{
    Replan::Query query;
    query.mode = Replan::ExecutionMode::Automatic;
    query.universeTimeSeconds = 100.0;
    query.acceptedSegmentValid = true;
    query.acceptedSegmentValidUntilUniverseTimeSeconds = 105.0;
    query.globalRouteValid = true;
    query.currentTopologyBranchValid = true;
    return query;
}

game::navigation::AcceptedManeuverProgram makeProgram(
    double acceptedAt,
    double validUntil,
    std::uint64_t revision,
    std::uint64_t objectiveRevision,
    const glm::dvec3& startPosition,
    const glm::dvec3& targetPosition,
    const glm::dvec3& targetVelocity,
    const glm::dvec3& feedForward = glm::dvec3(0.0)
)
{
    using Program = game::navigation::AcceptedManeuverProgram;

    Program program;
    program.valid = true;
    program.revision = revision;
    program.objectiveRevision = objectiveRevision;
    program.family = Program::ManeuverFamily::FreeTransit;
    program.acceptedAtUniverseTimeSeconds = acceptedAt;
    program.validUntilUniverseTimeSeconds = validUntil;
    program.sampleCount = 2;

    program.samples[0].timeOffsetSeconds = 0.0;
    program.samples[0].positionMapMeters = startPosition;
    program.samples[0].velocityMapMetersPerSecond = targetVelocity;
    program.samples[0].linearAccelerationFeedForwardMapMps2 = feedForward;

    program.samples[1] = program.samples[0];
    program.samples[1].timeOffsetSeconds = validUntil - acceptedAt;
    program.samples[1].positionMapMeters = targetPosition;

    program.tracking.linearFeedbackReserveMps2 = 100.0;
    program.tracking.angularFeedbackReserveRadPerSec2 = 100.0;
    program.terminalTolerance.positionMeters = 1.0;
    program.terminalTolerance.linearVelocityMps = 1000.0;
    program.terminalTolerance.forwardAngleRad = 3.14159265358979323846;
    program.terminalTolerance.angularVelocityRadPerSec = 1000.0;
    return program;
}

void testAcceptedProgramFollowerExecutesWithoutPlannerSearch()
{
    using Follower = game::navigation::TrajectoryFollower;

    auto program = makeProgram(
        100.0,
        102.0,
        77,
        5,
        {0.0, 0.0, 0.0},
        {100.0, 0.0, 0.0},
        {10.0, 0.0, 0.0}
    );
    program.tracking.positionErrorMeters = 20.0;

    game::navigation::ManeuverTrackingController::Policy tracking;
    tracking.positionGainPerSecond2 = 0.0;
    tracking.velocityGainPerSecond = 0.5;
    tracking.attitudeGainPerSecond2 = 0.0;
    tracking.angularVelocityGainPerSecond = 2.0;

    Follower::AgentState agent;
    agent.positionMapMeters = {10.0, 0.0, 0.0};
    agent.velocityMapMetersPerSecond = {4.0, 0.0, 0.0};

    const auto first = Follower::follow(
        program,
        100.0,
        agent,
        tracking
    );
    require(first.status == Follower::Status::Following,
            "accepted program follower did not remain active");
    require(first.intent.revision == 5,
            "follower did not preserve high-level objective revision");
    require(first.intent.targetRevision == 77,
            "follower did not publish accepted program revision");
    require(std::abs(
                first.intent.
                    idealLinearAccelerationLocalMps2.x -
                3.0) <= 1.0e-12,
            "follower did not track accepted reference velocity");

    agent.positionMapMeters = {20.0, 25.0, 0.0};
    const auto escaped = Follower::follow(
        program,
        100.0,
        agent,
        tracking
    );
    require(escaped.trackingErrorExceeded,
            "follower did not publish accepted-program envelope escape");
}

void testEmergencyRecoveryProgramExecutesFixedBrakeDemand()
{
    using Program = game::navigation::AcceptedManeuverProgram;
    using Follower = game::navigation::TrajectoryFollower;

    auto program = makeProgram(
        10.0,
        10.25,
        89,
        7,
        {0.0, 0.0, 0.0},
        {20.0, 0.0, 0.0},
        {0.0, 0.0, 0.0},
        {-2.0, 0.0, 0.0}
    );
    program.family = Program::ManeuverFamily::EmergencyRecovery;
    program.completionTriggersReplan = false;
    program.emergency = true;
    program.hazardUrgency01 = 1.0;

    game::navigation::ManeuverTrackingController::Policy tracking;
    tracking.positionGainPerSecond2 = 0.0;
    tracking.velocityGainPerSecond = 0.0;
    tracking.attitudeGainPerSecond2 = 0.0;
    tracking.angularVelocityGainPerSecond = 2.0;

    Follower::AgentState agent;
    agent.velocityMapMetersPerSecond = {8.0, 0.0, 0.0};

    const auto followed = Follower::follow(
        program,
        10.0,
        agent,
        tracking
    );
    require(followed.status == Follower::Status::Following,
            "emergency recovery program stopped before its validity window");
    require(followed.intent.emergency &&
            std::abs(followed.intent.hazardUrgency01 - 1.0) <= 1.0e-12,
            "emergency recovery metadata did not reach control intent");
    require(std::abs(
                followed.intent.
                    idealLinearAccelerationLocalMps2.x +
                2.0) <= 1.0e-12,
            "emergency recovery did not preserve fixed braking demand");
}

void testAcceptedHoldWaitsForExpiryInsteadOfCompletingEveryTick()
{
    using Follower = game::navigation::TrajectoryFollower;

    auto program = makeProgram(
        100.0,
        100.25,
        88,
        1,
        {0.0, 0.0, 0.0},
        {0.0, 0.0, 0.0},
        {0.0, 0.0, 0.0}
    );
    program.completionTriggersReplan = false;

    Follower::AgentState agent;
    const auto followed = Follower::follow(
        program,
        100.0,
        agent,
        game::navigation::ManeuverTrackingController::Policy {}
    );
    require(followed.status == Follower::Status::Following,
            "time-bounded hold completed immediately and would replan every tick");
}

void testPlanCountRemainsFarBelowExecutionCount()
{
    using Follower = game::navigation::TrajectoryFollower;

    auto program = makeProgram(
        100.0,
        103.0,
        91,
        7,
        {0.0, 0.0, 0.0},
        {1000.0, 0.0, 0.0},
        {20.0, 0.0, 0.0}
    );

    game::navigation::ManeuverTrackingController::Policy tracking;
    tracking.positionGainPerSecond2 = 0.0;
    tracking.velocityGainPerSecond = 0.5;
    tracking.attitudeGainPerSecond2 = 0.0;
    tracking.angularVelocityGainPerSecond = 2.0;

    Follower::AgentState agent;

    std::uint64_t planCount = 1;
    std::uint64_t executionCount = 0;

    Replan::Policy policy;
    Replan::Query query = stableAutomatic();
    query.acceptedSegmentValid = true;
    query.acceptedSegmentValidUntilUniverseTimeSeconds =
        program.validUntilUniverseTimeSeconds;

    for (int i = 0; i < 120; ++i)
    {
        query.universeTimeSeconds =
            100.0 + static_cast<double>(i) / 60.0;

        const auto scheduling = Replan::evaluate(policy, query);
        require(scheduling.scope == Replan::Scope::None &&
                scheduling.continueAcceptedAutomaticExecution,
                "stable accepted program unexpectedly woke the planner");

        const auto followed = Follower::follow(
            program,
            query.universeTimeSeconds,
            agent,
            tracking
        );
        require(followed.status == Follower::Status::Following,
                "stable accepted program stopped following");
        ++executionCount;
    }

    require(planCount * 20 < executionCount,
            "accepted-program seam did not separate planning from execution cadence");
}


} // namespace

int main()
{
    try
    {
        testAcceptedProgramFollowerExecutesWithoutPlannerSearch();
        testEmergencyRecoveryProgramExecutesFixedBrakeDemand();
        testAcceptedHoldWaitsForExpiryInsteadOfCompletingEveryTick();
        testPlanCountRemainsFarBelowExecutionCount();

        std::cout << "LEGACY ACCEPTED-PROGRAM FOLLOWER TESTS: PASS\n";
        std::cout << " - historical TrajectoryFollower execution behavior only\n";
        std::cout << " - not part of the PredictivePilot V2 production acceptance gate\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "LEGACY ACCEPTED-PROGRAM FOLLOWER TESTS: FAIL: "
            << error.what() << "\n";
        return 1;
    }
}
