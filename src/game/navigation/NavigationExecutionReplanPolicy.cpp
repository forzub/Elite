#include "NavigationExecutionReplanPolicy.h"

#include <cmath>

namespace game::navigation
{
namespace
{

using Policy = NavigationExecutionReplanPolicy;

Policy::Result local(
    Policy::Reason reason,
    bool manual,
    bool immediate
) noexcept
{
    Policy::Result result;
    result.scope = Policy::Scope::LocalHorizon;
    result.reason = reason;
    result.guidanceOnly = manual;
    result.immediate = immediate;
    return result;
}

Policy::Result full(
    Policy::Reason reason,
    bool manual
) noexcept
{
    Policy::Result result;
    result.scope = Policy::Scope::FullRoute;
    result.reason = reason;
    result.guidanceOnly = manual;
    result.immediate = true;
    return result;
}

} // namespace

NavigationExecutionReplanPolicy::Result
NavigationExecutionReplanPolicy::evaluate(
    const Policy& policy,
    const Query& query
) noexcept
{
    Result result;

    if (!std::isfinite(query.universeTimeSeconds) ||
        !std::isfinite(
            query.acceptedSegmentValidUntilUniverseTimeSeconds
        ) ||
        !std::isfinite(
            query.lastManualLocalRefreshUniverseTimeSeconds
        ) ||
        !std::isfinite(policy.manualLocalRefreshSeconds) ||
        policy.manualLocalRefreshSeconds < 0.0)
    {
        Result invalid;
        invalid.scope = Scope::FullRoute;
        invalid.reason = Reason::InvalidInput;
        invalid.guidanceOnly =
            query.mode == ExecutionMode::Manual;
        invalid.immediate = true;
        return invalid;
    }

    const bool manual =
        query.mode == ExecutionMode::Manual;

    // Route intent/topology invalidation changes the branch itself. A local
    // suffix cannot repair that safely.
    if (query.goalIntentChanged)
        return full(Reason::GoalIntentChanged, manual);

    if (!query.globalRouteValid ||
        !query.currentTopologyBranchValid)
    {
        return full(
            Reason::TopologyBranchInvalidated,
            manual
        );
    }

    // Everything below preserves the accepted global route and only replaces
    // the short executable/guidance suffix.
    if (!query.acceptedSegmentValid)
        return local(
            Reason::NoAcceptedSegment,
            manual,
            true
        );

    if (query.vehicleCapabilityChanged)
        return local(
            Reason::VehicleCapabilityChanged,
            manual,
            true
        );

    if (query.staticSafetyInvalidated)
        return local(
            Reason::StaticSafetyInvalidated,
            manual,
            true
        );

    if (query.dynamicHazardInvalidated)
        return local(
            Reason::DynamicHazardInvalidated,
            manual,
            true
        );

    if (!manual && query.trackingErrorExceeded)
        return local(
            Reason::TrackingErrorExceeded,
            false,
            true
        );

    if (manual && query.manualCorridorExited)
        return local(
            Reason::ManualCorridorExit,
            true,
            true
        );

    if (query.acceptedSegmentComplete)
        return local(
            Reason::SegmentCompleted,
            manual,
            true
        );

    if (query.acceptedSegmentValidUntilUniverseTimeSeconds > 0.0 &&
        query.universeTimeSeconds >=
            query.acceptedSegmentValidUntilUniverseTimeSeconds)
    {
        return local(
            Reason::SegmentExpired,
            manual,
            true
        );
    }

    if (manual)
    {
        const double elapsed =
            query.universeTimeSeconds -
            query.lastManualLocalRefreshUniverseTimeSeconds;

        if (policy.manualLocalRefreshSeconds <= 0.0 ||
            elapsed >= policy.manualLocalRefreshSeconds)
        {
            return local(
                Reason::ManualPeriodicRefresh,
                true,
                false
            );
        }

        // Manual flight never inherits automatic steering authority merely
        // because the current recommendation remains valid.
        return result;
    }

    // Stable automatic execution: keep following the already accepted segment.
    // The fixed-step loop may monitor it every frame, but it must not invoke the
    // planner again simply because another frame elapsed.
    result.continueAcceptedAutomaticExecution = true;
    return result;
}

} // namespace game::navigation
