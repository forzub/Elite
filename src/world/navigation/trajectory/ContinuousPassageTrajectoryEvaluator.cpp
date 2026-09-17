#include "ContinuousPassageTrajectoryEvaluator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace world::navigation
{
namespace
{

using Evaluator = ContinuousPassageTrajectoryEvaluator;
using Vec3d = Evaluator::Vec3d;
using Basis3d = Evaluator::Basis3d;
using Pose = Evaluator::Pose;
using PassageResult = OrientedPassageEvaluator::Result;

constexpr double kEpsilon = 1.0e-12;
constexpr double kTolerance = 1.0e-9;
constexpr double kPi = 3.141592653589793238462643383279502884;

struct Quaternion
{
    double w = 1.0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct Sample
{
    double u = 0.0;
    Pose pose {};
    Vec3d velocity {};
    Vec3d acceleration {};
    PassageResult passage {};
    double forwardAcceleration = 0.0;
    double lateralAcceleration = 0.0;
    double verticalAcceleration = 0.0;
    double assistedSlipAngleRad = 0.0;
};

bool finite(double value) noexcept
{
    return std::isfinite(value);
}

bool finite(const Vec3d& value) noexcept
{
    return finite(value.x) && finite(value.y) && finite(value.z);
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

Vec3d add(const Vec3d& a, const Vec3d& b) noexcept
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3d multiply(const Vec3d& value, double scale) noexcept
{
    return {value.x * scale, value.y * scale, value.z * scale};
}

double lengthSquared(const Vec3d& value) noexcept
{
    return dot(value, value);
}

double length(const Vec3d& value) noexcept
{
    return std::sqrt(lengthSquared(value));
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

Quaternion relativeRotation(
    const Basis3d& current,
    const Basis3d& target
) noexcept
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
            0.0,
            1.0 + m[0][0] - m[1][1] - m[2][2]
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
            0.0,
            1.0 + m[1][1] - m[0][0] - m[2][2]
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
            0.0,
            1.0 + m[2][2] - m[0][0] - m[1][1]
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
        0.0,
        1.0 - startToEnd.w * startToEnd.w
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
        add(multiply(p0, h00), multiply(v0, h10 * duration)),
        add(multiply(p1, h01), multiply(v1, h11 * duration))
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

    return multiply(
        add(
            add(multiply(p0, h00), multiply(v0, h10 * duration)),
            add(multiply(p1, h01), multiply(v1, h11 * duration))
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

    return multiply(
        add(
            add(multiply(p0, h00), multiply(v0, h10 * duration)),
            add(multiply(p1, h01), multiply(v1, h11 * duration))
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
    const double cosine = std::clamp(
        dot(a, b) / (aLength * bLength),
        -1.0,
        1.0
    );
    return std::acos(cosine);
}

bool validCapability(const Evaluator::LinearCapability& capability) noexcept
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

bool validQueryScalars(const Evaluator::Query& query) noexcept
{
    return finite(query.start.pose.centerMapMeters) &&
        finite(query.end.pose.centerMapMeters) &&
        finite(query.start.linearVelocityMapMetersPerSec) &&
        finite(query.end.linearVelocityMapMetersPerSec) &&
        finite(query.durationSeconds) &&
        query.durationSeconds > 0.0 &&
        validCapability(query.linearCapability) &&
        finite(query.angularCapability.maxAngularAccelerationRadPerSec2) &&
        query.angularCapability.maxAngularAccelerationRadPerSec2 >= 0.0 &&
        finite(query.angularCapability.maxAngularSpeedRadPerSec) &&
        query.angularCapability.maxAngularSpeedRadPerSec >= 0.0 &&
        finite(query.assistedMaxVelocityToForwardAngleRad) &&
        query.assistedMaxVelocityToForwardAngleRad >= 0.0 &&
        query.assistedMaxVelocityToForwardAngleRad <= kPi;
}

double hullRotationRadius(const Evaluator::HullProxy& hull) noexcept
{
    const Vec3d& h = hull.halfExtentsBodyMeters;
    return std::sqrt(h.x * h.x + h.y * h.y + h.z * h.z) +
        std::max(0.0, hull.additionalClearanceMeters);
}

} // namespace

ContinuousPassageTrajectoryEvaluator::Result
ContinuousPassageTrajectoryEvaluator::evaluate(const Query& query) noexcept
{
    Result result;

    if (!validQueryScalars(query))
        return result;

    const PassageResult startPassage = OrientedPassageEvaluator::evaluate(
        query.hull,
        query.start.pose,
        query.passage
    );
    const PassageResult endPassage = OrientedPassageEvaluator::evaluate(
        query.hull,
        query.end.pose,
        query.passage
    );
    if (startPassage.status == OrientedPassageEvaluator::Status::InvalidInput ||
        endPassage.status == OrientedPassageEvaluator::Status::InvalidInput)
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

    if (result.requiredPeakAngularSpeedRadPerSec >
            query.angularCapability.maxAngularSpeedRadPerSec + kTolerance ||
        result.requiredPeakAngularAccelerationRadPerSec2 >
            query.angularCapability.maxAngularAccelerationRadPerSec2 + kTolerance)
    {
        result.status = Status::AngularAuthorityExceeded;
        result.firstFailureIndex = 0;
        return result;
    }

    std::array<Sample, kPoseSamples> samples {};
    const double sampleDenominator =
        static_cast<double>(kPoseSamples - 1);

    result.status = Status::Feasible;
    result.feasible = true;

    for (std::size_t index = 0; index < kPoseSamples; ++index)
    {
        const double u = static_cast<double>(index) / sampleDenominator;
        Sample& sample = samples[index];
        sample.u = u;
        sample.pose.centerMapMeters = hermitePosition(
            query.start.pose.centerMapMeters,
            query.start.linearVelocityMapMetersPerSec,
            query.end.pose.centerMapMeters,
            query.end.linearVelocityMapMetersPerSec,
            query.durationSeconds,
            u
        );
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
        sample.passage = OrientedPassageEvaluator::evaluate(
            query.hull,
            sample.pose,
            query.passage
        );
        ++result.samplesEvaluated;

        if (sample.passage.status == OrientedPassageEvaluator::Status::InvalidInput)
        {
            result.status = Status::InvalidInput;
            result.feasible = false;
            result.firstFailureIndex = index;
            return result;
        }

        const double sampleClearance = std::min(
            sample.passage.widthClearanceMeters,
            sample.passage.heightClearanceMeters
        );
        result.minimumSampleClearanceMeters = std::min(
            result.minimumSampleClearanceMeters,
            sampleClearance
        );

        sample.forwardAcceleration = dot(
            sample.acceleration,
            sample.pose.bodyToMap.forward
        );
        sample.lateralAcceleration = dot(
            sample.acceleration,
            sample.pose.bodyToMap.right
        );
        sample.verticalAcceleration = dot(
            sample.acceleration,
            sample.pose.bodyToMap.up
        );

        if (query.controlMode == ControlMode::EliteAssisted)
        {
            sample.assistedSlipAngleRad = vectorAngle(
                sample.velocity,
                sample.pose.bodyToMap.forward
            );
            result.maximumObservedAssistedSlipAngleRad = std::max(
                result.maximumObservedAssistedSlipAngleRad,
                sample.assistedSlipAngleRad
            );
            if (sample.assistedSlipAngleRad >
                    query.assistedMaxVelocityToForwardAngleRad + kTolerance)
            {
                if (result.feasible)
                {
                    result.status = Status::AssistedSlipExceeded;
                    result.firstFailureIndex = index;
                }
                result.feasible = false;
            }
        }

        if (!sample.passage.fits)
        {
            if (result.feasible)
            {
                result.status = Status::GeometryBlocked;
                result.firstFailureIndex = index;
            }
            result.feasible = false;
        }
    }

    const double dt = query.durationSeconds / sampleDenominator;
    const double rotationRadius = hullRotationRadius(query.hull);
    const Vec3d passageRight = query.passage.passageToMap.right;
    const Vec3d passageUp = query.passage.passageToMap.up;

    for (std::size_t index = 0; index + 1 < kPoseSamples; ++index)
    {
        const Sample& a = samples[index];
        const Sample& b = samples[index + 1];

        const double s0 = smoothStep(a.u);
        const double s1 = smoothStep(b.u);
        const double intervalRotationRad =
            result.orientationChangeRad * std::abs(s1 - s0);
        const double rotationalSweepInflation =
            2.0 * rotationRadius *
            std::sin(0.5 * std::min(kPi, intervalRotationRad));
        result.maximumRotationalSweepInflationMeters = std::max(
            result.maximumRotationalSweepInflationMeters,
            rotationalSweepInflation
        );

        const double maxRightAcceleration = std::max(
            std::abs(dot(a.acceleration, passageRight)),
            std::abs(dot(b.acceleration, passageRight))
        );
        const double maxUpAcceleration = std::max(
            std::abs(dot(a.acceleration, passageUp)),
            std::abs(dot(b.acceleration, passageUp))
        );
        const double rightCurveDeviation =
            maxRightAcceleration * dt * dt / 8.0;
        const double upCurveDeviation =
            maxUpAcceleration * dt * dt / 8.0;
        result.maximumCenterCurveDeviationBoundMeters = std::max(
            result.maximumCenterCurveDeviationBoundMeters,
            std::max(rightCurveDeviation, upCurveDeviation)
        );

        const double widthBound =
            query.passage.halfWidthMeters -
            std::max(
                std::abs(a.passage.lateralOffsetMeters),
                std::abs(b.passage.lateralOffsetMeters)
            ) -
            std::max(
                a.passage.projectedHalfWidthMeters,
                b.passage.projectedHalfWidthMeters
            ) -
            rightCurveDeviation -
            rotationalSweepInflation;
        const double heightBound =
            query.passage.halfHeightMeters -
            std::max(
                std::abs(a.passage.verticalOffsetMeters),
                std::abs(b.passage.verticalOffsetMeters)
            ) -
            std::max(
                a.passage.projectedHalfHeightMeters,
                b.passage.projectedHalfHeightMeters
            ) -
            upCurveDeviation -
            rotationalSweepInflation;

        const double continuousClearance = std::min(widthBound, heightBound);
        result.minimumContinuousClearanceBoundMeters = std::min(
            result.minimumContinuousClearanceBoundMeters,
            continuousClearance
        );

        if (continuousClearance < -kTolerance)
        {
            if (result.feasible)
            {
                result.status = Status::GeometryBlocked;
                result.firstFailureIndex = index;
            }
            result.feasible = false;
        }
        ++result.intervalsProven;

        // Hermite acceleration is a convex linear interpolation of the two
        // endpoint acceleration vectors inside this interval. With a fixed
        // body axis, endpoint projections already contain the exact extrema.
        // Extra projection margin is needed only because the body axis rotates.
        const double maxAccelerationMagnitude = std::max(
            length(a.acceleration),
            length(b.acceleration)
        );
        const double projectionError =
            maxAccelerationMagnitude *
            2.0 * std::sin(0.5 * std::min(kPi, intervalRotationRad));

        const double forwardUpper = std::max(
            a.forwardAcceleration,
            b.forwardAcceleration
        ) + projectionError;
        const double forwardLower = std::min(
            a.forwardAcceleration,
            b.forwardAcceleration
        ) - projectionError;
        const double lateralRequired = std::max(
            std::abs(a.lateralAcceleration),
            std::abs(b.lateralAcceleration)
        ) + projectionError;
        const double verticalRequired = std::max(
            std::abs(a.verticalAcceleration),
            std::abs(b.verticalAcceleration)
        ) + projectionError;

        const double forwardRequired = std::max(0.0, forwardUpper);
        const double reverseRequired = std::max(0.0, -forwardLower);

        result.requiredPeakForwardAccelerationMetersPerSec2 = std::max(
            result.requiredPeakForwardAccelerationMetersPerSec2,
            forwardRequired
        );
        result.requiredPeakReverseAccelerationMetersPerSec2 = std::max(
            result.requiredPeakReverseAccelerationMetersPerSec2,
            reverseRequired
        );
        result.requiredPeakLateralAccelerationMetersPerSec2 = std::max(
            result.requiredPeakLateralAccelerationMetersPerSec2,
            lateralRequired
        );
        result.requiredPeakVerticalAccelerationMetersPerSec2 = std::max(
            result.requiredPeakVerticalAccelerationMetersPerSec2,
            verticalRequired
        );

        const bool linearExceeded =
            forwardRequired >
                query.linearCapability.maxForwardAccelerationMetersPerSec2 +
                    kTolerance ||
            reverseRequired >
                query.linearCapability.maxReverseAccelerationMetersPerSec2 +
                    kTolerance ||
            lateralRequired >
                query.linearCapability.maxLateralAccelerationMetersPerSec2 +
                    kTolerance ||
            verticalRequired >
                query.linearCapability.maxVerticalAccelerationMetersPerSec2 +
                    kTolerance;

        if (linearExceeded)
        {
            if (result.feasible)
            {
                result.status = Status::LinearAuthorityExceeded;
                result.firstFailureIndex = index;
            }
            result.feasible = false;
        }
    }

    if (result.minimumContinuousClearanceBoundMeters ==
        std::numeric_limits<double>::infinity())
    {
        result.minimumContinuousClearanceBoundMeters =
            result.minimumSampleClearanceMeters;
    }

    if (result.feasible)
        result.status = Status::Feasible;

    return result;
}

} // namespace world::navigation
