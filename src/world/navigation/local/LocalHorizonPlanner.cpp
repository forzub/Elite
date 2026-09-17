#include "LocalHorizonPlanner.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace world::navigation
{
namespace
{

using Vec3d = LocalHorizonPlanner::Vec3d;
using Candidate = NavigationMap::Candidate;

constexpr double kEpsilon = 1.0e-12;

bool finite(double value) noexcept
{
    return std::isfinite(value);
}

bool finite(const Vec3d& value) noexcept
{
    return finite(value.x) && finite(value.y) && finite(value.z);
}

Vec3d add(const Vec3d& a, const Vec3d& b) noexcept
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3d subtract(const Vec3d& a, const Vec3d& b) noexcept
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3d scale(const Vec3d& value, double scalar) noexcept
{
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

double dot(const Vec3d& a, const Vec3d& b) noexcept
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

double lengthSquared(const Vec3d& value) noexcept
{
    return dot(value, value);
}

double length(const Vec3d& value) noexcept
{
    return std::sqrt(lengthSquared(value));
}

double distance(const Vec3d& a, const Vec3d& b) noexcept
{
    return length(subtract(a, b));
}

Vec3d positionAt(
    const Vec3d& position,
    const Vec3d& velocity,
    const Vec3d& acceleration,
    double seconds
) noexcept
{
    return add(
        add(position, scale(velocity, seconds)),
        scale(acceleration, 0.5 * seconds * seconds)
    );
}

Vec3d velocityAt(
    const Vec3d& velocity,
    const Vec3d& acceleration,
    double seconds
) noexcept
{
    return add(velocity, scale(acceleration, seconds));
}

double distanceToSegmentSquared(
    const Vec3d& point,
    const Vec3d& segmentStart,
    const Vec3d& segmentEnd
) noexcept
{
    const Vec3d segment = subtract(segmentEnd, segmentStart);
    const double denominator = lengthSquared(segment);
    if (denominator <= kEpsilon)
        return lengthSquared(subtract(point, segmentStart));

    const double t = std::clamp(
        dot(subtract(point, segmentStart), segment) / denominator,
        0.0,
        1.0
    );
    const Vec3d closest = add(segmentStart, scale(segment, t));
    return lengthSquared(subtract(point, closest));
}

void validateCandidate(const Candidate& candidate)
{
    if (!finite(candidate.positionMapMeters) ||
        !finite(candidate.velocityMapMetersPerSecond) ||
        !finite(candidate.accelerationMapMetersPerSecond2) ||
        !finite(candidate.predictedEndPositionMapMeters) ||
        !finite(candidate.conservativeSweptCenterMapMeters) ||
        !finite(candidate.actorRadiusMeters) ||
        !finite(candidate.conservativeSweptRadiusMeters) ||
        candidate.actorRadiusMeters < 0.0 ||
        candidate.conservativeSweptRadiusMeters < candidate.actorRadiusMeters)
    {
        throw std::invalid_argument(
            "LocalHorizonPlanner candidate contains invalid dynamic state"
        );
    }
}

void validateQuery(const LocalHorizonPlanner::Query& query)
{
    const auto& agent = query.agent;
    const auto& target = query.nominalTarget;
    const auto& policy = query.policy;

    if (!finite(agent.positionMapMeters) ||
        !finite(agent.velocityMapMetersPerSecond) ||
        !finite(agent.accelerationMapMetersPerSecond2) ||
        !finite(agent.radiusMeters) ||
        agent.radiusMeters < 0.0 ||
        !finite(target.positionMapMeters) ||
        !finite(target.velocityMapMetersPerSecond) ||
        !finite(target.accelerationMapMetersPerSecond2) ||
        !finite(query.dynamicResultAgeSeconds) ||
        query.dynamicResultAgeSeconds < 0.0 ||
        !finite(policy.lookAheadSeconds) ||
        policy.lookAheadSeconds <= 0.0 ||
        !finite(policy.maxResultAgeSeconds) ||
        policy.maxResultAgeSeconds < 0.0 ||
        !finite(policy.maxBrakingAccelerationMetersPerSecond2) ||
        policy.maxBrakingAccelerationMetersPerSecond2 <= 0.0 ||
        !finite(policy.turnDistanceMeters) ||
        policy.turnDistanceMeters < 0.0 ||
        !finite(policy.safetyMarginMeters) ||
        policy.safetyMarginMeters < 0.0 ||
        !finite(policy.minimumHorizonMeters) ||
        policy.minimumHorizonMeters < 0.0)
    {
        throw std::invalid_argument("LocalHorizonPlanner query/policy is invalid");
    }
}

} // namespace

LocalHorizonPlanner::Result LocalHorizonPlanner::evaluate(
    const Query& query,
    const NavigationMap::QueryResult& dynamicCandidates
) const
{
    validateQuery(query);

    Result result;
    result.mapRevision = dynamicCandidates.mapRevision;
    result.sourceRevision = dynamicCandidates.sourceRevision;
    result.dynamicResultAgeSeconds = query.dynamicResultAgeSeconds;
    result.targetPositionMapMeters = query.agent.positionMapMeters;

    const double speed = length(query.agent.velocityMapMetersPerSecond);
    const double accelerationMagnitude =
        length(query.agent.accelerationMapMetersPerSecond2);
    const double age = query.dynamicResultAgeSeconds;

    const double latencyDistance =
        speed * age + 0.5 * accelerationMagnitude * age * age;
    const double brakingDistance =
        (speed * speed) /
        (2.0 * query.policy.maxBrakingAccelerationMetersPerSecond2);

    result.horizonDistanceMeters = std::max(
        query.policy.minimumHorizonMeters,
        latencyDistance +
            brakingDistance +
            query.policy.turnDistanceMeters +
            query.policy.safetyMarginMeters
    );

    const Vec3d nominalDelta = subtract(
        query.nominalTarget.positionMapMeters,
        query.agent.positionMapMeters
    );
    result.nominalDistanceMeters = length(nominalDelta);

    Vec3d boundedTarget = query.nominalTarget.positionMapMeters;
    bool nominalInsideHorizon = true;
    if (result.nominalDistanceMeters > result.horizonDistanceMeters &&
        result.nominalDistanceMeters > kEpsilon)
    {
        nominalInsideHorizon = false;
        boundedTarget = add(
            query.agent.positionMapMeters,
            scale(
                nominalDelta,
                result.horizonDistanceMeters / result.nominalDistanceMeters
            )
        );
    }

    if (age > query.policy.maxResultAgeSeconds)
    {
        result.status = Status::StaleHold;
        result.targetMode = TargetMode::Hold;
        result.safeProgressTargetDemonstrated = false;
        return result;
    }

    const double agentAgeTravel = latencyDistance;
    const double lookAhead = query.policy.lookAheadSeconds;

    double bestConflictTime = std::numeric_limits<double>::infinity();
    double bestConflictGap = std::numeric_limits<double>::infinity();
    EntityId bestConflictId = 0;
    double bestClosestDistance = 0.0;
    double bestRequiredSeparation = 0.0;

    for (const Candidate& candidate : dynamicCandidates.candidates)
    {
        if (query.agent.entityId != 0 &&
            candidate.entityId == query.agent.entityId)
        {
            continue;
        }

        validateCandidate(candidate);
        ++result.candidatesExamined;

        const Vec3d actorNow = positionAt(
            candidate.positionMapMeters,
            candidate.velocityMapMetersPerSecond,
            candidate.accelerationMapMetersPerSecond2,
            age
        );
        const Vec3d actorVelocityNow = velocityAt(
            candidate.velocityMapMetersPerSecond,
            candidate.accelerationMapMetersPerSecond2,
            age
        );

        const Vec3d relativePosition = subtract(
            actorNow,
            query.agent.positionMapMeters
        );
        const Vec3d relativeVelocity = subtract(
            actorVelocityNow,
            query.agent.velocityMapMetersPerSecond
        );

        double timeToClosest = 0.0;
        const double relativeSpeedSquared = lengthSquared(relativeVelocity);
        if (relativeSpeedSquared > kEpsilon)
        {
            timeToClosest = std::clamp(
                -dot(relativePosition, relativeVelocity) / relativeSpeedSquared,
                0.0,
                lookAhead
            );
        }

        const Vec3d agentClosest = positionAt(
            query.agent.positionMapMeters,
            query.agent.velocityMapMetersPerSecond,
            query.agent.accelerationMapMetersPerSecond2,
            timeToClosest
        );
        const Vec3d actorClosest = positionAt(
            actorNow,
            actorVelocityNow,
            candidate.accelerationMapMetersPerSecond2,
            timeToClosest
        );
        const double closestDistance = distance(agentClosest, actorClosest);

        const double requiredSeparation =
            query.agent.radiusMeters +
            candidate.actorRadiusMeters +
            query.policy.safetyMarginMeters;

        const bool closestApproachConflict =
            closestDistance <= requiredSeparation;

        // NavigationMap's conservative swept sphere is broadphase authority.
        // Inflate it for the agent envelope and the distance the agent could
        // have travelled while the completed result aged.
        const double sweptRequiredRadius =
            candidate.conservativeSweptRadiusMeters +
            query.agent.radiusMeters +
            query.policy.safetyMarginMeters +
            agentAgeTravel;
        const bool sweptCorridorConflict =
            distanceToSegmentSquared(
                candidate.conservativeSweptCenterMapMeters,
                query.agent.positionMapMeters,
                boundedTarget
            ) <= sweptRequiredRadius * sweptRequiredRadius;

        if (!closestApproachConflict && !sweptCorridorConflict)
            continue;

        ++result.conflictsFound;
        const double gap = closestDistance - requiredSeparation;
        const bool better =
            timeToClosest < bestConflictTime ||
            (timeToClosest == bestConflictTime && gap < bestConflictGap) ||
            (timeToClosest == bestConflictTime &&
             gap == bestConflictGap &&
             candidate.entityId < bestConflictId);

        if (better)
        {
            bestConflictTime = timeToClosest;
            bestConflictGap = gap;
            bestConflictId = candidate.entityId;
            bestClosestDistance = closestDistance;
            bestRequiredSeparation = requiredSeparation;
        }
    }

    if (result.conflictsFound != 0)
    {
        result.status = Status::ConflictHold;
        result.targetMode = TargetMode::Hold;
        result.safeProgressTargetDemonstrated = false;
        result.primaryConflictEntityId = bestConflictId;
        result.primaryTimeToClosestSeconds = bestConflictTime;
        result.primaryClosestDistanceMeters = bestClosestDistance;
        result.primaryRequiredSeparationMeters = bestRequiredSeparation;
        return result;
    }

    result.status = Status::Clear;
    result.safeProgressTargetDemonstrated = true;
    result.targetPositionMapMeters = boundedTarget;

    if (nominalInsideHorizon)
    {
        result.targetMode = TargetMode::Terminal;
        result.targetVelocityMapMetersPerSecond =
            query.nominalTarget.velocityMapMetersPerSecond;
        result.targetAccelerationMapMetersPerSecond2 =
            query.nominalTarget.accelerationMapMetersPerSecond2;
    }
    else
    {
        result.targetMode = TargetMode::PassThrough;
    }

    return result;
}

} // namespace world::navigation
