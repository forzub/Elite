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
using Candidate = NavigationMap::Candidate;

constexpr double kEpsilon = 1.0e-12;

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

double distance(const Vec3d& a, const Vec3d& b) noexcept
{
    return length(subtract(a, b));
}

Vec3d normalize(const Vec3d& value)
{
    const double magnitude = length(value);
    if (magnitude <= kEpsilon)
        throw std::invalid_argument(
            "LocalAvoidancePlanner cannot normalize zero vector"
        );
    return scale(value, 1.0 / magnitude);
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

LocalAvoidancePlanner::StaticQueries::Vec3d toSpaceVec(
    const Vec3d& value
) noexcept
{
    return {value.x, value.y, value.z};
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

double pointToSegmentDistance2d(
    double px,
    double py,
    double ax,
    double ay,
    double bx,
    double by
) noexcept
{
    const double dx = bx - ax;
    const double dy = by - ay;
    const double denominator = dx * dx + dy * dy;

    if (denominator <= kEpsilon)
    {
        const double ex = px - ax;
        const double ey = py - ay;
        return std::sqrt(ex * ex + ey * ey);
    }

    const double t = std::clamp(
        ((px - ax) * dx + (py - ay) * dy) / denominator,
        0.0,
        1.0
    );
    const double cx = ax + dx * t;
    const double cy = ay + dy * t;
    const double ex = px - cx;
    const double ey = py - cy;
    return std::sqrt(ex * ex + ey * ey);
}

void validatePolicy(
    const LocalAvoidancePlanner::Policy& policy
)
{
    if (policy.lateralGridHalfExtentSamples < 1 ||
        policy.lateralGridHalfExtentSamples > 16 ||
        !finite(policy.minimumLateralStepMeters) ||
        policy.minimumLateralStepMeters <= 0.0 ||
        !finite(policy.lateralStepEnvelopeMultiplier) ||
        policy.lateralStepEnvelopeMultiplier <= 0.0 ||
        !finite(policy.maximumLateralOffsetMeters) ||
        policy.maximumLateralOffsetMeters <= 0.0 ||
        policy.longitudinalSamples < 1 ||
        policy.longitudinalSamples > 16 ||
        !finite(policy.projectionPaddingMeters) ||
        policy.projectionPaddingMeters < 0.0 ||
        policy.trajectorySamples < 4 ||
        policy.trajectorySamples > 128 ||
        !finite(policy.staticAdditionalClearanceMeters) ||
        policy.staticAdditionalClearanceMeters < 0.0)
    {
        throw std::invalid_argument(
            "LocalAvoidancePlanner policy is invalid"
        );
    }
}

bool candidateRelevantToForwardHorizon(
    const Candidate& candidate,
    const LocalHorizonPlanner::Query& query,
    const Vec3d& forward,
    double probeDistance,
    double lookAheadSeconds
) noexcept
{
    if (query.agent.entityId != 0 &&
        candidate.entityId == query.agent.entityId)
    {
        return false;
    }

    const double age = query.dynamicResultAgeSeconds;
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
    const Vec3d actorFuture = positionAt(
        actorNow,
        actorVelocityNow,
        candidate.accelerationMapMetersPerSecond2,
        lookAheadSeconds
    );

    const Vec3d fromAgentNow =
        subtract(actorNow, query.agent.positionMapMeters);
    const Vec3d fromAgentFuture =
        subtract(actorFuture, query.agent.positionMapMeters);

    const double s0 = dot(fromAgentNow, forward);
    const double s1 = dot(fromAgentFuture, forward);
    const double padding =
        query.agent.radiusMeters +
        candidate.actorRadiusMeters +
        query.policy.safetyMarginMeters;

    return
        std::max(s0, s1) >= -padding &&
        std::min(s0, s1) <= probeDistance + padding;
}

double projectedClearanceForOffset(
    const Candidate& candidate,
    const LocalHorizonPlanner::Query& query,
    const Vec3d& forward,
    const Vec3d& lateralA,
    const Vec3d& lateralB,
    double offsetA,
    double offsetB,
    double lookAheadSeconds,
    double projectionPaddingMeters
) noexcept
{
    const double age = query.dynamicResultAgeSeconds;
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
    const Vec3d actorFuture = positionAt(
        actorNow,
        actorVelocityNow,
        candidate.accelerationMapMetersPerSecond2,
        lookAheadSeconds
    );

    const Vec3d nowRelative =
        subtract(actorNow, query.agent.positionMapMeters);
    const Vec3d futureRelative =
        subtract(actorFuture, query.agent.positionMapMeters);

    const double a0 = dot(nowRelative, lateralA);
    const double b0 = dot(nowRelative, lateralB);
    const double a1 = dot(futureRelative, lateralA);
    const double b1 = dot(futureRelative, lateralB);

    const double required =
        query.agent.radiusMeters +
        candidate.actorRadiusMeters +
        query.policy.safetyMarginMeters +
        projectionPaddingMeters;

    return
        pointToSegmentDistance2d(
            offsetA,
            offsetB,
            a0,
            b0,
            a1,
            b1
        ) -
        required;
}

bool timeCoupledBypassClear(
    const Vec3d& bypassTarget,
    const Vec3d& mergeTarget,
    const LocalHorizonPlanner::Query& query,
    const NavigationMap::QueryResult& dynamicCandidates,
    const Vec3d& forward,
    double probeDistance,
    double lookAheadSeconds,
    std::size_t samples,
    double projectionPaddingMeters
) noexcept
{
    const Vec3d start = query.agent.positionMapMeters;
    const double firstLength = distance(start, bypassTarget);
    const double secondLength = distance(bypassTarget, mergeTarget);
    const double totalLength = firstLength + secondLength;
    const double age = query.dynamicResultAgeSeconds;

    if (totalLength <= kEpsilon)
        return false;

    for (std::size_t i = 0; i <= samples; ++i)
    {
        const double alpha =
            static_cast<double>(i) /
            static_cast<double>(samples);
        const double t = lookAheadSeconds * alpha;
        const double pathDistance = totalLength * alpha;

        Vec3d ship {};
        if (pathDistance <= firstLength ||
            secondLength <= kEpsilon)
        {
            const double localAlpha =
                firstLength > kEpsilon
                    ? std::clamp(
                          pathDistance / firstLength,
                          0.0,
                          1.0
                      )
                    : 1.0;
            ship = add(
                start,
                scale(
                    subtract(bypassTarget, start),
                    localAlpha
                )
            );
        }
        else
        {
            const double localAlpha =
                std::clamp(
                    (pathDistance - firstLength) /
                        secondLength,
                    0.0,
                    1.0
                );
            ship = add(
                bypassTarget,
                scale(
                    subtract(mergeTarget, bypassTarget),
                    localAlpha
                )
            );
        }

        for (const Candidate& candidate : dynamicCandidates.candidates)
        {
            if (!candidateRelevantToForwardHorizon(
                    candidate,
                    query,
                    forward,
                    probeDistance,
                    lookAheadSeconds))
            {
                continue;
            }

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
            const Vec3d actor = positionAt(
                actorNow,
                actorVelocityNow,
                candidate.accelerationMapMetersPerSecond2,
                t
            );

            const double required =
                query.agent.radiusMeters +
                candidate.actorRadiusMeters +
                query.policy.safetyMarginMeters +
                projectionPaddingMeters;

            if (distance(ship, actor) <= required)
                return false;
        }
    }

    return true;
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

    const auto start = staticQueries.queryPoint(startQuery);
    result.spaceRevision = start.spaceRevision;
    result.spaceSourceRevision = start.sourceRevision;
    result.startRegionId = start.regionId;
    result.staticObstaclesExamined += start.obstaclesExamined;

    if (!start.traversable || start.regionId == 0)
    {
        result.status = Status::StaticHold;
        result.nominalStaticBlocked =
            !start.blockingObstacleId.empty();
        result.nominalStaticObstacleId =
            start.blockingObstacleId;
        result.nominalStaticObstacleEntityId =
            start.blockingObstacleEntityId;
        return result;
    }

    const Vec3d nominalDelta = subtract(
        query.horizon.nominalTarget.positionMapMeters,
        query.horizon.agent.positionMapMeters
    );
    const double nominalDistance = length(nominalDelta);

    if (nominalDistance <= kEpsilon)
    {
        result.status =
            nominal.status == LocalHorizonPlanner::Status::Clear
                ? Status::NominalClear
                : Status::ConflictHold;
        result.nominalPathClear =
            nominal.status == LocalHorizonPlanner::Status::Clear;
        return result;
    }

    const Vec3d forward = normalize(nominalDelta);
    const double probeDistance = std::min(
        nominalDistance,
        nominal.horizonDistanceMeters
    );
    const Vec3d boundedNominalTarget = add(
        query.horizon.agent.positionMapMeters,
        scale(forward, probeDistance)
    );
    result.mergeTargetMapMeters = boundedNominalTarget;

    LocalAvoidancePlanner::StaticQueries::SegmentQuery nominalStaticQuery;
    nominalStaticQuery.startMapMeters =
        toSpaceVec(query.horizon.agent.positionMapMeters);
    nominalStaticQuery.endMapMeters =
        toSpaceVec(boundedNominalTarget);
    nominalStaticQuery.envelope = startQuery.envelope;
    nominalStaticQuery.requireSameRegion = true;
    nominalStaticQuery.allowEndOnStartRegionBoundary =
        query.avoidance.nominalTargetIsProvenPortalBoundary;

    const auto nominalStatic =
        staticQueries.querySegment(nominalStaticQuery);
    result.staticObstaclesExamined +=
        nominalStatic.obstaclesExamined;

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
        result.nominalPathClear = true;
        result.mergeTargetMapMeters = boundedNominalTarget;
        return result;
    }

    if (probeDistance <= kEpsilon)
    {
        result.status =
            nominal.status == LocalHorizonPlanner::Status::Clear
                ? Status::StaticHold
                : Status::ConflictHold;
        result.localBypassExhausted = true;
        return result;
    }

    const Vec3d reference = leastAlignedAxis(forward);
    const Vec3d lateralA = normalize(cross(forward, reference));
    const Vec3d lateralB = normalize(cross(lateralA, forward));

    const double lookAhead =
        std::max(
            query.horizon.policy.lookAheadSeconds,
            nominal.effectiveLookAheadSeconds
        );
    const double envelopeScale =
        query.horizon.agent.radiusMeters +
        query.horizon.policy.safetyMarginMeters +
        query.avoidance.projectionPaddingMeters;
    const double step = std::max(
        query.avoidance.minimumLateralStepMeters,
        envelopeScale *
            query.avoidance.lateralStepEnvelopeMultiplier
    );

    for (const Candidate& candidate : dynamicCandidates.candidates)
    {
        if (candidateRelevantToForwardHorizon(
                candidate,
                query.horizon,
                forward,
                probeDistance,
                lookAhead))
        {
            ++result.projectedDynamicObstacles;
        }
    }

    bool found = false;
    Vec3d bestTarget {};
    Vec3d bestOffset {};
    double bestOffsetMagnitude =
        std::numeric_limits<double>::infinity();
    double bestRouteLength =
        std::numeric_limits<double>::infinity();
    double bestProjectedClearance =
        -std::numeric_limits<double>::infinity();
    double bestBypassForwardDistance = 0.0;
    std::size_t bestOrder =
        std::numeric_limits<std::size_t>::max();
    std::size_t order = 0;

    const int half =
        static_cast<int>(
            query.avoidance.lateralGridHalfExtentSamples
        );

    for (int ia = -half; ia <= half; ++ia)
    {
        for (int ib = -half; ib <= half; ++ib)
        {
            if (ia == 0 && ib == 0)
                continue;

            ++result.offsetCandidatesExamined;

            const double offsetA =
                static_cast<double>(ia) * step;
            const double offsetB =
                static_cast<double>(ib) * step;
            const double offsetMagnitude =
                std::sqrt(
                    offsetA * offsetA +
                    offsetB * offsetB
                );

            if (offsetMagnitude >
                query.avoidance.maximumLateralOffsetMeters)
            {
                ++result.projectionRejected;
                continue;
            }

            double projectedClearance = 1.0e30;
            bool projectionBlocked = false;
            bool anyProjectedActor = false;

            for (const Candidate& candidate :
                 dynamicCandidates.candidates)
            {
                if (!candidateRelevantToForwardHorizon(
                        candidate,
                        query.horizon,
                        forward,
                        probeDistance,
                        lookAhead))
                {
                    continue;
                }

                anyProjectedActor = true;
                const double clearance =
                    projectedClearanceForOffset(
                        candidate,
                        query.horizon,
                        forward,
                        lateralA,
                        lateralB,
                        offsetA,
                        offsetB,
                        lookAhead,
                        query.avoidance.projectionPaddingMeters
                    );
                projectedClearance =
                    std::min(projectedClearance, clearance);

                if (clearance <= 0.0)
                {
                    projectionBlocked = true;
                    break;
                }
            }

            if (projectionBlocked)
            {
                ++result.projectionRejected;
                continue;
            }

            if (!anyProjectedActor)
                projectedClearance =
                    query.avoidance.maximumLateralOffsetMeters;

            const Vec3d offset = add(
                scale(lateralA, offsetA),
                scale(lateralB, offsetB)
            );

            for (std::size_t longitudinalIndex = 1;
                 longitudinalIndex <=
                     query.avoidance.longitudinalSamples;
                 ++longitudinalIndex)
            {
                ++order;
                ++result.routeCandidatesExamined;

                const double forwardFraction =
                    static_cast<double>(longitudinalIndex) /
                    static_cast<double>(
                        query.avoidance.longitudinalSamples + 1
                    );
                const double bypassForwardDistance =
                    probeDistance * forwardFraction;

                const Vec3d bypassBase = add(
                    query.horizon.agent.positionMapMeters,
                    scale(forward, bypassForwardDistance)
                );
                const Vec3d candidateTarget = add(
                    bypassBase,
                    offset
                );

                LocalAvoidancePlanner::StaticQueries::SegmentQuery firstProbe;
                firstProbe.startMapMeters =
                    toSpaceVec(
                        query.horizon.agent.positionMapMeters
                    );
                firstProbe.endMapMeters =
                    toSpaceVec(candidateTarget);
                firstProbe.envelope = startQuery.envelope;
                firstProbe.requireSameRegion = true;

                const auto firstStatic =
                    staticQueries.querySegment(firstProbe);
                result.spaceRevision =
                    firstStatic.spaceRevision;
                result.spaceSourceRevision =
                    firstStatic.sourceRevision;
                result.staticObstaclesExamined +=
                    firstStatic.obstaclesExamined;

                if (firstStatic.spaceRevision !=
                        start.spaceRevision ||
                    firstStatic.sourceRevision !=
                        start.sourceRevision)
                {
                    result.status = Status::StaticHold;
                    return result;
                }

                if (!firstStatic.traversable ||
                    firstStatic.startRegionId != start.regionId)
                {
                    ++result.staticRejected;
                    continue;
                }

                LocalAvoidancePlanner::StaticQueries::SegmentQuery secondProbe;
                secondProbe.startMapMeters =
                    toSpaceVec(candidateTarget);
                secondProbe.endMapMeters =
                    toSpaceVec(boundedNominalTarget);
                secondProbe.envelope = startQuery.envelope;
                secondProbe.requireSameRegion = true;
                secondProbe.allowEndOnStartRegionBoundary =
                    query.avoidance.
                        nominalTargetIsProvenPortalBoundary;

                const auto secondStatic =
                    staticQueries.querySegment(secondProbe);
                result.spaceRevision =
                    secondStatic.spaceRevision;
                result.spaceSourceRevision =
                    secondStatic.sourceRevision;
                result.staticObstaclesExamined +=
                    secondStatic.obstaclesExamined;

                if (secondStatic.spaceRevision !=
                        start.spaceRevision ||
                    secondStatic.sourceRevision !=
                        start.sourceRevision)
                {
                    result.status = Status::StaticHold;
                    return result;
                }

                if (!secondStatic.traversable ||
                    secondStatic.startRegionId != start.regionId)
                {
                    ++result.staticRejected;
                    continue;
                }

                if (!timeCoupledBypassClear(
                        candidateTarget,
                        boundedNominalTarget,
                        query.horizon,
                        dynamicCandidates,
                        forward,
                        probeDistance,
                        lookAhead,
                        query.avoidance.trajectorySamples,
                        query.avoidance.
                            projectionPaddingMeters))
                {
                    ++result.dynamicRejected;
                    continue;
                }

                const double routeLength =
                    distance(
                        query.horizon.agent.positionMapMeters,
                        candidateTarget
                    ) +
                    distance(
                        candidateTarget,
                        boundedNominalTarget
                    );

                const bool better =
                    !found ||
                    offsetMagnitude <
                        bestOffsetMagnitude - kEpsilon ||
                    (std::abs(
                         offsetMagnitude -
                         bestOffsetMagnitude
                     ) <= kEpsilon &&
                     routeLength <
                         bestRouteLength - kEpsilon) ||
                    (std::abs(
                         offsetMagnitude -
                         bestOffsetMagnitude
                     ) <= kEpsilon &&
                     std::abs(
                         routeLength -
                         bestRouteLength
                     ) <= kEpsilon &&
                     projectedClearance >
                         bestProjectedClearance + kEpsilon) ||
                    (std::abs(
                         offsetMagnitude -
                         bestOffsetMagnitude
                     ) <= kEpsilon &&
                     std::abs(
                         routeLength -
                         bestRouteLength
                     ) <= kEpsilon &&
                     std::abs(
                         projectedClearance -
                         bestProjectedClearance
                     ) <= kEpsilon &&
                     order < bestOrder);

                if (better)
                {
                    found = true;
                    bestTarget = candidateTarget;
                    bestOffset = offset;
                    bestOffsetMagnitude = offsetMagnitude;
                    bestRouteLength = routeLength;
                    bestProjectedClearance =
                        projectedClearance;
                    bestBypassForwardDistance =
                        bypassForwardDistance;
                    bestOrder = order;
                }
            }
        }
    }

    if (!found)
    {
        result.localBypassExhausted = true;
        result.status =
            nominal.status == LocalHorizonPlanner::Status::Clear
                ? Status::StaticHold
                : Status::ConflictHold;
        return result;
    }

    result.status = Status::AdjustedClear;
    result.adjustedTarget = true;
    result.selectedLateralOffsetMap = bestOffset;
    result.selectedLateralOffsetMeters = bestOffsetMagnitude;
    result.selectedBypassForwardDistanceMeters =
        bestBypassForwardDistance;
    result.selectedProjectedClearanceMeters =
        std::isfinite(bestProjectedClearance)
            ? bestProjectedClearance
            : 0.0;

    result.target.status = LocalHorizonPlanner::Status::Clear;
    result.target.targetMode =
        LocalHorizonPlanner::TargetMode::PassThrough;
    result.target.targetPositionMapMeters = bestTarget;
    result.target.targetVelocityMapMetersPerSecond =
        {0.0, 0.0, 0.0};
    result.target.targetAccelerationMapMetersPerSecond2 =
        {0.0, 0.0, 0.0};
    result.target.safeProgressTargetDemonstrated = true;
    result.target.primaryConflictEntityId = 0;
    result.target.conflictsFound = 0;

    return result;
}

} // namespace world::navigation
