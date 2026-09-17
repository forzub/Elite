#include "MovingGapPredictor.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace world::navigation
{
namespace
{

using Predictor = MovingGapPredictor;
using Vec3d = Predictor::Vec3d;

constexpr double kEpsilon = 1.0e-12;
constexpr double kPi = 3.141592653589793238462643383279502884;

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

Vec3d scale(const Vec3d& value, double factor) noexcept
{
    return {value.x * factor, value.y * factor, value.z * factor};
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

Vec3d normalizeOrZero(const Vec3d& value) noexcept
{
    const double magnitude = length(value);
    if (!finite(magnitude) || magnitude <= kEpsilon)
        return {};
    return scale(value, 1.0 / magnitude);
}

bool validBoundary(const Predictor::BoundaryMotion& boundary) noexcept
{
    return finite(boundary.centerMapMeters) &&
        finite(boundary.linearVelocityMapMetersPerSec) &&
        finite(boundary.linearAccelerationMapMetersPerSec2) &&
        finite(boundary.angularVelocityMapRadPerSec) &&
        finite(boundary.conservativeRadiusMeters) &&
        boundary.conservativeRadiusMeters >= 0.0;
}

bool validPolicy(const Predictor::Policy& policy) noexcept
{
    return finite(policy.boundaryClearanceMeters) &&
        policy.boundaryClearanceMeters >= 0.0 &&
        finite(policy.secondaryClearanceMeters) &&
        policy.secondaryClearanceMeters > 0.0 &&
        finite(policy.minimumContinuousClearSeparationMeters) &&
        policy.minimumContinuousClearSeparationMeters >= 0.0 &&
        finite(policy.maximumAbsSeparationTravelDot) &&
        policy.maximumAbsSeparationTravelDot >= 0.0 &&
        policy.maximumAbsSeparationTravelDot <= 1.0;
}

Vec3d positionAt(
    const Predictor::BoundaryMotion& motion,
    double timeSeconds
) noexcept
{
    return add(
        add(
            motion.centerMapMeters,
            scale(motion.linearVelocityMapMetersPerSec, timeSeconds)
        ),
        scale(
            motion.linearAccelerationMapMetersPerSec2,
            0.5 * timeSeconds * timeSeconds
        )
    );
}

Vec3d velocityAt(
    const Predictor::BoundaryMotion& motion,
    double timeSeconds
) noexcept
{
    return add(
        motion.linearVelocityMapMetersPerSec,
        scale(motion.linearAccelerationMapMetersPerSec2, timeSeconds)
    );
}

double distancePointToSegmentFromOrigin(
    const Vec3d& a,
    const Vec3d& b
) noexcept
{
    const Vec3d delta = subtract(b, a);
    const double deltaLengthSquared = lengthSquared(delta);
    if (deltaLengthSquared <= kEpsilon)
        return length(a);

    const double t = std::clamp(
        -dot(a, delta) / deltaLengthSquared,
        0.0,
        1.0
    );
    return length(add(a, scale(delta, t)));
}

double maximumAbsoluteQuadraticProjection(
    double q0,
    double qv,
    double qa,
    double dt
) noexcept
{
    auto valueAt = [=](double time) noexcept
    {
        return q0 + qv * time + 0.5 * qa * time * time;
    };

    double maximum = std::max(std::abs(valueAt(0.0)), std::abs(valueAt(dt)));
    if (std::abs(qa) > kEpsilon)
    {
        const double stationary = -qv / qa;
        if (stationary > 0.0 && stationary < dt)
            maximum = std::max(maximum, std::abs(valueAt(stationary)));
    }
    return maximum;
}

bool fillSample(
    const Predictor::Query& query,
    const Vec3d& travelDirection,
    double timeSeconds,
    Predictor::Sample& sample,
    double& centerDistance
) noexcept
{
    const Vec3d primaryCenter = positionAt(query.primary, timeSeconds);
    const Vec3d secondaryCenter = positionAt(query.secondary, timeSeconds);
    const Vec3d primaryVelocity = velocityAt(query.primary, timeSeconds);
    const Vec3d secondaryVelocity = velocityAt(query.secondary, timeSeconds);
    const Vec3d centerDelta = subtract(secondaryCenter, primaryCenter);

    centerDistance = length(centerDelta);
    if (!finite(centerDistance) || centerDistance <= kEpsilon)
        return false;

    const Vec3d separationAxis = scale(centerDelta, 1.0 / centerDistance);
    const Vec3d relativeVelocity = subtract(secondaryVelocity, primaryVelocity);
    const double separationRate = dot(relativeVelocity, separationAxis);

    const double primaryInflatedRadius =
        query.primary.conservativeRadiusMeters +
        query.policy.boundaryClearanceMeters;
    const double secondaryInflatedRadius =
        query.secondary.conservativeRadiusMeters +
        query.policy.boundaryClearanceMeters;

    const Vec3d primaryClearanceSurface = add(
        primaryCenter,
        scale(separationAxis, primaryInflatedRadius)
    );
    const Vec3d secondaryClearanceSurface = subtract(
        secondaryCenter,
        scale(separationAxis, secondaryInflatedRadius)
    );
    const Vec3d gapCenter = scale(
        add(primaryClearanceSurface, secondaryClearanceSurface),
        0.5
    );

    const Vec3d relativeVelocityPerpendicular = subtract(
        relativeVelocity,
        scale(separationAxis, separationRate)
    );
    const Vec3d separationAxisRate = scale(
        relativeVelocityPerpendicular,
        1.0 / centerDistance
    );
    const Vec3d gapCenterVelocity = add(
        scale(add(primaryVelocity, secondaryVelocity), 0.5),
        scale(
            separationAxisRate,
            0.5 * (primaryInflatedRadius - secondaryInflatedRadius)
        )
    );

    const Vec3d primaryPhysicalLever = scale(
        separationAxis,
        query.primary.conservativeRadiusMeters
    );
    const Vec3d secondaryPhysicalLever = scale(
        separationAxis,
        -query.secondary.conservativeRadiusMeters
    );

    sample.timeSeconds = timeSeconds;
    sample.gap.centerMapMeters = gapCenter;
    sample.gap.travelDirectionMap = travelDirection;
    sample.gap.separationAxisMap = separationAxis;
    sample.gap.clearSeparationMeters =
        centerDistance - primaryInflatedRadius - secondaryInflatedRadius;
    sample.gap.secondaryClearanceMeters = query.policy.secondaryClearanceMeters;
    sample.gapCenterVelocityMapMetersPerSec = gapCenterVelocity;
    sample.separationRateMetersPerSec = separationRate;
    sample.absSeparationTravelDot = std::abs(dot(separationAxis, travelDirection));

    sample.primary.centerMapMeters = primaryCenter;
    sample.primary.linearVelocityMapMetersPerSec = primaryVelocity;
    sample.primary.surfacePointTowardGapMapMeters = add(
        primaryCenter,
        primaryPhysicalLever
    );
    sample.primary.normalTowardFreeSpaceMap = separationAxis;
    sample.primary.surfaceVelocityAtPointMapMetersPerSec = add(
        primaryVelocity,
        cross(query.primary.angularVelocityMapRadPerSec, primaryPhysicalLever)
    );

    sample.secondary.centerMapMeters = secondaryCenter;
    sample.secondary.linearVelocityMapMetersPerSec = secondaryVelocity;
    sample.secondary.surfacePointTowardGapMapMeters = add(
        secondaryCenter,
        secondaryPhysicalLever
    );
    sample.secondary.normalTowardFreeSpaceMap = scale(separationAxis, -1.0);
    sample.secondary.surfaceVelocityAtPointMapMetersPerSec = add(
        secondaryVelocity,
        cross(query.secondary.angularVelocityMapRadPerSec, secondaryPhysicalLever)
    );

    return finite(sample.gap.centerMapMeters) &&
        finite(sample.gapCenterVelocityMapMetersPerSec) &&
        finite(sample.gap.clearSeparationMeters) &&
        finite(sample.absSeparationTravelDot) &&
        finite(sample.primary.surfaceVelocityAtPointMapMetersPerSec) &&
        finite(sample.secondary.surfaceVelocityAtPointMapMetersPerSec);
}

} // namespace

MovingGapPredictor::Result MovingGapPredictor::predict(const Query& query) noexcept
{
    Result result;

    if (!finite(query.travelDirectionMap) ||
        !validBoundary(query.primary) ||
        !validBoundary(query.secondary) ||
        !finite(query.horizonSeconds) ||
        query.horizonSeconds <= 0.0 ||
        !validPolicy(query.policy) ||
        query.primary.obstacleId == query.secondary.obstacleId)
    {
        return result;
    }

    const Vec3d travelDirection = normalizeOrZero(query.travelDirectionMap);
    if (lengthSquared(travelDirection) <= kEpsilon)
        return result;

    result.validInput = true;
    if (query.primary.snapshotRevision != query.secondary.snapshotRevision)
    {
        result.status = Status::RevisionMismatch;
        return result;
    }

    const double dt = query.horizonSeconds / static_cast<double>(kIntervals);
    const double inflatedRadiusSum =
        query.primary.conservativeRadiusMeters +
        query.secondary.conservativeRadiusMeters +
        2.0 * query.policy.boundaryClearanceMeters;

    std::array<double, kSamples> centerDistances {};
    result.minimumSampleClearSeparationMeters =
        std::numeric_limits<double>::infinity();
    result.minimumContinuousClearSeparationMeters =
        std::numeric_limits<double>::infinity();

    bool sampleGapClosed = false;
    bool sampleAlignmentLost = false;

    for (std::size_t i = 0; i < kSamples; ++i)
    {
        const double timeSeconds = dt * static_cast<double>(i);
        if (!fillSample(
                query,
                travelDirection,
                timeSeconds,
                result.samples[i],
                centerDistances[i]))
        {
            // Coincident centers are valid motion input but imply that the
            // spherical gap has collapsed and its axis is undefined.
            result.status = Status::GapClosesDuringHorizon;
            result.minimumSampleClearSeparationMeters =
                -inflatedRadiusSum;
            result.minimumContinuousClearSeparationMeters =
                -inflatedRadiusSum;
            return result;
        }

        ++result.samplesEvaluated;
        result.minimumSampleClearSeparationMeters = std::min(
            result.minimumSampleClearSeparationMeters,
            result.samples[i].gap.clearSeparationMeters
        );
        result.maximumSampleAbsSeparationTravelDot = std::max(
            result.maximumSampleAbsSeparationTravelDot,
            result.samples[i].absSeparationTravelDot
        );

        if (result.samples[i].gap.clearSeparationMeters <
            query.policy.minimumContinuousClearSeparationMeters)
        {
            sampleGapClosed = true;
        }
        if (result.samples[i].absSeparationTravelDot >
            query.policy.maximumAbsSeparationTravelDot)
        {
            sampleAlignmentLost = true;
        }
    }

    const Vec3d relativeAcceleration = subtract(
        query.secondary.linearAccelerationMapMetersPerSec2,
        query.primary.linearAccelerationMapMetersPerSec2
    );
    const double relativeAccelerationMagnitude = length(relativeAcceleration);

    bool intervalGapClosed = false;
    bool intervalAlignmentLost = false;

    for (std::size_t i = 0; i < kIntervals; ++i)
    {
        const double t0 = result.samples[i].timeSeconds;
        const Vec3d relativePosition0 = subtract(
            result.samples[i].secondary.centerMapMeters,
            result.samples[i].primary.centerMapMeters
        );
        const Vec3d relativePosition1 = subtract(
            result.samples[i + 1].secondary.centerMapMeters,
            result.samples[i + 1].primary.centerMapMeters
        );
        const Vec3d relativeVelocity0 = subtract(
            result.samples[i].secondary.linearVelocityMapMetersPerSec,
            result.samples[i].primary.linearVelocityMapMetersPerSec
        );

        // Constant-acceleration motion differs from the endpoint chord by at
        // most |a_rel| * dt^2 / 8. Distance to the chord segment is exact, so
        // subtracting that deviation yields a conservative continuous lower
        // bound on obstacle-center separation for the whole interval.
        const double chordMinimumDistance = distancePointToSegmentFromOrigin(
            relativePosition0,
            relativePosition1
        );
        const double centerDeviationFromChord =
            relativeAccelerationMagnitude * dt * dt / 8.0;
        const double continuousCenterDistanceLowerBound = std::max(
            0.0,
            chordMinimumDistance - centerDeviationFromChord
        );
        const double continuousClearSeparationLowerBound =
            continuousCenterDistanceLowerBound - inflatedRadiusSum;

        result.minimumContinuousClearSeparationMeters = std::min(
            result.minimumContinuousClearSeparationMeters,
            continuousClearSeparationLowerBound
        );

        if (continuousClearSeparationLowerBound <
            query.policy.minimumContinuousClearSeparationMeters)
        {
            intervalGapClosed = true;
        }

        // Bound transverse alignment continuously. The numerator
        // dot(r(t), travel) is a scalar quadratic and its maximum absolute
        // value is obtained exactly from endpoints plus its stationary point.
        // Dividing by the conservative center-distance lower bound gives a
        // safe upper bound for |separationAxis dot travel| over the interval.
        double continuousAlignmentBound = 1.0;
        if (continuousCenterDistanceLowerBound > kEpsilon)
        {
            const double q0 = dot(relativePosition0, travelDirection);
            const double qv = dot(relativeVelocity0, travelDirection);
            const double qa = dot(relativeAcceleration, travelDirection);
            const double maxAbsNumerator = maximumAbsoluteQuadraticProjection(
                q0,
                qv,
                qa,
                dt
            );
            continuousAlignmentBound = std::min(
                1.0,
                maxAbsNumerator / continuousCenterDistanceLowerBound
            );
        }

        result.maximumContinuousAbsSeparationTravelDotBound = std::max(
            result.maximumContinuousAbsSeparationTravelDotBound,
            continuousAlignmentBound
        );
        if (continuousAlignmentBound >
            query.policy.maximumAbsSeparationTravelDot)
        {
            intervalAlignmentLost = true;
        }

        const Vec3d axis0 = result.samples[i].gap.separationAxisMap;
        const Vec3d axis1 = result.samples[i + 1].gap.separationAxisMap;
        const double endpointAxisAngle = std::acos(std::clamp(
            dot(axis0, axis1),
            -1.0,
            1.0
        ));
        double accelerationPerturbationAngle = 0.0;
        if (continuousCenterDistanceLowerBound > kEpsilon)
        {
            accelerationPerturbationAngle = std::asin(std::clamp(
                centerDeviationFromChord / continuousCenterDistanceLowerBound,
                0.0,
                1.0
            ));
        }
        else
        {
            accelerationPerturbationAngle = 0.5 * kPi;
        }

        const double axisSweepBound = std::min(
            kPi,
            endpointAxisAngle + 2.0 * accelerationPerturbationAngle
        );
        result.maximumAxisDeviationFromNearestSampleRad = std::max(
            result.maximumAxisDeviationFromNearestSampleRad,
            axisSweepBound
        );

        ++result.intervalsProven;
        (void)t0;
    }

    if (sampleGapClosed || intervalGapClosed)
    {
        result.status = Status::GapClosesDuringHorizon;
        return result;
    }
    if (sampleAlignmentLost || intervalAlignmentLost)
    {
        result.status = Status::AlignmentLost;
        return result;
    }

    result.status = Status::OpenForHorizon;
    return result;
}

} // namespace world::navigation
