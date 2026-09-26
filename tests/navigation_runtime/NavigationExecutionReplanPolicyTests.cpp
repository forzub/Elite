#include "game/navigation/NavigationExecutionReplanPolicy.h"
#include "game/navigation/AcceptedManeuverProgram.h"
#include "game/navigation/TrajectoryFollower.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

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

void testAutomaticDoesNotReplanEveryFrame()
{
    Replan::Policy policy;
    auto query = stableAutomatic();

    for (int i = 0; i < 120; ++i)
    {
        query.universeTimeSeconds = 100.0 + i / 60.0;
        const auto result = Replan::evaluate(policy, query);

        require(result.scope == Replan::Scope::None,
                "stable automatic execution replanned only because another frame elapsed");
        require(result.continueAcceptedAutomaticExecution,
                "stable automatic execution did not keep following accepted segment");
    }
}

void testAutomaticReplansOnlyLocalSuffixOnHazard()
{
    Replan::Policy policy;
    auto query = stableAutomatic();
    query.dynamicHazardInvalidated = true;

    const auto result = Replan::evaluate(policy, query);
    require(result.scope == Replan::Scope::LocalHorizon &&
            result.reason == Replan::Reason::DynamicHazardInvalidated &&
            result.immediate,
            "new hazard must invalidate only the short accepted suffix");
}

void testExactStaticSafetyInvalidationReplansLocally()
{
    Replan::Policy policy;
    auto query = stableAutomatic();
    query.staticSafetyInvalidated = true;

    const auto result = Replan::evaluate(policy, query);
    require(result.scope == Replan::Scope::LocalHorizon &&
            result.reason == Replan::Reason::StaticSafetyInvalidated &&
            result.immediate,
            "exact-static execution invalidation must immediately rebuild only local suffix");
}

void testTrackingErrorInvalidatesAutomaticSegment()
{
    Replan::Policy policy;
    auto query = stableAutomatic();
    query.trackingErrorExceeded = true;

    const auto result = Replan::evaluate(policy, query);
    require(result.scope == Replan::Scope::LocalHorizon &&
            result.reason == Replan::Reason::TrackingErrorExceeded,
            "automatic execution error must request a local suffix rebuild");
}

void testManualRefreshIsPeriodicAndGuidanceOnly()
{
    Replan::Policy policy;
    policy.manualLocalRefreshSeconds = 0.25;

    Replan::Query query;
    query.mode = Replan::ExecutionMode::Manual;
    query.acceptedSegmentValid = true;
    query.acceptedSegmentValidUntilUniverseTimeSeconds = 110.0;
    query.globalRouteValid = true;
    query.currentTopologyBranchValid = true;
    query.lastManualLocalRefreshUniverseTimeSeconds = 100.0;

    query.universeTimeSeconds = 100.10;
    auto early = Replan::evaluate(policy, query);
    require(early.scope == Replan::Scope::None &&
            !early.continueAcceptedAutomaticExecution,
            "manual mode gained autopilot authority before refresh");

    query.universeTimeSeconds = 100.25;
    auto due = Replan::evaluate(policy, query);
    require(due.scope == Replan::Scope::LocalHorizon &&
            due.reason == Replan::Reason::ManualPeriodicRefresh &&
            due.guidanceOnly &&
            !due.immediate,
            "manual guidance did not request periodic local-only refresh");
}

void testManualCorridorExitReplansImmediately()
{
    Replan::Policy policy;

    Replan::Query query;
    query.mode = Replan::ExecutionMode::Manual;
    query.universeTimeSeconds = 100.01;
    query.acceptedSegmentValid = true;
    query.acceptedSegmentValidUntilUniverseTimeSeconds = 110.0;
    query.globalRouteValid = true;
    query.currentTopologyBranchValid = true;
    query.lastManualLocalRefreshUniverseTimeSeconds = 100.0;
    query.manualCorridorExited = true;

    const auto result = Replan::evaluate(policy, query);
    require(result.scope == Replan::Scope::LocalHorizon &&
            result.reason == Replan::Reason::ManualCorridorExit &&
            result.guidanceOnly &&
            result.immediate,
            "leaving manual corridor must immediately refresh only the local guidance suffix");
}

void testManualDeviationDoesNotForceGlobalRoute()
{
    Replan::Policy policy;

    Replan::Query query;
    query.mode = Replan::ExecutionMode::Manual;
    query.universeTimeSeconds = 100.0;
    query.acceptedSegmentValid = true;
    query.acceptedSegmentValidUntilUniverseTimeSeconds = 110.0;
    query.globalRouteValid = true;
    query.currentTopologyBranchValid = true;
    query.manualCorridorExited = true;

    const auto result = Replan::evaluate(policy, query);
    require(result.scope == Replan::Scope::LocalHorizon,
            "manual local deviation unnecessarily rebuilt the whole route");
}

void testTopologyInvalidationForcesFullRoute()
{
    Replan::Policy policy;
    auto query = stableAutomatic();
    query.currentTopologyBranchValid = false;

    const auto result = Replan::evaluate(policy, query);
    require(result.scope == Replan::Scope::FullRoute &&
            result.reason == Replan::Reason::TopologyBranchInvalidated,
            "invalid topology branch must rebuild the global route");
}

void testVehicleDamageKeepsGlobalRouteButRebuildsTrajectory()
{
    Replan::Policy policy;
    auto query = stableAutomatic();
    query.vehicleCapabilityChanged = true;

    const auto result = Replan::evaluate(policy, query);
    require(result.scope == Replan::Scope::LocalHorizon &&
            result.reason == Replan::Reason::VehicleCapabilityChanged,
            "capability damage should rebuild executable suffix without discarding valid topology");
}

void testSegmentExpiryAdvancesLocally()
{
    Replan::Policy policy;
    auto query = stableAutomatic();
    query.universeTimeSeconds = 105.0;

    const auto result = Replan::evaluate(policy, query);
    require(result.scope == Replan::Scope::LocalHorizon &&
            result.reason == Replan::Reason::SegmentExpired,
            "accepted short segment expiry must request the next local suffix");
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
        testAutomaticDoesNotReplanEveryFrame();
        testAutomaticReplansOnlyLocalSuffixOnHazard();
        testExactStaticSafetyInvalidationReplansLocally();
        testTrackingErrorInvalidatesAutomaticSegment();
        testManualRefreshIsPeriodicAndGuidanceOnly();
        testManualCorridorExitReplansImmediately();
        testManualDeviationDoesNotForceGlobalRoute();
        testTopologyInvalidationForcesFullRoute();
        testVehicleDamageKeepsGlobalRouteButRebuildsTrajectory();
        testSegmentExpiryAdvancesLocally();
        testAcceptedProgramFollowerExecutesWithoutPlannerSearch();
        testEmergencyRecoveryProgramExecutesFixedBrakeDemand();
        testAcceptedHoldWaitsForExpiryInsteadOfCompletingEveryTick();
        testPlanCountRemainsFarBelowExecutionCount();

        std::cout << "NAVIGATION EXECUTION REPLAN POLICY TESTS: PASS\n";
        std::cout << " - stable autopilot executes accepted program without per-frame replanning\n";
        std::cout << " - dynamic hazard/tracking/capability events rebuild only local suffix\n";
        std::cout << " - manual guidance refreshes periodically and immediately on corridor exit\n";
        std::cout << " - manual local deviation preserves the global route branch\n";
        std::cout << " - topology invalidation alone escalates to full-route rebuild\n";
        std::cout << " - accepted program follower runs fixed-step without world search\n";
        std::cout << " - stable execution keeps planCount far below executionCount\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "NAVIGATION EXECUTION REPLAN POLICY TESTS: FAIL: "
            << error.what() << "\n";
        return 1;
    }
}
