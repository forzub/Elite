#include "src/game/navigation/AcceptedManeuverProgram.h"
#include "src/game/navigation/TrajectoryFollower.h"
#include "src/game/navigation/NavigationRuntimeControlBridge.h"
#include "src/game/navigation/DynamicMotionSystem.h"
#include "src/game/navigation/ManeuverPhaseGate.h"
#include "src/game/navigation/NavigationExecutionReplanPolicy.h"
#include "src/game/navigation/NavigationRuntimePlanner.h"
#include "src/game/shared/SharedShipPhysics.h"
#include "src/game/ship/controller/ManeuverDecisionController.h"
#include "src/game/ship/core/ShipParams.h"
#include "src/world/WorldParams.h"
#include "src/world/navigation/map/NavigationMap.h"
#include "src/world/navigation/space/NavigationSpace.h"
#include "src/world/navigation/space/NavigationStaticQueryApi.h"

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
using Gate = game::navigation::ManeuverPhaseGate;
using Replan = game::navigation::NavigationExecutionReplanPolicy;
using Planner = game::navigation::NavigationRuntimePlanner;
using Decision = game::ship::controller::ManeuverDecisionController;
using Law = game::navigation::LocalFlightControlLaw;
using Map = world::navigation::NavigationMap;
using Space = world::navigation::NavigationSpace;
using StaticQueries = world::navigation::NavigationStaticQueryApi;

constexpr double kPi = 3.14159265358979323846;
constexpr double kDt = 0.02;
constexpr double kWideHalfWidthMeters = 30.0;
constexpr double kNarrowHalfWidthMeters = 19.0;

const glm::dvec3 kBodyHalfExtents {13.0, 2.5, 11.1};
const double kHullBoundingRadiusMeters =
    glm::length(kBodyHalfExtents);

const glm::dvec3 kStaticObstacleCenter {150.0, 0.0, 0.0};
const glm::dvec3 kStaticObstacleHalfExtents {25.0, 12.0, 30.0};

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

double lateralBump(double u)
{
    const double oneMinus = 1.0 - u;
    return
        64.0 *
        u * u * u *
        oneMinus * oneMinus * oneMinus;
}

double lateralBumpDerivative(double u)
{
    return
        -384.0 * std::pow(u, 5.0) +
         960.0 * std::pow(u, 4.0) -
         768.0 * std::pow(u, 3.0) +
         192.0 * u * u;
}

double lateralBumpSecondDerivative(double u)
{
    return
        -1920.0 * std::pow(u, 4.0) +
         3840.0 * std::pow(u, 3.0) -
         2304.0 * u * u +
          384.0 * u;
}

struct Basis
{
    glm::dvec3 forward {1.0, 0.0, 0.0};
    glm::dvec3 right {0.0, 0.0, 1.0};
    glm::dvec3 up {0.0, 1.0, 0.0};
};

Basis basisForForward(
    const glm::dvec3& requestedForward,
    const Basis& fallback = Basis {}
)
{
    if (glm::length(requestedForward) <= 1.0e-9)
        return fallback;

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

Basis transportedBasisForForward(
    const glm::dvec3& requestedForward,
    const Basis& previous
)
{
    if (glm::length(requestedForward) <= 1.0e-9)
        return previous;

    const glm::dvec3 forward = glm::normalize(requestedForward);

    glm::dvec3 right =
        previous.right -
        forward * glm::dot(previous.right, forward);

    if (glm::length(right) <= 1.0e-9)
    {
        glm::dvec3 projectedUp =
            previous.up -
            forward * glm::dot(previous.up, forward);

        if (glm::length(projectedUp) > 1.0e-9)
        {
            projectedUp = glm::normalize(projectedUp);
            right = glm::cross(forward, projectedUp);
        }
        else
        {
            return basisForForward(forward, previous);
        }
    }

    right = glm::normalize(right);
    glm::dvec3 up =
        glm::normalize(glm::cross(right, forward));

    if (glm::dot(up, previous.up) < 0.0)
    {
        right = -right;
        up = -up;
    }

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

ShipParams cobraParams()
{
    ShipParams p {};
    p.maxPitchRate = 2.5f;
    p.maxYawRate = 2.5f;
    p.maxRollRate = 3.0f;
    p.angularAccel = 3.0f;
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
    profile.execution.deterministicSeed = 0xC0A1B17Eull;
    return profile;
}

struct VehicleState
{
    glm::dvec3 position {0.0};
    glm::dvec3 velocity {0.0};
    Basis basis {};
    glm::dvec3 angularVelocityMap {0.0};
};

struct Vehicle
{
    ShipTransform transform {};
    ShipParams params {};
    WorldParams world {};
    game::navigation::KinematicFrame frame {};
    Bridge bridge;
    double timeSeconds = 0.0;

    explicit Vehicle(Law law)
        : params(cobraParams()),
          bridge(expertProfile())
    {
        frame.systemId = 1;
        frame.frameId = "composite-proving-ground";
        frame.originMeters = {0.0, 0.0, 0.0};
        frame.localToWorldBasis = glm::dmat3(1.0);
        frame.valid = true;

        transform.motion.mode =
            game::navigation::MotionMode::HubTactical;
        transform.motion.systemId = 1;
        transform.motion.travelFrame = frame;
        transform.motion.localControlLaw = law;
        transform.motion.localPositionMeters = {0.0, 0.0, 0.0};
        transform.motion.localVelocityMps = {6.0, 0.0, 0.0};
        transform.setWorldPositionMeters({0.0, 0.0, 0.0});
        setTransformBasis(
            transform,
            basisForForward({1.0, 0.0, 0.0})
        );

        Bridge::Intent initial;
        initial.revision = 1;
        initial.targetRevision = 0;
        require(
            bridge.reset(0.0, initial),
            "composite bridge reset failed"
        );
    }
};

Basis actualBasis(const Vehicle& v)
{
    return {
        glm::dvec3(v.transform.forward()),
        glm::dvec3(v.transform.right()),
        glm::dvec3(v.transform.up())
    };
}

VehicleState captureState(const Vehicle& v)
{
    const Basis basis = actualBasis(v);

    VehicleState result;
    result.position = v.transform.motion.localPositionMeters;
    result.velocity = v.transform.motion.localVelocityMps;
    result.basis = basis;
    result.angularVelocityMap =
        basis.right * static_cast<double>(v.transform.pitchRate) +
        basis.up * static_cast<double>(v.transform.yawRate) +
        basis.forward * static_cast<double>(v.transform.rollRate);
    return result;
}

Follower::AgentState followerAgent(const Vehicle& v)
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

Planner::AgentState plannerAgent(
    const Vehicle& v,
    Law law
)
{
    Planner::AgentState a;
    a.entityId = 42;
    a.positionMapMeters = v.transform.motion.localPositionMeters;
    a.velocityMapMetersPerSecond = v.transform.motion.localVelocityMps;
    a.accelerationMapMetersPerSecond2 = {0.0, 0.0, 0.0};
    a.radiusMeters = kHullBoundingRadiusMeters;
    a.forwardMap = glm::dvec3(v.transform.forward());
    a.rightMap = glm::dvec3(v.transform.right());
    a.upMap = glm::dvec3(v.transform.up());
    a.pitchRateRadPerSec = v.transform.pitchRate;
    a.yawRateRadPerSec = v.transform.yawRate;
    a.rollRateRadPerSec = v.transform.rollRate;
    a.hullHalfExtentsBodyMeters = kBodyHalfExtents;
    a.controlMode =
        law == Law::Newtonian
            ? Planner::MovingPassage::ControlMode::Newtonian
            : Planner::MovingPassage::ControlMode::EliteAssisted;
    a.assistedMaxVelocityToForwardAngleRad = 0.35;
    a.linearCapability.maxForwardAccelerationMetersPerSec2 =
        7.5 * 9.80665;
    a.linearCapability.maxReverseAccelerationMetersPerSec2 =
        7.5 * 9.80665;
    a.linearCapability.maxLateralAccelerationMetersPerSec2 = 2.0;
    a.linearCapability.maxVerticalAccelerationMetersPerSec2 = 2.0;
    a.angularCapability.maxAngularAccelerationRadPerSec2 = 3.0;
    a.angularCapability.maxAngularSpeedRadPerSec = 2.5;
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

struct QuinticCurve
{
    std::array<glm::dvec3, 6> c {};
    double durationSeconds = 0.0;
};

QuinticCurve makeCurve(
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

enum class OrientationMode
{
    VelocityAligned,
    FixedStart
};

Program makeProgram(
    std::uint64_t revision,
    double acceptedAt,
    const VehicleState& start,
    const glm::dvec3& endPosition,
    const glm::dvec3& endVelocity,
    const Basis& terminalBasis,
    double duration,
    OrientationMode orientationMode,
    Program::ManeuverFamily family,
    const glm::dvec3& lateralAxis = glm::dvec3(0.0),
    double lateralOffsetMeters = 0.0
)
{
    const QuinticCurve curve =
        makeCurve(
            start.position,
            start.velocity,
            endPosition,
            endVelocity,
            duration
        );

    Program p;
    p.valid = true;
    p.revision = revision;
    p.objectiveRevision = 12000u;
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
        7.5 * 9.80665;
    p.capability.maxReverseAccelerationMetersPerSec2 =
        7.5 * 9.80665;
    p.capability.maxLateralAccelerationMetersPerSec2 = 2.0;
    p.capability.maxVerticalAccelerationMetersPerSec2 = 2.0;
    p.capability.maxAngularAccelerationRadPerSec2 = 3.0;
    p.capability.maxAngularSpeedRadPerSec = 2.5;

    p.sampleCount =
        static_cast<std::uint8_t>(Program::kMaxSamples);

    const glm::dvec3 lateral =
        glm::length(lateralAxis) > 1.0e-9
            ? glm::normalize(lateralAxis)
            : glm::dvec3(0.0);

    std::array<glm::dquat, Program::kMaxSamples> orientations {};
    const double denom =
        static_cast<double>(Program::kMaxSamples - 1);
    const double sampleDt = duration / denom;

    Basis previousBasis = start.basis;

    for (std::size_t i = 0; i < Program::kMaxSamples; ++i)
    {
        const double t =
            duration * static_cast<double>(i) / denom;
        const double u =
            duration > 1.0e-12
                ? t / duration
                : 1.0;

        glm::dvec3 position;
        glm::dvec3 velocity;
        glm::dvec3 acceleration;
        sampleCurve(curve, t, position, velocity, acceleration);

        if (lateralOffsetMeters != 0.0)
        {
            position +=
                lateral *
                (lateralOffsetMeters * lateralBump(u));
            velocity +=
                lateral *
                (lateralOffsetMeters *
                 lateralBumpDerivative(u) / duration);
            acceleration +=
                lateral *
                (lateralOffsetMeters *
                 lateralBumpSecondDerivative(u) /
                 (duration * duration));
        }

        Basis basis = start.basis;
        if (orientationMode == OrientationMode::VelocityAligned)
        {
            const glm::dvec3 requestedForward =
                glm::length(velocity) > 0.25
                    ? velocity
                    : terminalBasis.forward;

            basis =
                transportedBasisForForward(
                    requestedForward,
                    previousBasis
                );
        }

        if (i == 0)
            basis = start.basis;

        if (i + 1 == Program::kMaxSamples)
        {
            if (orientationMode == OrientationMode::VelocityAligned)
            {
                basis =
                    transportedBasisForForward(
                        terminalBasis.forward,
                        previousBasis
                    );
            }
            else
            {
                basis = terminalBasis;
            }
        }

        previousBasis = basis;
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
    omega[0] = start.angularVelocityMap;

    for (std::size_t i = 1; i < Program::kMaxSamples; ++i)
    {
        if (orientationMode == OrientationMode::FixedStart)
        {
            omega[i] = glm::dvec3(0.0);
        }
        else if (i + 1 == Program::kMaxSamples)
        {
            omega[i] =
                angularVelocityBetween(
                    orientations[i - 1],
                    orientations[i],
                    sampleDt
                );
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

        p.samples[i].angularVelocityMapRadPerSecond =
            omega[i];
    }
    p.samples[0].angularVelocityMapRadPerSecond = omega[0];

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

    p.proof.mapRevision = 1;
    p.proof.spaceRevision = 1;
    p.proof.minimumLinearAuthorityReserveMps2 = 0.05;
    p.proof.minimumAngularAuthorityReserveRadPerSec2 = 0.05;

    return p;
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

    const double t =
        std::clamp(
            glm::dot(p - a, ab) / ab2,
            0.0,
            1.0
        );
    return glm::length(p - (a + ab * t));
}

double pointToProgramPolylineDistance(
    const glm::dvec3& p,
    const Program& program
)
{
    double best = std::numeric_limits<double>::infinity();

    for (std::size_t i = 1; i < program.sampleCount; ++i)
    {
        best =
            std::min(
                best,
                pointToSegmentDistance(
                    p,
                    program.samples[i - 1].positionMapMeters,
                    program.samples[i].positionMapMeters
                )
            );
    }
    return best;
}

double hullRequiredHalfWidth(
    const Vehicle& v,
    const Program& program
)
{
    const glm::dvec3 center =
        v.transform.motion.localPositionMeters;
    const Basis basis = actualBasis(v);

    double maximum = 0.0;
    for (int sx : {-1, 1})
    {
        for (int sy : {-1, 1})
        {
            for (int sz : {-1, 1})
            {
                const glm::dvec3 corner =
                    center +
                    basis.right *
                        (static_cast<double>(sx) *
                         kBodyHalfExtents.x) +
                    basis.up *
                        (static_cast<double>(sy) *
                         kBodyHalfExtents.y) +
                    basis.forward *
                        (static_cast<double>(sz) *
                         kBodyHalfExtents.z);

                maximum =
                    std::max(
                        maximum,
                        pointToProgramPolylineDistance(
                            corner,
                            program
                        )
                    );
            }
        }
    }
    return maximum;
}

double pointToAabbDistance(
    const glm::dvec3& p,
    const glm::dvec3& center,
    const glm::dvec3& half
)
{
    const glm::dvec3 q =
        glm::abs(p - center) - half;
    const glm::dvec3 outside =
        glm::max(q, glm::dvec3(0.0));
    return glm::length(outside);
}

double conservativeStaticClearance(const Vehicle& v)
{
    return
        pointToAabbDistance(
            v.transform.motion.localPositionMeters,
            kStaticObstacleCenter,
            kStaticObstacleHalfExtents
        ) -
        kHullBoundingRadiusMeters;
}

struct DynamicHazard
{
    bool active = false;
    glm::dvec3 position {0.0};
    glm::dvec3 velocity {0.0};
    double radiusMeters = 8.0;
    double activationTimeSeconds = 0.0;
};

glm::dvec3 dynamicHazardPosition(
    const DynamicHazard& hazard,
    double universeTimeSeconds
)
{
    if (!hazard.active)
        return glm::dvec3(0.0);

    return
        hazard.position +
        hazard.velocity *
            std::max(
                0.0,
                universeTimeSeconds -
                    hazard.activationTimeSeconds
            );
}

double conservativeDynamicClearance(
    const Vehicle& v,
    const DynamicHazard& hazard
)
{
    if (!hazard.active)
        return std::numeric_limits<double>::infinity();

    return
        glm::length(
            v.transform.motion.localPositionMeters -
            dynamicHazardPosition(hazard, v.timeSeconds)
        ) -
        (kHullBoundingRadiusMeters + hazard.radiusMeters);
}

struct ExecutionMetrics
{
    bool valid = true;
    bool completed = false;
    bool captureTimedOut = false;
    std::size_t trackingExceededTicks = 0;
    double maxSlipDeg = 0.0;
    double maxForwardErrorDeg = 0.0;
    double maxHullHalfWidthMeters = 0.0;
    double minStaticClearanceMeters =
        std::numeric_limits<double>::infinity();
    double minDynamicClearanceMeters =
        std::numeric_limits<double>::infinity();
    double finalPositionErrorMeters = 0.0;
    double finalVelocityErrorMps = 0.0;
    double finalForwardErrorDeg = 0.0;
    double simulatedSeconds = 0.0;
};

ExecutionMetrics executeProgram(
    Vehicle& v,
    const Program& program,
    Gate::Mode mode,
    const DynamicHazard& hazard,
    double stopAfterSeconds = -1.0
)
{
    ExecutionMetrics m;

    Gate::Policy gatePolicy;
    gatePolicy.mode = mode;
    gatePolicy.maximumCaptureOverrunSeconds = 5.0;

    const double startTime = v.timeSeconds;
    const double nominalEnd =
        program.acceptedAtUniverseTimeSeconds +
        program.samples[
            static_cast<std::size_t>(program.sampleCount - 1)
        ].timeOffsetSeconds;

    const bool partial = stopAfterSeconds > 0.0;
    const double requestedStop =
        partial
            ? startTime + stopAfterSeconds
            : nominalEnd + gatePolicy.maximumCaptureOverrunSeconds + 0.10;

    while (v.timeSeconds <= requestedStop + 1.0e-9)
    {
        const auto follower =
            Follower::follow(
                program,
                v.timeSeconds,
                followerAgent(v)
            );

        if (follower.status == Follower::Status::InvalidInput)
        {
            m.valid = false;
            break;
        }

        m.trackingExceededTicks +=
            follower.trackingErrorExceeded ? 1u : 0u;
        m.maxForwardErrorDeg =
            std::max(
                m.maxForwardErrorDeg,
                follower.forwardAngleErrorRad *
                    180.0 / kPi
            );

        if (!partial)
        {
            const auto gate =
                Gate::evaluate(
                    program,
                    v.timeSeconds,
                    follower.status,
                    gatePolicy
                );

            if (gate.status == Gate::Status::InvalidInput)
            {
                m.valid = false;
                break;
            }
            if (gate.status == Gate::Status::Advance)
            {
                m.completed = true;
                break;
            }
            if (gate.status == Gate::Status::CaptureTimedOut)
            {
                m.captureTimedOut = true;
                break;
            }
        }
        else if (v.timeSeconds >= requestedStop - 1.0e-9)
        {
            break;
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
            m.valid = false;
            break;
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

        const double speed =
            glm::length(v.transform.motion.localVelocityMps);
        if (speed > 0.25)
        {
            m.maxSlipDeg =
                std::max(
                    m.maxSlipDeg,
                    angleRad(
                        v.transform.motion.localVelocityMps,
                        glm::dvec3(v.transform.forward())
                    ) * 180.0 / kPi
                );
        }

        m.maxHullHalfWidthMeters =
            std::max(
                m.maxHullHalfWidthMeters,
                hullRequiredHalfWidth(v, program)
            );

        m.minStaticClearanceMeters =
            std::min(
                m.minStaticClearanceMeters,
                conservativeStaticClearance(v)
            );
        m.minDynamicClearanceMeters =
            std::min(
                m.minDynamicClearanceMeters,
                conservativeDynamicClearance(v, hazard)
            );
    }

    const auto& terminal =
        program.samples[
            static_cast<std::size_t>(program.sampleCount - 1)
        ];

    m.finalPositionErrorMeters =
        glm::length(
            v.transform.motion.localPositionMeters -
            terminal.positionMapMeters
        );
    m.finalVelocityErrorMps =
        glm::length(
            v.transform.motion.localVelocityMps -
            terminal.velocityMapMetersPerSecond
        );
    m.finalForwardErrorDeg =
        angleRad(
            glm::dvec3(v.transform.forward()),
            terminal.forwardMap
        ) * 180.0 / kPi;
    m.simulatedSeconds = v.timeSeconds - startTime;

    return m;
}

Space::RegionInput region(
    Space::RegionId id,
    const glm::dvec3& minimum,
    const glm::dvec3& maximum
)
{
    Space::RegionInput out;
    out.regionId = id;
    out.boundsMapMeters.minMapMeters =
        {minimum.x, minimum.y, minimum.z};
    out.boundsMapMeters.maxMapMeters =
        {maximum.x, maximum.y, maximum.z};
    out.clearanceRadiusMeters = 60.0;
    out.geometryRevision = 1;
    return out;
}

Space::PortalInput portal(
    Space::PortalId id,
    Space::RegionId a,
    Space::RegionId b,
    const glm::dvec3& center,
    double clearance
)
{
    Space::PortalInput out;
    out.portalId = id;
    out.regionA = a;
    out.regionB = b;
    out.centerMapMeters = {center.x, center.y, center.z};
    out.clearanceRadiusMeters = clearance;
    out.bidirectional = true;
    out.geometryRevision = 1;
    return out;
}

Space buildStaticSpace()
{
    Space space;
    Space::StaticSpaceUpdate update;
    update.sourceRevision = 12001;

    update.regions = {
        region(
            1,
            {-20.0, -60.0, -50.0},
            {100.0, 60.0, 50.0}
        ),
        region(
            2,
            {100.0, 20.0, -50.0},
            {200.0, 100.0, 50.0}
        ),
        region(
            3,
            {200.0, -60.0, -50.0},
            {340.0, 100.0, 50.0}
        )
    };

    update.portals = {
        portal(101, 1, 2, {100.0, 40.0, 0.0}, 35.0),
        portal(102, 2, 3, {200.0, 40.0, 0.0}, kNarrowHalfWidthMeters)
    };

    world::navigation::NavigationObstacle wall;
    wall.id = "composite_static_wall";
    wall.entityId = 12050;
    wall.shape =
        world::navigation::NavigationObstacleShape::Box;
    wall.centerMeters = kStaticObstacleCenter;
    wall.localToWorldBasis = glm::dmat3(1.0);
    wall.halfExtentsMeters = kStaticObstacleHalfExtents;
    update.obstacles.push_back(wall);

    space.replaceStaticWorld(std::move(update));
    return space;
}

Planner::Policy plannerPolicy()
{
    Planner::Policy policy;
    policy.corridor.distanceWeight = 1.0;
    policy.corridor.preferredClearanceMultiple = 1.2;
    policy.corridor.clearancePenaltyMeters = 20.0;
    policy.corridor.turnPenaltyMetersPerRadian = 5.0;

    policy.horizon.lookAheadSeconds = 4.0;
    policy.horizon.maxResultAgeSeconds = 0.25;
    policy.horizon.maxBrakingAccelerationMetersPerSecond2 = 8.0;
    policy.horizon.turnDistanceMeters = 20.0;
    policy.horizon.safetyMarginMeters = 2.0;
    policy.horizon.minimumHorizonMeters = 30.0;

    policy.avoidance.primaryDeflectionRadians =
        0.3490658503988659;
    policy.avoidance.secondaryDeflectionRadians =
        0.6981317007977318;
    policy.avoidance.azimuthSamples = 16;
    policy.avoidance.staticAdditionalClearanceMeters = 0.0;

    policy.portalTraversal.enabled = false;
    return policy;
}

Planner::Goal finalGoal()
{
    Planner::Goal goal;
    goal.revision = 12002;
    goal.targetPositionMapMeters = {300.0, 0.0, 0.0};
    goal.maximumTargetSpeedMps = 12.0;
    goal.velocityResponsePerSecond = 0.75;
    goal.angularDampingPerSecond = 2.0;
    goal.arrivalRadiusMeters = 1.0;
    return goal;
}

Map::QueryResult emptyDynamic()
{
    Map::QueryResult result;
    result.mapRevision = 1;
    result.sourceRevision = 1;
    return result;
}

const char* lawName(Law law)
{
    return law == Law::Newtonian
        ? "newtonian"
        : "assisted";
}

const char* familyName(Program::ManeuverFamily family)
{
    switch (family)
    {
        case Program::ManeuverFamily::DriftPass:
            return "drift_pass";
        case Program::ManeuverFamily::PrecisionTransit:
            return "precision_transit";
        case Program::ManeuverFamily::PrecisionCapture:
            return "precision_capture";
        case Program::ManeuverFamily::FreeTransit:
            return "free_transit";
        default:
            return "other";
    }
}

struct ProgramChoice
{
    Program drift {};
    Program aligned {};
    std::array<Decision::Candidate, 2> candidates {};
};

Decision::Candidate candidateForProgram(
    std::uint64_t id,
    Decision::ControlLawRequirement lawRequirement,
    double timeSeconds,
    double exitSpeedMps
)
{
    Decision::Candidate c;
    c.candidateId = id;
    c.family = Decision::CandidateFamily::LocalVisibility;
    c.controlLawRequirement = lawRequirement;
    c.valid = true;
    c.collisionFree = true;
    c.contactExpected = false;
    c.progressesObjective = true;
    c.stopsOrBrakes = false;
    c.timeToObjectiveSeconds = timeSeconds;
    c.entrySpeedMps = 10.0;
    c.exitSpeedMps = exitSpeedMps;
    c.minimumClearanceMeters = 5.0;
    c.escapeReserve01 = 0.60;
    c.peakClosingNormalSpeedMps = 0.0;
    c.normalImpactEnergyProxyJ = 0.0;
    c.criticalDamageRisk01 = 0.05;
    c.missionDamageCost01 = 0.0;
    c.expendableDamageCost01 = 0.0;
    c.threatExposure = 0.0;
    return c;
}

ProgramChoice buildDoctrineChoice(
    const Vehicle& v,
    const glm::dvec3& target
)
{
    const VehicleState start = captureState(v);
    const Basis terminal =
        basisForForward({1.0, 0.0, 0.0}, start.basis);

    ProgramChoice choice;

    choice.drift =
        makeProgram(
            12100,
            v.timeSeconds,
            start,
            target,
            {12.0, 0.0, 0.0},
            start.basis,
            10.0,
            OrientationMode::FixedStart,
            Program::ManeuverFamily::DriftPass,
            {0.0, 1.0, 0.0},
            28.0
        );

    choice.aligned =
        makeProgram(
            12101,
            v.timeSeconds,
            start,
            target,
            {12.0, 0.0, 0.0},
            terminal,
            12.0,
            OrientationMode::VelocityAligned,
            Program::ManeuverFamily::PrecisionTransit,
            {0.0, 1.0, 0.0},
            28.0
        );

    choice.candidates[0] =
        candidateForProgram(
            1,
            Decision::ControlLawRequirement::NewtonianOnly,
            10.0,
            12.0
        );
    choice.candidates[1] =
        candidateForProgram(
            2,
            Decision::ControlLawRequirement::Any,
            12.0,
            12.0
        );

    return choice;
}

struct CompositeMetrics
{
    std::size_t phases = 0;
    std::size_t replans = 0;
    std::size_t trackingExceededTicks = 0;
    double minStaticClearanceMeters =
        std::numeric_limits<double>::infinity();
    double minDynamicClearanceMeters =
        std::numeric_limits<double>::infinity();
    double maxHullHalfWidthMeters = 0.0;
    double maxSlipDeg = 0.0;
    double maxForwardErrorDeg = 0.0;
    double finalPositionErrorMeters = 0.0;
    double finalSpeedMps = 0.0;
    double finalForwardErrorDeg = 0.0;
    double totalSeconds = 0.0;
    Program::ManeuverFamily selectedFamily =
        Program::ManeuverFamily::Undefined;
    Replan::Reason invalidationReason = Replan::Reason::None;
};

void absorb(
    CompositeMetrics& total,
    const ExecutionMetrics& phase
)
{
    total.trackingExceededTicks +=
        phase.trackingExceededTicks;
    total.minStaticClearanceMeters =
        std::min(
            total.minStaticClearanceMeters,
            phase.minStaticClearanceMeters
        );
    total.minDynamicClearanceMeters =
        std::min(
            total.minDynamicClearanceMeters,
            phase.minDynamicClearanceMeters
        );
    total.maxHullHalfWidthMeters =
        std::max(
            total.maxHullHalfWidthMeters,
            phase.maxHullHalfWidthMeters
        );
    total.maxSlipDeg =
        std::max(total.maxSlipDeg, phase.maxSlipDeg);
    total.maxForwardErrorDeg =
        std::max(
            total.maxForwardErrorDeg,
            phase.maxForwardErrorDeg
        );
}

CompositeMetrics runComposite(Law law)
{
    Vehicle v(law);
    CompositeMetrics total;

    Space space = buildStaticSpace();
    const StaticQueries staticQueries(space);
    const Planner::Policy pPolicy = plannerPolicy();
    const Planner::Goal goal = finalGoal();

    // Production exact-static proof: direct start -> final is forbidden.
    StaticQueries::SegmentQuery direct;
    direct.startMapMeters = {0.0, 0.0, 0.0};
    direct.endMapMeters = {300.0, 0.0, 0.0};
    direct.envelope.radiusMeters = kHullBoundingRadiusMeters;
    direct.exactObstaclesOnly = true;

    const auto directProof =
        staticQueries.querySegment(direct);
    require(
        !directProof.traversable &&
        directProof.blockingObstacleId == "composite_static_wall",
        "composite direct route was not blocked by exact static geometry"
    );

    // Production topology/runtime planner chooses the first safe portal.
    const Planner::Result initialPlan =
        Planner::plan(
            plannerAgent(v, law),
            goal,
            emptyDynamic(),
            0.0,
            staticQueries,
            pPolicy
        );

    require(
        initialPlan.status == Planner::Status::NominalClear &&
        initialPlan.usedPortalWaypoint,
        "composite initial topology did not produce first portal waypoint"
    );
    require(
        initialPlan.staticPortalPath.size() == 2 &&
        initialPlan.staticPortalPath[0] == 101 &&
        initialPlan.staticPortalPath[1] == 102,
        "composite topology changed expected portal route"
    );

    // Phase 1: execute to the first production-selected portal.
    {
        const VehicleState start = captureState(v);
        const Basis terminal =
            basisForForward({1.0, 0.0, 0.0}, start.basis);

        const Program p =
            makeProgram(
                12010,
                v.timeSeconds,
                start,
                initialPlan.coarseWaypointMapMeters,
                {10.0, 0.0, 0.0},
                terminal,
                14.0,
                OrientationMode::VelocityAligned,
                Program::ManeuverFamily::FreeTransit
            );

        const auto phase =
            executeProgram(
                v,
                p,
                Gate::Mode::ScheduledMoving,
                DynamicHazard {}
            );
        require(
            phase.valid && phase.completed,
            "composite first portal transit failed"
        );
        require(
            phase.maxHullHalfWidthMeters <= kWideHalfWidthMeters,
            "composite wide corridor exceeded"
        );

        absorb(total, phase);
        ++total.phases;
    }

    // Phase 2: B7 Extreme chooses a law-compatible physical family.
    const glm::dvec3 secondPortal {200.0, 40.0, 0.0};
    const ProgramChoice choice =
        buildDoctrineChoice(v, secondPortal);

    Decision::Context decisionContext;
    decisionContext.doctrine = Decision::Doctrine::Extreme;
    decisionContext.progress =
        Decision::ProgressRequirement::MustProgress;
    decisionContext.controlLaw = law;
    decisionContext.maximumPreferredCriticalDamageRisk01 = 0.20;

    const auto selection =
        Decision::select(
            decisionContext,
            choice.candidates.data(),
            choice.candidates.size()
        );

    require(
        selection.valid,
        "composite B7 selection failed"
    );

    const Program* selected = nullptr;
    if (selection.selectedCandidateId == 1)
        selected = &choice.drift;
    else if (selection.selectedCandidateId == 2)
        selected = &choice.aligned;

    require(
        selected != nullptr,
        "composite B7 returned unknown candidate"
    );

    if (law == Law::Newtonian)
    {
        require(
            selection.selectedCandidateId == 1 &&
            selected->family == Program::ManeuverFamily::DriftPass,
            "Newtonian composite Extreme did not select drift"
        );
    }
    else
    {
        require(
            selection.selectedCandidateId == 2 &&
            selected->family ==
                Program::ManeuverFamily::PrecisionTransit,
            "Assisted composite Extreme did not filter Newtonian drift"
        );
    }

    total.selectedFamily = selected->family;

    // Execute only a prefix, then inject a new dynamic hazard.
    const auto prefix =
        executeProgram(
            v,
            *selected,
            Gate::Mode::ScheduledMoving,
            DynamicHazard {},
            4.0
        );

    require(
        prefix.valid,
        "composite selected-program prefix failed"
    );
    absorb(total, prefix);

    DynamicHazard hazard;
    hazard.active = true;
    hazard.activationTimeSeconds = v.timeSeconds;
    hazard.radiusMeters = 8.0;

    const glm::dvec3 toPortal =
        secondPortal -
        v.transform.motion.localPositionMeters;
    require(
        glm::length(toPortal) > 40.0,
        "composite hazard fixture activated too late"
    );

    const glm::dvec3 towardPortal =
        glm::normalize(toPortal);
    hazard.position =
        v.transform.motion.localPositionMeters +
        towardPortal * 35.0;
    hazard.velocity = {0.0, -0.75, 0.0};

    // Production execution monitor invalidates the already accepted program.
    Replan::Policy replanPolicy;
    Replan::Query replanQuery;
    replanQuery.mode = Replan::ExecutionMode::Automatic;
    replanQuery.universeTimeSeconds = v.timeSeconds;
    replanQuery.acceptedSegmentValid = true;
    replanQuery.acceptedSegmentValidUntilUniverseTimeSeconds =
        selected->validUntilUniverseTimeSeconds;
    replanQuery.globalRouteValid = true;
    replanQuery.currentTopologyBranchValid = true;
    replanQuery.dynamicHazardInvalidated = true;

    const auto replan =
        Replan::evaluate(replanPolicy, replanQuery);

    require(
        replan.scope == Replan::Scope::LocalHorizon &&
        replan.reason ==
            Replan::Reason::DynamicHazardInvalidated &&
        replan.immediate &&
        !replan.continueAcceptedAutomaticExecution,
        "composite dynamic hazard did not invalidate old program"
    );

    total.replans = 1;
    total.invalidationReason = replan.reason;

    // Publish the newly observed hazard through NavigationMap.
    Map::Config mapConfig;
    mapConfig.halfExtentMeters = 500.0;
    mapConfig.cellSizeMeters = 25.0;
    mapConfig.predictionHorizonSeconds = 4.0;
    mapConfig.interactionMarginMeters = 0.0;

    Map map(mapConfig);
    Map::DynamicWorldUpdate dynamicUpdate;
    dynamicUpdate.sourceRevision = 12003;

    Map::DynamicActorInput blocker;
    blocker.entityId = 12060;
    blocker.positionMapMeters = {
        hazard.position.x,
        hazard.position.y,
        hazard.position.z
    };
    blocker.velocityMapMetersPerSecond = {
        hazard.velocity.x,
        hazard.velocity.y,
        hazard.velocity.z
    };
    blocker.accelerationMapMetersPerSecond2 = {0.0, 0.0, 0.0};
    blocker.radiusMeters = hazard.radiusMeters;
    blocker.motionRevision = 1;
    dynamicUpdate.actors.push_back(blocker);
    map.replaceDynamicWorld(std::move(dynamicUpdate));

    Map::SphereQuery sphere;
    sphere.centerMapMeters = {
        v.transform.motion.localPositionMeters.x,
        v.transform.motion.localPositionMeters.y,
        v.transform.motion.localPositionMeters.z
    };
    sphere.radiusMeters = 140.0;
    sphere.lookAheadSeconds = 4.0;

    const Map::QueryResult dynamic =
        map.querySphere(sphere);

    require(
        !dynamic.candidates.empty(),
        "composite dynamic hazard was not published into NavigationMap"
    );

    const Planner::Result adjusted =
        Planner::plan(
            plannerAgent(v, law),
            goal,
            dynamic,
            0.0,
            staticQueries,
            pPolicy
        );

    require(
        adjusted.status == Planner::Status::AdjustedClear &&
        adjusted.adjustedTarget,
        "composite production planner did not find adjusted dynamic bypass"
    );
    require(
        adjusted.nominalDynamicConflictsFound > 0,
        "composite adjusted plan lost dynamic conflict witness"
    );

    // Phase 3: replacement program starts from the actual invalidation state.
    {
        const VehicleState start = captureState(v);
        const glm::dvec3 delta =
            adjusted.selectedTargetMapMeters - start.position;
        require(
            glm::length(delta) > 5.0,
            "composite adjusted target too close for replacement phase"
        );

        const glm::dvec3 endVelocity =
            glm::normalize(delta) * 8.0;
        const Basis terminal =
            transportedBasisForForward(
                endVelocity,
                start.basis
            );
        const double duration =
            std::max(
                6.0,
                glm::length(delta) / 8.0 * 1.5
            );

        const Program replacement =
            makeProgram(
                12020,
                v.timeSeconds,
                start,
                adjusted.selectedTargetMapMeters,
                endVelocity,
                terminal,
                duration,
                OrientationMode::VelocityAligned,
                Program::ManeuverFamily::PrecisionTransit
            );

        const auto phase =
            executeProgram(
                v,
                replacement,
                Gate::Mode::ScheduledMoving,
                hazard
            );

        require(
            phase.valid && phase.completed,
            "composite replacement program failed"
        );
        require(
            phase.minDynamicClearanceMeters > 0.5,
            "composite replacement did not clear dynamic hazard"
        );

        absorb(total, phase);
        ++total.phases;
    }

    // Production planner resumes the same topology after the local bypass.
    const Planner::Result resumed =
        Planner::plan(
            plannerAgent(v, law),
            goal,
            emptyDynamic(),
            0.0,
            staticQueries,
            pPolicy
        );

    require(
        resumed.status == Planner::Status::NominalClear &&
        resumed.usedPortalWaypoint,
        "composite did not resume topology toward narrow portal"
    );
    require(
        resumed.staticPortalPath.size() == 1 &&
        resumed.staticPortalPath.front() == 102,
        "composite resumed wrong portal after local replan"
    );

    // Phase 4: narrow passage to the second portal.
    {
        const VehicleState start = captureState(v);
        const Basis terminal =
            basisForForward({1.0, 0.0, 0.0}, start.basis);

        const Program narrow =
            makeProgram(
                12030,
                v.timeSeconds,
                start,
                resumed.coarseWaypointMapMeters,
                {6.0, 0.0, 0.0},
                terminal,
                10.0,
                OrientationMode::VelocityAligned,
                Program::ManeuverFamily::PrecisionTransit
            );

        const auto phase =
            executeProgram(
                v,
                narrow,
                Gate::Mode::ScheduledMoving,
                hazard
            );

        require(
            phase.valid && phase.completed,
            "composite narrow passage failed"
        );
        require(
            phase.maxHullHalfWidthMeters <=
                kNarrowHalfWidthMeters,
            "composite full hull exceeded narrow passage"
        );

        absorb(total, phase);
        ++total.phases;
    }

    // Phase 5: final precision capture from the second portal to the objective.
    const glm::dvec3 finalTarget = goal.targetPositionMapMeters;
    const Basis finalBasis =
        basisForForward({1.0, 0.0, 0.0}, captureState(v).basis);

    {
        const VehicleState start = captureState(v);

        const Program capture =
            makeProgram(
                12040,
                v.timeSeconds,
                start,
                finalTarget,
                {0.0, 0.0, 0.0},
                finalBasis,
                18.0,
                OrientationMode::VelocityAligned,
                Program::ManeuverFamily::PrecisionCapture
            );

        const auto phase =
            executeProgram(
                v,
                capture,
                Gate::Mode::StateCapture,
                hazard
            );

        require(
            phase.valid &&
            phase.completed &&
            !phase.captureTimedOut,
            "composite final StateCapture failed"
        );

        absorb(total, phase);
        ++total.phases;
    }

    total.finalPositionErrorMeters =
        glm::length(
            v.transform.motion.localPositionMeters -
            finalTarget
        );
    total.finalSpeedMps =
        glm::length(v.transform.motion.localVelocityMps);
    total.finalForwardErrorDeg =
        angleRad(
            glm::dvec3(v.transform.forward()),
            finalBasis.forward
        ) * 180.0 / kPi;
    total.totalSeconds = v.timeSeconds;

    return total;
}

void testCompositeProvingGround()
{
    for (const Law law :
         {Law::Newtonian, Law::Assisted})
    {
        const CompositeMetrics m = runComposite(law);

        require(
            m.phases == 4,
            "composite expected four completed post-selection phases"
        );
        require(
            m.replans == 1 &&
            m.invalidationReason ==
                Replan::Reason::DynamicHazardInvalidated,
            "composite replan accounting wrong"
        );
        require(
            m.trackingExceededTicks == 0,
            std::string("composite tracking envelope exceeded for ") +
                lawName(law)
        );
        require(
            m.minStaticClearanceMeters > 0.5,
            std::string("composite static clearance lost for ") +
                lawName(law)
        );
        require(
            m.minDynamicClearanceMeters > 0.5,
            std::string("composite dynamic clearance lost for ") +
                lawName(law)
        );
        require(
            m.maxHullHalfWidthMeters <= kWideHalfWidthMeters,
            "composite hull envelope exceeded wide bound"
        );
        require(
            m.finalPositionErrorMeters <= 1.0,
            std::string("composite final position miss for ") +
                lawName(law)
        );
        require(
            m.finalSpeedMps <= 0.60,
            std::string("composite final speed too high for ") +
                lawName(law)
        );
        require(
            m.finalForwardErrorDeg <= 4.0,
            std::string("composite final attitude miss for ") +
                lawName(law)
        );

        if (law == Law::Newtonian)
        {
            require(
                m.selectedFamily ==
                    Program::ManeuverFamily::DriftPass,
                "composite Newtonian did not select drift family"
            );
            require(
                m.maxSlipDeg >= 15.0,
                "composite Newtonian did not exhibit material drift"
            );
        }
        else
        {
            require(
                m.selectedFamily ==
                    Program::ManeuverFamily::PrecisionTransit,
                "composite Assisted did not select aligned family"
            );
            require(
                m.maxSlipDeg <= 8.0,
                "composite Assisted accumulated excessive slip"
            );
        }

        std::cout
            << std::fixed << std::setprecision(6)
            << "[COMPOSITE]"
            << " law=" << lawName(law)
            << " doctrine=extreme"
            << " selected_family="
            << familyName(m.selectedFamily)
            << " completed_phases=" << m.phases
            << " replans=" << m.replans
            << " invalidation=dynamic_hazard"
            << " min_static_clearance_m="
            << m.minStaticClearanceMeters
            << " min_dynamic_clearance_m="
            << m.minDynamicClearanceMeters
            << " max_hull_half_width_m="
            << m.maxHullHalfWidthMeters
            << " max_slip_deg=" << m.maxSlipDeg
            << " max_forward_error_deg="
            << m.maxForwardErrorDeg
            << " tracking_exceeded_ticks="
            << m.trackingExceededTicks
            << " final_pos_error_m="
            << m.finalPositionErrorMeters
            << " final_speed_mps="
            << m.finalSpeedMps
            << " final_forward_error_deg="
            << m.finalForwardErrorDeg
            << " total_s=" << m.totalSeconds
            << "\n";
    }
}

} // namespace

int main()
{
    try
    {
        testCompositeProvingGround();

        std::cout
            << "NAVIGATION COMPOSITE PROVING GROUND: PASS\n";
        std::cout
            << " - production NavigationSpace/NavigationRuntimePlanner own static topology and dynamic local bypass\n";
        std::cout
            << " - B7 Extreme selects a law-compatible physical family\n";
        std::cout
            << " - accepted program execution crosses B9/B10 -> PilotSkill -> real physics\n";
        std::cout
            << " - a newly published dynamic hazard invalidates the old accepted program immediately\n";
        std::cout
            << " - replacement execution starts from actual live state with no reset\n";
        std::cout
            << " - full Cobra hull passes the constrained second portal and StateCapture finishes precisely\n";
        std::cout
            << " - physical time-program authoring remains test-side until full production B5 Assisted/general migration is complete\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "NAVIGATION COMPOSITE PROVING GROUND: FAIL: "
            << error.what() << "\n";
        return 1;
    }
}
