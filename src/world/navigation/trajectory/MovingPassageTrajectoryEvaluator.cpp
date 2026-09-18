#include "MovingPassageTrajectoryEvaluator.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace world::navigation
{
namespace
{

using Evaluator = MovingPassageTrajectoryEvaluator;
using Vec3d = Evaluator::Vec3d;
using Basis3d = Evaluator::Basis3d;
using Pose = Evaluator::Pose;
using Passage = OrientedPassageEvaluator::Passage;
using PassageResult = OrientedPassageEvaluator::Result;

constexpr double kEpsilon = 1.0e-12;
constexpr double kTolerance = 1.0e-9;
constexpr double kBasisTolerance = 1.0e-6;
constexpr double kPi = 3.141592653589793238462643383279502884;

struct Quaternion
{
    double w = 1.0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct ShipSample
{
    Pose pose {};
    Vec3d velocity {};
    Vec3d acceleration {};
    Passage passage {};
    PassageResult passageResult {};
};

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

Vec3d transverseTo(const Vec3d& value, const Vec3d& forward) noexcept
{
    return subtract(value, scale(forward, dot(value, forward)));
}

bool unitVector(const Vec3d& value) noexcept
{
    return finite(value) && std::abs(lengthSquared(value) - 1.0) <= kBasisTolerance;
}

bool validBasis(const Basis3d& basis) noexcept
{
    if (!unitVector(basis.right) || !unitVector(basis.up) ||
        !unitVector(basis.forward) ||
        std::abs(dot(basis.right, basis.up)) > kBasisTolerance ||
        std::abs(dot(basis.right, basis.forward)) > kBasisTolerance ||
        std::abs(dot(basis.up, basis.forward)) > kBasisTolerance)
    {
        return false;
    }
    return dot(cross(basis.right, basis.up), basis.forward) >=
        1.0 - kBasisTolerance;
}

bool validHull(const Evaluator::HullProxy& hull) noexcept
{
    return finite(hull.halfExtentsBodyMeters) &&
        hull.halfExtentsBodyMeters.x >= 0.0 &&
        hull.halfExtentsBodyMeters.y >= 0.0 &&
        hull.halfExtentsBodyMeters.z >= 0.0 &&
        finite(hull.additionalClearanceMeters) &&
        hull.additionalClearanceMeters >= 0.0;
}

bool validLinearCapability(const Evaluator::LinearCapability& capability) noexcept
{
    return finite(capability.maxForwardAccelerationMetersPerSec2) &&
        capability.maxForwardAccelerationMetersPerSec2 >= 0.0 &&
        finite(capability.maxReverseAccelerationMetersPerSec2) &&
        capability.maxReverseAccelerationMetersPerSec2 >= 0.0 &&
        finite(capability.maxLateralAccelerationMetersPerSec2) &&
        capability.maxLateralAccelerationMetersPerSec2 >= 0.0 &&
        finite(capability.maxVerticalAccelerationMetersPerSec2) &&
        capability.maxVerticalAccelerationMetersPerSec2 >= 0.0;
}

bool validQuery(const Evaluator::Query& query) noexcept
{
    return query.movingGap != nullptr &&
        validHull(query.hull) &&
        finite(query.start.pose.centerMapMeters) &&
        finite(query.end.pose.centerMapMeters) &&
        validBasis(query.start.pose.bodyToMap) &&
        validBasis(query.end.pose.bodyToMap) &&
        finite(query.start.linearVelocityMapMetersPerSec) &&
        finite(query.end.linearVelocityMapMetersPerSec) &&
        finite(query.durationSeconds) && query.durationSeconds > 0.0 &&
        validLinearCapability(query.linearCapability) &&
        finite(query.angularCapability.maxAngularAccelerationRadPerSec2) &&
        query.angularCapability.maxAngularAccelerationRadPerSec2 >= 0.0 &&
        finite(query.angularCapability.maxAngularSpeedRadPerSec) &&
        query.angularCapability.maxAngularSpeedRadPerSec >= 0.0 &&
        finite(query.assistedMaxVelocityToForwardAngleRad) &&
        query.assistedMaxVelocityToForwardAngleRad >= 0.0 &&
        query.assistedMaxVelocityToForwardAngleRad <= kPi &&
        finite(query.maximumAcceptedGapTravelAlignment) &&
        query.maximumAcceptedGapTravelAlignment >= 0.0 &&
        query.maximumAcceptedGapTravelAlignment <= 1.0;
}

Quaternion normalizeQuaternion(Quaternion q) noexcept
{
    const double magnitude = std::sqrt(
        q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z
    );
    if (!finite(magnitude) || magnitude <= kEpsilon)
        return {};

    q.w /= magnitude;
    q.x /= magnitude;
    q.y /= magnitude;
    q.z /= magnitude;
    if (q.w < 0.0)
    {
        q.w = -q.w;
        q.x = -q.x;
        q.y = -q.y;
        q.z = -q.z;
    }
    return q;
}

Quaternion relativeRotation(const Basis3d& current, const Basis3d& target) noexcept
{
    const double c[3][3] = {
        {current.right.x, current.up.x, current.forward.x},
        {current.right.y, current.up.y, current.forward.y},
        {current.right.z, current.up.z, current.forward.z}
    };
    const double t[3][3] = {
        {target.right.x, target.up.x, target.forward.x},
        {target.right.y, target.up.y, target.forward.y},
        {target.right.z, target.up.z, target.forward.z}
    };

    double m[3][3] {};
    for (int row = 0; row < 3; ++row)
    {
        for (int col = 0; col < 3; ++col)
        {
            m[row][col] =
                t[row][0] * c[col][0] +
                t[row][1] * c[col][1] +
                t[row][2] * c[col][2];
        }
    }

    Quaternion q;
    const double trace = m[0][0] + m[1][1] + m[2][2];
    if (trace > 0.0)
    {
        const double s = std::sqrt(trace + 1.0) * 2.0;
        q.w = 0.25 * s;
        q.x = (m[2][1] - m[1][2]) / s;
        q.y = (m[0][2] - m[2][0]) / s;
        q.z = (m[1][0] - m[0][1]) / s;
    }
    else if (m[0][0] > m[1][1] && m[0][0] > m[2][2])
    {
        const double s = std::sqrt(std::max(
            0.0, 1.0 + m[0][0] - m[1][1] - m[2][2]
        )) * 2.0;
        if (s <= kEpsilon)
            return {};
        q.w = (m[2][1] - m[1][2]) / s;
        q.x = 0.25 * s;
        q.y = (m[0][1] + m[1][0]) / s;
        q.z = (m[0][2] + m[2][0]) / s;
    }
    else if (m[1][1] > m[2][2])
    {
        const double s = std::sqrt(std::max(
            0.0, 1.0 + m[1][1] - m[0][0] - m[2][2]
        )) * 2.0;
        if (s <= kEpsilon)
            return {};
        q.w = (m[0][2] - m[2][0]) / s;
        q.x = (m[0][1] + m[1][0]) / s;
        q.y = 0.25 * s;
        q.z = (m[1][2] + m[2][1]) / s;
    }
    else
    {
        const double s = std::sqrt(std::max(
            0.0, 1.0 + m[2][2] - m[0][0] - m[1][1]
        )) * 2.0;
        if (s <= kEpsilon)
            return {};
        q.w = (m[1][0] - m[0][1]) / s;
        q.x = (m[0][2] + m[2][0]) / s;
        q.y = (m[1][2] + m[2][1]) / s;
        q.z = 0.25 * s;
    }
    return normalizeQuaternion(q);
}

double quaternionAngle(const Quaternion& q) noexcept
{
    return 2.0 * std::acos(std::clamp(q.w, 0.0, 1.0));
}

Vec3d rotate(const Quaternion& q, const Vec3d& value) noexcept
{
    const Vec3d qv {q.x, q.y, q.z};
    const Vec3d first = cross(qv, value);
    const Vec3d second = cross(qv, first);
    return {
        value.x + 2.0 * (q.w * first.x + second.x),
        value.y + 2.0 * (q.w * first.y + second.y),
        value.z + 2.0 * (q.w * first.z + second.z)
    };
}

Basis3d interpolateBasis(
    const Basis3d& start,
    const Quaternion& startToEnd,
    double fraction
) noexcept
{
    fraction = std::clamp(fraction, 0.0, 1.0);
    const double fullAngle = quaternionAngle(startToEnd);
    if (fullAngle <= kEpsilon || fraction <= 0.0)
        return start;

    const double sinHalfFull = std::sqrt(std::max(
        0.0, 1.0 - startToEnd.w * startToEnd.w
    ));
    if (sinHalfFull <= kEpsilon)
        return start;

    const Vec3d axis {
        startToEnd.x / sinHalfFull,
        startToEnd.y / sinHalfFull,
        startToEnd.z / sinHalfFull
    };
    const double halfAngle = 0.5 * fullAngle * fraction;
    const double sine = std::sin(halfAngle);
    const Quaternion partial {
        std::cos(halfAngle),
        axis.x * sine,
        axis.y * sine,
        axis.z * sine
    };

    Basis3d result;
    result.right = rotate(partial, start.right);
    result.up = rotate(partial, start.up);
    result.forward = rotate(partial, start.forward);
    return result;
}

double smoothStep(double u) noexcept
{
    return u * u * (3.0 - 2.0 * u);
}

Vec3d hermitePosition(
    const Vec3d& p0,
    const Vec3d& v0,
    const Vec3d& p1,
    const Vec3d& v1,
    double duration,
    double u
) noexcept
{
    const double u2 = u * u;
    const double u3 = u2 * u;
    const double h00 = 2.0 * u3 - 3.0 * u2 + 1.0;
    const double h10 = u3 - 2.0 * u2 + u;
    const double h01 = -2.0 * u3 + 3.0 * u2;
    const double h11 = u3 - u2;
    return add(
        add(scale(p0, h00), scale(v0, h10 * duration)),
        add(scale(p1, h01), scale(v1, h11 * duration))
    );
}

Vec3d hermiteVelocity(
    const Vec3d& p0,
    const Vec3d& v0,
    const Vec3d& p1,
    const Vec3d& v1,
    double duration,
    double u
) noexcept
{
    const double u2 = u * u;
    const double h00 = 6.0 * u2 - 6.0 * u;
    const double h10 = 3.0 * u2 - 4.0 * u + 1.0;
    const double h01 = -6.0 * u2 + 6.0 * u;
    const double h11 = 3.0 * u2 - 2.0 * u;
    return scale(
        add(
            add(scale(p0, h00), scale(v0, h10 * duration)),
            add(scale(p1, h01), scale(v1, h11 * duration))
        ),
        1.0 / duration
    );
}

Vec3d hermiteAcceleration(
    const Vec3d& p0,
    const Vec3d& v0,
    const Vec3d& p1,
    const Vec3d& v1,
    double duration,
    double u
) noexcept
{
    const double h00 = 12.0 * u - 6.0;
    const double h10 = 6.0 * u - 4.0;
    const double h01 = -12.0 * u + 6.0;
    const double h11 = 6.0 * u - 2.0;
    return scale(
        add(
            add(scale(p0, h00), scale(v0, h10 * duration)),
            add(scale(p1, h01), scale(v1, h11 * duration))
        ),
        1.0 / (duration * duration)
    );
}

double vectorAngle(const Vec3d& a, const Vec3d& b) noexcept
{
    const double aLength = length(a);
    const double bLength = length(b);
    if (aLength <= kEpsilon || bLength <= kEpsilon)
        return 0.0;
    return std::acos(std::clamp(
        dot(a, b) / (aLength * bLength), -1.0, 1.0
    ));
}

double distancePointToSegmentFromOrigin(const Vec3d& a, const Vec3d& b) noexcept
{
    const Vec3d delta = subtract(b, a);
    const double deltaLengthSquared = lengthSquared(delta);
    if (deltaLengthSquared <= kEpsilon)
        return length(a);
    const double t = std::clamp(
        -dot(a, delta) / deltaLengthSquared, 0.0, 1.0
    );
    return length(add(a, scale(delta, t)));
}

double hullRotationRadius(const Evaluator::HullProxy& hull) noexcept
{
    const Vec3d& h = hull.halfExtentsBodyMeters;
    return std::sqrt(h.x * h.x + h.y * h.y + h.z * h.z) +
        hull.additionalClearanceMeters;
}

bool exceeds(double required, double available) noexcept
{
    return required > available + kTolerance;
}

} // namespace

MovingPassageTrajectoryEvaluator::Result
MovingPassageTrajectoryEvaluator::evaluate(const Query& query) noexcept
{
    Result result;
    if (!validQuery(query))
        return result;

    const MovingGap& movingGap = *query.movingGap;
    if (!movingGap.validInput ||
        movingGap.status != MovingGapPredictor::Status::OpenForHorizon ||
        movingGap.samplesEvaluated != kPoseSamples ||
        movingGap.intervalsProven != kIntervals ||
        movingGap.maximumContinuousAbsSeparationTravelDotBound >
            query.maximumAcceptedGapTravelAlignment + kTolerance)
    {
        result.status = Status::GapUnavailable;
        return result;
    }

    if (!finite(movingGap.samples.front().timeSeconds) ||
        !finite(movingGap.samples.back().timeSeconds) ||
        std::abs(movingGap.samples.front().timeSeconds) > kTolerance ||
        std::abs(movingGap.samples.back().timeSeconds - query.durationSeconds) >
            std::max(kTolerance, query.durationSeconds * 1.0e-9))
    {
        return result;
    }

    const Quaternion startToEnd = relativeRotation(
        query.start.pose.bodyToMap,
        query.end.pose.bodyToMap
    );
    result.orientationChangeRad = quaternionAngle(startToEnd);
    if (!finite(result.orientationChangeRad))
        return result;

    result.requiredPeakAngularSpeedRadPerSec =
        1.5 * result.orientationChangeRad / query.durationSeconds;
    result.requiredPeakAngularAccelerationRadPerSec2 =
        6.0 * result.orientationChangeRad /
        (query.durationSeconds * query.durationSeconds);

    if (exceeds(result.requiredPeakAngularSpeedRadPerSec,
                query.angularCapability.maxAngularSpeedRadPerSec) ||
        exceeds(result.requiredPeakAngularAccelerationRadPerSec2,
                query.angularCapability.maxAngularAccelerationRadPerSec2))
    {
        result.status = Status::AngularAuthorityExceeded;
        result.firstFailureIndex = 0;
        return result;
    }

    std::array<ShipSample, kPoseSamples> samples {};
    result.status = Status::Feasible;
    result.feasible = true;

    const double denominator = static_cast<double>(kIntervals);
    for (std::size_t i = 0; i < kPoseSamples; ++i)
    {
        const double u = static_cast<double>(i) / denominator;
        ShipSample& sample = samples[i];
        sample.pose.centerMapMeters = hermitePosition(
            query.start.pose.centerMapMeters,
            query.start.linearVelocityMapMetersPerSec,
            query.end.pose.centerMapMeters,
            query.end.linearVelocityMapMetersPerSec,
            query.durationSeconds,
            u
        );
        result.trajectory.centerSamplesMapMeters[i] =
            sample.pose.centerMapMeters;
        sample.velocity = hermiteVelocity(
            query.start.pose.centerMapMeters,
            query.start.linearVelocityMapMetersPerSec,
            query.end.pose.centerMapMeters,
            query.end.linearVelocityMapMetersPerSec,
            query.durationSeconds,
            u
        );
        sample.acceleration = hermiteAcceleration(
            query.start.pose.centerMapMeters,
            query.start.linearVelocityMapMetersPerSec,
            query.end.pose.centerMapMeters,
            query.end.linearVelocityMapMetersPerSec,
            query.durationSeconds,
            u
        );
        sample.pose.bodyToMap = interpolateBasis(
            query.start.pose.bodyToMap,
            startToEnd,
            smoothStep(u)
        );

        sample.passage = OrientedPassageEvaluator::makeObstacleGapPassage(
            movingGap.samples[i].gap
        );
        sample.passageResult = OrientedPassageEvaluator::evaluate(
            query.hull,
            sample.pose,
            sample.passage
        );
        ++result.samplesEvaluated;

        if (sample.passageResult.status ==
            OrientedPassageEvaluator::Status::InvalidInput)
        {
            result.status = Status::GapUnavailable;
            result.feasible = false;
            result.firstFailureIndex = i;
            return result;
        }
        if (!sample.passageResult.fits)
        {
            result.status = Status::GeometryBlocked;
            result.feasible = false;
            result.firstFailureIndex = i;
            result.minimumSampleClearanceMeters = std::min(
                sample.passageResult.widthClearanceMeters,
                sample.passageResult.heightClearanceMeters
            );
            return result;
        }

        const double sampleClearance = std::min(
            sample.passageResult.widthClearanceMeters,
            sample.passageResult.heightClearanceMeters
        );
        result.minimumSampleClearanceMeters = std::min(
            result.minimumSampleClearanceMeters,
            sampleClearance
        );

        const double forwardAcceleration = dot(
            sample.acceleration,
            sample.pose.bodyToMap.forward
        );
        const double requiredForward = std::max(0.0, forwardAcceleration);
        const double requiredReverse = std::max(0.0, -forwardAcceleration);
        const double requiredLateral = std::abs(dot(
            sample.acceleration,
            sample.pose.bodyToMap.right
        ));
        const double requiredVertical = std::abs(dot(
            sample.acceleration,
            sample.pose.bodyToMap.up
        ));

        result.requiredPeakForwardAccelerationMetersPerSec2 = std::max(
            result.requiredPeakForwardAccelerationMetersPerSec2,
            requiredForward
        );
        result.requiredPeakReverseAccelerationMetersPerSec2 = std::max(
            result.requiredPeakReverseAccelerationMetersPerSec2,
            requiredReverse
        );
        result.requiredPeakLateralAccelerationMetersPerSec2 = std::max(
            result.requiredPeakLateralAccelerationMetersPerSec2,
            requiredLateral
        );
        result.requiredPeakVerticalAccelerationMetersPerSec2 = std::max(
            result.requiredPeakVerticalAccelerationMetersPerSec2,
            requiredVertical
        );

        if (exceeds(requiredForward,
                    query.linearCapability.maxForwardAccelerationMetersPerSec2) ||
            exceeds(requiredReverse,
                    query.linearCapability.maxReverseAccelerationMetersPerSec2) ||
            exceeds(requiredLateral,
                    query.linearCapability.maxLateralAccelerationMetersPerSec2) ||
            exceeds(requiredVertical,
                    query.linearCapability.maxVerticalAccelerationMetersPerSec2))
        {
            result.status = Status::LinearAuthorityExceeded;
            result.feasible = false;
            result.firstFailureIndex = i;
            return result;
        }

        if (query.controlMode == ControlMode::EliteAssisted)
        {
            const double slipAngle = vectorAngle(
                sample.velocity,
                sample.pose.bodyToMap.forward
            );
            result.maximumObservedAssistedSlipAngleRad = std::max(
                result.maximumObservedAssistedSlipAngleRad,
                slipAngle
            );
            if (slipAngle > query.assistedMaxVelocityToForwardAngleRad + kTolerance)
            {
                result.status = Status::AssistedSlipExceeded;
                result.feasible = false;
                result.firstFailureIndex = i;
                return result;
            }
        }
    }

    const double dt = query.durationSeconds / denominator;
    const double halfDt = 0.5 * dt;
    const double rotationRadius = hullRotationRadius(query.hull);

    for (std::size_t i = 0; i < kIntervals; ++i)
    {
        const ShipSample& a = samples[i];
        const ShipSample& b = samples[i + 1];

        // Hermite acceleration is linear over each interval. Its vector norm is
        // convex, so the maximum endpoint magnitude bounds |p''(t)| throughout
        // the interval. The standard chord-deviation bound A*dt^2/8 therefore
        // encloses the exact cubic centerline continuously.
        const double accelerationMagnitudeBound = std::max(
            length(a.acceleration),
            length(b.acceleration)
        );
        result.trajectory.intervalCenterlineDeviationBoundsMeters[i] =
            accelerationMagnitudeBound * dt * dt / 8.0;

        const auto& gapA = movingGap.samples[i];
        const auto& gapB = movingGap.samples[i + 1];

        const Vec3d relativePositionA = subtract(
            gapA.secondary.centerMapMeters,
            gapA.primary.centerMapMeters
        );
        const Vec3d relativePositionB = subtract(
            gapB.secondary.centerMapMeters,
            gapB.primary.centerMapMeters
        );
        const Vec3d relativeVelocityA = subtract(
            gapA.secondary.linearVelocityMapMetersPerSec,
            gapA.primary.linearVelocityMapMetersPerSec
        );
        const Vec3d relativeVelocityB = subtract(
            gapB.secondary.linearVelocityMapMetersPerSec,
            gapB.primary.linearVelocityMapMetersPerSec
        );
        const Vec3d relativeAcceleration = scale(
            subtract(relativeVelocityB, relativeVelocityA),
            1.0 / dt
        );

        const double inflatedRadiusSum =
            length(relativePositionA) - gapA.gap.clearSeparationMeters;
        if (!finite(inflatedRadiusSum) || inflatedRadiusSum < 0.0)
        {
            result.status = Status::GapUnavailable;
            result.feasible = false;
            result.firstFailureIndex = i;
            return result;
        }

        const double centerDeviation = length(relativeAcceleration) * dt * dt / 8.0;
        const double chordMinimumDistance = distancePointToSegmentFromOrigin(
            relativePositionA,
            relativePositionB
        );
        const double continuousCenterDistanceLowerBound = std::max(
            0.0,
            chordMinimumDistance - centerDeviation
        );
        const double continuousClearSeparationLowerBound =
            continuousCenterDistanceLowerBound - inflatedRadiusSum;
        const double halfWidthLowerBound =
            0.5 * continuousClearSeparationLowerBound;
        const double halfHeightLowerBound = 0.5 * std::min(
            gapA.gap.secondaryClearanceMeters,
            gapB.gap.secondaryClearanceMeters
        );

        if (!finite(halfWidthLowerBound) || !finite(halfHeightLowerBound) ||
            halfWidthLowerBound <= 0.0 || halfHeightLowerBound <= 0.0)
        {
            result.status = Status::GeometryBlocked;
            result.feasible = false;
            result.firstFailureIndex = i;
            result.minimumContinuousClearanceBoundMeters = std::min(
                result.minimumContinuousClearanceBoundMeters,
                std::min(halfWidthLowerBound, halfHeightLowerBound)
            );
            return result;
        }

        const double endpointSeparationAxisAngle = vectorAngle(
            gapA.gap.separationAxisMap,
            gapB.gap.separationAxisMap
        );
        double accelerationPerturbationAngle = 0.5 * kPi;
        if (continuousCenterDistanceLowerBound > kEpsilon)
        {
            accelerationPerturbationAngle = std::asin(std::clamp(
                centerDeviation / continuousCenterDistanceLowerBound,
                0.0,
                1.0
            ));
        }
        const double separationAxisSweepBound = std::min(
            kPi,
            endpointSeparationAxisAngle + 2.0 * accelerationPerturbationAngle
        );

        // Projecting a transverse separation axis onto the plane normal to the
        // fixed travel direction can amplify its angular change. Under the
        // accepted <=0.5 alignment contract, a factor of two is conservative.
        const double passageAxisInflation = std::min(
            kPi,
            2.0 * separationAxisSweepBound
        );
        result.maximumPassageAxisInflationRad = std::max(
            result.maximumPassageAxisInflationRad,
            passageAxisInflation
        );

        const Vec3d midpointA = scale(add(
            gapA.primary.centerMapMeters,
            gapA.secondary.centerMapMeters
        ), 0.5);
        const Vec3d midpointB = scale(add(
            gapB.primary.centerMapMeters,
            gapB.secondary.centerMapMeters
        ), 0.5);
        const Vec3d midpointVelocityA = scale(add(
            gapA.primary.linearVelocityMapMetersPerSec,
            gapA.secondary.linearVelocityMapMetersPerSec
        ), 0.5);
        const Vec3d midpointVelocityB = scale(add(
            gapB.primary.linearVelocityMapMetersPerSec,
            gapB.secondary.linearVelocityMapMetersPerSec
        ), 0.5);
        const Vec3d midpointAcceleration = scale(
            subtract(midpointVelocityB, midpointVelocityA),
            1.0 / dt
        );

        const Vec3d passageForward = a.passage.passageToMap.forward;
        const Vec3d pA = subtract(a.pose.centerMapMeters, midpointA);
        const Vec3d pB = subtract(b.pose.centerMapMeters, midpointB);
        const Vec3d vA = subtract(a.velocity, midpointVelocityA);
        const Vec3d vB = subtract(b.velocity, midpointVelocityB);
        const Vec3d accA = subtract(a.acceleration, midpointAcceleration);
        const Vec3d accB = subtract(b.acceleration, midpointAcceleration);

        const Vec3d pTransverseA = transverseTo(pA, passageForward);
        const Vec3d pTransverseB = transverseTo(pB, passageForward);
        const Vec3d vTransverseA = transverseTo(vA, passageForward);
        const Vec3d vTransverseB = transverseTo(vB, passageForward);
        const Vec3d accTransverseA = transverseTo(accA, passageForward);
        const Vec3d accTransverseB = transverseTo(accB, passageForward);

        const double transverseAccelerationBound = std::max(
            length(accTransverseA),
            length(accTransverseB)
        );
        const double relativeCenterMotionBound =
            std::max(length(vTransverseA), length(vTransverseB)) * halfDt +
            0.5 * transverseAccelerationBound * halfDt * halfDt;
        result.maximumRelativeCenterMotionBoundMeters = std::max(
            result.maximumRelativeCenterMotionBoundMeters,
            relativeCenterMotionBound
        );

        const double transversePositionMagnitudeBound = std::max(
            length(pTransverseA),
            length(pTransverseB)
        ) + relativeCenterMotionBound;
        const double axisProjectionInflation =
            transversePositionMagnitudeBound *
            2.0 * std::sin(0.5 * passageAxisInflation);

        const double lateralOffsetBound = std::max(
            std::abs(a.passageResult.lateralOffsetMeters),
            std::abs(b.passageResult.lateralOffsetMeters)
        ) + relativeCenterMotionBound + axisProjectionInflation;
        const double verticalOffsetBound = std::max(
            std::abs(a.passageResult.verticalOffsetMeters),
            std::abs(b.passageResult.verticalOffsetMeters)
        ) + relativeCenterMotionBound + axisProjectionInflation;

        const double u0 = static_cast<double>(i) / denominator;
        const double u1 = static_cast<double>(i + 1) / denominator;
        const double bodyIntervalAngle = result.orientationChangeRad * std::abs(
            smoothStep(u1) - smoothStep(u0)
        );
        const double relativeOrientationSweep = std::min(
            kPi,
            bodyIntervalAngle + passageAxisInflation
        );
        const double orientationSweepInflation =
            2.0 * rotationRadius * std::sin(0.5 * relativeOrientationSweep);
        result.maximumRelativeOrientationSweepInflationMeters = std::max(
            result.maximumRelativeOrientationSweepInflationMeters,
            orientationSweepInflation
        );

        const double projectedHalfWidthBound = std::max(
            a.passageResult.projectedHalfWidthMeters,
            b.passageResult.projectedHalfWidthMeters
        ) + orientationSweepInflation;
        const double projectedHalfHeightBound = std::max(
            a.passageResult.projectedHalfHeightMeters,
            b.passageResult.projectedHalfHeightMeters
        ) + orientationSweepInflation;

        const double widthClearanceBound =
            halfWidthLowerBound - lateralOffsetBound - projectedHalfWidthBound;
        const double heightClearanceBound =
            halfHeightLowerBound - verticalOffsetBound - projectedHalfHeightBound;
        const double intervalClearanceBound = std::min(
            widthClearanceBound,
            heightClearanceBound
        );
        result.minimumContinuousClearanceBoundMeters = std::min(
            result.minimumContinuousClearanceBoundMeters,
            intervalClearanceBound
        );

        if (intervalClearanceBound < -kTolerance)
        {
            result.status = Status::GeometryBlocked;
            result.feasible = false;
            result.firstFailureIndex = i;
            return result;
        }

        // Hermite acceleration is linear in time. Fixed-axis projection extrema
        // are at interval endpoints; body rotation adds only this conservative
        // projection margin, matching the accepted static verifier contract.
        const double bodyProjectionMargin =
            accelerationMagnitudeBound *
            2.0 * std::sin(0.5 * bodyIntervalAngle);

        const double forwardA = dot(a.acceleration, a.pose.bodyToMap.forward);
        const double forwardB = dot(b.acceleration, b.pose.bodyToMap.forward);
        const double requiredForward = std::max({
            0.0, forwardA, forwardB
        }) + bodyProjectionMargin;
        const double requiredReverse = std::max({
            0.0, -forwardA, -forwardB
        }) + bodyProjectionMargin;
        const double requiredLateral = std::max(
            std::abs(dot(a.acceleration, a.pose.bodyToMap.right)),
            std::abs(dot(b.acceleration, b.pose.bodyToMap.right))
        ) + bodyProjectionMargin;
        const double requiredVertical = std::max(
            std::abs(dot(a.acceleration, a.pose.bodyToMap.up)),
            std::abs(dot(b.acceleration, b.pose.bodyToMap.up))
        ) + bodyProjectionMargin;

        result.requiredPeakForwardAccelerationMetersPerSec2 = std::max(
            result.requiredPeakForwardAccelerationMetersPerSec2,
            requiredForward
        );
        result.requiredPeakReverseAccelerationMetersPerSec2 = std::max(
            result.requiredPeakReverseAccelerationMetersPerSec2,
            requiredReverse
        );
        result.requiredPeakLateralAccelerationMetersPerSec2 = std::max(
            result.requiredPeakLateralAccelerationMetersPerSec2,
            requiredLateral
        );
        result.requiredPeakVerticalAccelerationMetersPerSec2 = std::max(
            result.requiredPeakVerticalAccelerationMetersPerSec2,
            requiredVertical
        );

        if (exceeds(requiredForward,
                    query.linearCapability.maxForwardAccelerationMetersPerSec2) ||
            exceeds(requiredReverse,
                    query.linearCapability.maxReverseAccelerationMetersPerSec2) ||
            exceeds(requiredLateral,
                    query.linearCapability.maxLateralAccelerationMetersPerSec2) ||
            exceeds(requiredVertical,
                    query.linearCapability.maxVerticalAccelerationMetersPerSec2))
        {
            result.status = Status::LinearAuthorityExceeded;
            result.feasible = false;
            result.firstFailureIndex = i;
            return result;
        }

        ++result.intervalsProven;
    }

    // Publish the control sample from the very trajectory we just proved.
    // Downstream runtime composition must not solve a second, merely similar
    // curve after accepting this result.
    result.initialLinearAccelerationMapMetersPerSec2 =
        samples.front().acceleration;
    result.trajectory.conservativeHullRadiusMeters = rotationRadius;
    result.trajectory.valid = true;

    return result;
}

} // namespace world::navigation
