#include "EmergencyPassageMitigator.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace world::navigation
{
namespace
{

using Mitigator = EmergencyPassageMitigator;
using Vec3d = Mitigator::Vec3d;
using Basis3d = Mitigator::Basis3d;
using PassageResult = Mitigator::PassageResult;

constexpr double kEpsilon = 1.0e-12;
constexpr double kPi = 3.141592653589793238462643383279502884;

struct Quaternion
{
    double w = 1.0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
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

    // q and -q represent the same rotation. Keep w non-negative so the
    // interpolation follows the shortest orientation arc (<= pi radians).
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
    // C/T are orientation matrices whose columns are right/up/forward.
    // Q = T * transpose(C) maps current map-space body axes to target axes.
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
        const double s = std::sqrt(
            std::max(0.0, 1.0 + m[0][0] - m[1][1] - m[2][2])
        ) * 2.0;
        if (s <= kEpsilon)
            return {};
        q.w = (m[2][1] - m[1][2]) / s;
        q.x = 0.25 * s;
        q.y = (m[0][1] + m[1][0]) / s;
        q.z = (m[0][2] + m[2][0]) / s;
    }
    else if (m[1][1] > m[2][2])
    {
        const double s = std::sqrt(
            std::max(0.0, 1.0 + m[1][1] - m[0][0] - m[2][2])
        ) * 2.0;
        if (s <= kEpsilon)
            return {};
        q.w = (m[0][2] - m[2][0]) / s;
        q.x = (m[0][1] + m[1][0]) / s;
        q.y = 0.25 * s;
        q.z = (m[1][2] + m[2][1]) / s;
    }
    else
    {
        const double s = std::sqrt(
            std::max(0.0, 1.0 + m[2][2] - m[0][0] - m[1][1])
        ) * 2.0;
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
    const Basis3d& current,
    const Quaternion& currentToPreferred,
    double fraction
) noexcept
{
    fraction = std::clamp(fraction, 0.0, 1.0);
    const double fullAngle = quaternionAngle(currentToPreferred);
    if (fullAngle <= kEpsilon || fraction <= 0.0)
        return current;

    const double sinHalfFull = std::sqrt(std::max(
        0.0,
        1.0 - currentToPreferred.w * currentToPreferred.w
    ));
    if (sinHalfFull <= kEpsilon)
        return current;

    const Vec3d axis {
        currentToPreferred.x / sinHalfFull,
        currentToPreferred.y / sinHalfFull,
        currentToPreferred.z / sinHalfFull
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
    result.right = rotate(partial, current.right);
    result.up = rotate(partial, current.up);
    result.forward = rotate(partial, current.forward);
    return result;
}

bool validScalarInputs(const Mitigator::Query& query) noexcept
{
    const auto& capability = query.angularCapability;
    return finite(query.predictedEntryCenterMapMeters) &&
        finite(query.currentAngularVelocityMapRadPerSec) &&
        finite(query.distanceToEntryMeters) &&
        query.distanceToEntryMeters >= 0.0 &&
        finite(query.closingSpeedMetersPerSec) &&
        query.closingSpeedMetersPerSec >= 0.0 &&
        finite(query.maxBrakingAccelerationMetersPerSec2) &&
        query.maxBrakingAccelerationMetersPerSec2 >= 0.0 &&
        finite(capability.maxAngularAccelerationRadPerSec2) &&
        capability.maxAngularAccelerationRadPerSec2 >= 0.0 &&
        finite(capability.maxAngularSpeedRadPerSec) &&
        capability.maxAngularSpeedRadPerSec >= 0.0;
}

double stopDistance(double speed, double deceleration) noexcept
{
    if (speed <= 0.0)
        return 0.0;
    if (deceleration <= 0.0)
        return std::numeric_limits<double>::infinity();
    return speed * speed / (2.0 * deceleration);
}

double timeToEntryWithMaxBraking(
    double distance,
    double speed,
    double deceleration,
    bool& canStop
) noexcept
{
    canStop = false;
    if (distance <= 0.0)
        return 0.0;
    if (speed <= kEpsilon)
    {
        canStop = true;
        return std::numeric_limits<double>::infinity();
    }
    if (deceleration <= 0.0)
        return distance / speed;

    const double stoppingDistance = stopDistance(speed, deceleration);
    if (stoppingDistance <= distance)
    {
        canStop = true;
        return std::numeric_limits<double>::infinity();
    }

    const double discriminant = std::max(
        0.0,
        speed * speed - 2.0 * deceleration * distance
    );
    return (speed - std::sqrt(discriminant)) / deceleration;
}

double entrySpeedWithMaxBraking(
    double distance,
    double speed,
    double deceleration,
    bool canStop
) noexcept
{
    if (canStop || speed <= 0.0)
        return 0.0;
    if (deceleration <= 0.0)
        return speed;
    return std::sqrt(std::max(
        0.0,
        speed * speed - 2.0 * deceleration * distance
    ));
}

double maxRestToRestAngle(
    double timeSeconds,
    double maxAngularAcceleration,
    double maxAngularSpeed
) noexcept
{
    if (timeSeconds <= 0.0 ||
        maxAngularAcceleration <= 0.0 ||
        maxAngularSpeed <= 0.0)
    {
        return 0.0;
    }
    if (!finite(timeSeconds))
        return kPi;

    const double timeToMaxSpeed =
        maxAngularSpeed / maxAngularAcceleration;
    if (timeSeconds <= 2.0 * timeToMaxSpeed)
    {
        return 0.25 * maxAngularAcceleration *
            timeSeconds * timeSeconds;
    }

    return maxAngularSpeed * timeSeconds -
        (maxAngularSpeed * maxAngularSpeed) / maxAngularAcceleration;
}

double minimumClearance(const PassageResult& result) noexcept
{
    return std::min(
        result.widthClearanceMeters,
        result.heightClearanceMeters
    );
}

double clearanceDeficit(const PassageResult& result) noexcept
{
    return
        std::max(0.0, -result.widthClearanceMeters) +
        std::max(0.0, -result.heightClearanceMeters);
}

struct Sample
{
    Basis3d basis {};
    PassageResult passage {};
    double fraction = 0.0;
    double correctionRad = 0.0;
    double minClearanceMeters = -std::numeric_limits<double>::infinity();
    double deficitMeters = std::numeric_limits<double>::infinity();
};

bool betterSample(const Sample& candidate, const Sample& currentBest) noexcept
{
    if (candidate.passage.fits != currentBest.passage.fits)
        return candidate.passage.fits;

    if (candidate.passage.fits)
    {
        if (candidate.minClearanceMeters != currentBest.minClearanceMeters)
            return candidate.minClearanceMeters > currentBest.minClearanceMeters;
    }
    else if (candidate.deficitMeters != currentBest.deficitMeters)
    {
        return candidate.deficitMeters < currentBest.deficitMeters;
    }

    // If geometric severity is equal, prefer the pose farther along the arc
    // toward the requested passage attitude (a more glancing/slot-aligned hit).
    return candidate.fraction > currentBest.fraction;
}

} // namespace

EmergencyPassageMitigator::Result EmergencyPassageMitigator::evaluate(
    const Query& query
) noexcept
{
    Result result;
    result.recommendedAimPointMapMeters = query.passage.centerMapMeters;
    result.desiredTravelDirectionMap = query.passage.passageToMap.forward;
    result.bestEffortBodyToMap = query.currentBodyToMap;

    if (!validScalarInputs(query))
        return result;

    OrientedPassageEvaluator::Pose currentPose;
    currentPose.centerMapMeters = query.predictedEntryCenterMapMeters;
    currentPose.bodyToMap = query.currentBodyToMap;

    OrientedPassageEvaluator::Pose preferredPose = currentPose;
    preferredPose.bodyToMap = query.preferredBodyToMap;

    const PassageResult currentPassage = OrientedPassageEvaluator::evaluate(
        query.hull,
        currentPose,
        query.passage
    );
    const PassageResult preferredPassage = OrientedPassageEvaluator::evaluate(
        query.hull,
        preferredPose,
        query.passage
    );

    if (currentPassage.status == OrientedPassageEvaluator::Status::InvalidInput ||
        preferredPassage.status == OrientedPassageEvaluator::Status::InvalidInput)
    {
        return result;
    }

    const Quaternion rotation = relativeRotation(
        query.currentBodyToMap,
        query.preferredBodyToMap
    );
    result.preferredAttitudeErrorRad = quaternionAngle(rotation);
    if (!finite(result.preferredAttitudeErrorRad))
        return result;

    bool canStop = false;
    result.availableTimeBeforeEntrySeconds = timeToEntryWithMaxBraking(
        query.distanceToEntryMeters,
        query.closingSpeedMetersPerSec,
        query.maxBrakingAccelerationMetersPerSec2,
        canStop
    );
    result.canStopBeforeEntry = canStop;

    const double currentAngularSpeed = length(
        query.currentAngularVelocityMapRadPerSec
    );
    const double angularAcceleration =
        query.angularCapability.maxAngularAccelerationRadPerSec2;
    const double angularSpeedLimit =
        query.angularCapability.maxAngularSpeedRadPerSec;

    if (currentAngularSpeed > kEpsilon && angularAcceleration > kEpsilon)
    {
        result.angularSettleTimeSeconds =
            currentAngularSpeed / angularAcceleration;
    }
    else if (currentAngularSpeed > kEpsilon)
    {
        result.angularSettleTimeSeconds =
            std::numeric_limits<double>::infinity();
    }

    double usableTime = result.availableTimeBeforeEntrySeconds;
    if (finite(usableTime))
    {
        usableTime = std::max(
            0.0,
            usableTime - result.angularSettleTimeSeconds
        );
    }

    result.maximumCorrectiveAngleRad = std::min(
        result.preferredAttitudeErrorRad,
        maxRestToRestAngle(
            usableTime,
            angularAcceleration,
            angularSpeedLimit
        )
    );

    double maxFraction = 0.0;
    if (result.preferredAttitudeErrorRad <= kEpsilon)
        maxFraction = 1.0;
    else
        maxFraction = std::clamp(
            result.maximumCorrectiveAngleRad /
                result.preferredAttitudeErrorRad,
            0.0,
            1.0
        );

    // Fixed-size sampling deliberately keeps the emergency fallback bounded.
    // It also avoids assuming that OBB cross-section clearance is monotonic
    // along an arbitrary 3D attitude interpolation arc.
    Sample best;
    bool haveBest = false;
    for (std::size_t index = 0; index < kOrientationSamples; ++index)
    {
        const double alpha =
            kOrientationSamples > 1
                ? static_cast<double>(index) /
                    static_cast<double>(kOrientationSamples - 1)
                : 0.0;
        const double fraction = maxFraction * alpha;
        const Basis3d basis = interpolateBasis(
            query.currentBodyToMap,
            rotation,
            fraction
        );

        OrientedPassageEvaluator::Pose pose;
        pose.centerMapMeters = query.predictedEntryCenterMapMeters;
        pose.bodyToMap = basis;

        const PassageResult passage = OrientedPassageEvaluator::evaluate(
            query.hull,
            pose,
            query.passage
        );
        if (passage.status == OrientedPassageEvaluator::Status::InvalidInput)
            continue;

        Sample sample;
        sample.basis = basis;
        sample.passage = passage;
        sample.fraction = fraction;
        sample.correctionRad = fraction * result.preferredAttitudeErrorRad;
        sample.minClearanceMeters = minimumClearance(passage);
        sample.deficitMeters = clearanceDeficit(passage);

        if (!haveBest || betterSample(sample, best))
        {
            best = sample;
            haveBest = true;
        }
    }

    if (!haveBest)
        return result;

    result.commandValid = true;
    result.bestEffortBodyToMap = best.basis;
    result.passageAtEntry = best.passage;
    result.appliedCorrectionFraction = best.fraction;
    result.appliedCorrectionRad = best.correctionRad;
    result.remainingAttitudeErrorRad = std::max(
        0.0,
        result.preferredAttitudeErrorRad - result.appliedCorrectionRad
    );
    result.minimumClearanceMeters = best.minClearanceMeters;
    result.clearanceDeficitMeters = best.deficitMeters;

    const double coastTime =
        query.closingSpeedMetersPerSec > kEpsilon
            ? query.distanceToEntryMeters / query.closingSpeedMetersPerSec
            : std::numeric_limits<double>::infinity();
    double coastUsableTime = coastTime;
    if (finite(coastUsableTime))
    {
        coastUsableTime = std::max(
            0.0,
            coastUsableTime - result.angularSettleTimeSeconds
        );
    }
    const double coastCorrectiveAngle = std::min(
        result.preferredAttitudeErrorRad,
        maxRestToRestAngle(
            coastUsableTime,
            angularAcceleration,
            angularSpeedLimit
        )
    );

    if (best.passage.fits)
    {
        result.status = Status::SafeEntryPose;
        result.collisionFreeEntryPoseProven = true;
        result.maximumBrakingRecommended =
            best.correctionRad > coastCorrectiveAngle + 1.0e-9;
        result.estimatedEntrySpeedMetersPerSec =
            result.maximumBrakingRecommended
                ? entrySpeedWithMaxBraking(
                    query.distanceToEntryMeters,
                    query.closingSpeedMetersPerSec,
                    query.maxBrakingAccelerationMetersPerSec2,
                    result.canStopBeforeEntry
                )
                : query.closingSpeedMetersPerSec;
        return result;
    }

    if (result.canStopBeforeEntry)
    {
        result.status = Status::EmergencyStopBeforeEntry;
        result.maximumBrakingRecommended =
            query.closingSpeedMetersPerSec > 0.0 &&
            query.maxBrakingAccelerationMetersPerSec2 > 0.0;
        result.estimatedEntrySpeedMetersPerSec = 0.0;
        return result;
    }

    result.status = Status::EmergencyMitigatedContact;
    result.contactExpected = true;
    result.maximumBrakingRecommended =
        query.closingSpeedMetersPerSec > 0.0 &&
        query.maxBrakingAccelerationMetersPerSec2 > 0.0;
    result.estimatedEntrySpeedMetersPerSec = entrySpeedWithMaxBraking(
        query.distanceToEntryMeters,
        query.closingSpeedMetersPerSec,
        query.maxBrakingAccelerationMetersPerSec2,
        false
    );
    return result;
}

} // namespace world::navigation
