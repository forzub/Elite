#include "src/game/navigation/AcceptedManeuverProgram.h"
#include "src/game/navigation/TrajectoryFollower.h"
#include "src/game/navigation/NavigationRuntimeControlBridge.h"
#include "src/game/navigation/ManeuverPhaseGate.h"
#include "src/game/navigation/DynamicMotionSystem.h"
#include "src/game/shared/SharedShipPhysics.h"
#include "src/game/ship/core/ShipParams.h"
#include "src/world/WorldParams.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using Program = game::navigation::AcceptedManeuverProgram;
using Follower = game::navigation::TrajectoryFollower;
using Bridge = game::navigation::NavigationRuntimeControlBridge;
using Gate = game::navigation::ManeuverPhaseGate;
using Law = game::navigation::LocalFlightControlLaw;

constexpr double kPi = 3.14159265358979323846;
constexpr double kDt = 0.02;
constexpr double kStandardGravityMps2 = 9.80665;
constexpr double kCorridorHalfWidthMeters = 32.0;
constexpr double kCornerZoneGateMeters = 35.0;

enum class CornerMode
{
    StopTurnGo,
    RadiusTurn,
    DriftTurn
};

const char* lawName(Law law)
{
    return law == Law::Newtonian ? "newtonian" : "assisted";
}

const char* modeName(CornerMode mode)
{
    switch (mode)
    {
    case CornerMode::StopTurnGo:
        return "stop_turn_go";
    case CornerMode::RadiusTurn:
        return "radius_turn";
    case CornerMode::DriftTurn:
        return "drift_turn";
    }
    return "unknown";
}

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

double clampDot(double value)
{
    return std::clamp(value, -1.0, 1.0);
}

double angleRad(const glm::dvec3& a, const glm::dvec3& b)
{
    const double la = glm::length(a);
    const double lb = glm::length(b);
    if (la <= 1.0e-12 || lb <= 1.0e-12)
        return 0.0;
    return std::acos(clampDot(glm::dot(a / la, b / lb)));
}

double yawFromForward(const glm::dvec3& forward)
{
    return std::atan2(-forward.x, -forward.z);
}

double shortestAngleDelta(double from, double to)
{
    double delta = std::fmod(to - from + kPi, 2.0 * kPi);
    if (delta < 0.0)
        delta += 2.0 * kPi;
    return delta - kPi;
}

struct QuinticYawProfile
{
    double a0 = 0.0;
    double a1 = 0.0;
    double a2 = 0.0;
    double a3 = 0.0;
    double a4 = 0.0;
    double a5 = 0.0;
    double durationSeconds = 0.0;
};

QuinticYawProfile quinticYawProfile(
    double startYaw,
    double startYawRate,
    double targetYaw,
    double durationSeconds
)
{
    const double targetUnwrapped =
        startYaw + shortestAngleDelta(startYaw, targetYaw);
    const double wT = startYawRate * durationSeconds;
    const double remaining =
        targetUnwrapped - startYaw - wT;

    QuinticYawProfile q;
    q.a0 = startYaw;
    q.a1 = wT;
    q.a2 = 0.0;
    q.a3 = 10.0 * remaining + 4.0 * wT;
    q.a4 = -15.0 * remaining - 7.0 * wT;
    q.a5 = 6.0 * remaining + 3.0 * wT;
    q.durationSeconds = durationSeconds;
    return q;
}

void sampleQuinticYaw(
    const QuinticYawProfile& q,
    double t,
    double& yaw,
    double& yawRate,
    double& yawAccel
)
{
    const double T = q.durationSeconds;
    const double u =
        T > 1.0e-12
            ? std::clamp(t / T, 0.0, 1.0)
            : 1.0;
    const double u2 = u * u;
    const double u3 = u2 * u;
    const double u4 = u3 * u;
    const double u5 = u4 * u;

    yaw =
        q.a0 +
        q.a1 * u +
        q.a2 * u2 +
        q.a3 * u3 +
        q.a4 * u4 +
        q.a5 * u5;

    yawRate =
        T > 1.0e-12
            ? (q.a1 +
               2.0 * q.a2 * u +
               3.0 * q.a3 * u2 +
               4.0 * q.a4 * u3 +
               5.0 * q.a5 * u4) / T
            : 0.0;

    yawAccel =
        T > 1.0e-12
            ? (2.0 * q.a2 +
               6.0 * q.a3 * u +
               12.0 * q.a4 * u2 +
               20.0 * q.a5 * u3) / (T * T)
            : 0.0;
}

bool quinticFitsAngularLimits(
    const QuinticYawProfile& q,
    double maxAbsYawRate,
    double maxAbsYawAccel,
    double* peakYawRate = nullptr,
    double* peakYawAccel = nullptr
)
{
    double peakRate = 0.0;
    double peakAccel = 0.0;
    constexpr int kProbeCount = 512;

    for (int i = 0; i <= kProbeCount; ++i)
    {
        const double t =
            q.durationSeconds *
            static_cast<double>(i) /
            static_cast<double>(kProbeCount);
        double yaw = 0.0;
        double rate = 0.0;
        double accel = 0.0;
        sampleQuinticYaw(q, t, yaw, rate, accel);
        (void)yaw;
        peakRate = std::max(peakRate, std::abs(rate));
        peakAccel = std::max(peakAccel, std::abs(accel));
    }

    if (peakYawRate)
        *peakYawRate = peakRate;
    if (peakYawAccel)
        *peakYawAccel = peakAccel;

    return
        peakRate <= maxAbsYawRate &&
        peakAccel <= maxAbsYawAccel;
}

double chooseQuinticYawDuration(
    double startYaw,
    double startYawRate,
    double targetYaw,
    double maxAbsYawRate,
    double maxAbsYawAccel
)
{
    double high = 0.25;
    while (high < 20.0)
    {
        const auto q =
            quinticYawProfile(
                startYaw,
                startYawRate,
                targetYaw,
                high
            );
        if (quinticFitsAngularLimits(
                q,
                maxAbsYawRate,
                maxAbsYawAccel))
        {
            break;
        }
        high *= 1.5;
    }

    require(high < 20.0,
            "could not bound moving attitude capture horizon");

    double low = 0.0;
    for (int i = 0; i < 60; ++i)
    {
        const double mid = 0.5 * (low + high);
        const auto q =
            quinticYawProfile(
                startYaw,
                startYawRate,
                targetYaw,
                mid
            );
        if (quinticFitsAngularLimits(
                q,
                maxAbsYawRate,
                maxAbsYawAccel))
        {
            high = mid;
        }
        else
        {
            low = mid;
        }
    }

    // Small proof margin for sample interpolation and fixed-step execution.
    return high * 1.05;
}

double smooth5(double u)
{
    const double u2 = u * u;
    const double u3 = u2 * u;
    const double u4 = u3 * u;
    const double u5 = u4 * u;
    return 10.0 * u3 - 15.0 * u4 + 6.0 * u5;
}

double smooth5d1(double u)
{
    const double u2 = u * u;
    const double u3 = u2 * u;
    const double u4 = u3 * u;
    return 30.0 * u2 - 60.0 * u3 + 30.0 * u4;
}

double smooth5d2(double u)
{
    const double u2 = u * u;
    const double u3 = u2 * u;
    return 60.0 * u - 180.0 * u2 + 120.0 * u3;
}

struct RigidVehicleModel
{
    glm::dvec3 halfExtentsBodyMeters {13.0, 2.5, 11.1};
    double aftMainAccelerationMps2 = 7.5 * kStandardGravityMps2;
    double assistedForeMainAccelerationMps2 =
        7.5 * kStandardGravityMps2;
    double manoeuvreRcsAccelerationMps2 = 2.0;
    double vectoringAngularAccelerationRadPerSec2 = 3.0;
    double maxPitchRateRadPerSec = 2.5;
    double maxYawRateRadPerSec = 2.5;
    double maxRollRateRadPerSec = 3.0;
};

double effectiveAngularAccelerationLimit(const ShipParams& p)
{
    double limit = std::max(0.0, static_cast<double>(p.angularAccel));
    if (p.maxGs > 0.0f && p.turnRadius > 0.0f)
    {
        limit = std::min(
            limit,
            static_cast<double>(p.maxGs) *
                kStandardGravityMps2 /
                static_cast<double>(p.turnRadius)
        );
    }
    return limit;
}

double effectiveAngularRateLimit(
    const ShipParams& p,
    double configuredAxisRate
)
{
    double limit = configuredAxisRate;
    if (p.maxGs > 0.0f && p.turnRadius > 0.0f)
    {
        limit = std::min(
            limit,
            std::sqrt(
                static_cast<double>(p.maxGs) *
                kStandardGravityMps2 /
                static_cast<double>(p.turnRadius)
            )
        );
    }
    return limit;
}

ShipParams cobraParams(const RigidVehicleModel& model)
{
    ShipParams p {};
    p.maxPitchRate = static_cast<float>(model.maxPitchRateRadPerSec);
    p.maxYawRate = static_cast<float>(model.maxYawRateRadPerSec);
    p.maxRollRate = static_cast<float>(model.maxRollRateRadPerSec);
    p.angularAccel =
        static_cast<float>(model.vectoringAngularAccelerationRadPerSec2);
    p.angularDamping = 2.5f;
    p.maxCombatSpeed = 500.0f;
    p.maxCruiseSpeed = 1000.0f;
    p.throttleAccel = 5.0f;
    p.autoLevelStrength = 0.0f;
    p.strafeAccel = 20.0f;
    p.strafeDamping = 6.0f;
    p.maxStrafeSpeed = 80.0f;
    p.manoeuvreThrusterAccel =
        static_cast<float>(model.manoeuvreRcsAccelerationMps2);
    p.manoeuvreGasUsePerSecond = 0.0f;
    p.manoeuvreGasRechargePerSecond = 0.0f;
    p.maxGs = 5.0f;
    p.maxLinearGs = 7.5f;
    p.turnRadius = 20.0f;
    p.massKg = 260000.0;
    p.pitchInertiaKgM2 = 11219866.6666667;
    p.yawInertiaKgM2 = 25324866.6666667;
    p.rollInertiaKgM2 = 15188333.3333333;
    return p;
}

Bridge::PilotSkillProfile expertProfile()
{
    Bridge::PilotSkillProfile profile;
    profile.execution.reactionDelaySeconds = 0.0;
    profile.execution.perceptionDecisionRateHz = 100.0;
    profile.execution.commandLatencySeconds = 0.0;
    profile.execution.responseFrequencyHz = 10.0;
    profile.execution.dampingRatio = 1.0;
    profile.execution.commandGain = 1.0;
    profile.execution.maxLinearCommandSlewMetersPerSec3 = 1000.0;
    profile.execution.maxAngularCommandSlewRadPerSec3 = 1000.0;
    profile.execution.deterministicSeed = 0xC0A71001ull;
    return profile;
}

Bridge::PilotSkillProfile competentProfile()
{
    Bridge::PilotSkillProfile profile;
    profile.execution.reactionDelaySeconds = 0.12;
    profile.execution.perceptionDecisionRateHz = 12.0;
    profile.execution.commandLatencySeconds = 0.06;
    profile.execution.responseFrequencyHz = 2.0;
    profile.execution.dampingRatio = 0.85;
    profile.execution.commandGain = 1.0;
    profile.execution.maxLinearCommandSlewMetersPerSec3 = 80.0;
    profile.execution.maxAngularCommandSlewRadPerSec3 = 8.0;
    profile.execution.deterministicLinearNoiseAmplitudeMetersPerSec2 = 0.02;
    profile.execution.deterministicAngularNoiseAmplitudeRadPerSec2 = 0.002;
    profile.execution.deterministicSeed = 0xC0A7C002ull;
    profile.execution.emergencyResponseThreshold01 = 0.75;
    profile.execution.emergencyReactionDelayScale = 0.35;
    return profile;
}

Bridge::PilotSkillProfile rookieProfile()
{
    Bridge::PilotSkillProfile profile;
    profile.execution.reactionDelaySeconds = 0.30;
    profile.execution.perceptionDecisionRateHz = 6.0;
    profile.execution.commandLatencySeconds = 0.12;
    profile.execution.responseFrequencyHz = 1.2;
    profile.execution.dampingRatio = 0.72;
    profile.execution.commandGain = 0.95;
    profile.execution.maxLinearCommandSlewMetersPerSec3 = 35.0;
    profile.execution.maxAngularCommandSlewRadPerSec3 = 3.5;
    profile.execution.deterministicLinearNoiseAmplitudeMetersPerSec2 = 0.07;
    profile.execution.deterministicAngularNoiseAmplitudeRadPerSec2 = 0.008;
    profile.execution.deterministicSeed = 0xC0A7B003ull;
    profile.execution.emergencyResponseThreshold01 = 0.80;
    profile.execution.emergencyReactionDelayScale = 0.50;
    return profile;
}

struct PilotCase
{
    const char* name = "";
    Bridge::PilotSkillProfile profile {};
    bool strict = false;
};

struct Basis
{
    glm::dvec3 forward {0.0, 0.0, -1.0};
    glm::dvec3 right {1.0, 0.0, 0.0};
    glm::dvec3 up {0.0, 1.0, 0.0};
};

Basis yawBasis(double radians)
{
    const double c = std::cos(radians);
    const double s = std::sin(radians);
    return {
        glm::dvec3(-s, 0.0, -c),
        glm::dvec3(c, 0.0, -s),
        glm::dvec3(0.0, 1.0, 0.0)
    };
}

void setTransformBasis(ShipTransform& transform, const Basis& basis)
{
    transform.orientation = glm::mat4(1.0f);
    transform.orientation[0] =
        glm::vec4(glm::vec3(basis.right), 0.0f);
    transform.orientation[1] =
        glm::vec4(glm::vec3(basis.up), 0.0f);
    transform.orientation[2] =
        glm::vec4(glm::vec3(-basis.forward), 0.0f);
}

void fillCommon(
    Program& p,
    std::uint64_t revision,
    double acceptedAt,
    double duration,
    Program::ManeuverFamily family
)
{
    p.valid = true;
    p.revision = revision;
    p.objectiveRevision = 7000u;
    p.family = family;
    p.acceptedAtUniverseTimeSeconds = acceptedAt;
    p.validUntilUniverseTimeSeconds = acceptedAt + duration + 8.0;
    p.completionTriggersReplan = true;

    p.terminalTolerance.positionMeters = 0.90;
    p.terminalTolerance.linearVelocityMps = 0.55;
    p.terminalTolerance.forwardAngleRad = 0.055;
    p.terminalTolerance.angularVelocityRadPerSec = 0.08;

    p.tracking.positionErrorMeters = 14.0;
    p.tracking.linearVelocityErrorMps = 7.0;
    p.tracking.forwardAngleErrorRad = 0.90;
    p.tracking.angularVelocityErrorRadPerSec = 0.90;
    p.tracking.linearFeedbackReserveMps2 = 0.55;
    p.tracking.angularFeedbackReserveRadPerSec2 = 0.35;

    p.capability.revision = 1;
    p.capability.maxForwardAccelerationMetersPerSec2 =
        7.5 * kStandardGravityMps2;
    p.capability.maxReverseAccelerationMetersPerSec2 =
        7.5 * kStandardGravityMps2;
    p.capability.maxLateralAccelerationMetersPerSec2 = 2.0;
    p.capability.maxVerticalAccelerationMetersPerSec2 = 2.0;
    p.capability.maxAngularAccelerationRadPerSec2 = 3.0;
    p.capability.maxAngularSpeedRadPerSec = 2.5;
}

Program makeConstantAccelerationProgram(
    std::uint64_t revision,
    double acceptedAt,
    const glm::dvec3& startPosition,
    const glm::dvec3& startVelocity,
    const glm::dvec3& acceleration,
    double yaw,
    double duration,
    Program::ManeuverFamily family
)
{
    Program p;
    fillCommon(p, revision, acceptedAt, duration, family);
    p.sampleCount = static_cast<std::uint8_t>(Program::kMaxSamples);

    const Basis basis = yawBasis(yaw);
    const double denom =
        static_cast<double>(Program::kMaxSamples - 1);

    for (std::size_t i = 0; i < Program::kMaxSamples; ++i)
    {
        const double t =
            duration * static_cast<double>(i) / denom;
        auto& s = p.samples[i];

        s.timeOffsetSeconds = t;
        s.positionMapMeters =
            startPosition +
            startVelocity * t +
            0.5 * acceleration * t * t;
        s.velocityMapMetersPerSecond =
            startVelocity + acceleration * t;
        s.linearAccelerationFeedForwardMapMps2 = acceleration;
        s.forwardMap = basis.forward;
        s.rightMap = basis.right;
        s.upMap = basis.up;
        s.angularVelocityMapRadPerSecond = {0.0, 0.0, 0.0};
        s.angularAccelerationFeedForwardMapRadPerSec2 =
            {0.0, 0.0, 0.0};
    }

    return p;
}

Program makeAccelerationCaptureProgram(
    std::uint64_t revision,
    double acceptedAt,
    const glm::dvec3& startPosition,
    const glm::dvec3& startVelocity,
    const glm::dvec3& acceleration,
    double yaw,
    double duration,
    Program::ManeuverFamily family
)
{
    Program p =
        makeConstantAccelerationProgram(
            revision,
            acceptedAt,
            startPosition,
            startVelocity,
            acceleration,
            yaw,
            duration,
            family
        );

    auto& terminal =
        p.samples[
            static_cast<std::size_t>(p.sampleCount - 1)
        ];
    terminal.linearAccelerationFeedForwardMapMps2 =
        {0.0, 0.0, 0.0};
    return p;
}

Program makeCoastProgram(
    std::uint64_t revision,
    double acceptedAt,
    const glm::dvec3& startPosition,
    const glm::dvec3& velocity,
    double yaw,
    double duration,
    Program::ManeuverFamily family = Program::ManeuverFamily::Coast
)
{
    return makeConstantAccelerationProgram(
        revision,
        acceptedAt,
        startPosition,
        velocity,
        glm::dvec3(0.0),
        yaw,
        duration,
        family
    );
}

Program makeCoastRotateProgram(
    std::uint64_t revision,
    double acceptedAt,
    const glm::dvec3& startPosition,
    const glm::dvec3& velocity,
    double startYaw,
    double endYaw,
    double duration,
    Program::ManeuverFamily family
)
{
    Program p;
    fillCommon(p, revision, acceptedAt, duration, family);
    p.sampleCount = static_cast<std::uint8_t>(Program::kMaxSamples);

    const double deltaYaw = endYaw - startYaw;
    const double denom =
        static_cast<double>(Program::kMaxSamples - 1);

    for (std::size_t i = 0; i < Program::kMaxSamples; ++i)
    {
        const double t =
            duration * static_cast<double>(i) / denom;
        const double u =
            duration > 1.0e-12 ? t / duration : 1.0;
        const double yaw =
            startYaw + deltaYaw * smooth5(u);
        const Basis basis = yawBasis(yaw);

        auto& s = p.samples[i];
        s.timeOffsetSeconds = t;
        s.positionMapMeters =
            startPosition + velocity * t;
        s.velocityMapMetersPerSecond = velocity;
        s.linearAccelerationFeedForwardMapMps2 = {0.0, 0.0, 0.0};
        s.forwardMap = basis.forward;
        s.rightMap = basis.right;
        s.upMap = basis.up;
        s.angularVelocityMapRadPerSecond =
            {0.0, deltaYaw * smooth5d1(u) / duration, 0.0};
        s.angularAccelerationFeedForwardMapRadPerSec2 =
            {0.0,
             deltaYaw * smooth5d2(u) / (duration * duration),
             0.0};
    }

    return p;
}

Program makeMovingAttitudeCaptureProgram(
    std::uint64_t revision,
    double acceptedAt,
    const glm::dvec3& startPosition,
    const glm::dvec3& velocity,
    double startYaw,
    double startYawRate,
    double endYaw,
    double maxAbsYawRate,
    double maxAbsYawAccel,
    double& chosenDurationSeconds,
    double& peakFeedForwardYawRate,
    double& peakFeedForwardYawAccel
)
{
    chosenDurationSeconds =
        chooseQuinticYawDuration(
            startYaw,
            startYawRate,
            endYaw,
            maxAbsYawRate,
            maxAbsYawAccel
        );

    const auto q =
        quinticYawProfile(
            startYaw,
            startYawRate,
            endYaw,
            chosenDurationSeconds
        );

    require(
        quinticFitsAngularLimits(
            q,
            maxAbsYawRate,
            maxAbsYawAccel,
            &peakFeedForwardYawRate,
            &peakFeedForwardYawAccel
        ),
        "chosen moving attitude capture violates angular limits"
    );

    Program p;
    fillCommon(
        p,
        revision,
        acceptedAt,
        chosenDurationSeconds,
        Program::ManeuverFamily::PrecisionCapture
    );
    p.sampleCount =
        static_cast<std::uint8_t>(Program::kMaxSamples);

    const double denom =
        static_cast<double>(Program::kMaxSamples - 1);

    for (std::size_t i = 0; i < Program::kMaxSamples; ++i)
    {
        const double t =
            chosenDurationSeconds *
            static_cast<double>(i) / denom;

        double yaw = 0.0;
        double yawRate = 0.0;
        double yawAccel = 0.0;
        sampleQuinticYaw(q, t, yaw, yawRate, yawAccel);

        const Basis basis = yawBasis(yaw);
        auto& s = p.samples[i];
        s.timeOffsetSeconds = t;
        s.positionMapMeters =
            startPosition + velocity * t;
        s.velocityMapMetersPerSecond = velocity;
        s.linearAccelerationFeedForwardMapMps2 =
            {0.0, 0.0, 0.0};
        s.forwardMap = basis.forward;
        s.rightMap = basis.right;
        s.upMap = basis.up;
        s.angularVelocityMapRadPerSecond =
            {0.0, yawRate, 0.0};
        s.angularAccelerationFeedForwardMapRadPerSec2 =
            {0.0, yawAccel, 0.0};
    }

    return p;
}

Program makeRotateInPlaceProgram(
    std::uint64_t revision,
    double acceptedAt,
    const glm::dvec3& position,
    double startYaw,
    double endYaw,
    double duration
)
{
    return makeCoastRotateProgram(
        revision,
        acceptedAt,
        position,
        glm::dvec3(0.0),
        startYaw,
        endYaw,
        duration,
        Program::ManeuverFamily::PrecisionCapture
    );
}

Program makeArcProgram(
    std::uint64_t revision,
    double acceptedAt,
    double radiusMeters,
    double speedMps,
    bool driftBody,
    double sweepRadians
)
{
    const double angularRate = speedMps / radiusMeters;
    const double duration =
        sweepRadians / angularRate;

    Program p;
    fillCommon(
        p,
        revision,
        acceptedAt,
        duration,
        driftBody
            ? Program::ManeuverFamily::DriftPass
            : Program::ManeuverFamily::PrecisionTransit
    );
    p.sampleCount = static_cast<std::uint8_t>(Program::kMaxSamples);

    const glm::dvec3 center(radiusMeters, 0.0, radiusMeters);
    const double denom =
        static_cast<double>(Program::kMaxSamples - 1);

    for (std::size_t i = 0; i < Program::kMaxSamples; ++i)
    {
        const double t =
            duration * static_cast<double>(i) / denom;
        const double phi =
            kPi + angularRate * t;

        const glm::dvec3 radial(
            radiusMeters * std::cos(phi),
            0.0,
            radiusMeters * std::sin(phi)
        );
        const glm::dvec3 position = center + radial;
        const glm::dvec3 velocity(
            -speedMps * std::sin(phi),
            0.0,
            speedMps * std::cos(phi)
        );
        const glm::dvec3 acceleration =
            -radial * (speedMps * speedMps /
                       (radiusMeters * radiusMeters));

        const glm::dvec3 bodyForward =
            driftBody
                ? glm::normalize(acceleration)
                : glm::normalize(velocity);

        double yaw = 0.0;
        if (driftBody)
        {
            yaw = -0.5 * kPi - angularRate * t;
        }
        else
        {
            yaw = -angularRate * t;
        }

        const Basis basis = yawBasis(yaw);
        auto& s = p.samples[i];
        s.timeOffsetSeconds = t;
        s.positionMapMeters = position;
        s.velocityMapMetersPerSecond = velocity;
        s.linearAccelerationFeedForwardMapMps2 = acceleration;
        s.forwardMap = bodyForward;
        s.rightMap = basis.right;
        s.upMap = basis.up;
        s.angularVelocityMapRadPerSecond =
            {0.0, -angularRate, 0.0};
        s.angularAccelerationFeedForwardMapRadPerSec2 =
            {0.0, 0.0, 0.0};
    }

    return p;
}

Program makeQuarterArcProgram(
    std::uint64_t revision,
    double acceptedAt,
    double radiusMeters,
    double speedMps,
    bool driftBody
)
{
    return makeArcProgram(
        revision,
        acceptedAt,
        radiusMeters,
        speedMps,
        driftBody,
        0.5 * kPi
    );
}

struct Vehicle
{
    ShipTransform transform {};
    ShipParams params {};
    WorldParams world {};
    game::navigation::KinematicFrame frame {};
    Bridge bridge;
    double timeSeconds = 0.0;

    Vehicle(
        const RigidVehicleModel& model,
        const PilotCase& pilot,
        Law law
    )
        : params(cobraParams(model)),
          bridge(pilot.profile)
    {
        frame.systemId = 1;
        frame.frameId = "corner-family-matrix";
        frame.originMeters = {0.0, 0.0, 0.0};
        frame.localToWorldBasis = glm::dmat3(1.0);
        frame.valid = true;

        transform.motion.mode =
            game::navigation::MotionMode::HubTactical;
        transform.motion.systemId = 1;
        transform.motion.travelFrame = frame;
        transform.motion.localControlLaw = law;
        transform.motion.localPositionMeters = {0.0, 0.0, 60.0};
        transform.motion.localVelocityMps = {0.0, 0.0, -10.0};
        transform.setWorldPositionMeters({0.0, 0.0, 60.0});
        setTransformBasis(transform, Basis {});

        Bridge::Intent initial;
        initial.revision = 1;
        initial.targetRevision = 0;
        require(
            bridge.reset(0.0, initial),
            "corner-family bridge reset failed"
        );
    }
};

Follower::AgentState agentState(const Vehicle& v)
{
    Follower::AgentState a;
    a.positionMapMeters = v.transform.motion.localPositionMeters;
    a.velocityMapMetersPerSecond = v.transform.motion.localVelocityMps;
    a.forwardMap = glm::dvec3(v.transform.forward());
    a.rightMap = glm::dvec3(v.transform.right());
    a.upMap = glm::dvec3(v.transform.up());
    a.pitchRateRadPerSec = v.transform.pitchRate;
    a.yawRateRadPerSec = v.transform.yawRate;
    a.rollRateRadPerSec = v.transform.rollRate;
    return a;
}

game::navigation::NavigationSystemControlIntent toSystemIntent(
    const game::navigation::NavigationLocalControlIntent& local
)
{
    game::navigation::NavigationSystemControlIntent system;
    system.revision = local.revision;
    system.targetRevision = local.targetRevision;
    system.idealLinearAccelerationSystemMps2 =
        local.idealLinearAccelerationLocalMps2;
    system.idealAngularAccelerationSystemRadPerSec2 =
        local.idealAngularAccelerationLocalRadPerSec2;
    system.emergency = local.emergency;
    system.hazardUrgency01 = local.hazardUrgency01;
    return system;
}

double pointToSegmentDistance(
    const glm::dvec3& p,
    const glm::dvec3& a,
    const glm::dvec3& b
)
{
    const glm::dvec3 ab = b - a;
    const double ab2 = glm::dot(ab, ab);
    if (ab2 <= 1.0e-12)
        return glm::length(p - a);

    const double u = std::clamp(
        glm::dot(p - a, ab) / ab2,
        0.0,
        1.0
    );
    return glm::length(p - (a + ab * u));
}

double pointToCornerPolylineDistance(const glm::dvec3& p)
{
    const glm::dvec3 incomingStart(0.0, 0.0, 80.0);
    const glm::dvec3 corner(0.0, 0.0, 0.0);
    const glm::dvec3 outgoingEnd(140.0, 0.0, 0.0);

    return std::min(
        pointToSegmentDistance(p, incomingStart, corner),
        pointToSegmentDistance(p, corner, outgoingEnd)
    );
}

double hullRequiredHalfWidthMeters(
    const Vehicle& v,
    const RigidVehicleModel& model
)
{
    const glm::dvec3 center =
        v.transform.motion.localPositionMeters;
    const glm::dvec3 right(v.transform.right());
    const glm::dvec3 up(v.transform.up());
    const glm::dvec3 forward(v.transform.forward());

    double maximum = 0.0;
    for (int sx : {-1, 1})
    {
        for (int sy : {-1, 1})
        {
            for (int sz : {-1, 1})
            {
                const glm::dvec3 corner =
                    center +
                    right *
                        (static_cast<double>(sx) *
                         model.halfExtentsBodyMeters.x) +
                    up *
                        (static_cast<double>(sy) *
                         model.halfExtentsBodyMeters.y) +
                    forward *
                        (static_cast<double>(sz) *
                         model.halfExtentsBodyMeters.z);

                maximum = std::max(
                    maximum,
                    pointToCornerPolylineDistance(corner)
                );
            }
        }
    }
    return maximum;
}

struct Metrics
{
    bool valid = true;
    bool completed = false;
    std::size_t phasesCompleted = 0;

    double entryGateTimeSeconds = -1.0;
    double exitGateTimeSeconds = -1.0;
    double cornerZoneTimeSeconds = -1.0;
    double totalTimeSeconds = 0.0;

    double minimumCornerZoneSpeedMps =
        std::numeric_limits<double>::infinity();
    double maximumDriftAngleDeg = 0.0;
    double maximumHullRequiredHalfWidthMeters = 0.0;
    double maximumCorridorViolationMeters = 0.0;
    double maximumCenterCrossTrackMeters = 0.0;

    double peakAftMainMps2 = 0.0;
    double peakForeMainMps2 = 0.0;
    double peakRcsMps2 = 0.0;

    std::size_t trackingEnvelopeExceededTicks = 0;
    std::size_t captureTimedOutPhases = 0;
    double maximumCaptureOverrunSeconds = 0.0;

    double finalPositionErrorMeters = 0.0;
    double finalVelocityErrorMps = 0.0;
    double finalForwardErrorDeg = 0.0;

    bool outgoingAttitudeCaptured = false;
    double outgoingAttitudeCaptureTimeSeconds = -1.0;
    double outgoingAttitudeCaptureXMeters = -1.0;
    double outgoingAttitudeCaptureDistanceAfterOldExitMeters = -1.0;

    double attitudeCaptureProgramDurationSeconds = 0.0;
    double attitudeCaptureStartYawRateRadPerSec = 0.0;
    double attitudeCapturePeakFeedForwardYawRateRadPerSec = 0.0;
    double attitudeCapturePeakFeedForwardYawAccelRadPerSec2 = 0.0;
};

struct LongArcMetrics
{
    bool valid = true;
    bool completed = false;
    double maximumCenterlineErrorMeters = 0.0;
    double maximumHullRequiredHalfWidthMeters = 0.0;
    double maximumForwardErrorDeg = 0.0;
    double finalPositionErrorMeters = 0.0;
    double finalVelocityErrorMps = 0.0;
    double finalForwardErrorDeg = 0.0;
    std::size_t trackingEnvelopeExceededTicks = 0;
};

void updateLongArcMetrics(
    const Vehicle& v,
    const RigidVehicleModel& model,
    const glm::dvec3& center,
    double radiusMeters,
    LongArcMetrics& m
)
{
    const glm::dvec3 position =
        v.transform.motion.localPositionMeters;
    glm::dvec3 radial(
        position.x - center.x,
        0.0,
        position.z - center.z
    );
    const double radialLength = glm::length(radial);
    if (radialLength > 1.0e-9)
    {
        m.maximumCenterlineErrorMeters =
            std::max(
                m.maximumCenterlineErrorMeters,
                std::abs(radialLength - radiusMeters)
            );

        const glm::dvec3 tangent =
            glm::normalize(
                glm::dvec3(-radial.z, 0.0, radial.x)
            );
        m.maximumForwardErrorDeg =
            std::max(
                m.maximumForwardErrorDeg,
                angleRad(
                    glm::dvec3(v.transform.forward()),
                    tangent
                ) * 180.0 / kPi
            );
    }

    const glm::dvec3 right(v.transform.right());
    const glm::dvec3 up(v.transform.up());
    const glm::dvec3 forward(v.transform.forward());
    for (int sx : {-1, 1})
    {
        for (int sy : {-1, 1})
        {
            for (int sz : {-1, 1})
            {
                const glm::dvec3 corner =
                    position +
                    right *
                        (static_cast<double>(sx) *
                         model.halfExtentsBodyMeters.x) +
                    up *
                        (static_cast<double>(sy) *
                         model.halfExtentsBodyMeters.y) +
                    forward *
                        (static_cast<double>(sz) *
                         model.halfExtentsBodyMeters.z);

                const glm::dvec3 cornerRadial(
                    corner.x - center.x,
                    0.0,
                    corner.z - center.z
                );
                m.maximumHullRequiredHalfWidthMeters =
                    std::max(
                        m.maximumHullRequiredHalfWidthMeters,
                        std::abs(
                            glm::length(cornerRadial) -
                            radiusMeters
                        )
                    );
            }
        }
    }
}

void updateGeometryMetrics(
    const Vehicle& v,
    const RigidVehicleModel& model,
    Metrics& m
)
{
    const glm::dvec3 center =
        v.transform.motion.localPositionMeters;
    const double centerCross =
        pointToCornerPolylineDistance(center);
    m.maximumCenterCrossTrackMeters =
        std::max(m.maximumCenterCrossTrackMeters, centerCross);

    const double hullHalfWidth =
        hullRequiredHalfWidthMeters(v, model);
    m.maximumHullRequiredHalfWidthMeters =
        std::max(
            m.maximumHullRequiredHalfWidthMeters,
            hullHalfWidth
        );
    m.maximumCorridorViolationMeters =
        std::max(
            m.maximumCorridorViolationMeters,
            std::max(
                0.0,
                hullHalfWidth - kCorridorHalfWidthMeters
            )
        );

    if (m.entryGateTimeSeconds < 0.0 &&
        center.z <= kCornerZoneGateMeters)
    {
        m.entryGateTimeSeconds = v.timeSeconds;
    }

    if (m.entryGateTimeSeconds >= 0.0 &&
        m.exitGateTimeSeconds < 0.0)
    {
        const double speed =
            glm::length(v.transform.motion.localVelocityMps);
        m.minimumCornerZoneSpeedMps =
            std::min(m.minimumCornerZoneSpeedMps, speed);

        if (speed > 0.25)
        {
            const double drift =
                angleRad(
                    v.transform.motion.localVelocityMps,
                    glm::dvec3(v.transform.forward())
                ) * 180.0 / kPi;
            m.maximumDriftAngleDeg =
                std::max(m.maximumDriftAngleDeg, drift);
        }
    }

    if (m.entryGateTimeSeconds >= 0.0 &&
        m.exitGateTimeSeconds < 0.0 &&
        center.x >= kCornerZoneGateMeters)
    {
        m.exitGateTimeSeconds = v.timeSeconds;
        m.cornerZoneTimeSeconds =
            m.exitGateTimeSeconds -
            m.entryGateTimeSeconds;
    }

    if (!m.outgoingAttitudeCaptured &&
        center.x >= 60.0)
    {
        const double forwardErrorDeg =
            angleRad(
                glm::dvec3(v.transform.forward()),
                glm::dvec3(1.0, 0.0, 0.0)
            ) * 180.0 / kPi;
        const double angularSpeedRadPerSec =
            std::sqrt(
                static_cast<double>(v.transform.pitchRate) *
                    static_cast<double>(v.transform.pitchRate) +
                static_cast<double>(v.transform.yawRate) *
                    static_cast<double>(v.transform.yawRate) +
                static_cast<double>(v.transform.rollRate) *
                    static_cast<double>(v.transform.rollRate)
            );

        if (forwardErrorDeg <= 5.0 &&
            angularSpeedRadPerSec <= 0.08)
        {
            m.outgoingAttitudeCaptured = true;
            m.outgoingAttitudeCaptureTimeSeconds = v.timeSeconds;
            m.outgoingAttitudeCaptureXMeters = center.x;
            m.outgoingAttitudeCaptureDistanceAfterOldExitMeters =
                std::max(0.0, center.x - 60.0);
        }
    }
}

struct RunResult
{
    bool valid = true;
    bool completed = false;
    bool captureTimedOut = false;
    double captureOverrunSeconds = 0.0;
};

void finalizeMetrics(
    const Vehicle& v,
    Metrics& m
);


RunResult runProgram(
    Vehicle& v,
    const RigidVehicleModel& model,
    const Program& program,
    Metrics& m,
    Gate::Mode mode,
    double maximumCaptureOverrunSeconds = 6.0,
    std::function<void(const Vehicle&)> observer = {}
)
{
    Gate::Policy gatePolicy;
    gatePolicy.mode = mode;
    gatePolicy.maximumCaptureOverrunSeconds =
        maximumCaptureOverrunSeconds;

    while (true)
    {
        const auto follower =
            Follower::follow(
                program,
                v.timeSeconds,
                agentState(v)
            );

        if (follower.status == Follower::Status::InvalidInput)
            return {false, false, false, 0.0};

        if (follower.trackingErrorExceeded)
            ++m.trackingEnvelopeExceededTicks;

        const auto gateBefore =
            Gate::evaluate(
                program,
                v.timeSeconds,
                follower.status,
                gatePolicy
            );

        if (gateBefore.status == Gate::Status::InvalidInput)
            return {false, false, false, 0.0};

        if (gateBefore.status == Gate::Status::Advance)
        {
            return {
                true,
                true,
                false,
                gateBefore.captureOverrunSeconds
            };
        }

        if (gateBefore.status == Gate::Status::CaptureTimedOut)
        {
            return {
                true,
                false,
                true,
                gateBefore.captureOverrunSeconds
            };
        }

        const auto bridgeResult =
            v.bridge.step(
                v.timeSeconds + kDt,
                kDt,
                toSystemIntent(follower.intent)
            );

        if (bridgeResult.status !=
            Bridge::PilotExecutor::Status::Ok)
        {
            return {false, false, false, 0.0};
        }

        SharedShipPhysics::integrate(
            v.transform,
            v.params,
            bridgeResult.control,
            v.world,
            static_cast<float>(kDt)
        );

        game::navigation::DynamicMotionSystem::applySystemAccelerationDemand(
            v.transform.motion,
            v.params,
            bridgeResult.control.
                navigationLinearAccelerationDemandSystemMps2,
            v.transform.forward()
        );

        const glm::dvec3 bodyForward(v.transform.forward());
        const double longitudinalMain =
            glm::dot(
                v.transform.motion.mainEngineAccelerationMps2,
                bodyForward
            );
        m.peakAftMainMps2 =
            std::max(
                m.peakAftMainMps2,
                std::max(0.0, longitudinalMain)
            );
        m.peakForeMainMps2 =
            std::max(
                m.peakForeMainMps2,
                std::max(0.0, -longitudinalMain)
            );
        m.peakRcsMps2 =
            std::max(
                m.peakRcsMps2,
                glm::length(
                    v.transform.motion.manoeuvreAccelerationMps2
                )
            );

        game::navigation::DynamicMotionSystem::updateLocalFrameMotion(
            v.transform.motion,
            v.transform.worldPosition,
            v.frame,
            v.params,
            kDt
        );

        v.transform.syncLegacyPositionFromWorld();
        v.timeSeconds += kDt;

        updateGeometryMetrics(v, model, m);
        if (observer)
            observer(v);
    }
}

bool executePhase(
    Vehicle& v,
    const RigidVehicleModel& model,
    const Program& p,
    Metrics& m,
    Gate::Mode mode = Gate::Mode::ScheduledMoving,
    double maximumCaptureOverrunSeconds = 6.0
)
{
    const auto r =
        runProgram(
            v,
            model,
            p,
            m,
            mode,
            maximumCaptureOverrunSeconds
        );

    m.maximumCaptureOverrunSeconds =
        std::max(
            m.maximumCaptureOverrunSeconds,
            r.captureOverrunSeconds
        );

    if (!r.valid)
    {
        m.valid = false;
        m.completed = false;
        finalizeMetrics(v, m);
        return false;
    }

    if (r.captureTimedOut)
    {
        ++m.captureTimedOutPhases;
        m.completed = false;
        finalizeMetrics(v, m);
        return false;
    }

    if (!r.completed)
    {
        m.completed = false;
        finalizeMetrics(v, m);
        return false;
    }

    ++m.phasesCompleted;
    return true;
}

Metrics runStopTurnGo(
    const RigidVehicleModel& model,
    const PilotCase& pilot,
    Law law
)
{
    Vehicle v(model, pilot, law);
    Metrics m;
    std::uint64_t revision = 7100u;

    // Keep the braking point identical for both control laws. The accepted
    // rigid-body Newtonian baseline needs about 5 s for a clean 180 deg
    // lead-rotation with this Cobra/pilot model; asking a 2.6 s scheduled flip
    // to hand immediately into aft-main braking leaves B10 tracking reserve to
    // repair a maneuver-authoring error.
    constexpr double kPreBrakeTravelSeconds = 5.375;
    constexpr double kNewtonianFlipSeconds = 5.0;
    constexpr double kNewtonianApproachSeconds =
        kPreBrakeTravelSeconds - kNewtonianFlipSeconds;
    constexpr double kBrakeSeconds = 1.25;

    if (law == Law::Newtonian)
    {
        if (!executePhase(
                v, model,
                makeCoastProgram(
                    revision++, v.timeSeconds,
                    {0.0, 0.0, 60.0},
                    {0.0, 0.0, -10.0},
                    0.0,
                    kNewtonianApproachSeconds
                ),
                m))
            return m;

        const glm::dvec3 p0 =
            v.transform.motion.localPositionMeters;
        if (!executePhase(
                v, model,
                makeCoastRotateProgram(
                    revision++, v.timeSeconds,
                    p0,
                    {0.0, 0.0, -10.0},
                    0.0,
                    kPi,
                    kNewtonianFlipSeconds,
                    Program::ManeuverFamily::FlipAndBurn
                ),
                m))
            return m;

        const glm::dvec3 p1 =
            v.transform.motion.localPositionMeters;
        if (!executePhase(
                v, model,
                makeAccelerationCaptureProgram(
                    revision++, v.timeSeconds,
                    p1,
                    {0.0, 0.0, -10.0},
                    {0.0, 0.0, 8.0},
                    kPi,
                    kBrakeSeconds,
                    Program::ManeuverFamily::Brake
                ),
                m,
                Gate::Mode::StateCapture))
            return m;
    }
    else
    {
        if (!executePhase(
                v, model,
                makeCoastProgram(
                    revision++, v.timeSeconds,
                    {0.0, 0.0, 60.0},
                    {0.0, 0.0, -10.0},
                    0.0,
                    kPreBrakeTravelSeconds
                ),
                m))
            return m;

        const glm::dvec3 p0 =
            v.transform.motion.localPositionMeters;
        if (!executePhase(
                v, model,
                makeAccelerationCaptureProgram(
                    revision++, v.timeSeconds,
                    p0,
                    {0.0, 0.0, -10.0},
                    {0.0, 0.0, 8.0},
                    0.0,
                    kBrakeSeconds,
                    Program::ManeuverFamily::Brake
                ),
                m,
                Gate::Mode::StateCapture))
            return m;
    }

    const double startYaw =
        law == Law::Newtonian ? kPi : 0.0;
    const double endYaw =
        law == Law::Newtonian ? 1.5 * kPi : -0.5 * kPi;

    const glm::dvec3 atCorner =
        v.transform.motion.localPositionMeters;

    if (!executePhase(
            v, model,
            makeRotateInPlaceProgram(
                revision++, v.timeSeconds,
                atCorner,
                startYaw,
                endYaw,
                1.9
            ),
            m,
            Gate::Mode::StateCapture))
        return m;

    const glm::dvec3 afterRotate =
        v.transform.motion.localPositionMeters;

    if (!executePhase(
            v, model,
            makeConstantAccelerationProgram(
                revision++, v.timeSeconds,
                afterRotate,
                {0.0, 0.0, 0.0},
                {8.0, 0.0, 0.0},
                -0.5 * kPi,
                1.25,
                Program::ManeuverFamily::LeadRotateMainBurn
            ),
            m))
        return m;

    const glm::dvec3 afterAccel =
        v.transform.motion.localPositionMeters;

    if (!executePhase(
            v, model,
            makeCoastProgram(
                revision++, v.timeSeconds,
                afterAccel,
                {10.0, 0.0, 0.0},
                -0.5 * kPi,
                5.375,
                Program::ManeuverFamily::FreeTransit
            ),
            m))
        return m;

    const glm::dvec3 oldExit =
        v.transform.motion.localPositionMeters;

    executePhase(
        v, model,
        makeCoastProgram(
            revision++, v.timeSeconds,
            oldExit,
            {10.0, 0.0, 0.0},
            -0.5 * kPi,
            6.0,
            Program::ManeuverFamily::FreeTransit
        ),
        m
    );

    finalizeMetrics(v, m);
    return m;
}

Metrics runRadiusTurn(
    const RigidVehicleModel& model,
    const PilotCase& pilot,
    Law law
)
{
    Vehicle v(model, pilot, law);
    Metrics m;
    std::uint64_t revision = 7200u;

    if (!executePhase(
            v, model,
            makeConstantAccelerationProgram(
                revision++, v.timeSeconds,
                {0.0, 0.0, 60.0},
                {0.0, 0.0, -10.0},
                {0.0, 0.0, 2.0},
                0.0,
                1.0,
                Program::ManeuverFamily::Trim
            ),
            m))
        return m;

    const glm::dvec3 p0 =
        v.transform.motion.localPositionMeters;

    if (!executePhase(
            v, model,
            makeCoastProgram(
                revision++, v.timeSeconds,
                p0,
                {0.0, 0.0, -8.0},
                0.0,
                2.0,
                Program::ManeuverFamily::FreeTransit
            ),
            m))
        return m;

    if (!executePhase(
            v, model,
            makeQuarterArcProgram(
                revision++, v.timeSeconds,
                35.0,
                8.0,
                false
            ),
            m))
        return m;

    const glm::dvec3 p1 =
        v.transform.motion.localPositionMeters;

    if (!executePhase(
            v, model,
            makeCoastProgram(
                revision++, v.timeSeconds,
                p1,
                {8.0, 0.0, 0.0},
                -0.5 * kPi,
                2.0,
                Program::ManeuverFamily::FreeTransit
            ),
            m))
        return m;

    const glm::dvec3 p2 =
        v.transform.motion.localPositionMeters;

    if (!executePhase(
            v, model,
            makeConstantAccelerationProgram(
                revision++, v.timeSeconds,
                p2,
                {8.0, 0.0, 0.0},
                {2.0, 0.0, 0.0},
                -0.5 * kPi,
                1.0,
                Program::ManeuverFamily::Trim
            ),
            m))
        return m;

    const glm::dvec3 oldExit =
        v.transform.motion.localPositionMeters;

    executePhase(
        v, model,
        makeCoastProgram(
            revision++, v.timeSeconds,
            oldExit,
            {10.0, 0.0, 0.0},
            -0.5 * kPi,
            6.0,
            Program::ManeuverFamily::FreeTransit
        ),
        m
    );

    finalizeMetrics(v, m);
    return m;
}

Metrics runDriftTurn(
    const RigidVehicleModel& model,
    const PilotCase& pilot,
    Law law
)
{
    Vehicle v(model, pilot, law);
    Metrics m;
    std::uint64_t revision = 7300u;

    if (!executePhase(
            v, model,
            makeCoastProgram(
                revision++, v.timeSeconds,
                {0.0, 0.0, 60.0},
                {0.0, 0.0, -10.0},
                0.0,
                2.0,
                Program::ManeuverFamily::FreeTransit
            ),
            m))
        return m;

    const glm::dvec3 p0 =
        v.transform.motion.localPositionMeters;

    if (!executePhase(
            v, model,
            makeCoastRotateProgram(
                revision++, v.timeSeconds,
                p0,
                {0.0, 0.0, -10.0},
                0.0,
                -0.5 * kPi,
                2.0,
                Program::ManeuverFamily::DriftPass
            ),
            m))
        return m;

    if (!executePhase(
            v, model,
            makeQuarterArcProgram(
                revision++, v.timeSeconds,
                20.0,
                10.0,
                true
            ),
            m))
        return m;

    const glm::dvec3 p1 =
        v.transform.motion.localPositionMeters;

    // Planner-authored moving attitude capture. Start from the ACTUAL
    // attitude/angular rate left by the drift arc, then solve a quintic
    // transition to the outgoing heading with terminal yaw-rate zero.
    // Duration is chosen from the real ShipController angular envelopes,
    // reserving B10 authority for tracking rather than using B10 as the
    // primary 90-degree maneuver generator.
    const double startYaw =
        yawFromForward(glm::dvec3(v.transform.forward()));
    const double startYawRate =
        static_cast<double>(v.transform.yawRate);
    const double physicalAngularAccel =
        effectiveAngularAccelerationLimit(v.params);
    const double physicalYawRate =
        effectiveAngularRateLimit(
            v.params,
            static_cast<double>(v.params.maxYawRate)
        );
    const double feedForwardAccelLimit =
        std::max(
            0.05,
            physicalAngularAccel - 0.35
        );

    m.attitudeCaptureStartYawRateRadPerSec = startYawRate;

    if (!executePhase(
            v, model,
            makeMovingAttitudeCaptureProgram(
                revision++, v.timeSeconds,
                p1,
                {10.0, 0.0, 0.0},
                startYaw,
                startYawRate,
                -0.5 * kPi,
                physicalYawRate * 0.95,
                feedForwardAccelLimit * 0.95,
                m.attitudeCaptureProgramDurationSeconds,
                m.attitudeCapturePeakFeedForwardYawRateRadPerSec,
                m.attitudeCapturePeakFeedForwardYawAccelRadPerSec2
            ),
            m))
        return m;

    // Continue the same outgoing route after the computed capture horizon.
    // x=60 remains only a checkpoint; the common finite comparison endpoint
    // is x=120.
    const glm::dvec3 afterCapture =
        v.transform.motion.localPositionMeters;
    const double remainingMeters =
        std::max(0.0, 120.0 - afterCapture.x);
    const double remainingSeconds =
        remainingMeters / 10.0;

    if (remainingSeconds > 1.0e-9)
    {
        executePhase(
            v, model,
            makeCoastProgram(
                revision++, v.timeSeconds,
                afterCapture,
                {10.0, 0.0, 0.0},
                -0.5 * kPi,
                remainingSeconds,
                Program::ManeuverFamily::FreeTransit
            ),
            m
        );
    }

    finalizeMetrics(v, m);
    return m;
}

void finalizeMetrics(
    const Vehicle& v,
    Metrics& m
)
{
    const glm::dvec3 finalPosition(120.0, 0.0, 0.0);
    const glm::dvec3 finalVelocity(10.0, 0.0, 0.0);
    const glm::dvec3 finalForward(1.0, 0.0, 0.0);

    m.totalTimeSeconds = v.timeSeconds;
    m.completed =
        m.valid &&
        m.captureTimedOutPhases == 0 &&
        m.entryGateTimeSeconds >= 0.0 &&
        m.exitGateTimeSeconds >= 0.0;

    m.finalPositionErrorMeters =
        glm::length(
            v.transform.motion.localPositionMeters -
            finalPosition
        );
    m.finalVelocityErrorMps =
        glm::length(
            v.transform.motion.localVelocityMps -
            finalVelocity
        );
    m.finalForwardErrorDeg =
        angleRad(
            glm::dvec3(v.transform.forward()),
            finalForward
        ) * 180.0 / kPi;

    if (!std::isfinite(m.minimumCornerZoneSpeedMps))
        m.minimumCornerZoneSpeedMps = 0.0;
}

LongArcMetrics runLongArc(
    const RigidVehicleModel& model,
    const PilotCase& pilot,
    Law law
)
{
    constexpr double kRadiusMeters = 80.0;
    constexpr double kSpeedMps = 10.0;
    constexpr double kSweepRadians = kPi;

    Vehicle v(model, pilot, law);
    v.transform.motion.localPositionMeters =
        {0.0, 0.0, kRadiusMeters};
    v.transform.motion.localVelocityMps =
        {0.0, 0.0, -kSpeedMps};
    v.transform.setWorldPositionMeters(
        {0.0, 0.0, kRadiusMeters}
    );
    setTransformBasis(v.transform, Basis {});

    Metrics executionMetrics;
    LongArcMetrics arcMetrics;
    const glm::dvec3 center(
        kRadiusMeters,
        0.0,
        kRadiusMeters
    );

    const Program program =
        makeArcProgram(
            7400u,
            v.timeSeconds,
            kRadiusMeters,
            kSpeedMps,
            false,
            kSweepRadians
        );

    const RunResult run =
        runProgram(
            v,
            model,
            program,
            executionMetrics,
            Gate::Mode::ScheduledMoving,
            6.0,
            [&](const Vehicle& current)
            {
                updateLongArcMetrics(
                    current,
                    model,
                    center,
                    kRadiusMeters,
                    arcMetrics
                );
            }
        );

    arcMetrics.valid = run.valid;
    arcMetrics.completed = run.completed;
    arcMetrics.trackingEnvelopeExceededTicks =
        executionMetrics.trackingEnvelopeExceededTicks;

    const glm::dvec3 finalPosition(
        2.0 * kRadiusMeters,
        0.0,
        kRadiusMeters
    );
    const glm::dvec3 finalVelocity(
        0.0,
        0.0,
        kSpeedMps
    );
    const glm::dvec3 finalForward(
        0.0,
        0.0,
        1.0
    );

    arcMetrics.finalPositionErrorMeters =
        glm::length(
            v.transform.motion.localPositionMeters -
            finalPosition
        );
    arcMetrics.finalVelocityErrorMps =
        glm::length(
            v.transform.motion.localVelocityMps -
            finalVelocity
        );
    arcMetrics.finalForwardErrorDeg =
        angleRad(
            glm::dvec3(v.transform.forward()),
            finalForward
        ) * 180.0 / kPi;

    return arcMetrics;
}

Metrics runCase(
    const RigidVehicleModel& model,
    const PilotCase& pilot,
    Law law,
    CornerMode mode
)
{
    Metrics m;
    switch (mode)
    {
    case CornerMode::StopTurnGo:
        m = runStopTurnGo(model, pilot, law);
        break;
    case CornerMode::RadiusTurn:
        m = runRadiusTurn(model, pilot, law);
        break;
    case CornerMode::DriftTurn:
        m = runDriftTurn(model, pilot, law);
        break;
    }

    return m;
}

struct CaseRecord
{
    const char* pilot = "";
    Law law = Law::Newtonian;
    CornerMode mode = CornerMode::StopTurnGo;
    Metrics metrics {};
};

void printCase(const CaseRecord& r)
{
    const Metrics& m = r.metrics;
    std::cout
        << std::fixed << std::setprecision(6)
        << "[CORNER-MATRIX]"
        << " pilot=" << r.pilot
        << " law=" << lawName(r.law)
        << " mode=" << modeName(r.mode)
        << " valid=" << (m.valid ? 1 : 0)
        << " completed=" << (m.completed ? 1 : 0)
        << " phases=" << m.phasesCompleted
        << " total_time_s=" << m.totalTimeSeconds
        << " corner_zone_time_s=" << m.cornerZoneTimeSeconds
        << " final_pos_error_m=" << m.finalPositionErrorMeters
        << " final_velocity_error_mps=" << m.finalVelocityErrorMps
        << " final_forward_error_deg=" << m.finalForwardErrorDeg
        << " min_corner_speed_mps=" << m.minimumCornerZoneSpeedMps
        << " max_drift_angle_deg=" << m.maximumDriftAngleDeg
        << " max_center_cross_track_m="
        << m.maximumCenterCrossTrackMeters
        << " max_hull_required_half_width_m="
        << m.maximumHullRequiredHalfWidthMeters
        << " corridor_half_width_m="
        << kCorridorHalfWidthMeters
        << " max_corridor_violation_m="
        << m.maximumCorridorViolationMeters
        << " peak_aft_main_mps2=" << m.peakAftMainMps2
        << " peak_fore_main_mps2=" << m.peakForeMainMps2
        << " peak_rcs_mps2=" << m.peakRcsMps2
        << " tracking_envelope_exceeded_ticks="
        << m.trackingEnvelopeExceededTicks
        << " capture_timeout_phases="
        << m.captureTimedOutPhases
        << " max_capture_overrun_s="
        << m.maximumCaptureOverrunSeconds
        << " outgoing_attitude_captured="
        << (m.outgoingAttitudeCaptured ? 1 : 0)
        << " outgoing_attitude_capture_x_m="
        << m.outgoingAttitudeCaptureXMeters
        << " outgoing_attitude_capture_after_old_exit_m="
        << m.outgoingAttitudeCaptureDistanceAfterOldExitMeters
        << " attitude_capture_program_s="
        << m.attitudeCaptureProgramDurationSeconds
        << " attitude_capture_start_yaw_rate_radps="
        << m.attitudeCaptureStartYawRateRadPerSec
        << " attitude_capture_peak_ff_yaw_rate_radps="
        << m.attitudeCapturePeakFeedForwardYawRateRadPerSec
        << " attitude_capture_peak_ff_yaw_accel_radps2="
        << m.attitudeCapturePeakFeedForwardYawAccelRadPerSec2
        << "\n";
}

void testCornerFamilyMatrix()
{
    const RigidVehicleModel model;

    std::cout
        << std::fixed << std::setprecision(6)
        << "[CORNER-MODEL]"
        << " incoming_gate_m=60"
        << " outgoing_gate_m=60"
        << " corner_zone_gate_m=" << kCornerZoneGateMeters
        << " corridor_half_width_m=" << kCorridorHalfWidthMeters
        << " cobra_width_m=" << 2.0 * model.halfExtentsBodyMeters.x
        << " cobra_height_m=" << 2.0 * model.halfExtentsBodyMeters.y
        << " cobra_length_m=" << 2.0 * model.halfExtentsBodyMeters.z
        << " initial_speed_mps=10"
        << "\n";

    const std::array<PilotCase, 3> pilots {{
        {"expert", expertProfile(), true},
        {"competent", competentProfile(), false},
        {"rookie", rookieProfile(), false}
    }};
    const std::array<Law, 2> laws {{
        Law::Newtonian,
        Law::Assisted
    }};
    const std::array<CornerMode, 3> modes {{
        CornerMode::StopTurnGo,
        CornerMode::RadiusTurn,
        CornerMode::DriftTurn
    }};

    std::vector<CaseRecord> records;
    records.reserve(18);

    for (const auto& pilot : pilots)
    {
        for (const Law law : laws)
        {
            for (const CornerMode mode : modes)
            {
                CaseRecord r;
                r.pilot = pilot.name;
                r.law = law;
                r.mode = mode;
                r.metrics = runCase(model, pilot, law, mode);
                printCase(r);
                records.push_back(r);
            }
        }
    }

    require(records.size() == 18u,
            "corner matrix did not execute 3x2x3 rows");

    for (const auto& pilot : pilots)
    {
        for (const CornerMode mode : modes)
        {
            const CaseRecord* newtonian = nullptr;
            const CaseRecord* assisted = nullptr;

            for (const auto& r : records)
            {
                if (std::string(r.pilot) != pilot.name ||
                    r.mode != mode)
                {
                    continue;
                }

                if (r.law == Law::Newtonian)
                    newtonian = &r;
                else
                    assisted = &r;
            }

            require(
                newtonian != nullptr && assisted != nullptr,
                "corner timing comparison lost a law row"
            );

            std::cout
                << std::fixed << std::setprecision(6)
                << "[CORNER-COMPARE]"
                << " pilot=" << pilot.name
                << " mode=" << modeName(mode)
                << " newtonian_total_s="
                << newtonian->metrics.totalTimeSeconds
                << " assisted_total_s="
                << assisted->metrics.totalTimeSeconds
                << " assisted_minus_newtonian_s="
                << assisted->metrics.totalTimeSeconds -
                   newtonian->metrics.totalTimeSeconds
                << " newtonian_corner_s="
                << newtonian->metrics.cornerZoneTimeSeconds
                << " assisted_corner_s="
                << assisted->metrics.cornerZoneTimeSeconds
                << " newtonian_half_width_m="
                << newtonian->metrics.maximumHullRequiredHalfWidthMeters
                << " assisted_half_width_m="
                << assisted->metrics.maximumHullRequiredHalfWidthMeters
                << "\n";
        }
    }

    // Long-arc diagnostic: 180 degrees at R=80 m and 10 m/s is ~251.3 m
    // / 25.1 s of continuous curved flight. It separates a general angular
    // tracking defect from a DriftTurn-specific recovery/handoff defect.
    for (const auto& pilot : pilots)
    {
        for (const Law law : laws)
        {
            const LongArcMetrics arc =
                runLongArc(model, pilot, law);

            std::cout
                << std::fixed << std::setprecision(6)
                << "[LONG-ARC]"
                << " pilot=" << pilot.name
                << " law=" << lawName(law)
                << " radius_m=80"
                << " sweep_deg=180"
                << " arc_length_m=" << 80.0 * kPi
                << " nominal_duration_s=" << 8.0 * kPi
                << " valid=" << (arc.valid ? 1 : 0)
                << " completed=" << (arc.completed ? 1 : 0)
                << " final_pos_error_m="
                << arc.finalPositionErrorMeters
                << " final_velocity_error_mps="
                << arc.finalVelocityErrorMps
                << " final_forward_error_deg="
                << arc.finalForwardErrorDeg
                << " max_centerline_error_m="
                << arc.maximumCenterlineErrorMeters
                << " max_hull_required_half_width_m="
                << arc.maximumHullRequiredHalfWidthMeters
                << " corridor_half_width_m="
                << kCorridorHalfWidthMeters
                << " max_forward_error_deg="
                << arc.maximumForwardErrorDeg
                << " tracking_envelope_exceeded_ticks="
                << arc.trackingEnvelopeExceededTicks
                << "\n";

            require(
                arc.valid && arc.completed,
                std::string("long arc did not complete: ") +
                    pilot.name + "/" + lawName(law)
            );

            if (pilot.strict)
            {
                require(
                    arc.maximumHullRequiredHalfWidthMeters <=
                        kCorridorHalfWidthMeters,
                    std::string("expert left long-arc hull corridor: ") +
                        lawName(law)
                );
                require(
                    arc.finalPositionErrorMeters <= 1.5,
                    std::string("expert missed long-arc exit gate: ") +
                        lawName(law)
                );
                require(
                    arc.finalVelocityErrorMps <= 1.0,
                    std::string("expert missed long-arc exit velocity: ") +
                        lawName(law)
                );
                require(
                    arc.finalForwardErrorDeg <= 5.0,
                    std::string("expert missed long-arc exit attitude: ") +
                        lawName(law)
                );
                require(
                    arc.maximumForwardErrorDeg <= 10.0,
                    std::string("expert angular tracking lagged on long arc: ") +
                        lawName(law)
                );
            }
        }
    }

    // First target-machine pass is diagnostic for timing ranking. The strict
    // quality assertions below only encode the semantic difference between
    // the three families for expert execution.
    for (const auto& r : records)
    {
        require(r.metrics.valid,
                std::string("corner matrix invalid: ") +
                r.pilot + "/" + lawName(r.law) + "/" +
                modeName(r.mode));

        if (std::string(r.pilot) != "expert")
            continue;

        require(
            r.metrics.completed,
            std::string("expert corner mode did not complete: ") +
                lawName(r.law) + "/" + modeName(r.mode)
        );
        require(
            r.metrics.maximumCorridorViolationMeters <= 1.0e-6,
            std::string("expert left common 32 m hull corridor: ") +
                lawName(r.law) + "/" + modeName(r.mode)
        );
        require(
            r.metrics.finalPositionErrorMeters <= 1.5,
            std::string("expert missed common exit gate: ") +
                lawName(r.law) + "/" + modeName(r.mode)
        );
        require(
            r.metrics.finalVelocityErrorMps <= 1.0,
            std::string("expert missed common exit velocity: ") +
                lawName(r.law) + "/" + modeName(r.mode)
        );
        require(
            r.metrics.finalForwardErrorDeg <= 5.0,
            std::string("expert missed extended exit attitude: ") +
                lawName(r.law) + "/" + modeName(r.mode)
        );
        require(
            r.metrics.outgoingAttitudeCaptured,
            std::string("expert never converged to outgoing attitude while moving: ") +
                lawName(r.law) + "/" + modeName(r.mode)
        );

        if (r.mode == CornerMode::StopTurnGo)
        {
            require(
                r.metrics.minimumCornerZoneSpeedMps <= 0.75,
                "stop-turn-go never achieved a real near-stop in corner zone"
            );
        }
        else if (r.mode == CornerMode::RadiusTurn)
        {
            require(
                r.metrics.minimumCornerZoneSpeedMps >= 5.0,
                "radius turn collapsed toward a stop-turn maneuver"
            );
            require(
                r.metrics.maximumDriftAngleDeg <= 20.0,
                "radius turn accumulated excessive body/velocity drift angle"
            );
        }
        else
        {
            require(
                r.metrics.minimumCornerZoneSpeedMps >= 7.0,
                "drift turn lost too much translational speed"
            );
            require(
                r.metrics.maximumDriftAngleDeg >= 60.0,
                "drift turn never produced a material body/velocity slip angle"
            );
        }
    }
}

} // namespace

int main()
{
    try
    {
        testCornerFamilyMatrix();

        std::cout << "MANEUVER CORNER FAMILY MATRIX TESTS: PASS\n";
        std::cout << " - stop-turn-go, radius and drift use the same L-shaped rigid-hull corridor\n";
        std::cout << " - Newtonian and Assisted run every family with the same three PilotSkill profiles\n";
        std::cout << " - family-specific phase handoff uses ScheduledMoving and StateCapture\n";
        std::cout << " - total corridor time and corner-zone time are reported separately\n";
        std::cout << " - drift is defined by sustained speed plus material body/velocity slip angle\n";
        std::cout << " - outgoing attitude convergence is tracked while the reference keeps moving beyond the old x=60 checkpoint\n";
        std::cout << " - long 180 deg arc probes continuous angle correction during ~251 m of curved flight\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "MANEUVER CORNER FAMILY MATRIX TESTS: FAIL: "
            << error.what() << "\n";
        return 1;
    }
}
