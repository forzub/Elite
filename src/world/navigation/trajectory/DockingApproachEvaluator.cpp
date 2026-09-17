#include "DockingApproachEvaluator.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace world::navigation
{
namespace
{

using Evaluator = DockingApproachEvaluator;
using Vec3d = Evaluator::Vec3d;
using Basis3d = Evaluator::Basis3d;
using Pose = Evaluator::Pose;
using Terminal = Evaluator::TerminalEvaluator;
using Geometry = Evaluator::GeometryEvaluator;

constexpr double kEpsilon = 1.0e-12;
constexpr double kTolerance = 1.0e-9;
constexpr double kBasisTolerance = 1.0e-6;
constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kHugeCapability = 1.0e18;

struct Quaternion
{
    double w = 1.0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct PhysicalSample
{
    Vec3d localPosition {};
    Vec3d localVelocity {};
    Vec3d localAcceleration {};
    Pose worldPose {};
    Vec3d worldVelocity {};
    Vec3d worldAcceleration {};
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

bool validAngularCapability(const Evaluator::AngularCapability& capability) noexcept
{
    return finite(capability.maxAngularAccelerationRadPerSec2) &&
        capability.maxAngularAccelerationRadPerSec2 >= 0.0 &&
        finite(capability.maxAngularSpeedRadPerSec) &&
        capability.maxAngularSpeedRadPerSec >= 0.0;
}

bool validPort(const Terminal::LocalPortFrame& port) noexcept
{
    return finite(port.positionLocalMeters) &&
        unitVector(port.matingNormalLocal) &&
        unitVector(port.referenceUpLocal) &&
        std::abs(dot(port.matingNormalLocal, port.referenceUpLocal)) <=
            kBasisTolerance;
}

bool validDock(const Terminal::MovingDockState& dock) noexcept
{
    return finite(dock.originPoseAtReferenceTime.centerMapMeters) &&
        validBasis(dock.originPoseAtReferenceTime.bodyToMap) &&
        finite(dock.originLinearVelocityMapMetersPerSec) &&
        finite(dock.originLinearAccelerationMapMetersPerSec2) &&
        finite(dock.angularVelocityMapRadPerSec);
}

bool validTerminalContract(const Terminal::MatingContract& contract) noexcept
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
    return validHull(query.hull) &&
        finite(query.start.pose.centerMapMeters) &&
        validBasis(query.start.pose.bodyToMap) &&
        finite(query.start.linearVelocityMapMetersPerSec) &&
        finite(query.end.pose.centerMapMeters) &&
        validBasis(query.end.pose.bodyToMap) &&
        finite(query.end.linearVelocityMapMetersPerSec) &&
        finite(query.durationSeconds) && query.durationSeconds > 0.0 &&
        validLinearCapability(query.linearCapability) &&
        validAngularCapability(query.angularCapability) &&
        finite(query.assistedMaxVelocityToForwardAngleRad) &&
        query.assistedMaxVelocityToForwardAngleRad >= 0.0 &&
        query.assistedMaxVelocityToForwardAngleRad <= kPi &&
        validPort(query.shipPort) &&
        validDock(query.dock) &&
        validPort(query.dockPort) &&
        validTerminalContract(query.terminalContract) &&
        finite(query.corridor.halfWidthMeters) &&
        query.corridor.halfWidthMeters > 0.0 &&
        finite(query.corridor.halfHeightMeters) &&
        query.corridor.halfHeightMeters > 0.0;
}

Vec3d toWorld(const Basis3d& basis, const Vec3d& local) noexcept
{
    return add(
        add(scale(basis.right, local.x), scale(basis.up, local.y)),
        scale(basis.forward, local.z)
    );
}

Vec3d toLocal(const Basis3d& basis, const Vec3d& world) noexcept
{
    return {
        dot(world, basis.right),
        dot(world, basis.up),
        dot(world, basis.forward)
    };
}

Basis3d basisWorldToLocal(const Basis3d& frame, const Basis3d& body) noexcept
{
    Basis3d result;
    result.right = toLocal(frame, body.right);
    result.up = toLocal(frame, body.up);
    result.forward = toLocal(frame, body.forward);
    return result;
}

Basis3d basisLocalToWorld(const Basis3d& frame, const Basis3d& bodyLocal) noexcept
{
    Basis3d result;
    result.right = toWorld(frame, bodyLocal.right);
    result.up = toWorld(frame, bodyLocal.up);
    result.forward = toWorld(frame, bodyLocal.forward);
    return result;
}

Basis3d corridorBasis(const Terminal::PortWorldState& port) noexcept
{
    Basis3d basis;
    basis.forward = scale(port.matingNormalMap, -1.0);
    basis.up = port.referenceUpMap;
    basis.right = cross(basis.up, basis.forward);
    return basis;
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

Basis3d predictDockOriginBasis(
    const Terminal::MovingDockState& dock,
    double timeSeconds
) noexcept
{
    const double angularSpeed = length(dock.angularVelocityMapRadPerSec);
    if (angularSpeed <= kEpsilon || timeSeconds <= 0.0)
        return dock.originPoseAtReferenceTime.bodyToMap;

    const Vec3d axis = scale(
        dock.angularVelocityMapRadPerSec,
        1.0 / angularSpeed
    );
    const double angle = angularSpeed * timeSeconds;

    Basis3d result;
    result.right = rotateAroundAxis(
        dock.originPoseAtReferenceTime.bodyToMap.right,
        axis,
        angle
    );
    result.up = rotateAroundAxis(
        dock.originPoseAtReferenceTime.bodyToMap.up,
        axis,
        angle
    );
    result.forward = rotateAroundAxis(
        dock.originPoseAtReferenceTime.bodyToMap.forward,
        axis,
        angle
    );
    return result;
}

Vec3d dockPortAcceleration(
    const Evaluator::Query& query,
    double timeSeconds
) noexcept
{
    const Basis3d originBasis = predictDockOriginBasis(query.dock, timeSeconds);
    const Vec3d portOffset = toWorld(
        originBasis,
        query.dockPort.positionLocalMeters
    );
    return add(
        query.dock.originLinearAccelerationMapMetersPerSec2,
        cross(
            query.dock.angularVelocityMapRadPerSec,
            cross(query.dock.angularVelocityMapRadPerSec, portOffset)
        )
    );
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

double smoothStepRate(double u, double durationSeconds) noexcept
{
    return 6.0 * u * (1.0 - u) / durationSeconds;
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

Vec3d hermiteJerk(
    const Vec3d& p0,
    const Vec3d& v0,
    const Vec3d& p1,
    const Vec3d& v1,
    double duration
) noexcept
{
    const Vec3d numerator = add(
        scale(subtract(p0, p1), 12.0),
        scale(add(v0, v1), 6.0 * duration)
    );
    return scale(numerator, 1.0 / (duration * duration * duration));
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

bool exceeds(double required, double available) noexcept
{
    return required > available + kTolerance;
}

double maxSmoothStepRateOnInterval(
    double u0,
    double u1,
    double durationSeconds
) noexcept
{
    const double probe = std::clamp(0.5, u0, u1);
    return std::max({
        smoothStepRate(u0, durationSeconds),
        smoothStepRate(u1, durationSeconds),
        smoothStepRate(probe, durationSeconds)
    });
}

} // namespace

DockingApproachEvaluator::Result DockingApproachEvaluator::evaluate(
    const Query& query
) noexcept
{
    Result result;
    if (!validQuery(query))
        return result;

    const Terminal::PortWorldState port0 =
        Terminal::predictDockPortWorldState(query.dock, query.dockPort, 0.0);
    const Terminal::PortWorldState port1 =
        Terminal::predictDockPortWorldState(
            query.dock,
            query.dockPort,
            query.durationSeconds
        );
    const Basis3d frame0 = corridorBasis(port0);
    const Basis3d frame1 = corridorBasis(port1);
    if (!validBasis(frame0) || !validBasis(frame1))
        return result;

    const Vec3d relativePositionWorld0 = subtract(
        query.start.pose.centerMapMeters,
        port0.positionMapMeters
    );
    const Vec3d relativePositionWorld1 = subtract(
        query.end.pose.centerMapMeters,
        port1.positionMapMeters
    );
    const Vec3d relativeVelocityWorld0 = subtract(
        subtract(
            query.start.linearVelocityMapMetersPerSec,
            port0.linearVelocityMapMetersPerSec
        ),
        cross(query.dock.angularVelocityMapRadPerSec, relativePositionWorld0)
    );
    const Vec3d relativeVelocityWorld1 = subtract(
        subtract(
            query.end.linearVelocityMapMetersPerSec,
            port1.linearVelocityMapMetersPerSec
        ),
        cross(query.dock.angularVelocityMapRadPerSec, relativePositionWorld1)
    );

    const Vec3d localPosition0 = toLocal(frame0, relativePositionWorld0);
    const Vec3d localPosition1 = toLocal(frame1, relativePositionWorld1);
    const Vec3d localVelocity0 = toLocal(frame0, relativeVelocityWorld0);
    const Vec3d localVelocity1 = toLocal(frame1, relativeVelocityWorld1);

    const Basis3d relativeBody0 = basisWorldToLocal(
        frame0,
        query.start.pose.bodyToMap
    );
    const Basis3d relativeBody1 = basisWorldToLocal(
        frame1,
        query.end.pose.bodyToMap
    );
    if (!validBasis(relativeBody0) || !validBasis(relativeBody1))
        return result;

    Geometry::Query geometryQuery;
    geometryQuery.hull = query.hull;
    geometryQuery.passage.source =
        OrientedPassageEvaluator::PassageSource::DockingCorridor;
    geometryQuery.passage.centerMapMeters = {};
    geometryQuery.passage.passageToMap = {};
    geometryQuery.passage.halfWidthMeters = query.corridor.halfWidthMeters;
    geometryQuery.passage.halfHeightMeters = query.corridor.halfHeightMeters;
    geometryQuery.start.pose.centerMapMeters = localPosition0;
    geometryQuery.start.pose.bodyToMap = relativeBody0;
    geometryQuery.start.linearVelocityMapMetersPerSec = localVelocity0;
    geometryQuery.end.pose.centerMapMeters = localPosition1;
    geometryQuery.end.pose.bodyToMap = relativeBody1;
    geometryQuery.end.linearVelocityMapMetersPerSec = localVelocity1;
    geometryQuery.durationSeconds = query.durationSeconds;
    geometryQuery.linearCapability.maxForwardAccelerationMetersPerSec2 =
        kHugeCapability;
    geometryQuery.linearCapability.maxReverseAccelerationMetersPerSec2 =
        kHugeCapability;
    geometryQuery.linearCapability.maxLateralAccelerationMetersPerSec2 =
        kHugeCapability;
    geometryQuery.linearCapability.maxVerticalAccelerationMetersPerSec2 =
        kHugeCapability;
    geometryQuery.angularCapability.maxAngularAccelerationRadPerSec2 =
        kHugeCapability;
    geometryQuery.angularCapability.maxAngularSpeedRadPerSec = kHugeCapability;
    geometryQuery.controlMode = ControlMode::Newtonian;

    result.dockLocalGeometry = Geometry::evaluate(geometryQuery);
    result.minimumSampleCorridorClearanceMeters =
        result.dockLocalGeometry.minimumSampleClearanceMeters;
    result.minimumContinuousCorridorClearanceMeters =
        result.dockLocalGeometry.minimumContinuousClearanceBoundMeters;
    result.intervalsProven = result.dockLocalGeometry.intervalsProven;

    if (result.dockLocalGeometry.status == Geometry::Status::GeometryBlocked)
    {
        result.status = Status::CorridorBlocked;
        result.firstFailureIndex = result.dockLocalGeometry.firstFailureIndex;
        return result;
    }
    if (result.dockLocalGeometry.status != Geometry::Status::Feasible)
        return result;

    const Quaternion relativeStartToEnd = relativeRotation(
        relativeBody0,
        relativeBody1
    );
    result.relativeOrientationChangeRad = quaternionAngle(relativeStartToEnd);
    if (!finite(result.relativeOrientationChangeRad))
        return result;

    result.requiredPeakRelativeAngularSpeedRadPerSec =
        1.5 * result.relativeOrientationChangeRad / query.durationSeconds;
    result.requiredPeakRelativeAngularAccelerationRadPerSec2 =
        6.0 * result.relativeOrientationChangeRad /
        (query.durationSeconds * query.durationSeconds);

    const double dockAngularSpeed = length(
        query.dock.angularVelocityMapRadPerSec
    );
    result.requiredPeakWorldAngularSpeedBoundRadPerSec =
        dockAngularSpeed + result.requiredPeakRelativeAngularSpeedRadPerSec;
    result.requiredPeakWorldAngularAccelerationBoundRadPerSec2 =
        result.requiredPeakRelativeAngularAccelerationRadPerSec2 +
        dockAngularSpeed * result.requiredPeakRelativeAngularSpeedRadPerSec;

    if (exceeds(result.requiredPeakWorldAngularSpeedBoundRadPerSec,
                query.angularCapability.maxAngularSpeedRadPerSec) ||
        exceeds(result.requiredPeakWorldAngularAccelerationBoundRadPerSec2,
                query.angularCapability.maxAngularAccelerationRadPerSec2))
    {
        result.status = Status::AngularAuthorityExceeded;
        result.firstFailureIndex = 0;
        return result;
    }

    std::array<PhysicalSample, kPoseSamples> samples {};
    const double denominator = static_cast<double>(kIntervals);
    const Vec3d localJerk = hermiteJerk(
        localPosition0,
        localVelocity0,
        localPosition1,
        localVelocity1,
        query.durationSeconds
    );

    for (std::size_t i = 0; i < kPoseSamples; ++i)
    {
        const double u = static_cast<double>(i) / denominator;
        const double timeSeconds = query.durationSeconds * u;
        PhysicalSample& sample = samples[i];

        sample.localPosition = hermitePosition(
            localPosition0,
            localVelocity0,
            localPosition1,
            localVelocity1,
            query.durationSeconds,
            u
        );
        sample.localVelocity = hermiteVelocity(
            localPosition0,
            localVelocity0,
            localPosition1,
            localVelocity1,
            query.durationSeconds,
            u
        );
        sample.localAcceleration = hermiteAcceleration(
            localPosition0,
            localVelocity0,
            localPosition1,
            localVelocity1,
            query.durationSeconds,
            u
        );

        const Terminal::PortWorldState port =
            Terminal::predictDockPortWorldState(
                query.dock,
                query.dockPort,
                timeSeconds
            );
        const Basis3d frame = corridorBasis(port);
        if (!validBasis(frame))
            return Result {};

        const Vec3d relativeOffsetWorld = toWorld(
            frame,
            sample.localPosition
        );
        const Vec3d relativeVelocityInFrameWorld = toWorld(
            frame,
            sample.localVelocity
        );
        const Vec3d relativeAccelerationInFrameWorld = toWorld(
            frame,
            sample.localAcceleration
        );

        sample.worldPose.centerMapMeters = add(
            port.positionMapMeters,
            relativeOffsetWorld
        );
        sample.worldVelocity = add(
            add(
                port.linearVelocityMapMetersPerSec,
                cross(
                    query.dock.angularVelocityMapRadPerSec,
                    relativeOffsetWorld
                )
            ),
            relativeVelocityInFrameWorld
        );
        sample.worldAcceleration = add(
            add(
                dockPortAcceleration(query, timeSeconds),
                cross(
                    query.dock.angularVelocityMapRadPerSec,
                    cross(
                        query.dock.angularVelocityMapRadPerSec,
                        relativeOffsetWorld
                    )
                )
            ),
            add(
                scale(
                    cross(
                        query.dock.angularVelocityMapRadPerSec,
                        relativeVelocityInFrameWorld
                    ),
                    2.0
                ),
                relativeAccelerationInFrameWorld
            )
        );

        const Basis3d relativeBody = interpolateBasis(
            relativeBody0,
            relativeStartToEnd,
            smoothStep(u)
        );
        sample.worldPose.bodyToMap = basisLocalToWorld(frame, relativeBody);
        if (!validBasis(sample.worldPose.bodyToMap) ||
            !finite(sample.worldPose.centerMapMeters) ||
            !finite(sample.worldVelocity) ||
            !finite(sample.worldAcceleration))
        {
            return Result {};
        }

        result.maximumWorldAccelerationMagnitudeMetersPerSec2 = std::max(
            result.maximumWorldAccelerationMagnitudeMetersPerSec2,
            length(sample.worldAcceleration)
        );

        if (query.controlMode == ControlMode::EliteAssisted)
        {
            const double slipAngle = vectorAngle(
                sample.worldVelocity,
                sample.worldPose.bodyToMap.forward
            );
            result.maximumObservedAssistedSlipAngleRad = std::max(
                result.maximumObservedAssistedSlipAngleRad,
                slipAngle
            );
            if (slipAngle > query.assistedMaxVelocityToForwardAngleRad + kTolerance)
            {
                result.status = Status::AssistedSlipExceeded;
                result.firstFailureIndex = i;
                result.samplesEvaluated = i + 1;
                return result;
            }
        }

        ++result.samplesEvaluated;
    }

    const double dt = query.durationSeconds / denominator;
    const double halfDt = 0.5 * dt;
    const double localJerkMagnitude = length(localJerk);
    const double dockPortOffsetMagnitude = length(
        query.dockPort.positionLocalMeters
    );
    const double originAccelerationMagnitude = length(
        query.dock.originLinearAccelerationMapMetersPerSec2
    );

    for (std::size_t i = 0; i < kIntervals; ++i)
    {
        const PhysicalSample& a = samples[i];
        const PhysicalSample& b = samples[i + 1];

        const double localAccelerationBound = std::max(
            length(a.localAcceleration),
            length(b.localAcceleration)
        );
        const double localVelocityBound = std::max(
            length(a.localVelocity),
            length(b.localVelocity)
        ) + localAccelerationBound * halfDt;
        const double localPositionBound = std::max(
            length(a.localPosition),
            length(b.localPosition)
        ) + localVelocityBound * halfDt +
            0.5 * localAccelerationBound * halfDt * halfDt;

        const double omega = dockAngularSpeed;
        const double accelerationMagnitudeBound =
            originAccelerationMagnitude +
            omega * omega * (dockPortOffsetMagnitude + localPositionBound) +
            2.0 * omega * localVelocityBound +
            localAccelerationBound;
        const double jerkMagnitudeBound =
            omega * omega * omega *
                (dockPortOffsetMagnitude + localPositionBound) +
            3.0 * omega * omega * localVelocityBound +
            3.0 * omega * localAccelerationBound +
            localJerkMagnitude;

        const double u0 = static_cast<double>(i) / denominator;
        const double u1 = static_cast<double>(i + 1) / denominator;
        const double relativeAngularSpeedBound =
            result.relativeOrientationChangeRad *
            maxSmoothStepRateOnInterval(u0, u1, query.durationSeconds);
        const double bodyAngularSpeedBound = omega + relativeAngularSpeedBound;
        const double projectionMargin =
            (jerkMagnitudeBound +
             accelerationMagnitudeBound * bodyAngularSpeedBound) * halfDt;

        const double forwardA = dot(
            a.worldAcceleration,
            a.worldPose.bodyToMap.forward
        );
        const double forwardB = dot(
            b.worldAcceleration,
            b.worldPose.bodyToMap.forward
        );
        const double requiredForward = std::max({0.0, forwardA, forwardB}) +
            projectionMargin;
        const double requiredReverse = std::max({0.0, -forwardA, -forwardB}) +
            projectionMargin;
        const double requiredLateral = std::max(
            std::abs(dot(a.worldAcceleration, a.worldPose.bodyToMap.right)),
            std::abs(dot(b.worldAcceleration, b.worldPose.bodyToMap.right))
        ) + projectionMargin;
        const double requiredVertical = std::max(
            std::abs(dot(a.worldAcceleration, a.worldPose.bodyToMap.up)),
            std::abs(dot(b.worldAcceleration, b.worldPose.bodyToMap.up))
        ) + projectionMargin;

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
            result.firstFailureIndex = i;
            return result;
        }
    }

    Terminal::Query terminalQuery;
    terminalQuery.shipAtCapture.pose = samples.back().worldPose;
    terminalQuery.shipAtCapture.linearVelocityMapMetersPerSec =
        samples.back().worldVelocity;
    // Relative attitude interpolation is rest-to-rest in dock-local space, so
    // world angular velocity equals the dock angular velocity at capture.
    terminalQuery.shipAtCapture.angularVelocityMapRadPerSec =
        query.dock.angularVelocityMapRadPerSec;
    terminalQuery.shipPort = query.shipPort;
    terminalQuery.dock = query.dock;
    terminalQuery.dockPort = query.dockPort;
    terminalQuery.captureTimeSeconds = query.durationSeconds;
    terminalQuery.contract = query.terminalContract;

    result.terminal = Terminal::evaluate(terminalQuery);
    if (result.terminal.status != Terminal::Status::Capturable)
    {
        result.status = Status::TerminalNotCapturable;
        result.firstFailureIndex = kPoseSamples - 1;
        return result;
    }

    result.status = Status::FeasibleForCapture;
    result.feasible = true;
    return result;
}

} // namespace world::navigation
