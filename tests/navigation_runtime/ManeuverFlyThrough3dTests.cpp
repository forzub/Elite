#include "src/game/navigation/AcceptedManeuverProgram.h"
#include "src/game/navigation/TrajectoryFollower.h"
#include "src/game/navigation/NavigationRuntimeControlBridge.h"
#include "src/game/navigation/DynamicMotionSystem.h"
#include "src/game/shared/SharedShipPhysics.h"
#include "src/game/ship/core/ShipParams.h"
#include "src/world/WorldParams.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

#include <glm/gtc/quaternion.hpp>

namespace
{

using Program = game::navigation::AcceptedManeuverProgram;
using Follower = game::navigation::TrajectoryFollower;
using Bridge = game::navigation::NavigationRuntimeControlBridge;
using Law = game::navigation::LocalFlightControlLaw;

constexpr double kPi = 3.14159265358979323846;
constexpr double kDt = 0.02;
constexpr double kStandardGravityMps2 = 9.80665;
constexpr double kFlyThroughSpeedMps = 8.0;
constexpr double kCornerCutMeters = 45.0;
constexpr double kCorridorHalfWidthMeters = 32.0;
constexpr std::uint64_t kObjectiveRevision = 8100u;

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

struct RigidVehicleModel
{
    glm::dvec3 halfExtentsBodyMeters {13.0, 2.5, 11.1};
    double vectoringAngularAccelerationRadPerSec2 = 3.0;
    double maxPitchRateRadPerSec = 2.5;
    double maxYawRateRadPerSec = 2.5;
    double maxRollRateRadPerSec = 3.0;
};

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
    p.manoeuvreThrusterAccel = 2.0f;
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
    profile.execution.deterministicSeed = 0x3DF17E01ull;
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
    profile.execution.deterministicSeed = 0x3DF17C02ull;
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
    profile.execution.deterministicSeed = 0x3DF17B03ull;
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

Basis basisForForward(const glm::dvec3& requestedForward)
{
    const glm::dvec3 forward = glm::normalize(requestedForward);

    glm::dvec3 upSeed(0.0, 1.0, 0.0);
    if (std::abs(glm::dot(forward, upSeed)) > 0.92)
        upSeed = glm::dvec3(1.0, 0.0, 0.0);

    const glm::dvec3 right =
        glm::normalize(glm::cross(forward, upSeed));
    const glm::dvec3 up =
        glm::normalize(glm::cross(right, forward));
    return {forward, right, up};
}

glm::dquat quaternionForBasis(const Basis& basis)
{
    glm::dmat3 m(1.0);
    m[0] = basis.right;
    m[1] = basis.up;
    m[2] = -basis.forward;
    return glm::normalize(glm::quat_cast(m));
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

glm::dvec3 angularVelocityBetween(
    glm::dquat a,
    glm::dquat b,
    double dt
)
{
    if (dt <= 1.0e-12)
        return glm::dvec3(0.0);

    if (glm::dot(a, b) < 0.0)
        b = -b;

    glm::dquat delta =
        glm::normalize(b * glm::inverse(a));
    if (delta.w < 0.0)
        delta = -delta;

    const double w = std::clamp(delta.w, -1.0, 1.0);
    const double angle = 2.0 * std::acos(w);
    const double sinHalf =
        std::sqrt(std::max(0.0, 1.0 - w * w));

    if (angle <= 1.0e-12 || sinHalf <= 1.0e-12)
        return glm::dvec3(0.0);

    const glm::dvec3 axis =
        glm::normalize(
            glm::dvec3(delta.x, delta.y, delta.z) / sinHalf
        );
    return axis * (angle / dt);
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
    p.objectiveRevision = kObjectiveRevision;
    p.family = family;
    p.acceptedAtUniverseTimeSeconds = acceptedAt;
    p.validUntilUniverseTimeSeconds = acceptedAt + duration + 8.0;
    p.completionTriggersReplan = true;

    p.terminalTolerance.positionMeters = 1.0;
    p.terminalTolerance.linearVelocityMps = 0.60;
    p.terminalTolerance.forwardAngleRad = 0.07;
    p.terminalTolerance.angularVelocityRadPerSec = 0.10;

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

using RoutePoints = std::array<glm::dvec3, 6>;

const RoutePoints& routePoints()
{
    // Five segments with approximately 35 / 60 / 90 / 120 degree turns.
    // Every later direction has simultaneous XYZ content except the entry leg.
    static const RoutePoints route {{
        {0.0, 0.0, 0.0},
        {0.0, 0.0, -120.0},
        {0.0, 68.82917236, -218.29824531},
        {102.95880270, 143.42116087, -245.41872094},
        {58.98649759, 238.05127213, -152.08288417},
        {198.40933382, 183.39523142, -143.48114303}
    }};
    return route;
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

    const double t = std::clamp(
        glm::dot(p - a, ab) / ab2,
        0.0,
        1.0
    );
    return glm::length(p - (a + ab * t));
}

double pointToRouteDistance(
    const glm::dvec3& p,
    const RoutePoints& route
)
{
    double best = std::numeric_limits<double>::infinity();
    for (std::size_t i = 1; i < route.size(); ++i)
    {
        best = std::min(
            best,
            pointToSegmentDistance(p, route[i - 1], route[i])
        );
    }
    return best;
}

struct QuinticCurve
{
    std::array<glm::dvec3, 6> c {};
    double durationSeconds = 0.0;
};

QuinticCurve makeCornerCurve(
    const glm::dvec3& start,
    const glm::dvec3& startVelocity,
    const glm::dvec3& end,
    const glm::dvec3& endVelocity,
    double duration
)
{
    QuinticCurve q;
    q.durationSeconds = duration;

    q.c[0] = start;
    q.c[1] = startVelocity * duration;
    q.c[2] = glm::dvec3(0.0);

    const glm::dvec3 P =
        end - q.c[0] - q.c[1];
    const glm::dvec3 V =
        endVelocity * duration - q.c[1];

    q.c[3] = 10.0 * P - 4.0 * V;
    q.c[4] = -15.0 * P + 7.0 * V;
    q.c[5] = 6.0 * P - 3.0 * V;
    return q;
}

void sampleCurve(
    const QuinticCurve& q,
    double t,
    glm::dvec3& position,
    glm::dvec3& velocity,
    glm::dvec3& acceleration
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

    position =
        q.c[0] +
        q.c[1] * u +
        q.c[2] * u2 +
        q.c[3] * u3 +
        q.c[4] * u4 +
        q.c[5] * u5;

    const glm::dvec3 d1 =
        q.c[1] +
        2.0 * q.c[2] * u +
        3.0 * q.c[3] * u2 +
        4.0 * q.c[4] * u3 +
        5.0 * q.c[5] * u4;

    const glm::dvec3 d2 =
        2.0 * q.c[2] +
        6.0 * q.c[3] * u +
        12.0 * q.c[4] * u2 +
        20.0 * q.c[5] * u3;

    velocity =
        T > 1.0e-12 ? d1 / T : glm::dvec3(0.0);
    acceleration =
        T > 1.0e-12 ? d2 / (T * T) : glm::dvec3(0.0);
}

Program makeStraightProgram(
    std::uint64_t revision,
    double acceptedAt,
    const glm::dvec3& start,
    const glm::dvec3& end,
    const glm::dvec3& direction
)
{
    const double distance = glm::length(end - start);
    const double duration = distance / kFlyThroughSpeedMps;

    Program p;
    fillCommon(
        p,
        revision,
        acceptedAt,
        duration,
        Program::ManeuverFamily::FreeTransit
    );
    p.sampleCount = static_cast<std::uint8_t>(Program::kMaxSamples);

    const Basis basis = basisForForward(direction);
    const glm::dvec3 velocity =
        glm::normalize(direction) * kFlyThroughSpeedMps;
    const double denom =
        static_cast<double>(Program::kMaxSamples - 1);

    for (std::size_t i = 0; i < Program::kMaxSamples; ++i)
    {
        const double t =
            duration * static_cast<double>(i) / denom;
        auto& s = p.samples[i];
        s.timeOffsetSeconds = t;
        s.positionMapMeters = start + velocity * t;
        s.velocityMapMetersPerSecond = velocity;
        s.linearAccelerationFeedForwardMapMps2 = {0.0, 0.0, 0.0};
        s.forwardMap = basis.forward;
        s.rightMap = basis.right;
        s.upMap = basis.up;
        s.angularVelocityMapRadPerSecond = {0.0, 0.0, 0.0};
        s.angularAccelerationFeedForwardMapRadPerSec2 = {0.0, 0.0, 0.0};
    }

    return p;
}

Program makeCornerProgram(
    std::uint64_t revision,
    double acceptedAt,
    const glm::dvec3& vertex,
    const glm::dvec3& incomingDirection,
    const glm::dvec3& outgoingDirection,
    double& plannedMaximumAccelerationMps2,
    double& plannedMinimumSpeedMps
)
{
    const glm::dvec3 inDir = glm::normalize(incomingDirection);
    const glm::dvec3 outDir = glm::normalize(outgoingDirection);

    const glm::dvec3 start =
        vertex - inDir * kCornerCutMeters;
    const glm::dvec3 end =
        vertex + outDir * kCornerCutMeters;

    // T=2d/v gives endpoint velocity magnitude v while allowing the quintic
    // to reduce speed naturally inside hard corners instead of stopping.
    const double duration =
        2.0 * kCornerCutMeters / kFlyThroughSpeedMps;

    const QuinticCurve curve =
        makeCornerCurve(
            start,
            inDir * kFlyThroughSpeedMps,
            end,
            outDir * kFlyThroughSpeedMps,
            duration
        );

    Program p;
    fillCommon(
        p,
        revision,
        acceptedAt,
        duration,
        Program::ManeuverFamily::PrecisionTransit
    );
    p.sampleCount = static_cast<std::uint8_t>(Program::kMaxSamples);

    std::array<glm::dquat, Program::kMaxSamples> orientations {};
    const double denom =
        static_cast<double>(Program::kMaxSamples - 1);
    const double sampleDt = duration / denom;

    plannedMaximumAccelerationMps2 = 0.0;
    plannedMinimumSpeedMps =
        std::numeric_limits<double>::infinity();

    for (std::size_t i = 0; i < Program::kMaxSamples; ++i)
    {
        const double t =
            duration * static_cast<double>(i) / denom;

        glm::dvec3 position;
        glm::dvec3 velocity;
        glm::dvec3 acceleration;
        sampleCurve(curve, t, position, velocity, acceleration);

        plannedMaximumAccelerationMps2 =
            std::max(
                plannedMaximumAccelerationMps2,
                glm::length(acceleration)
            );
        plannedMinimumSpeedMps =
            std::min(
                plannedMinimumSpeedMps,
                glm::length(velocity)
            );

        const Basis basis = basisForForward(velocity);
        orientations[i] = quaternionForBasis(basis);

        auto& s = p.samples[i];
        s.timeOffsetSeconds = t;
        s.positionMapMeters = position;
        s.velocityMapMetersPerSecond = velocity;
        s.linearAccelerationFeedForwardMapMps2 = acceleration;
        s.forwardMap = basis.forward;
        s.rightMap = basis.right;
        s.upMap = basis.up;
    }

    std::array<glm::dvec3, Program::kMaxSamples> omega {};
    for (std::size_t i = 0; i < Program::kMaxSamples; ++i)
    {
        if (i == 0 || i + 1 == Program::kMaxSamples)
        {
            // The quintic has zero endpoint translational acceleration, so
            // tangent angular velocity is physically zero at both seams.
            omega[i] = glm::dvec3(0.0);
        }
        else
        {
            omega[i] =
                angularVelocityBetween(
                    orientations[i - 1],
                    orientations[i + 1],
                    2.0 * sampleDt
                );
        }
        p.samples[i].angularVelocityMapRadPerSecond = omega[i];
    }

    for (std::size_t i = 0; i < Program::kMaxSamples; ++i)
    {
        glm::dvec3 alpha(0.0);
        if (i == 0)
        {
            alpha = (omega[1] - omega[0]) / sampleDt;
        }
        else if (i + 1 == Program::kMaxSamples)
        {
            alpha = (omega[i] - omega[i - 1]) / sampleDt;
        }
        else
        {
            alpha =
                (omega[i + 1] - omega[i - 1]) /
                (2.0 * sampleDt);
        }

        p.samples[i].angularAccelerationFeedForwardMapRadPerSec2 =
            alpha;
    }

    return p;
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
        Law law,
        const glm::dvec3& startDirection
    )
        : params(cobraParams(model)),
          bridge(pilot.profile)
    {
        frame.systemId = 1;
        frame.frameId = "fly-through-3d";
        frame.originMeters = {0.0, 0.0, 0.0};
        frame.localToWorldBasis = glm::dmat3(1.0);
        frame.valid = true;

        transform.motion.mode =
            game::navigation::MotionMode::HubTactical;
        transform.motion.systemId = 1;
        transform.motion.travelFrame = frame;
        transform.motion.localControlLaw = law;
        transform.motion.localPositionMeters = routePoints().front();
        transform.motion.localVelocityMps =
            glm::normalize(startDirection) * kFlyThroughSpeedMps;
        transform.setWorldPositionMeters(routePoints().front());
        setTransformBasis(
            transform,
            basisForForward(startDirection)
        );

        Bridge::Intent initial;
        initial.revision = 1;
        initial.targetRevision = 0;
        require(
            bridge.reset(0.0, initial),
            "fly-through bridge reset failed"
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

double hullRequiredHalfWidthMeters(
    const Vehicle& v,
    const RigidVehicleModel& model,
    const RoutePoints& route
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

                maximum =
                    std::max(
                        maximum,
                        pointToRouteDistance(corner, route)
                    );
            }
        }
    }
    return maximum;
}

struct CornerMetrics
{
    double routeAngleDeg = 0.0;
    double plannedMaximumAccelerationMps2 = 0.0;
    double plannedMinimumSpeedMps = 0.0;

    double minimumActualSpeedMps =
        std::numeric_limits<double>::infinity();
    double maximumSlipAngleDeg = 0.0;
    double maximumCenterCrossTrackMeters = 0.0;
    double maximumHullRequiredHalfWidthMeters = 0.0;
    double minimumObservedTurnRadiusMeters =
        std::numeric_limits<double>::infinity();
};

struct Metrics
{
    bool valid = true;
    bool completed = true;
    std::size_t completedPhases = 0;
    std::size_t trackingEnvelopeExceededTicks = 0;

    double minimumRouteSpeedMps =
        std::numeric_limits<double>::infinity();
    double maximumCenterCrossTrackMeters = 0.0;
    double maximumHullRequiredHalfWidthMeters = 0.0;
    double maximumCorridorViolationMeters = 0.0;
    double maximumForwardTrackingErrorDeg = 0.0;

    double finalPositionErrorMeters = 0.0;
    double finalSpeedErrorMps = 0.0;
    double finalForwardErrorDeg = 0.0;
    double simulatedSeconds = 0.0;

    std::array<CornerMetrics, 4> corners {};
};

struct ProgramRunResult
{
    bool valid = true;
    bool completed = false;
};

ProgramRunResult runProgram(
    Vehicle& v,
    const RigidVehicleModel& model,
    const Program& program,
    Metrics& metrics,
    int cornerIndex
)
{
    const RoutePoints& route = routePoints();
    const double endTime =
        program.acceptedAtUniverseTimeSeconds +
        program.samples[
            static_cast<std::size_t>(program.sampleCount - 1)
        ].timeOffsetSeconds;

    glm::dvec3 previousVelocity =
        v.transform.motion.localVelocityMps;

    while (v.timeSeconds < endTime - 1.0e-9)
    {
        const auto follower =
            Follower::follow(
                program,
                v.timeSeconds,
                agentState(v)
            );

        if (follower.status == Follower::Status::InvalidInput)
            return {false, false};

        if (follower.trackingErrorExceeded)
            ++metrics.trackingEnvelopeExceededTicks;

        metrics.maximumForwardTrackingErrorDeg =
            std::max(
                metrics.maximumForwardTrackingErrorDeg,
                follower.forwardAngleErrorRad * 180.0 / kPi
            );

        const auto bridgeResult =
            v.bridge.step(
                v.timeSeconds + kDt,
                kDt,
                toSystemIntent(follower.intent)
            );

        if (bridgeResult.status !=
            Bridge::PilotExecutor::Status::Ok)
        {
            return {false, false};
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

        game::navigation::DynamicMotionSystem::updateLocalFrameMotion(
            v.transform.motion,
            v.transform.worldPosition,
            v.frame,
            v.params,
            kDt
        );

        v.transform.syncLegacyPositionFromWorld();
        v.timeSeconds += kDt;

        const glm::dvec3 center =
            v.transform.motion.localPositionMeters;
        const glm::dvec3 velocity =
            v.transform.motion.localVelocityMps;
        const double speed = glm::length(velocity);

        metrics.minimumRouteSpeedMps =
            std::min(metrics.minimumRouteSpeedMps, speed);

        const double centerCrossTrack =
            pointToRouteDistance(center, route);
        const double hullHalfWidth =
            hullRequiredHalfWidthMeters(v, model, route);

        metrics.maximumCenterCrossTrackMeters =
            std::max(
                metrics.maximumCenterCrossTrackMeters,
                centerCrossTrack
            );
        metrics.maximumHullRequiredHalfWidthMeters =
            std::max(
                metrics.maximumHullRequiredHalfWidthMeters,
                hullHalfWidth
            );
        metrics.maximumCorridorViolationMeters =
            std::max(
                metrics.maximumCorridorViolationMeters,
                std::max(
                    0.0,
                    hullHalfWidth - kCorridorHalfWidthMeters
                )
            );

        if (cornerIndex >= 0)
        {
            auto& cm =
                metrics.corners[
                    static_cast<std::size_t>(cornerIndex)
                ];

            cm.minimumActualSpeedMps =
                std::min(cm.minimumActualSpeedMps, speed);
            cm.maximumCenterCrossTrackMeters =
                std::max(
                    cm.maximumCenterCrossTrackMeters,
                    centerCrossTrack
                );
            cm.maximumHullRequiredHalfWidthMeters =
                std::max(
                    cm.maximumHullRequiredHalfWidthMeters,
                    hullHalfWidth
                );

            if (speed > 0.25)
            {
                cm.maximumSlipAngleDeg =
                    std::max(
                        cm.maximumSlipAngleDeg,
                        angleRad(
                            velocity,
                            glm::dvec3(v.transform.forward())
                        ) * 180.0 / kPi
                    );

                const glm::dvec3 actualAcceleration =
                    (velocity - previousVelocity) / kDt;
                const glm::dvec3 tangent = velocity / speed;
                const glm::dvec3 perpendicularAcceleration =
                    actualAcceleration -
                    tangent *
                        glm::dot(actualAcceleration, tangent);
                const double aPerp =
                    glm::length(perpendicularAcceleration);

                if (aPerp > 0.05)
                {
                    cm.minimumObservedTurnRadiusMeters =
                        std::min(
                            cm.minimumObservedTurnRadiusMeters,
                            speed * speed / aPerp
                        );
                }
            }
        }

        previousVelocity = velocity;
    }

    return {true, true};
}

const char* lawName(Law law)
{
    return
        law == Law::Newtonian
            ? "newtonian"
            : "assisted";
}

Metrics runCase(
    const RigidVehicleModel& model,
    const PilotCase& pilot,
    Law law
)
{
    const RoutePoints& route = routePoints();

    std::array<glm::dvec3, 5> directions {};
    std::array<double, 5> lengths {};
    for (std::size_t i = 0; i < directions.size(); ++i)
    {
        const glm::dvec3 delta = route[i + 1] - route[i];
        lengths[i] = glm::length(delta);
        directions[i] = delta / lengths[i];
    }

    Vehicle v(model, pilot, law, directions[0]);
    Metrics m;
    std::uint64_t revision = 8200u;

    for (std::size_t corner = 0; corner < 4; ++corner)
    {
        auto& cm = m.corners[corner];
        cm.routeAngleDeg =
            angleRad(
                directions[corner],
                directions[corner + 1]
            ) * 180.0 / kPi;

        const glm::dvec3 straightStart =
            corner == 0
                ? route[0]
                : route[corner] +
                    directions[corner] * kCornerCutMeters;
        const glm::dvec3 straightEnd =
            route[corner + 1] -
            directions[corner] * kCornerCutMeters;

        if (glm::length(straightEnd - straightStart) > 1.0e-9)
        {
            const Program straight =
                makeStraightProgram(
                    revision++,
                    v.timeSeconds,
                    straightStart,
                    straightEnd,
                    directions[corner]
                );

            const auto result =
                runProgram(v, model, straight, m, -1);
            if (!result.valid)
            {
                m.valid = false;
                m.completed = false;
                break;
            }
            if (!result.completed)
            {
                m.completed = false;
                break;
            }
            ++m.completedPhases;
        }

        const Program turn =
            makeCornerProgram(
                revision++,
                v.timeSeconds,
                route[corner + 1],
                directions[corner],
                directions[corner + 1],
                cm.plannedMaximumAccelerationMps2,
                cm.plannedMinimumSpeedMps
            );

        const auto turnResult =
            runProgram(
                v,
                model,
                turn,
                m,
                static_cast<int>(corner)
            );
        if (!turnResult.valid)
        {
            m.valid = false;
            m.completed = false;
            break;
        }
        if (!turnResult.completed)
        {
            m.completed = false;
            break;
        }
        ++m.completedPhases;
    }

    if (m.valid && m.completed)
    {
        const glm::dvec3 finalStraightStart =
            route[4] +
            directions[4] * kCornerCutMeters;
        const glm::dvec3 finalStraightEnd = route[5];

        const Program finalStraight =
            makeStraightProgram(
                revision++,
                v.timeSeconds,
                finalStraightStart,
                finalStraightEnd,
                directions[4]
            );

        const auto result =
            runProgram(v, model, finalStraight, m, -1);
        if (!result.valid)
        {
            m.valid = false;
            m.completed = false;
        }
        else if (!result.completed)
        {
            m.completed = false;
        }
        else
        {
            ++m.completedPhases;
        }
    }

    const glm::dvec3 finalDirection =
        glm::normalize(route[5] - route[4]);

    m.finalPositionErrorMeters =
        glm::length(
            v.transform.motion.localPositionMeters - route[5]
        );
    m.finalSpeedErrorMps =
        std::abs(
            glm::length(v.transform.motion.localVelocityMps) -
            kFlyThroughSpeedMps
        );
    m.finalForwardErrorDeg =
        angleRad(
            glm::dvec3(v.transform.forward()),
            finalDirection
        ) * 180.0 / kPi;
    m.simulatedSeconds = v.timeSeconds;

    std::cout
        << std::fixed << std::setprecision(6)
        << "[FLY3D]"
        << " pilot=" << pilot.name
        << " law=" << lawName(law)
        << " valid=" << (m.valid ? 1 : 0)
        << " completed=" << (m.completed ? 1 : 0)
        << " phases=" << m.completedPhases << "/9"
        << " final_pos_error_m=" << m.finalPositionErrorMeters
        << " final_speed_error_mps=" << m.finalSpeedErrorMps
        << " final_forward_error_deg=" << m.finalForwardErrorDeg
        << " min_route_speed_mps=" << m.minimumRouteSpeedMps
        << " max_center_cross_track_m="
        << m.maximumCenterCrossTrackMeters
        << " max_hull_required_half_width_m="
        << m.maximumHullRequiredHalfWidthMeters
        << " corridor_half_width_m="
        << kCorridorHalfWidthMeters
        << " max_corridor_violation_m="
        << m.maximumCorridorViolationMeters
        << " max_forward_tracking_error_deg="
        << m.maximumForwardTrackingErrorDeg
        << " tracking_envelope_exceeded_ticks="
        << m.trackingEnvelopeExceededTicks
        << " simulated_s=" << m.simulatedSeconds
        << "\n";

    for (std::size_t i = 0; i < m.corners.size(); ++i)
    {
        const auto& cm = m.corners[i];
        std::cout
            << std::fixed << std::setprecision(6)
            << "[FLY3D-CORNER]"
            << " pilot=" << pilot.name
            << " law=" << lawName(law)
            << " corner=" << (i + 1)
            << " route_angle_deg=" << cm.routeAngleDeg
            << " planned_peak_accel_mps2="
            << cm.plannedMaximumAccelerationMps2
            << " planned_min_speed_mps="
            << cm.plannedMinimumSpeedMps
            << " actual_min_speed_mps="
            << cm.minimumActualSpeedMps
            << " max_slip_deg="
            << cm.maximumSlipAngleDeg
            << " min_observed_turn_radius_m="
            << cm.minimumObservedTurnRadiusMeters
            << " max_center_cross_track_m="
            << cm.maximumCenterCrossTrackMeters
            << " max_hull_required_half_width_m="
            << cm.maximumHullRequiredHalfWidthMeters
            << "\n";
    }

    return m;
}

void testContinuous3dFlyThrough()
{
    const RigidVehicleModel model;

    const std::array<PilotCase, 3> pilots {{
        {"expert", expertProfile(), true},
        {"competent", competentProfile(), false},
        {"rookie", rookieProfile(), false}
    }};

    const std::array<Law, 2> laws {{
        Law::Newtonian,
        Law::Assisted
    }};

    for (const auto& pilot : pilots)
    {
        for (const Law law : laws)
        {
            const Metrics m =
                runCase(model, pilot, law);

            require(
                m.valid,
                std::string("fly-through invalid for ") +
                    pilot.name + "/" + lawName(law)
            );

            if (!pilot.strict)
                continue;

            require(
                m.completed && m.completedPhases == 9,
                std::string("expert did not complete fly-through in ") +
                    lawName(law)
            );
            require(
                m.maximumCorridorViolationMeters <= 1.0e-9,
                std::string("expert hull left 32 m fly-through corridor in ") +
                    lawName(law)
            );
            require(
                m.trackingEnvelopeExceededTicks == 0,
                std::string("expert exceeded tracking envelope in ") +
                    lawName(law)
            );
            require(
                m.minimumRouteSpeedMps >= 3.0,
                std::string("expert collapsed fly-through into stop-turn-go in ") +
                    lawName(law)
            );
            require(
                m.finalPositionErrorMeters <= 1.5,
                std::string("expert fly-through final position miss in ") +
                    lawName(law)
            );
            require(
                m.finalSpeedErrorMps <= 0.75,
                std::string("expert fly-through final speed miss in ") +
                    lawName(law)
            );
            require(
                m.finalForwardErrorDeg <= 5.0,
                std::string("expert fly-through final attitude miss in ") +
                    lawName(law)
            );

            for (std::size_t i = 0; i < m.corners.size(); ++i)
            {
                const auto& cm = m.corners[i];
                require(
                    cm.minimumActualSpeedMps >= 3.0,
                    std::string("expert stopped in fly-through corner ") +
                        std::to_string(i + 1) + "/" + lawName(law)
                );
                require(
                    std::isfinite(cm.minimumObservedTurnRadiusMeters),
                    std::string("expert produced no measurable turn radius in corner ") +
                        std::to_string(i + 1) + "/" + lawName(law)
                );
                require(
                    cm.plannedMaximumAccelerationMps2 <= 2.0 + 1.0e-6,
                    std::string("planned fly-through exceeds 2 m/s2 corner authority in corner ") +
                        std::to_string(i + 1)
                );
            }
        }
    }
}

} // namespace

int main()
{
    try
    {
        testContinuous3dFlyThrough();

        std::cout << "MANEUVER 3D FLY-THROUGH TESTS: PASS\n";
        std::cout << " - 5 connected segments exercise ~35/60/90/120 degree 3D turns\n";
        std::cout << " - every corner keeps non-zero velocity; no stop-turn-go substitution\n";
        std::cout << " - quintic C2 corner references preserve moving position/velocity/attitude continuity\n";
        std::cout << " - full Cobra OBB corners are checked against the 32 m route corridor\n";
        std::cout << " - per-corner speed, slip, observed radius and hull envelope are reported\n";
        std::cout << " - Newtonian and Assisted execute the same accepted geometric trajectory\n";
        std::cout << " - expert is strict; competent/rookie establish diagnostics\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "MANEUVER 3D FLY-THROUGH TESTS: FAIL: "
            << error.what() << "\n";
        return 1;
    }
}
