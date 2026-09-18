#pragma once

#include <cstdint>

namespace game::navigation
{

// Decides when an already accepted short-horizon navigation product must be
// rebuilt. It does not build trajectories and does not execute controls.
//
// Automatic execution is plan -> accept -> execute -> monitor. Manual guidance
// additionally receives a periodic local refresh because the player may depart
// from the recommended corridor at any time.
class NavigationExecutionReplanPolicy final
{
public:
    enum class ExecutionMode : std::uint8_t
    {
        Automatic = 0,
        Manual
    };

    enum class Scope : std::uint8_t
    {
        None = 0,
        LocalHorizon,
        FullRoute
    };

    enum class Reason : std::uint8_t
    {
        None = 0,
        NoAcceptedSegment,
        SegmentExpired,
        SegmentCompleted,
        TrackingErrorExceeded,
        ManualCorridorExit,
        StaticSafetyInvalidated,
        DynamicHazardInvalidated,
        VehicleCapabilityChanged,
        GoalIntentChanged,
        TopologyBranchInvalidated,
        ManualPeriodicRefresh,
        InvalidInput
    };

    struct Policy
    {
        // Manual guidance is advisory and may be locally refreshed even while
        // the player remains inside the recommended corridor. This is a local
        // suffix refresh only; it is not a full route solve.
        double manualLocalRefreshSeconds = 0.25;
    };

    struct Query
    {
        ExecutionMode mode = ExecutionMode::Automatic;
        double universeTimeSeconds = 0.0;

        bool acceptedSegmentValid = false;
        double acceptedSegmentValidUntilUniverseTimeSeconds = 0.0;
        bool acceptedSegmentComplete = false;

        // Tracking/corridor errors are already measured by the execution or
        // guidance monitor. This policy does not duplicate geometry math.
        bool trackingErrorExceeded = false;
        bool manualCorridorExited = false;

        // The current accepted execution is no longer physically safe against
        // authoritative exact-static HitVolumes under current kinematics.
        bool staticSafetyInvalidated = false;

        // The accepted prediction was invalidated by a newly observed dynamic
        // hazard, not merely by the passage of one fixed simulation frame.
        bool dynamicHazardInvalidated = false;

        // A damaged/changed vehicle can invalidate the previously proven
        // trajectory without invalidating the global route topology.
        bool vehicleCapabilityChanged = false;

        // Route intent and topology are the only conditions here that demand a
        // full route rebuild rather than a short local suffix.
        bool goalIntentChanged = false;
        bool globalRouteValid = true;
        bool currentTopologyBranchValid = true;

        double lastManualLocalRefreshUniverseTimeSeconds = 0.0;
    };

    struct Result
    {
        Scope scope = Scope::None;
        Reason reason = Reason::None;

        // Automatic mode may continue following the accepted segment when no
        // invalidation exists. Manual mode never acquires vehicle authority.
        bool continueAcceptedAutomaticExecution = false;

        // Manual refreshes update advisory corridor only.
        bool guidanceOnly = false;

        // Immediate means do not wait for the next periodic manual refresh.
        bool immediate = false;
    };

    [[nodiscard]] static Result evaluate(
        const Policy& policy,
        const Query& query
    ) noexcept;
};

} // namespace game::navigation
