#include "AttitudeReachabilityEvaluator.h"

#include <algorithm>
#include <cmath>

namespace world::navigation
{
namespace
{

using Evaluator = AttitudeReachabilityEvaluator;
using Vec3d = Evaluator::Vec3d;
using Basis3d = Evaluator::Basis3d;

constexpr double kEpsilon = 1.0e-12;
constexpr double kBasisTolerance = 1.0e-6;
constexpr double kReadyAngleToleranceRad = 1.0e-6;
constexpr double kReadyAngularSpeedToleranceRadPerSec = 1.0e-6;
constexpr double kPi = 3.141592653589793238462643383279502884;

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

double lengthSquared(const Vec3d& value) noexcept
{
    return dot(value, value);
}

double length(const Vec3d& value) noexcept
{
    return std::sqrt(lengthSquared(value));
}

bool unitVector(const Vec3d& value) noexcept
{
    return finite(value) && std::abs(lengthSquared(value) - 1.0) <= kBasisTolerance;
}

bool validBasis(const Basis3d& basis) noexcept
{
    if (!unitVector(basis.right) ||
        !unitVector(basis.up) ||
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

bool validCapability(const Evaluator::AngularCapability& capability) noexcept
{
    return finite(capability.maxAngularAccelerationRadPerSec2) &&
        finite(capability.maxAngularSpeedRadPerSec) &&
        capability.maxAngularAccelerationRadPerSec2 > 0.0 &&
        capability.maxAngularSpeedRadPerSec > 0.0;
}

bool validApproach(const Evaluator::ApproachState& approach) noexcept
{
    return validBasis(approach.currentBodyToMap) &&
        validBasis(approach.requiredBodyToMap) &&
        finite(approach.currentAngularVelocityMapRadPerSec) &&
        finite(approach.distanceToEntryMeters) &&
        approach.distanceToEntryMeters >= 0.0 &&
        finite(approach.closingSpeedMetersPerSec) &&
        approach.closingSpeedMetersPerSec >= 0.0 &&
        finite(approach.maxBrakingAccelerationMetersPerSec2) &&
        approach.maxBrakingAccelerationMetersPerSec2 >= 0.0;
}

double attitudeErrorRad(
    const Basis3d& current,
    const Basis3d& target
) noexcept
{
    const double trace =
        dot(current.right, target.right) +
        dot(current.up, target.up) +
        dot(current.forward, target.forward);
    const double cosine = std::clamp(0.5 * (trace - 1.0), -1.0, 1.0);
    return std::acos(cosine);
}

double restToRestRotationTime(
    double angleRad,
    double maxAngularAcceleration,
    double maxAngularSpeed
) noexcept
{
    if (angleRad <= kReadyAngleToleranceRad)
        return 0.0;

    const double triangularAngle =
        (maxAngularSpeed * maxAngularSpeed) / maxAngularAcceleration;

    if (angleRad <= triangularAngle)
    {
        return 2.0 * std::sqrt(angleRad / maxAngularAcceleration);
    }

    const double accelerationTime = maxAngularSpeed / maxAngularAcceleration;
    const double cruiseAngle = angleRad - triangularAngle;
    return 2.0 * accelerationTime + cruiseAngle / maxAngularSpeed;
}

double distanceWhileBraking(
    double initialSpeed,
    double brakingAcceleration,
    double timeSeconds
) noexcept
{
    if (initialSpeed <= 0.0 || timeSeconds <= 0.0)
        return 0.0;

    if (brakingAcceleration <= 0.0)
        return initialSpeed * timeSeconds;

    const double stopTime = initialSpeed / brakingAcceleration;
    if (timeSeconds >= stopTime)
    {
        return 0.5 * initialSpeed * stopTime;
    }

    return initialSpeed * timeSeconds -
        0.5 * brakingAcceleration * timeSeconds * timeSeconds;
}

} // namespace

AttitudeReachabilityEvaluator::Result AttitudeReachabilityEvaluator::evaluate(
    const AngularCapability& capability,
    const ApproachState& approach
) noexcept
{
    Result result;
    result.availableDistanceMeters = approach.distanceToEntryMeters;

    if (!validCapability(capability) || !validApproach(approach))
    {
        result.status = Status::InvalidInput;
        return result;
    }

    result.attitudeErrorRad = attitudeErrorRad(
        approach.currentBodyToMap,
        approach.requiredBodyToMap
    );

    const double currentAngularSpeed = length(
        approach.currentAngularVelocityMapRadPerSec
    );

    if (result.attitudeErrorRad <= kReadyAngleToleranceRad &&
        currentAngularSpeed <= kReadyAngularSpeedToleranceRadPerSec)
    {
        result.status = Status::AlreadyReady;
        result.reachable = true;
        result.distanceMarginMeters = result.availableDistanceMeters;
        return result;
    }

    // Current angular velocity may be around an unhelpful axis. Before claiming
    // a guaranteed target-oriented rotation, conservatively budget time to
    // settle it and the maximum orientation drift accumulated while doing so.
    result.angularSettleTimeSeconds =
        currentAngularSpeed / capability.maxAngularAccelerationRadPerSec2;
    result.conservativeSettleAngleRad = std::min(
        kPi,
        0.5 * currentAngularSpeed * result.angularSettleTimeSeconds
    );

    const double conservativeAngle = std::min(
        kPi,
        result.attitudeErrorRad + result.conservativeSettleAngleRad
    );

    result.minimumRotationTimeSeconds =
        result.angularSettleTimeSeconds +
        restToRestRotationTime(
            conservativeAngle,
            capability.maxAngularAccelerationRadPerSec2,
            capability.maxAngularSpeedRadPerSec
        );

    result.coastDistanceBeforeReadyMeters =
        approach.closingSpeedMetersPerSec * result.minimumRotationTimeSeconds;

    if (result.coastDistanceBeforeReadyMeters <= result.availableDistanceMeters)
    {
        result.minimumDistanceBeforeReadyMeters =
            result.coastDistanceBeforeReadyMeters;
        result.distanceMarginMeters =
            result.availableDistanceMeters - result.minimumDistanceBeforeReadyMeters;
        result.status = Status::ReachableCoast;
        result.reachable = true;
        return result;
    }

    result.minimumDistanceBeforeReadyMeters = distanceWhileBraking(
        approach.closingSpeedMetersPerSec,
        approach.maxBrakingAccelerationMetersPerSec2,
        result.minimumRotationTimeSeconds
    );
    result.distanceMarginMeters =
        result.availableDistanceMeters - result.minimumDistanceBeforeReadyMeters;

    if (approach.maxBrakingAccelerationMetersPerSec2 > 0.0 &&
        result.minimumDistanceBeforeReadyMeters <= result.availableDistanceMeters)
    {
        result.status = Status::ReachableWithBraking;
        result.reachable = true;
        result.brakingRequired = true;
        return result;
    }

    result.status = Status::UnreachableBeforeEntry;
    return result;
}

} // namespace world::navigation
