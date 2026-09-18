#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/NavigationRuntimeControlBridge.h"
#include "src/world/navigation/local/LocalAvoidancePlanner.h"
#include "src/world/navigation/map/NavigationMap.h"
#include "src/world/navigation/space/NavigationSpace.h"

namespace game::navigation
{

// Live composition seam between accepted NavigationWorld products and the
// accepted pilot/runtime-control bridge.
//
// This class does not own world geometry, spatial indexes, vehicle physics or
// replication. It consumes one already-published dynamic query plus one
// NavigationSpace snapshot, selects the next bounded target, and converts that
// target into the ideal acceleration intent consumed by PilotSkillExecutor.
class NavigationRuntimePlanner final
{
public:
    using Map = world::navigation::NavigationMap;
    using Space = world::navigation::NavigationSpace;
    using Horizon = world::navigation::LocalHorizonPlanner;
    using Avoidance = world::navigation::LocalAvoidancePlanner;
    using Bridge = NavigationRuntimeControlBridge;

    struct AgentState
    {
        Map::EntityId entityId = 0;

        glm::dvec3 positionMapMeters {0.0};
        glm::dvec3 velocityMapMetersPerSecond {0.0};
        glm::dvec3 accelerationMapMetersPerSecond2 {0.0};
        double radiusMeters = 0.0;

        glm::dvec3 forwardMap {0.0, 0.0, -1.0};
        glm::dvec3 rightMap {1.0, 0.0, 0.0};
        glm::dvec3 upMap {0.0, 1.0, 0.0};

        double pitchRateRadPerSec = 0.0;
        double yawRateRadPerSec = 0.0;
        double rollRateRadPerSec = 0.0;
    };

    struct Goal
    {
        std::uint64_t revision = 1;

        glm::dvec3 targetPositionMapMeters {0.0};
        glm::dvec3 targetVelocityMapMetersPerSecond {0.0};
        glm::dvec3 targetAccelerationMapMetersPerSecond2 {0.0};

        double maximumTargetSpeedMps = 20.0;
        double velocityResponsePerSecond = 0.75;
        double angularDampingPerSecond = 2.0;
        double arrivalRadiusMeters = 1.0;

        bool emergency = false;
        double hazardUrgency01 = 0.0;
    };

    struct Policy
    {
        Space::CorridorCostPolicy corridor {};
        Horizon::Policy horizon {};
        Avoidance::Policy avoidance {};
    };

    enum class Status : std::uint8_t
    {
        NominalClear = 0,
        AdjustedClear,
        ConflictHold,
        StaleHold,
        StaticHold,
        InvalidInput
    };

    struct Result
    {
        Status status = Status::InvalidInput;

        // Planner-space product. All vectors here are expressed in the
        // NavigationMap working frame. Before this intent enters the accepted
        // Stage-11 PilotSkillExecutor / ShipControlState seam it MUST pass
        // through mapIntentToWorld().
        Bridge::Intent intent {};

        bool safeProgressTargetDemonstrated = false;
        bool usedPortalWaypoint = false;
        bool adjustedTarget = false;

        glm::dvec3 coarseWaypointMapMeters {0.0};
        glm::dvec3 selectedTargetMapMeters {0.0};
        glm::dvec3 desiredVelocityMapMetersPerSecond {0.0};

        Space::Revision spaceRevision = 0;
        Space::Revision spaceSourceRevision = 0;
        Map::Revision mapRevision = 0;
        Map::Revision mapSourceRevision = 0;

        std::vector<Space::RegionId> staticRegionPath;
        std::vector<Space::PortalId> staticPortalPath;
        std::vector<Space::Vec3d> staticPortalCentersMapMeters;

        Map::EntityId primaryConflictEntityId = 0;
        Map::EntityId nominalPrimaryConflictEntityId = 0;
        std::size_t dynamicCandidatesExamined = 0;
        std::size_t dynamicConflictsFound = 0;
        std::size_t nominalDynamicConflictsFound = 0;
        std::size_t avoidanceProbesExamined = 0;
    };

    [[nodiscard]] static Result plan(
        const AgentState& agent,
        const Goal& goal,
        const Map::QueryResult& dynamicCandidates,
        double dynamicResultAgeSeconds,
        const Space& staticSpace,
        const Policy& policy
    );

    // NavigationWorld planning products are expressed in the published map
    // working frame. Stage-11 control/physics consumes world/system-space
    // acceleration demand. This explicit boundary prevents identity-frame
    // tests from hiding a missing basis transform.
    [[nodiscard]] static Bridge::Intent mapIntentToWorld(
        const Bridge::Intent& mapIntent,
        const Map::WorkingFrame& workingFrame
    );
};

} // namespace game::navigation
