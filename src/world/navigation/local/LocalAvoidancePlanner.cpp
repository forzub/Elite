#include "LocalAvoidancePlanner.h"

#include <algorithm>
#include <cmath>
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

NavigationSpace::Vec3d toSpaceVec(const Vec3d& value) noexcept
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

} // namespace

LocalAvoidancePlanner::Result LocalAvoidancePlanner::evaluate(
    const Query& query,
    const NavigationMap::QueryResult& dynamicCandidates,
    const NavigationSpace& staticSpace
) const
{
    validatePolicy(query.avoidance);

    LocalHorizonPlanner horizonPlanner;
    const LocalHorizonPlanner::Result nominal =
        horizonPlanner.evaluate(query.horizon, dynamicCandidates);

    Result result;
    result.target = nominal;

    if (nominal.status == LocalHorizonPlanner::Status::StaleHold)
    {
        result.status = Status::StaleHold;
        return result;
    }

    if (nominal.status == LocalHorizonPlanner::Status::Clear)
    {
        result.status = Status::NominalClear;
        return result;
    }

    NavigationSpace::PointQuery startQuery;
    startQuery.pointMapMeters = toSpaceVec(query.horizon.agent.positionMapMeters);
    startQuery.envelope.radiusMeters = query.horizon.agent.radiusMeters;
    startQuery.envelope.additionalClearanceMeters =
        query.avoidance.staticAdditionalClearanceMeters;

    const NavigationSpace::PointQueryResult start =
        staticSpace.queryPoint(startQuery);
    result.spaceRevision = start.spaceRevision;
    result.spaceSourceRevision = start.sourceRevision;
    result.startRegionId = start.regionId;

    if (!start.traversable || start.regionId == 0)
    {
        result.status = Status::StaticHold;
        return result;
    }

    const Vec3d nominalDelta = subtract(
        query.horizon.nominalTarget.positionMapMeters,
        query.horizon.agent.positionMapMeters
    );
    const double nominalDistance = length(nominalDelta);
    const double probeDistance = std::min(
        nominalDistance,
        nominal.horizonDistanceMeters
    );

    if (probeDistance <= kEpsilon)
    {
        result.status = Status::ConflictHold;
        return result;
    }

    const Vec3d forward = normalize(nominalDelta);
    const Vec3d reference = leastAlignedAxis(forward);
    const Vec3d lateralA = normalize(cross(forward, reference));
    const Vec3d lateralB = normalize(cross(lateralA, forward));

    const double rings[2] = {
        query.avoidance.primaryDeflectionRadians,
        query.avoidance.secondaryDeflectionRadians
    };

    for (double deflection : rings)
    {
        const double forwardScale = std::cos(deflection);
        const double lateralScale = std::sin(deflection);

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

            NavigationSpace::PointQuery targetQuery;
            targetQuery.pointMapMeters = toSpaceVec(candidateTarget);
            targetQuery.envelope = startQuery.envelope;
            const NavigationSpace::PointQueryResult targetStatic =
                staticSpace.queryPoint(targetQuery);

            result.spaceRevision = targetStatic.spaceRevision;
            result.spaceSourceRevision = targetStatic.sourceRevision;

            // Do not combine static evidence from two different publications.
            if (targetStatic.spaceRevision != start.spaceRevision ||
                targetStatic.sourceRevision != start.sourceRevision)
            {
                result.status = Status::StaticHold;
                return result;
            }

            // Same-region proof is intentionally conservative. Region free space
            // is an AABB and therefore convex; if both envelope-safe endpoints
            // resolve to the same region, the complete straight segment remains
            // inside that shrunken free-space volume.
            if (!targetStatic.traversable ||
                targetStatic.regionId != start.regionId)
            {
                ++result.staticRejected;
                continue;
            }

            LocalHorizonPlanner::Query probe = query.horizon;
            probe.nominalTarget.positionMapMeters = candidateTarget;
            probe.nominalTarget.velocityMapMetersPerSecond = {0.0, 0.0, 0.0};
            probe.nominalTarget.accelerationMapMetersPerSecond2 = {0.0, 0.0, 0.0};

            LocalHorizonPlanner::Result adjusted =
                horizonPlanner.evaluate(probe, dynamicCandidates);
            if (adjusted.status != LocalHorizonPlanner::Status::Clear)
            {
                ++result.dynamicRejected;
                continue;
            }

            // An avoidance target is always temporary/pass-through even when its
            // probe distance lies numerically inside the current horizon.
            adjusted.targetMode = LocalHorizonPlanner::TargetMode::PassThrough;
            adjusted.targetVelocityMapMetersPerSecond = {0.0, 0.0, 0.0};
            adjusted.targetAccelerationMapMetersPerSecond2 = {0.0, 0.0, 0.0};

            result.status = Status::AdjustedClear;
            result.target = adjusted;
            result.adjustedTarget = true;
            return result;
        }
    }

    result.status = Status::ConflictHold;
    return result;
}

} // namespace world::navigation
