#pragma once

#include <cstddef>
#include <cstdint>

#include "world/navigation/map/NavigationMap.h"

namespace world::navigation
{

// Backend-neutral consumer of already reduced NavigationMap products.
// It owns no actor table, spatial index, GPU state or global route planner.
class LocalHorizonPlanner final
{
public:
    using EntityId = NavigationMap::EntityId;
    using Revision = NavigationMap::Revision;
    using Vec3d = NavigationMap::Vec3d;

    struct AgentState
    {
        EntityId entityId = 0;
        Vec3d positionMapMeters {};
        Vec3d velocityMapMetersPerSecond {};
        Vec3d accelerationMapMetersPerSecond2 {};
        double radiusMeters = 0.0;
    };

    // Upstream route intent. The local layer is allowed to shorten this target
    // to the current physical horizon, but it does not invent a new global path.
    struct NominalTargetState
    {
        Vec3d positionMapMeters {};
        Vec3d velocityMapMetersPerSecond {};
        Vec3d accelerationMapMetersPerSecond2 {};
    };

    struct Policy
    {
        // Dynamic conflict look-ahead. This is intentionally bounded and must
        // not be interpreted as a whole-route planning duration.
        double lookAheadSeconds = 3.0;

        // Completed NavigationMap results older than this fail closed.
        double maxResultAgeSeconds = 0.25;

        // Used only to size the physical receding horizon.
        double maxBrakingAccelerationMetersPerSecond2 = 10.0;
        double turnDistanceMeters = 0.0;
        double safetyMarginMeters = 10.0;
        double minimumHorizonMeters = 10.0;
    };

    struct Query
    {
        AgentState agent {};
        NominalTargetState nominalTarget {};

        // Age of the completed NavigationMap result being consumed. The map
        // itself remains immutable to this planner.
        double dynamicResultAgeSeconds = 0.0;
        Policy policy {};
    };

    enum class Status
    {
        Clear,
        ConflictHold,
        StaleHold
    };

    enum class TargetMode
    {
        // Intermediate point on the accepted route intent. Downstream local
        // kinematics must treat it as pass-through rather than terminal stop.
        PassThrough,

        // The nominal target lies inside the current physical horizon; its
        // supplied terminal velocity/acceleration may be consumed downstream.
        Terminal,

        // No safe progressing target was demonstrated. Downstream control must
        // use fail-closed braking/holding behavior rather than continue blindly.
        Hold
    };

    struct Result
    {
        Status status = Status::StaleHold;
        TargetMode targetMode = TargetMode::Hold;

        Vec3d targetPositionMapMeters {};
        Vec3d targetVelocityMapMetersPerSecond {};
        Vec3d targetAccelerationMapMetersPerSecond2 {};

        bool safeProgressTargetDemonstrated = false;

        Revision mapRevision = 0;
        Revision sourceRevision = 0;
        double dynamicResultAgeSeconds = 0.0;

        double horizonDistanceMeters = 0.0;
        double nominalDistanceMeters = 0.0;

        EntityId primaryConflictEntityId = 0;
        double primaryTimeToClosestSeconds = 0.0;
        double primaryClosestDistanceMeters = 0.0;
        double primaryRequiredSeparationMeters = 0.0;

        std::size_t candidatesExamined = 0;
        std::size_t conflictsFound = 0;
    };

    [[nodiscard]] Result evaluate(
        const Query& query,
        const NavigationMap::QueryResult& dynamicCandidates
    ) const;
};

} // namespace world::navigation
