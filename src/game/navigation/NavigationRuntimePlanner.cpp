#include "NavigationRuntimePlanner.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace game::navigation
{
namespace
{

constexpr double kEpsilon = 1.0e-12;

bool finite(double value) noexcept
{
    return std::isfinite(value);
}

bool finite(const glm::dvec3& value) noexcept
{
    return finite(value.x) && finite(value.y) && finite(value.z);
}

glm::dvec3 normalizedOr(
    const glm::dvec3& value,
    const glm::dvec3& fallback
) noexcept
{
    const double lengthSquared = glm::dot(value, value);
    if (!finite(lengthSquared) || lengthSquared <= kEpsilon)
        return fallback;
    return value / std::sqrt(lengthSquared);
}

world::navigation::NavigationMap::Vec3d toMapVec(
    const glm::dvec3& value
) noexcept
{
    return {value.x, value.y, value.z};
}

world::navigation::NavigationSpace::Vec3d toSpaceVec(
    const glm::dvec3& value
) noexcept
{
    return {value.x, value.y, value.z};
}

glm::dvec3 toGlm(
    const world::navigation::NavigationMap::Vec3d& value
) noexcept
{
    return {value.x, value.y, value.z};
}

glm::dvec3 toGlm(
    const world::navigation::NavigationSpace::Vec3d& value
) noexcept
{
    return {value.x, value.y, value.z};
}

NavigationRuntimePlanner::Bridge::Vec3d toBridgeVec(
    const glm::dvec3& value
) noexcept
{
    return {value.x, value.y, value.z};
}

NavigationRuntimePlanner::Bridge::Intent holdIntent(
    const NavigationRuntimePlanner::AgentState& agent,
    const NavigationRuntimePlanner::Goal& goal,
    double urgency
) noexcept
{
    NavigationRuntimePlanner::Bridge::Intent intent;
    intent.revision = goal.revision;
    intent.emergency = goal.emergency || urgency >= 0.75;
    intent.hazardUrgency01 = std::clamp(
        std::max(goal.hazardUrgency01, urgency),
        0.0,
        1.0
    );

    const double response = std::max(0.0, goal.velocityResponsePerSecond);
    const glm::dvec3 linearDemand =
        -agent.velocityMapMetersPerSecond * response;

    const double angularDamping =
        std::max(0.0, goal.angularDampingPerSecond);
    const glm::dvec3 forward = normalizedOr(
        agent.forwardMap,
        glm::dvec3(0.0, 0.0, -1.0)
    );
    const glm::dvec3 right = normalizedOr(
        agent.rightMap,
        glm::dvec3(1.0, 0.0, 0.0)
    );
    const glm::dvec3 up = normalizedOr(
        agent.upMap,
        glm::dvec3(0.0, 1.0, 0.0)
    );
    const glm::dvec3 angularDemand =
        right * (-agent.pitchRateRadPerSec * angularDamping) +
        up * (-agent.yawRateRadPerSec * angularDamping) +
        forward * (-agent.rollRateRadPerSec * angularDamping);

    intent.idealLinearAccelerationDemandMapMps2 =
        toBridgeVec(linearDemand);
    intent.idealAngularAccelerationDemandMapRadPerSec2 =
        toBridgeVec(angularDemand);
    return intent;
}

bool validInput(
    const NavigationRuntimePlanner::AgentState& agent,
    const NavigationRuntimePlanner::Goal& goal,
    double dynamicResultAgeSeconds
) noexcept
{
    return
        finite(agent.positionMapMeters) &&
        finite(agent.velocityMapMetersPerSecond) &&
        finite(agent.accelerationMapMetersPerSecond2) &&
        finite(agent.radiusMeters) &&
        agent.radiusMeters >= 0.0 &&
        finite(agent.forwardMap) &&
        finite(agent.rightMap) &&
        finite(agent.upMap) &&
        finite(agent.pitchRateRadPerSec) &&
        finite(agent.yawRateRadPerSec) &&
        finite(agent.rollRateRadPerSec) &&
        finite(goal.targetPositionMapMeters) &&
        finite(goal.targetVelocityMapMetersPerSecond) &&
        finite(goal.targetAccelerationMapMetersPerSecond2) &&
        finite(goal.maximumTargetSpeedMps) &&
        goal.maximumTargetSpeedMps >= 0.0 &&
        finite(goal.velocityResponsePerSecond) &&
        goal.velocityResponsePerSecond >= 0.0 &&
        finite(goal.angularDampingPerSecond) &&
        goal.angularDampingPerSecond >= 0.0 &&
        finite(goal.arrivalRadiusMeters) &&
        goal.arrivalRadiusMeters >= 0.0 &&
        finite(goal.hazardUrgency01) &&
        finite(dynamicResultAgeSeconds) &&
        dynamicResultAgeSeconds >= 0.0;
}

} // namespace

NavigationRuntimePlanner::Result NavigationRuntimePlanner::plan(
    const AgentState& agent,
    const Goal& goal,
    const Map::QueryResult& dynamicCandidates,
    double dynamicResultAgeSeconds,
    const Space& staticSpace,
    const Policy& policy
)
{
    Result result;
    result.intent.revision = goal.revision;
    result.mapRevision = dynamicCandidates.mapRevision;
    result.mapSourceRevision = dynamicCandidates.sourceRevision;

    if (!validInput(agent, goal, dynamicResultAgeSeconds))
        return result;

    Space::CorridorQuery corridorQuery;
    corridorQuery.startMapMeters = toSpaceVec(agent.positionMapMeters);
    corridorQuery.endMapMeters = toSpaceVec(goal.targetPositionMapMeters);
    corridorQuery.envelope.radiusMeters = agent.radiusMeters;
    corridorQuery.envelope.additionalClearanceMeters =
        policy.avoidance.staticAdditionalClearanceMeters;

    const Space::CostedCorridorResult corridor =
        staticSpace.queryCostedCorridor(corridorQuery, policy.corridor);

    result.spaceRevision = corridor.spaceRevision;
    result.spaceSourceRevision = corridor.sourceRevision;
    result.staticRegionPath = corridor.regionPath;
    result.staticPortalPath = corridor.portalPath;
    result.staticPortalCentersMapMeters = corridor.portalCentersMapMeters;

    if (!corridor.found)
    {
        result.status = Status::StaticHold;
        result.selectedTargetMapMeters = agent.positionMapMeters;
        result.coarseWaypointMapMeters = agent.positionMapMeters;
        result.intent = holdIntent(agent, goal, 0.5);
        return result;
    }

    glm::dvec3 coarseTarget = goal.targetPositionMapMeters;
    glm::dvec3 coarseVelocity = goal.targetVelocityMapMetersPerSecond;
    glm::dvec3 coarseAcceleration = goal.targetAccelerationMapMetersPerSecond2;

    if (!corridor.portalCentersMapMeters.empty())
    {
        coarseTarget = toGlm(corridor.portalCentersMapMeters.front());
        coarseVelocity = glm::dvec3(0.0);
        coarseAcceleration = glm::dvec3(0.0);
        result.usedPortalWaypoint = true;
    }
    result.coarseWaypointMapMeters = coarseTarget;

    Avoidance::Query localQuery;
    localQuery.horizon.agent.entityId = agent.entityId;
    localQuery.horizon.agent.positionMapMeters = toMapVec(agent.positionMapMeters);
    localQuery.horizon.agent.velocityMapMetersPerSecond =
        toMapVec(agent.velocityMapMetersPerSecond);
    localQuery.horizon.agent.accelerationMapMetersPerSecond2 =
        toMapVec(agent.accelerationMapMetersPerSecond2);
    localQuery.horizon.agent.radiusMeters = agent.radiusMeters;

    localQuery.horizon.nominalTarget.positionMapMeters = toMapVec(coarseTarget);
    localQuery.horizon.nominalTarget.velocityMapMetersPerSecond =
        toMapVec(coarseVelocity);
    localQuery.horizon.nominalTarget.accelerationMapMetersPerSecond2 =
        toMapVec(coarseAcceleration);
    localQuery.horizon.dynamicResultAgeSeconds = dynamicResultAgeSeconds;
    localQuery.horizon.policy = policy.horizon;
    localQuery.avoidance = policy.avoidance;

    const Avoidance::Result local = Avoidance{}.evaluate(
        localQuery,
        dynamicCandidates,
        staticSpace
    );

    result.adjustedTarget = local.adjustedTarget;
    result.avoidanceProbesExamined = local.targetProbesExamined;
    result.primaryConflictEntityId = local.target.primaryConflictEntityId;
    result.dynamicCandidatesExamined = local.target.candidatesExamined;
    result.dynamicConflictsFound = local.target.conflictsFound;
    result.selectedTargetMapMeters = toGlm(local.target.targetPositionMapMeters);

    switch (local.status)
    {
        case Avoidance::Status::NominalClear:
            result.status = Status::NominalClear;
            break;
        case Avoidance::Status::AdjustedClear:
            result.status = Status::AdjustedClear;
            break;
        case Avoidance::Status::ConflictHold:
            result.status = Status::ConflictHold;
            result.intent = holdIntent(agent, goal, 1.0);
            return result;
        case Avoidance::Status::StaleHold:
            result.status = Status::StaleHold;
            result.intent = holdIntent(agent, goal, 0.75);
            return result;
        case Avoidance::Status::StaticHold:
        default:
            result.status = Status::StaticHold;
            result.intent = holdIntent(agent, goal, 0.5);
            return result;
    }

    result.safeProgressTargetDemonstrated =
        local.target.safeProgressTargetDemonstrated;

    const glm::dvec3 selectedTarget =
        toGlm(local.target.targetPositionMapMeters);
    const glm::dvec3 delta = selectedTarget - agent.positionMapMeters;
    const double distance = glm::length(delta);

    glm::dvec3 desiredVelocity(0.0);
    const bool trueTerminal =
        !result.usedPortalWaypoint &&
        local.target.targetMode == Horizon::TargetMode::Terminal;

    if (trueTerminal && distance <= goal.arrivalRadiusMeters)
    {
        desiredVelocity = goal.targetVelocityMapMetersPerSecond;
    }
    else if (distance > kEpsilon)
    {
        double speed = goal.maximumTargetSpeedMps;

        if (trueTerminal)
        {
            const double brakingDistance = std::max(
                0.0,
                distance - goal.arrivalRadiusMeters
            );
            const double brakingAcceleration =
                std::max(kEpsilon, policy.horizon.maxBrakingAccelerationMetersPerSecond2);
            const double brakingLimitedSpeed =
                std::sqrt(2.0 * brakingAcceleration * brakingDistance);
            speed = std::min(speed, brakingLimitedSpeed);
        }

        desiredVelocity = glm::normalize(delta) * speed;
        if (trueTerminal)
            desiredVelocity += goal.targetVelocityMapMetersPerSecond;
    }

    result.desiredVelocityMapMetersPerSecond = desiredVelocity;

    result.intent.revision = goal.revision;
    result.intent.emergency = goal.emergency;
    result.intent.hazardUrgency01 =
        std::clamp(goal.hazardUrgency01, 0.0, 1.0);

    const glm::dvec3 linearDemand =
        (desiredVelocity - agent.velocityMapMetersPerSecond) *
        goal.velocityResponsePerSecond;

    const glm::dvec3 forward = normalizedOr(
        agent.forwardMap,
        glm::dvec3(0.0, 0.0, -1.0)
    );
    const glm::dvec3 right = normalizedOr(
        agent.rightMap,
        glm::dvec3(1.0, 0.0, 0.0)
    );
    const glm::dvec3 up = normalizedOr(
        agent.upMap,
        glm::dvec3(0.0, 1.0, 0.0)
    );
    const glm::dvec3 angularDemand =
        right * (-agent.pitchRateRadPerSec * goal.angularDampingPerSecond) +
        up * (-agent.yawRateRadPerSec * goal.angularDampingPerSecond) +
        forward * (-agent.rollRateRadPerSec * goal.angularDampingPerSecond);

    result.intent.idealLinearAccelerationDemandMapMps2 =
        toBridgeVec(linearDemand);
    result.intent.idealAngularAccelerationDemandMapRadPerSec2 =
        toBridgeVec(angularDemand);
    return result;
}

} // namespace game::navigation
