#include "DockingTerminalEvaluator.h"

#include <algorithm>
#include <cmath>

namespace world::navigation
{
namespace
{

using Evaluator = DockingTerminalEvaluator;
using Vec3d = Evaluator::Vec3d;
using Basis3d = Evaluator::Basis3d;
using Pose = Evaluator::Pose;

constexpr double kEpsilon = 1.0e-12;
constexpr double kBasisTolerance = 1.0e-6;
constexpr double kTolerance = 1.0e-9;
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

Vec3d transformDirection(const Basis3d& basis, const Vec3d& local) noexcept
{
    return add(
        add(scale(basis.right, local.x), scale(basis.up, local.y)),
        scale(basis.forward, local.z)
    );
}

bool validPort(const Evaluator::LocalPortFrame& port) noexcept
{
    if (!finite(port.positionLocalMeters) ||
        !unitVector(port.matingNormalLocal) ||
        !unitVector(port.referenceUpLocal))
    {
        return false;
    }

    return std::abs(dot(port.matingNormalLocal, port.referenceUpLocal)) <=
        kBasisTolerance;
}

bool validRigidBody(const Evaluator::RigidBodyState& body) noexcept
{
    return finite(body.pose.centerMapMeters) &&
        validBasis(body.pose.bodyToMap) &&
        finite(body.linearVelocityMapMetersPerSec) &&
        finite(body.angularVelocityMapRadPerSec);
}

bool validDock(const Evaluator::MovingDockState& dock) noexcept
{
    return finite(dock.originPoseAtReferenceTime.centerMapMeters) &&
        validBasis(dock.originPoseAtReferenceTime.bodyToMap) &&
        finite(dock.originLinearVelocityMapMetersPerSec) &&
        finite(dock.originLinearAccelerationMapMetersPerSec2) &&
        finite(dock.angularVelocityMapRadPerSec);
}

bool validContract(const Evaluator::MatingContract& contract) noexcept
{
    return finite(contract.maxPositionErrorMeters) &&
        contract.maxPositionErrorMeters >= 0.0 &&
        finite(contract.maxRelativeLinearSpeedMetersPerSec) &&
        contract.maxRelativeLinearSpeedMetersPerSec >= 0.0 &&
        finite(contract.maxNormalAlignmentErrorRad) &&
        contract.maxNormalAlignmentErrorRad >= 0.0 &&
        contract.maxNormalAlignmentErrorRad <= kPi &&
        finite(contract.maxRollAlignmentErrorRad) &&
        contract.maxRollAlignmentErrorRad >= 0.0 &&
        contract.maxRollAlignmentErrorRad <= kPi &&
        finite(contract.maxRelativeAngularSpeedRadPerSec) &&
        contract.maxRelativeAngularSpeedRadPerSec >= 0.0;
}

bool validQuery(const Evaluator::Query& query) noexcept
{
    return validRigidBody(query.shipAtCapture) &&
        validPort(query.shipPort) &&
        validDock(query.dock) &&
        validPort(query.dockPort) &&
        finite(query.captureTimeSeconds) &&
        query.captureTimeSeconds >= 0.0 &&
        validContract(query.contract);
}

Vec3d rotateAroundAxis(
    const Vec3d& value,
    const Vec3d& unitAxis,
    double angleRad
) noexcept
{
    if (std::abs(angleRad) <= kEpsilon)
        return value;

    const double cosine = std::cos(angleRad);
    const double sine = std::sin(angleRad);
    return add(
        add(
            scale(value, cosine),
            scale(cross(unitAxis, value), sine)
        ),
        scale(unitAxis, dot(unitAxis, value) * (1.0 - cosine))
    );
}

Basis3d predictBasis(
    const Basis3d& initial,
    const Vec3d& angularVelocityMapRadPerSec,
    double timeSeconds
) noexcept
{
    const double angularSpeed = length(angularVelocityMapRadPerSec);
    if (!finite(angularSpeed) || angularSpeed <= kEpsilon || timeSeconds <= 0.0)
        return initial;

    const Vec3d axis = scale(angularVelocityMapRadPerSec, 1.0 / angularSpeed);
    const double angle = angularSpeed * timeSeconds;

    Basis3d result;
    result.right = rotateAroundAxis(initial.right, axis, angle);
    result.up = rotateAroundAxis(initial.up, axis, angle);
    result.forward = rotateAroundAxis(initial.forward, axis, angle);
    return result;
}

Evaluator::PortWorldState shipPortWorld(
    const Evaluator::RigidBodyState& ship,
    const Evaluator::LocalPortFrame& port
) noexcept
{
    Evaluator::PortWorldState state;
    const Vec3d offset = transformDirection(
        ship.pose.bodyToMap,
        port.positionLocalMeters
    );

    state.positionMapMeters = add(ship.pose.centerMapMeters, offset);
    state.linearVelocityMapMetersPerSec = add(
        ship.linearVelocityMapMetersPerSec,
        cross(ship.angularVelocityMapRadPerSec, offset)
    );
    state.angularVelocityMapRadPerSec = ship.angularVelocityMapRadPerSec;
    state.matingNormalMap = transformDirection(
        ship.pose.bodyToMap,
        port.matingNormalLocal
    );
    state.referenceUpMap = transformDirection(
        ship.pose.bodyToMap,
        port.referenceUpLocal
    );
    return state;
}

Evaluator::PortWorldState dockPortWorldUnchecked(
    const Evaluator::MovingDockState& dock,
    const Evaluator::LocalPortFrame& port,
    double timeSeconds
) noexcept
{
    Evaluator::PortWorldState state;

    const Vec3d originPosition = add(
        add(
            dock.originPoseAtReferenceTime.centerMapMeters,
            scale(dock.originLinearVelocityMapMetersPerSec, timeSeconds)
        ),
        scale(
            dock.originLinearAccelerationMapMetersPerSec2,
            0.5 * timeSeconds * timeSeconds
        )
    );
    const Vec3d originVelocity = add(
        dock.originLinearVelocityMapMetersPerSec,
        scale(dock.originLinearAccelerationMapMetersPerSec2, timeSeconds)
    );
    const Basis3d predictedBasis = predictBasis(
        dock.originPoseAtReferenceTime.bodyToMap,
        dock.angularVelocityMapRadPerSec,
        timeSeconds
    );
    const Vec3d offset = transformDirection(
        predictedBasis,
        port.positionLocalMeters
    );

    state.positionMapMeters = add(originPosition, offset);
    // Required moving-port kinematics: v_port = v_origin + omega x r.
    state.linearVelocityMapMetersPerSec = add(
        originVelocity,
        cross(dock.angularVelocityMapRadPerSec, offset)
    );
    state.angularVelocityMapRadPerSec = dock.angularVelocityMapRadPerSec;
    state.matingNormalMap = transformDirection(
        predictedBasis,
        port.matingNormalLocal
    );
    state.referenceUpMap = transformDirection(
        predictedBasis,
        port.referenceUpLocal
    );
    return state;
}

double angleBetweenUnit(const Vec3d& a, const Vec3d& b) noexcept
{
    return std::acos(std::clamp(dot(a, b), -1.0, 1.0));
}

} // namespace

DockingTerminalEvaluator::PortWorldState
DockingTerminalEvaluator::predictDockPortWorldState(
    const MovingDockState& dock,
    const LocalPortFrame& port,
    double timeSeconds
) noexcept
{
    if (!validDock(dock) || !validPort(port) ||
        !finite(timeSeconds) || timeSeconds < 0.0)
    {
        return {};
    }

    return dockPortWorldUnchecked(dock, port, timeSeconds);
}

DockingTerminalEvaluator::Result DockingTerminalEvaluator::evaluate(
    const Query& query
) noexcept
{
    Result result;
    if (!validQuery(query))
        return result;

    if (query.shipPort.surface != query.contract.requiredShipSurface ||
        query.dockPort.surface != query.contract.requiredDockSurface)
    {
        result.status = Status::PortSemanticMismatch;
        return result;
    }

    result.shipPortWorld = shipPortWorld(query.shipAtCapture, query.shipPort);
    result.dockPortWorld = predictDockPortWorldState(
        query.dock,
        query.dockPort,
        query.captureTimeSeconds
    );

    const Vec3d positionDelta = subtract(
        result.shipPortWorld.positionMapMeters,
        result.dockPortWorld.positionMapMeters
    );
    result.positionErrorMeters = length(positionDelta);
    result.axialPositionErrorMeters = std::abs(dot(
        positionDelta,
        result.dockPortWorld.matingNormalMap
    ));
    const Vec3d lateralPositionDelta = subtract(
        positionDelta,
        scale(
            result.dockPortWorld.matingNormalMap,
            dot(positionDelta, result.dockPortWorld.matingNormalMap)
        )
    );
    result.lateralPositionErrorMeters = length(lateralPositionDelta);

    const Vec3d relativeLinearVelocity = subtract(
        result.shipPortWorld.linearVelocityMapMetersPerSec,
        result.dockPortWorld.linearVelocityMapMetersPerSec
    );
    result.relativeLinearSpeedMetersPerSec = length(relativeLinearVelocity);
    result.relativeNormalSpeedMetersPerSec = std::abs(dot(
        relativeLinearVelocity,
        result.dockPortWorld.matingNormalMap
    ));
    const Vec3d relativeTangentialVelocity = subtract(
        relativeLinearVelocity,
        scale(
            result.dockPortWorld.matingNormalMap,
            dot(relativeLinearVelocity, result.dockPortWorld.matingNormalMap)
        )
    );
    result.relativeTangentialSpeedMetersPerSec = length(relativeTangentialVelocity);

    const Vec3d desiredShipNormal = scale(
        result.dockPortWorld.matingNormalMap,
        -1.0
    );
    result.matingNormalAlignmentErrorRad = angleBetweenUnit(
        result.shipPortWorld.matingNormalMap,
        desiredShipNormal
    );
    result.rollAlignmentErrorRad = angleBetweenUnit(
        result.shipPortWorld.referenceUpMap,
        result.dockPortWorld.referenceUpMap
    );

    const Vec3d relativeAngularVelocity = subtract(
        result.shipPortWorld.angularVelocityMapRadPerSec,
        result.dockPortWorld.angularVelocityMapRadPerSec
    );
    result.relativeAngularSpeedRadPerSec = length(relativeAngularVelocity);

    if (!finite(result.positionErrorMeters) ||
        !finite(result.axialPositionErrorMeters) ||
        !finite(result.lateralPositionErrorMeters) ||
        !finite(result.relativeLinearSpeedMetersPerSec) ||
        !finite(result.relativeNormalSpeedMetersPerSec) ||
        !finite(result.relativeTangentialSpeedMetersPerSec) ||
        !finite(result.matingNormalAlignmentErrorRad) ||
        !finite(result.rollAlignmentErrorRad) ||
        !finite(result.relativeAngularSpeedRadPerSec))
    {
        return Result {};
    }

    if (result.positionErrorMeters >
        query.contract.maxPositionErrorMeters + kTolerance)
    {
        result.status = Status::PositionMismatch;
        return result;
    }
    if (result.relativeLinearSpeedMetersPerSec >
        query.contract.maxRelativeLinearSpeedMetersPerSec + kTolerance)
    {
        result.status = Status::RelativeLinearVelocityMismatch;
        return result;
    }
    if (result.matingNormalAlignmentErrorRad >
        query.contract.maxNormalAlignmentErrorRad + kTolerance)
    {
        result.status = Status::MatingNormalMismatch;
        return result;
    }
    if (result.rollAlignmentErrorRad >
        query.contract.maxRollAlignmentErrorRad + kTolerance)
    {
        result.status = Status::RollAlignmentMismatch;
        return result;
    }
    if (result.relativeAngularSpeedRadPerSec >
        query.contract.maxRelativeAngularSpeedRadPerSec + kTolerance)
    {
        result.status = Status::RelativeAngularVelocityMismatch;
        return result;
    }

    result.status = Status::Capturable;
    result.capturable = true;
    return result;
}

} // namespace world::navigation
