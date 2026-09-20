#include "LocalAvoidancePlanner.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace world::navigation
{
namespace
{

using Vec3d = LocalAvoidancePlanner::Vec3d;

constexpr double kEpsilon = 1.0e-12;
constexpr double kTwoPi = 6.283185307179586476925286766559;

bool finite(double value) noexcept
{
    return std::isfinite(value);
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

Vec3d cross(const Vec3d& a, const Vec3d& b) noexcept
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

double lengthSquared(const Vec3d& value) noexcept
{
    return dot(value, value);
}

double length(const Vec3d& value) noexcept
{
    return std::sqrt(lengthSquared(value));
}

Vec3d normalize(const Vec3d& value)
{
    const double magnitude = length(value);
    if (magnitude <= kEpsilon)
        throw std::invalid_argument("LocalAvoidancePlanner cannot normalize zero vector");
    return scale(value, 1.0 / magnitude);
}

LocalAvoidancePlanner::StaticQueries::Vec3d toSpaceVec(const Vec3d& value) noexcept
{
    return {value.x, value.y, value.z};
}

void validatePolicy(const LocalAvoidancePlanner::Policy& policy)
{
    if (!finite(policy.primaryDeflectionRadians) ||
        !finite(policy.secondaryDeflectionRadians) ||
        policy.primaryDeflectionRadians <= 0.0 ||
        policy.secondaryDeflectionRadians <= policy.primaryDeflectionRadians ||
        policy.secondaryDeflectionRadians >= 1.5707963267948966 ||
        !finite(policy.maximumDeflectionRadians) ||
        policy.maximumDeflectionRadians < policy.secondaryDeflectionRadians ||
        policy.maximumDeflectionRadians >= 1.5707963267948966 ||
        policy.azimuthSamples < 4 ||
        policy.azimuthSamples > 32 ||
        !finite(policy.staticAdditionalClearanceMeters) ||
        policy.staticAdditionalClearanceMeters < 0.0)
    {
        throw std::invalid_argument("LocalAvoidancePlanner policy is invalid");
    }
}

Vec3d leastAlignedAxis(const Vec3d& forward) noexcept
{
    const double ax = std::abs(forward.x);
    const double ay = std::abs(forward.y);
    const double az = std::abs(forward.z);

    if (ax <= ay && ax <= az)
        return {1.0, 0.0, 0.0};
    if (ay <= az)
        return {0.0, 1.0, 0.0};
    return {0.0, 0.0, 1.0};
}


Vec3d normalizedOr(
    const Vec3d& value,
    const Vec3d& fallback
) noexcept
{
    const double magnitudeSquared = lengthSquared(value);
    if (!finite(magnitudeSquared) ||
        magnitudeSquared <= kEpsilon)
    {
        return fallback;
    }

    const double inverseMagnitude =
        1.0 / std::sqrt(magnitudeSquared);
    return scale(value, inverseMagnitude);
}

} // namespace

LocalAvoidancePlanner::Result LocalAvoidancePlanner::evaluate(
    const Query& query,
    const NavigationMap::QueryResult& dynamicCandidates,
    const LocalAvoidancePlanner::StaticQueries& staticQueries
) const
{
    validatePolicy(query.avoidance);

    LocalHorizonPlanner horizonPlanner;
    const LocalHorizonPlanner::Result nominal =
        horizonPlanner.evaluate(query.horizon, dynamicCandidates);

    Result result;
    result.target = nominal;
    result.nominalPrimaryConflictEntityId =
        nominal.primaryConflictEntityId;
    result.nominalConflictsFound =
        nominal.conflictsFound;

    if (nominal.status == LocalHorizonPlanner::Status::StaleHold)
    {
        result.status = Status::StaleHold;
        return result;
    }

    LocalAvoidancePlanner::StaticQueries::PointQuery startQuery;
    startQuery.pointMapMeters =
        toSpaceVec(query.horizon.agent.positionMapMeters);
    startQuery.envelope.radiusMeters =
        query.horizon.agent.radiusMeters;
    startQuery.envelope.additionalClearanceMeters =
        query.avoidance.staticAdditionalClearanceMeters;

    const LocalAvoidancePlanner::StaticQueries::PointQueryResult start =
        staticQueries.queryPoint(startQuery);
    result.spaceRevision = start.spaceRevision;
    result.spaceSourceRevision = start.sourceRevision;
    result.startRegionId = start.regionId;
    result.staticObstaclesExamined += start.obstaclesExamined;

    if (!start.traversable || start.regionId == 0)
    {
        result.status = Status::StaticHold;
        result.nominalStaticBlocked = !start.blockingObstacleId.empty();
        result.nominalStaticObstacleId = start.blockingObstacleId;
        result.nominalStaticObstacleEntityId =
            start.blockingObstacleEntityId;
        return result;
    }

    // Dynamic and static truth are independent layers. Even when the dynamic
    // horizon says Clear, the bounded nominal segment must be proven against
    // exact persistent static geometry through the read API before it may pass through.
    const Vec3d nominalDelta = subtract(
        query.horizon.nominalTarget.positionMapMeters,
        query.horizon.agent.positionMapMeters
    );
    const double nominalDistance = length(nominalDelta);

    Vec3d boundedNominalTarget =
        query.horizon.nominalTarget.positionMapMeters;
    if (nominalDistance > nominal.horizonDistanceMeters &&
        nominalDistance > kEpsilon)
    {
        boundedNominalTarget = add(
            query.horizon.agent.positionMapMeters,
            scale(
                nominalDelta,
                nominal.horizonDistanceMeters / nominalDistance
            )
        );
    }

    LocalAvoidancePlanner::StaticQueries::SegmentQuery nominalStaticQuery;
    nominalStaticQuery.startMapMeters =
        toSpaceVec(query.horizon.agent.positionMapMeters);
    nominalStaticQuery.endMapMeters =
        toSpaceVec(boundedNominalTarget);
    nominalStaticQuery.envelope = startQuery.envelope;
    nominalStaticQuery.requireSameRegion = true;
    nominalStaticQuery.allowEndOnStartRegionBoundary =
        query.avoidance.nominalTargetIsProvenPortalBoundary;

    const LocalAvoidancePlanner::StaticQueries::SegmentQueryResult nominalStatic =
        staticQueries.querySegment(nominalStaticQuery);
    result.staticObstaclesExamined += nominalStatic.obstaclesExamined;

    if (nominalStatic.spaceRevision != start.spaceRevision ||
        nominalStatic.sourceRevision != start.sourceRevision)
    {
        result.status = Status::StaticHold;
        return result;
    }

    result.nominalStaticBlocked = !nominalStatic.traversable;
    result.nominalStaticObstacleId =
        nominalStatic.blockingObstacleId;
    result.nominalStaticObstacleEntityId =
        nominalStatic.blockingObstacleEntityId;

    if (nominal.status == LocalHorizonPlanner::Status::Clear &&
        nominalStatic.traversable)
    {
        result.status = Status::NominalClear;
        result.nominalVisibilityClear = true;
        return result;
    }

    const double probeDistance = std::min(
        nominalDistance,
        nominal.horizonDistanceMeters
    );

    if (probeDistance <= kEpsilon)
    {
        result.status =
            nominal.status == LocalHorizonPlanner::Status::Clear
                ? Status::StaticHold
                : Status::ConflictHold;
        return result;
    }

    const Vec3d forward = normalize(nominalDelta);
    const Vec3d reference = leastAlignedAxis(forward);
    const Vec3d lateralA = normalize(cross(forward, reference));
    const Vec3d lateralB = normalize(cross(lateralA, forward));

    const double deflectionStep =
        query.avoidance.secondaryDeflectionRadians -
        query.avoidance.primaryDeflectionRadians;

    const Vec3d continuityDirection =
        query.preferredDirectionValid
            ? normalizedOr(
                  query.preferredDirectionMap,
                  normalizedOr(
                      query.horizon.agent.velocityMapMetersPerSecond,
                      forward
                  )
              )
            : normalizedOr(
                  query.horizon.agent.velocityMapMetersPerSecond,
                  forward
              );

    for (double deflection =
             query.avoidance.primaryDeflectionRadians;
         deflection <=
             query.avoidance.maximumDeflectionRadians + kEpsilon;
         deflection += deflectionStep)
    {
        const double forwardScale = std::cos(deflection);
        const double lateralScale = std::sin(deflection);

        bool ringHasSafeCandidate = false;
        double bestContinuityScore =
            -std::numeric_limits<double>::infinity();
        std::size_t bestAzimuthIndex =
            std::numeric_limits<std::size_t>::max();
        LocalHorizonPlanner::Result bestAdjusted;

        for (std::size_t azimuthIndex = 0;
             azimuthIndex < query.avoidance.azimuthSamples;
             ++azimuthIndex)
        {
            ++result.targetProbesExamined;

            const double azimuth =
                kTwoPi * static_cast<double>(azimuthIndex) /
                static_cast<double>(query.avoidance.azimuthSamples);
            const Vec3d radial = add(
                scale(lateralA, std::cos(azimuth)),
                scale(lateralB, std::sin(azimuth))
            );
            const Vec3d direction = normalize(add(
                scale(forward, forwardScale),
                scale(radial, lateralScale)
            ));
            const Vec3d candidateTarget = add(
                query.horizon.agent.positionMapMeters,
                scale(direction, probeDistance)
            );

            LocalAvoidancePlanner::StaticQueries::SegmentQuery staticProbe;
            staticProbe.startMapMeters =
                toSpaceVec(query.horizon.agent.positionMapMeters);
            staticProbe.endMapMeters =
                toSpaceVec(candidateTarget);
            staticProbe.envelope = startQuery.envelope;
            staticProbe.requireSameRegion = true;

            const LocalAvoidancePlanner::StaticQueries::SegmentQueryResult staticResult =
                staticQueries.querySegment(staticProbe);

            result.spaceRevision = staticResult.spaceRevision;
            result.spaceSourceRevision = staticResult.sourceRevision;
            result.staticObstaclesExamined +=
                staticResult.obstaclesExamined;

            // Do not combine exact static evidence from two publications.
            if (staticResult.spaceRevision != start.spaceRevision ||
                staticResult.sourceRevision != start.sourceRevision)
            {
                result.status = Status::StaticHold;
                return result;
            }

            if (!staticResult.traversable ||
                staticResult.startRegionId != start.regionId)
            {
                ++result.staticRejected;
                continue;
            }

            LocalHorizonPlanner::Query probe = query.horizon;
            probe.nominalTarget.positionMapMeters = candidateTarget;
            probe.nominalTarget.velocityMapMetersPerSecond =
                {0.0, 0.0, 0.0};
            probe.nominalTarget.accelerationMapMetersPerSecond2 =
                {0.0, 0.0, 0.0};

            LocalHorizonPlanner::Result adjusted =
                horizonPlanner.evaluate(probe, dynamicCandidates);
            if (adjusted.status != LocalHorizonPlanner::Status::Clear)
            {
                ++result.dynamicRejected;
                continue;
            }

            // An avoidance target is always temporary/pass-through even when
            // its probe distance lies numerically inside the current horizon.
            adjusted.targetMode =
                LocalHorizonPlanner::TargetMode::PassThrough;
            adjusted.targetVelocityMapMetersPerSecond =
                {0.0, 0.0, 0.0};
            adjusted.targetAccelerationMapMetersPerSecond2 =
                {0.0, 0.0, 0.0};

            // Search the complete minimum-deflection ring before deciding.
            // Replanning used to return the first safe azimuth. Because the
            // local transverse basis changes as the craft moves, that could
            // alternate between opposite sides of the same obstacle. Prefer
            // the safe candidate that best continues the actual velocity
            // direction. This adds side-continuity without hidden planner
            // state, while preserving the existing "smallest deflection ring"
            // contract.
            const double continuityScore =
                dot(direction, continuityDirection);

            if (!ringHasSafeCandidate ||
                continuityScore >
                    bestContinuityScore + kEpsilon ||
                (std::abs(
                     continuityScore - bestContinuityScore
                 ) <= kEpsilon &&
                 azimuthIndex < bestAzimuthIndex))
            {
                ringHasSafeCandidate = true;
                bestContinuityScore = continuityScore;
                bestAzimuthIndex = azimuthIndex;
                bestAdjusted = adjusted;
            }
        }

        if (ringHasSafeCandidate)
        {
            result.status = Status::AdjustedClear;
            result.target = bestAdjusted;
            result.adjustedTarget = true;
            result.selectedDeflectionRadians = deflection;
            return result;
        }
    }

    result.ordinarySearchExhausted = true;
    result.status =
        nominal.status == LocalHorizonPlanner::Status::Clear
            ? Status::StaticHold
            : Status::ConflictHold;
    return result;
}

} // namespace world::navigation
