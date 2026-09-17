#include "BoundedGapCandidateBuilder.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace world::navigation
{
namespace
{

using Builder = BoundedGapCandidateBuilder;
using Vec3d = Builder::Vec3d;

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

Vec3d normalizeOrZero(const Vec3d& value) noexcept
{
    const double magnitude = length(value);
    if (!finite(magnitude) || magnitude <= kEpsilon)
        return {};
    return scale(value, 1.0 / magnitude);
}

bool validWitness(const Builder::ObstacleWitness& witness) noexcept
{
    return finite(witness.centerMapMeters) &&
        finite(witness.conservativeRadiusMeters) &&
        witness.conservativeRadiusMeters >= 0.0;
}

bool validPolicy(const Builder::Policy& policy) noexcept
{
    return
        finite(policy.boundaryClearanceMeters) &&
        policy.boundaryClearanceMeters >= 0.0 &&
        finite(policy.minimumClearSeparationMeters) &&
        policy.minimumClearSeparationMeters >= 0.0 &&
        finite(policy.maximumClearSeparationMeters) &&
        policy.maximumClearSeparationMeters >= policy.minimumClearSeparationMeters &&
        finite(policy.secondaryClearanceMeters) &&
        policy.secondaryClearanceMeters > 0.0 &&
        finite(policy.minimumForwardDistanceMeters) &&
        finite(policy.maximumForwardDistanceMeters) &&
        policy.maximumForwardDistanceMeters >= policy.minimumForwardDistanceMeters &&
        finite(policy.maximumCenterlineOffsetMeters) &&
        policy.maximumCenterlineOffsetMeters >= 0.0 &&
        finite(policy.maximumAbsSeparationTravelDot) &&
        policy.maximumAbsSeparationTravelDot >= 0.0 &&
        policy.maximumAbsSeparationTravelDot <= 1.0;
}

bool betterCandidate(
    const Builder::Candidate& a,
    const Builder::Candidate& b
) noexcept
{
    if (a.centerlineOffsetMeters != b.centerlineOffsetMeters)
        return a.centerlineOffsetMeters < b.centerlineOffsetMeters;
    if (a.forwardDistanceMeters != b.forwardDistanceMeters)
        return a.forwardDistanceMeters < b.forwardDistanceMeters;
    if (a.gap.clearSeparationMeters != b.gap.clearSeparationMeters)
        return a.gap.clearSeparationMeters > b.gap.clearSeparationMeters;
    return a.neighborObstacleId < b.neighborObstacleId;
}

void insertBounded(
    std::vector<Builder::Candidate>& candidates,
    Builder::Candidate candidate,
    std::size_t limit
)
{
    if (limit == 0)
        return;

    const auto insertAt = std::lower_bound(
        candidates.begin(),
        candidates.end(),
        candidate,
        betterCandidate
    );

    if (candidates.size() >= limit && insertAt == candidates.end())
        return;

    candidates.insert(insertAt, std::move(candidate));
    if (candidates.size() > limit)
        candidates.pop_back();
}

} // namespace

BoundedGapCandidateBuilder::Result BoundedGapCandidateBuilder::build(
    const Query& query
)
{
    Result result;
    result.candidateLimitApplied = std::min(
        query.policy.maxCandidates,
        kHardCandidateLimit
    );
    result.candidates.reserve(result.candidateLimitApplied);

    if (!finite(query.referencePointMapMeters) ||
        !finite(query.travelDirectionMap) ||
        !validWitness(query.primary) ||
        !validPolicy(query.policy))
    {
        return result;
    }

    const Vec3d travelDirection = normalizeOrZero(query.travelDirectionMap);
    if (lengthSquared(travelDirection) <= kEpsilon)
        return result;

    result.validInput = true;

    const double primaryRadius =
        query.primary.conservativeRadiusMeters +
        query.policy.boundaryClearanceMeters;

    for (const ObstacleWitness& neighbor : query.neighbors)
    {
        ++result.diagnostics.neighborsExamined;

        if (!validWitness(neighbor))
        {
            ++result.diagnostics.rejectedInvalid;
            continue;
        }
        if (neighbor.obstacleId == query.primary.obstacleId)
        {
            ++result.diagnostics.rejectedSameObstacle;
            continue;
        }
        if (neighbor.snapshotRevision != query.primary.snapshotRevision)
        {
            ++result.diagnostics.rejectedRevision;
            continue;
        }

        const Vec3d centerDelta = subtract(
            neighbor.centerMapMeters,
            query.primary.centerMapMeters
        );
        const double centerDistance = length(centerDelta);
        if (!finite(centerDistance) || centerDistance <= kEpsilon)
        {
            ++result.diagnostics.rejectedInvalid;
            continue;
        }

        const double neighborRadius =
            neighbor.conservativeRadiusMeters +
            query.policy.boundaryClearanceMeters;
        const double clearSeparation =
            centerDistance - primaryRadius - neighborRadius;

        if (clearSeparation <= 0.0)
        {
            ++result.diagnostics.rejectedOverlapping;
            continue;
        }
        if (clearSeparation < query.policy.minimumClearSeparationMeters ||
            clearSeparation > query.policy.maximumClearSeparationMeters)
        {
            ++result.diagnostics.rejectedGapRange;
            continue;
        }

        const Vec3d separationAxis = scale(centerDelta, 1.0 / centerDistance);
        const double separationTravelDot =
            std::abs(dot(separationAxis, travelDirection));
        if (separationTravelDot > query.policy.maximumAbsSeparationTravelDot)
        {
            ++result.diagnostics.rejectedAlignment;
            continue;
        }

        const Vec3d primarySurface = add(
            query.primary.centerMapMeters,
            scale(separationAxis, primaryRadius)
        );
        const Vec3d neighborSurface = subtract(
            neighbor.centerMapMeters,
            scale(separationAxis, neighborRadius)
        );
        const Vec3d gapCenter = scale(
            add(primarySurface, neighborSurface),
            0.5
        );

        const Vec3d fromReference = subtract(
            gapCenter,
            query.referencePointMapMeters
        );
        const double forwardDistance = dot(fromReference, travelDirection);
        if (forwardDistance < query.policy.minimumForwardDistanceMeters ||
            forwardDistance > query.policy.maximumForwardDistanceMeters)
        {
            ++result.diagnostics.rejectedLongitudinalWindow;
            continue;
        }

        const Vec3d centerlineResidual = subtract(
            fromReference,
            scale(travelDirection, forwardDistance)
        );
        const double centerlineOffset = length(centerlineResidual);
        if (!finite(centerlineOffset) ||
            centerlineOffset > query.policy.maximumCenterlineOffsetMeters)
        {
            ++result.diagnostics.rejectedCenterlineOffset;
            continue;
        }

        Candidate candidate;
        candidate.primaryObstacleId = query.primary.obstacleId;
        candidate.neighborObstacleId = neighbor.obstacleId;
        candidate.forwardDistanceMeters = forwardDistance;
        candidate.centerlineOffsetMeters = centerlineOffset;
        candidate.gap.centerMapMeters = gapCenter;
        candidate.gap.travelDirectionMap = travelDirection;
        candidate.gap.separationAxisMap = separationAxis;
        candidate.gap.clearSeparationMeters = clearSeparation;
        candidate.gap.secondaryClearanceMeters =
            query.policy.secondaryClearanceMeters;

        insertBounded(
            result.candidates,
            std::move(candidate),
            result.candidateLimitApplied
        );
    }

    return result;
}

} // namespace world::navigation
