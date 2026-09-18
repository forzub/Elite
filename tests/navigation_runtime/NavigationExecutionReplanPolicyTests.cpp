#include "game/navigation/NavigationExecutionReplanPolicy.h"

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

} // namespace

int main()
{
    try
    {
        testAutomaticDoesNotReplanEveryFrame();
        testAutomaticReplansOnlyLocalSuffixOnHazard();
        testTrackingErrorInvalidatesAutomaticSegment();
        testManualRefreshIsPeriodicAndGuidanceOnly();
        testManualCorridorExitReplansImmediately();
        testManualDeviationDoesNotForceGlobalRoute();
        testTopologyInvalidationForcesFullRoute();
        testVehicleDamageKeepsGlobalRouteButRebuildsTrajectory();
        testSegmentExpiryAdvancesLocally();

        std::cout << "NAVIGATION EXECUTION REPLAN POLICY TESTS: PASS\n";
        std::cout << " - stable autopilot executes accepted segment without per-frame replanning\n";
        std::cout << " - dynamic hazard/tracking/capability events rebuild only local suffix\n";
        std::cout << " - manual guidance refreshes periodically and immediately on corridor exit\n";
        std::cout << " - manual local deviation preserves the global route branch\n";
        std::cout << " - topology invalidation alone escalates to full-route rebuild\n";
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
