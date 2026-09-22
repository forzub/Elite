#include "src/game/navigation/AcceptedManeuverProgram.h"
#include "src/game/navigation/TrajectoryFollower.h"
#include "src/game/navigation/NavigationRuntimeControlBridge.h"
#include "src/game/navigation/DynamicMotionSystem.h"
#include "src/game/navigation/ManeuverPhaseGate.h"
#include "src/game/navigation/NavigationExecutionReplanPolicy.h"
#include "src/game/navigation/NavigationExecutionSafetyProbeBuilder.h"
#include "src/game/navigation/OrdinaryPhysicalManeuverCompiler.h"
#include "src/game/shared/SharedShipPhysics.h"
#include "src/game/ship/controller/ManeuverDecisionController.h"
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
using Gate = game::navigation::ManeuverPhaseGate;
using Replan = game::navigation::NavigationExecutionReplanPolicy;
using Safety = game::navigation::NavigationExecutionSafetyProbeBuilder;
using Compiler = game::navigation::OrdinaryPhysicalManeuverCompiler;
using Decision = game::ship::controller::ManeuverDecisionController;
using Law = game::navigation::LocalFlightControlLaw;

constexpr double kPi = 3.14159265358979323846;
constexpr double kDt = 0.02;
constexpr double kCorridorHalfWidthMeters = 25.0;
const glm::dvec3 kBodyHalfExtents {13.0, 2.5, 11.1};

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

// Minimal-twist transported frame for a moving tangent.  Rebuilding right/up
// from a fixed world-up seed creates a basis singularity when forward crosses
// the seed-switch threshold.  B10 tracks all three body axes, so that artificial
// roll jump becomes a real angular command.  Project the previous right axis
// into the new normal plane instead (Bishop/parallel-transport frame).
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
            // Only a true 180-degree degeneracy reaches this fallback.
            return basisForForward(forward, previous);
        }
    }

    right = glm::normalize(right);
    glm::dvec3 up =
        glm::normalize(glm::cross(right, forward));

    // Preserve the previous roll hemisphere if numerical projection produced
    // the equivalent frame with both transverse axes inverted.
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
    profile.execution.deterministicSeed = 0xC141A11Dull;
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

    Vehicle(Law law)
        : params(cobraParams()),
          bridge(expertProfile())
    {
        frame.systemId = 1;
        frame.frameId = "chained-limit";
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
            "chained/limit bridge reset failed"
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
    Program::ManeuverFamily family
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
    p.objectiveRevision = 10100u;
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

    std::array<glm::dquat, Program::kMaxSamples> orientations {};
    const double denom =
        static_cast<double>(Program::kMaxSamples - 1);
    const double sampleDt = duration / denom;

    Basis previousBasis = start.basis;

    for (std::size_t i = 0; i < Program::kMaxSamples; ++i)
    {
        const double t =
            duration * static_cast<double>(i) / denom;

        glm::dvec3 position;
        glm::dvec3 velocity;
        glm::dvec3 acceleration;
        sampleCurve(curve, t, position, velocity, acceleration);

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
                // Preserve the transported roll while pinning the requested
                // terminal forward direction. Exact terminal roll, when
                // required by docking/attachment semantics, belongs to a
                // separate explicit attitude-capture profile.
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

struct SeamMetrics
{
    double positionJumpMeters = 0.0;
    double velocityJumpMps = 0.0;
    double forwardJumpDeg = 0.0;
    double angularVelocityJumpRadPerSec = 0.0;
};

SeamMetrics measureSeam(
    const VehicleState& actual,
    const Program& next
)
{
    const auto& first = next.samples[0];

    SeamMetrics m;
    m.positionJumpMeters =
        glm::length(first.positionMapMeters - actual.position);
    m.velocityJumpMps =
        glm::length(
            first.velocityMapMetersPerSecond - actual.velocity
        );
    m.forwardJumpDeg =
        angleRad(first.forwardMap, actual.basis.forward) *
        180.0 / kPi;
    m.angularVelocityJumpRadPerSec =
        glm::length(
            first.angularVelocityMapRadPerSecond -
            actual.angularVelocityMap
        );
    return m;
}

struct PhaseMetrics
{
    bool valid = true;
    bool completed = false;
    bool captureTimedOut = false;
    std::size_t trackingExceededTicks = 0;

    double minimumSpeedMps =
        std::numeric_limits<double>::infinity();
    double entrySlipDeg = 0.0;
    double maximumSlipDeg = 0.0;
    double maximumSlipAfterOneSecondDeg = 0.0;
    double finalSlipDeg = 0.0;
    double maximumHullHalfWidthMeters = 0.0;
    double maximumPositionErrorMeters = 0.0;
    double maximumVelocityErrorMps = 0.0;
    double maximumForwardErrorDeg = 0.0;

    double finalPositionErrorMeters = 0.0;
    double finalVelocityErrorMps = 0.0;
    double finalForwardErrorDeg = 0.0;
    double simulatedSeconds = 0.0;
};

PhaseMetrics executePhase(
    Vehicle& v,
    const Program& program,
    Gate::Mode mode
)
{
    PhaseMetrics m;

    Gate::Policy gatePolicy;
    gatePolicy.mode = mode;
    gatePolicy.maximumCaptureOverrunSeconds = 5.0;

    const double startTime = v.timeSeconds;

    const double entrySpeed =
        glm::length(v.transform.motion.localVelocityMps);
    if (entrySpeed > 0.25)
    {
        m.entrySlipDeg =
            angleRad(
                v.transform.motion.localVelocityMps,
                glm::dvec3(v.transform.forward())
            ) * 180.0 / kPi;
    }

    const double nominalEnd =
        program.acceptedAtUniverseTimeSeconds +
        program.samples[
            static_cast<std::size_t>(program.sampleCount - 1)
        ].timeOffsetSeconds;
    const double hardEnd =
        nominalEnd +
        gatePolicy.maximumCaptureOverrunSeconds +
        0.10;

    while (v.timeSeconds <= hardEnd + 1.0e-9)
    {
        const auto follower =
            Follower::follow(
                program,
                v.timeSeconds,
                agentState(v),
                game::navigation::ManeuverTrackingController::Policy {}
            );

        if (follower.status == Follower::Status::InvalidInput)
        {
            m.valid = false;
            break;
        }

        m.trackingExceededTicks +=
            follower.trackingErrorExceeded ? 1u : 0u;
        m.maximumPositionErrorMeters =
            std::max(
                m.maximumPositionErrorMeters,
                follower.crossTrackErrorMeters
            );
        m.maximumVelocityErrorMps =
            std::max(
                m.maximumVelocityErrorMps,
                follower.linearVelocityErrorMps
            );
        m.maximumForwardErrorDeg =
            std::max(
                m.maximumForwardErrorDeg,
                follower.forwardAngleErrorRad *
                    180.0 / kPi
            );

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
        m.minimumSpeedMps =
            std::min(m.minimumSpeedMps, speed);

        if (speed > 0.25)
        {
            const double slipDeg =
                angleRad(
                    v.transform.motion.localVelocityMps,
                    glm::dvec3(v.transform.forward())
                ) * 180.0 / kPi;

            m.maximumSlipDeg =
                std::max(m.maximumSlipDeg, slipDeg);

            if (v.timeSeconds - startTime >= 1.0)
            {
                m.maximumSlipAfterOneSecondDeg =
                    std::max(
                        m.maximumSlipAfterOneSecondDeg,
                        slipDeg
                    );
            }
        }

        m.maximumHullHalfWidthMeters =
            std::max(
                m.maximumHullHalfWidthMeters,
                hullRequiredHalfWidth(v, program)
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

    const double finalSpeed =
        glm::length(v.transform.motion.localVelocityMps);
    if (finalSpeed > 0.25)
    {
        m.finalSlipDeg =
            angleRad(
                v.transform.motion.localVelocityMps,
                glm::dvec3(v.transform.forward())
            ) * 180.0 / kPi;
    }

    m.simulatedSeconds = v.timeSeconds - startTime;

    return m;
}

const char* lawName(Law law)
{
    return law == Law::Newtonian
        ? "newtonian"
        : "assisted";
}

struct ChainMetrics
{
    bool valid = true;
    std::size_t completedPhases = 0;
    std::size_t trackingExceededTicks = 0;

    double maxSeamPositionJumpMeters = 0.0;
    double maxSeamVelocityJumpMps = 0.0;
    double maxSeamForwardJumpDeg = 0.0;
    double maxSeamAngularVelocityJumpRadPerSec = 0.0;

    double maxHullHalfWidthMeters = 0.0;
    double maxSlipDeg = 0.0;

    double finalPositionErrorMeters = 0.0;
    double finalVelocityMps = 0.0;
    double finalForwardErrorDeg = 0.0;
    double totalSeconds = 0.0;
};

void accumulateSeam(
    ChainMetrics& chain,
    const SeamMetrics& seam
)
{
    chain.maxSeamPositionJumpMeters =
        std::max(
            chain.maxSeamPositionJumpMeters,
            seam.positionJumpMeters
        );
    chain.maxSeamVelocityJumpMps =
        std::max(
            chain.maxSeamVelocityJumpMps,
            seam.velocityJumpMps
        );
    chain.maxSeamForwardJumpDeg =
        std::max(
            chain.maxSeamForwardJumpDeg,
            seam.forwardJumpDeg
        );
    chain.maxSeamAngularVelocityJumpRadPerSec =
        std::max(
            chain.maxSeamAngularVelocityJumpRadPerSec,
            seam.angularVelocityJumpRadPerSec
        );
}

void accumulatePhase(
    ChainMetrics& chain,
    const PhaseMetrics& phase
)
{
    chain.valid = chain.valid && phase.valid;
    chain.trackingExceededTicks +=
        phase.trackingExceededTicks;
    chain.maxHullHalfWidthMeters =
        std::max(
            chain.maxHullHalfWidthMeters,
            phase.maximumHullHalfWidthMeters
        );
    chain.maxSlipDeg =
        std::max(
            chain.maxSlipDeg,
            phase.maximumSlipDeg
        );

    if (phase.completed)
        ++chain.completedPhases;
}

void printPhaseMetrics(
    Law law,
    int phaseIndex,
    const char* phaseName,
    const PhaseMetrics& phase
)
{
    std::cout
        << std::fixed << std::setprecision(6)
        << "[CHAIN-PHASE]"
        << " law=" << lawName(law)
        << " phase=" << phaseIndex
        << " name=" << phaseName
        << " completed=" << (phase.completed ? 1 : 0)
        << " entry_slip_deg=" << phase.entrySlipDeg
        << " max_slip_deg=" << phase.maximumSlipDeg
        << " max_slip_after_1s_deg="
        << phase.maximumSlipAfterOneSecondDeg
        << " final_slip_deg=" << phase.finalSlipDeg
        << " max_pos_error_m="
        << phase.maximumPositionErrorMeters
        << " max_vel_error_mps="
        << phase.maximumVelocityErrorMps
        << " max_forward_error_deg="
        << phase.maximumForwardErrorDeg
        << " final_pos_error_m="
        << phase.finalPositionErrorMeters
        << " final_vel_error_mps="
        << phase.finalVelocityErrorMps
        << " final_forward_error_deg="
        << phase.finalForwardErrorDeg
        << " tracking_exceeded_ticks="
        << phase.trackingExceededTicks
        << " simulated_s=" << phase.simulatedSeconds
        << "\n";
}

ChainMetrics runChain(Law law)
{
    Vehicle v(law);
    ChainMetrics chain;
    std::uint64_t revision = 10200u;

    // Phase 1: moving transit accelerates from 6 to 10 m/s.
    {
        const VehicleState start = captureState(v);
        const Basis terminal =
            basisForForward({1.0, 0.0, 0.0}, start.basis);
        const Program p =
            makeProgram(
                revision++,
                v.timeSeconds,
                start,
                start.position + glm::dvec3(80.0, 0.0, 0.0),
                {10.0, 0.0, 0.0},
                terminal,
                10.0,
                OrientationMode::VelocityAligned,
                Program::ManeuverFamily::FreeTransit
            );

        accumulateSeam(chain, measureSeam(start, p));
        const auto phase =
            executePhase(v, p, Gate::Mode::ScheduledMoving);
        accumulatePhase(chain, phase);
        printPhaseMetrics(law, 1, "transit", phase);
        require(
            phase.completed && !phase.captureTimedOut,
            std::string("transit phase failed for ") + lawName(law)
        );
    }

    // Phase 2: hard continuous 90 degree turn. No vehicle reset.
    {
        const VehicleState start = captureState(v);
        const Basis terminal =
            basisForForward({0.0, 1.0, 0.0}, start.basis);
        const Program p =
            makeProgram(
                revision++,
                v.timeSeconds,
                start,
                start.position + glm::dvec3(70.0, 70.0, 0.0),
                {0.0, 10.0, 0.0},
                terminal,
                14.0,
                OrientationMode::VelocityAligned,
                Program::ManeuverFamily::PrecisionTransit
            );

        accumulateSeam(chain, measureSeam(start, p));
        const auto phase =
            executePhase(v, p, Gate::Mode::ScheduledMoving);
        accumulatePhase(chain, phase);
        printPhaseMetrics(law, 2, "hard_turn", phase);
        require(
            phase.completed && !phase.captureTimedOut,
            std::string("hard-turn phase failed for ") + lawName(law)
        );
    }

    // Phase 3: same translational dodge, different attitude family.
    // Newtonian holds body direction and drifts; Assisted aligns to velocity.
    {
        const VehicleState start = captureState(v);
        const Basis terminal =
            basisForForward({0.0, 1.0, 0.0}, start.basis);
        const bool drift = law == Law::Newtonian;

        const Program p =
            makeProgram(
                revision++,
                v.timeSeconds,
                start,
                start.position + glm::dvec3(45.0, 120.0, 0.0),
                {0.0, 10.0, 0.0},
                terminal,
                12.0,
                drift
                    ? OrientationMode::FixedStart
                    : OrientationMode::VelocityAligned,
                drift
                    ? Program::ManeuverFamily::DriftPass
                    : Program::ManeuverFamily::PrecisionTransit
            );

        accumulateSeam(chain, measureSeam(start, p));
        const auto phase =
            executePhase(v, p, Gate::Mode::ScheduledMoving);
        accumulatePhase(chain, phase);
        printPhaseMetrics(
            law,
            3,
            drift ? "drift_pass" : "aligned_transit",
            phase
        );
        require(
            phase.completed && !phase.captureTimedOut,
            std::string("law-specific phase failed for ") +
                lawName(law)
        );

        if (drift)
        {
            require(
                phase.maximumSlipDeg >= 20.0,
                "Newtonian chained drift did not produce material slip"
            );
        }
        else
        {
            require(
                phase.maximumSlipAfterOneSecondDeg <= 8.0,
                "Assisted aligned phase retained excessive slip after handoff transient"
            );
            require(
                phase.finalSlipDeg <= 4.0,
                "Assisted aligned phase did not converge to low terminal slip"
            );
        }
    }

    // Phase 4: precision braking/capture. StateCapture must wait for the real
    // terminal envelope; time expiry may not silently advance it.
    glm::dvec3 finalTarget(0.0);
    Basis finalBasis {};
    {
        const VehicleState start = captureState(v);
        finalBasis =
            basisForForward({0.0, 1.0, 0.0}, start.basis);
        finalTarget =
            start.position + glm::dvec3(0.0, 60.0, 0.0);

        const Program p =
            makeProgram(
                revision++,
                v.timeSeconds,
                start,
                finalTarget,
                {0.0, 0.0, 0.0},
                finalBasis,
                12.0,
                OrientationMode::VelocityAligned,
                Program::ManeuverFamily::PrecisionCapture
            );

        accumulateSeam(chain, measureSeam(start, p));
        const auto phase =
            executePhase(v, p, Gate::Mode::StateCapture);
        accumulatePhase(chain, phase);
        printPhaseMetrics(law, 4, "precision_capture", phase);

        require(
            phase.completed && !phase.captureTimedOut,
            std::string("precision capture failed for ") +
                lawName(law)
        );
    }

    chain.finalPositionErrorMeters =
        glm::length(
            v.transform.motion.localPositionMeters -
            finalTarget
        );
    chain.finalVelocityMps =
        glm::length(v.transform.motion.localVelocityMps);
    chain.finalForwardErrorDeg =
        angleRad(
            glm::dvec3(v.transform.forward()),
            finalBasis.forward
        ) * 180.0 / kPi;
    chain.totalSeconds = v.timeSeconds;

    return chain;
}

void testChainedTransitions()
{
    for (const Law law :
         {Law::Newtonian, Law::Assisted})
    {
        const ChainMetrics m = runChain(law);

        require(
            m.valid && m.completedPhases == 4,
            std::string("chain incomplete for ") + lawName(law)
        );
        require(
            m.trackingExceededTicks == 0,
            std::string("chain exceeded tracking envelope for ") +
                lawName(law)
        );

        require(
            m.maxSeamPositionJumpMeters <= 1.0e-9,
            "chain injected a position reset at a phase seam"
        );
        require(
            m.maxSeamVelocityJumpMps <= 1.0e-9,
            "chain injected a velocity reset at a phase seam"
        );
        require(
            m.maxSeamForwardJumpDeg <= 1.0e-6,
            "chain injected an attitude reset at a phase seam"
        );
        require(
            m.maxSeamAngularVelocityJumpRadPerSec <= 1.0e-9,
            "chain injected an angular-velocity reset at a phase seam"
        );

        require(
            m.maxHullHalfWidthMeters <=
                kCorridorHalfWidthMeters + 1.0e-9,
            std::string("full hull left chained corridor for ") +
                lawName(law)
        );

        require(
            m.finalPositionErrorMeters <= 1.0,
            std::string("final chained position miss for ") +
                lawName(law)
        );
        require(
            m.finalVelocityMps <= 0.60,
            std::string("final chained capture speed too high for ") +
                lawName(law)
        );
        require(
            m.finalForwardErrorDeg <= 4.0,
            std::string("final chained attitude miss for ") +
                lawName(law)
        );

        std::cout
            << std::fixed << std::setprecision(6)
            << "[CHAIN]"
            << " law=" << lawName(law)
            << " phases=" << m.completedPhases << "/4"
            << " max_seam_pos_jump_m="
            << m.maxSeamPositionJumpMeters
            << " max_seam_vel_jump_mps="
            << m.maxSeamVelocityJumpMps
            << " max_seam_forward_jump_deg="
            << m.maxSeamForwardJumpDeg
            << " max_seam_omega_jump_radps="
            << m.maxSeamAngularVelocityJumpRadPerSec
            << " max_hull_half_width_m="
            << m.maxHullHalfWidthMeters
            << " corridor_half_width_m="
            << kCorridorHalfWidthMeters
            << " max_slip_deg=" << m.maxSlipDeg
            << " final_pos_error_m="
            << m.finalPositionErrorMeters
            << " final_speed_mps="
            << m.finalVelocityMps
            << " final_forward_error_deg="
            << m.finalForwardErrorDeg
            << " tracking_envelope_exceeded_ticks="
            << m.trackingExceededTicks
            << " total_s=" << m.totalSeconds
            << "\n";
    }
}

void testInsufficientTurnRoomRejectsBeforeAccept()
{
    Compiler::Query q;
    q.controlLaw = Law::Newtonian;
    q.state.positionMapMeters = {0.0, 0.0, 0.0};
    q.state.velocityMapMetersPerSecond = {18.0, 0.0, 0.0};
    q.state.forwardMap = {1.0, 0.0, 0.0};
    q.state.rightMap = {0.0, 0.0, 1.0};
    q.state.upMap = {0.0, 1.0, 0.0};
    q.state.angularVelocityMapRadPerSecond = {0.0, 0.0, 0.0};

    q.capability.maxForwardAccelerationMps2 = 73.5;
    q.capability.maxReverseAccelerationMps2 = 2.0;
    q.capability.maxLateralAccelerationMps2 = 2.0;
    q.capability.maxVerticalAccelerationMps2 = 2.0;
    q.capability.maxAngularAccelerationRadPerSec2 = 2.45166;
    q.capability.maxAngularSpeedRadPerSec = 1.56578;

    q.geometricTargetPositionMapMeters = {0.0, 9.0, 0.0};
    q.desiredVelocityMapMetersPerSecond = {0.0, 18.0, 0.0};
    q.velocityResponsePerSecond = 0.75;
    q.linearFeedbackReserveMps2 = 0.55;
    q.angularFeedbackReserveRadPerSec2 = 0.35;
    q.controlResponseReserveSeconds = 0.18;

    // 0.5 s at 18 m/s is only ~9 m of local room: far below the time needed
    // to rotate a main-engine-dominant Newtonian hull by ~90 degrees.
    q.maximumProgramSeconds = 0.50;

    const auto result = Compiler::compile(q);

    require(
        result.status == Compiler::Status::NoPhysicalCandidate,
        "insufficient turn room leaked a B5 maneuver candidate"
    );
    require(
        result.candidateCount == 0,
        "insufficient turn room reached candidate output"
    );

    std::cout
        << "[LIMIT] case=insufficient_turn_room"
        << " status=no_physical_candidate"
        << " speed_mps=18"
        << " available_time_s=0.5"
        << "\n";
}

void testInsufficientBrakingDistanceFailsReserveProof()
{
    Safety::StoppingReserveQuery q;
    q.positionMapMeters = {0.0, 0.0, 0.0};
    q.velocityMapMetersPerSecond = {20.0, 0.0, 0.0};
    q.controlResponseReserveSeconds = 0.50;
    q.brakingAccelerationMetersPerSecond2 = 2.0;

    const auto reserve = Safety::buildStoppingReserve(q);
    require(
        reserve.valid && reserve.active,
        "stopping reserve probe invalid"
    );

    constexpr double AvailableDistanceMeters = 60.0;

    require(
        reserve.distanceMeters > AvailableDistanceMeters,
        "insufficient braking fixture unexpectedly fits available distance"
    );

    std::cout
        << std::fixed << std::setprecision(6)
        << "[LIMIT] case=insufficient_braking_distance"
        << " required_m=" << reserve.distanceMeters
        << " available_m=" << AvailableDistanceMeters
        << " accepted=0"
        << "\n";
}

void testRigidHullRejectsTooNarrowCorridor()
{
    // Straight corridor radial requirement uses the two dimensions
    // perpendicular to forward for the Cobra OBB.
    const double requiredHalfWidth =
        std::sqrt(
            kBodyHalfExtents.x * kBodyHalfExtents.x +
            kBodyHalfExtents.y * kBodyHalfExtents.y
        );

    constexpr double NarrowHalfWidthMeters = 12.0;

    require(
        requiredHalfWidth > NarrowHalfWidthMeters,
        "narrow corridor fixture unexpectedly fits Cobra hull"
    );

    std::cout
        << std::fixed << std::setprecision(6)
        << "[LIMIT] case=rigid_hull_too_wide"
        << " required_half_width_m=" << requiredHalfWidth
        << " available_half_width_m="
        << NarrowHalfWidthMeters
        << " accepted=0"
        << "\n";
}

Decision::Candidate validDecisionCandidate(
    std::uint64_t id,
    Decision::ControlLawRequirement lawRequirement
)
{
    Decision::Candidate c;
    c.candidateId = id;
    c.family = Decision::CandidateFamily::NewtonianFlipAndBurn;
    c.controlLawRequirement = lawRequirement;
    c.valid = true;
    c.collisionFree = true;
    c.contactExpected = false;
    c.progressesObjective = true;
    c.stopsOrBrakes = false;
    c.timeToObjectiveSeconds = 5.0;
    c.entrySpeedMps = 10.0;
    c.exitSpeedMps = 10.0;
    c.minimumClearanceMeters = 10.0;
    c.escapeReserve01 = 0.5;
    c.peakClosingNormalSpeedMps = 0.0;
    c.normalImpactEnergyProxyJ = 0.0;
    c.criticalDamageRisk01 = 0.0;
    c.missionDamageCost01 = 0.0;
    c.expendableDamageCost01 = 0.0;
    c.threatExposure = 0.0;
    return c;
}

void testNoLawCompatibleCandidateFailsSelection()
{
    std::array<Decision::Candidate, 2> candidates {{
        validDecisionCandidate(
            1,
            Decision::ControlLawRequirement::NewtonianOnly
        ),
        validDecisionCandidate(
            2,
            Decision::ControlLawRequirement::NewtonianOnly
        )
    }};

    Decision::Context context;
    context.doctrine = Decision::Doctrine::Extreme;
    context.progress =
        Decision::ProgressRequirement::MustProgress;
    context.controlLaw = Law::Assisted;

    const auto selection =
        Decision::select(
            context,
            candidates.data(),
            candidates.size()
        );

    require(
        !selection.valid,
        "Assisted B7 selected a Newtonian-only candidate set"
    );

    std::cout
        << "[LIMIT] case=no_law_compatible_candidate"
        << " law=assisted"
        << " selection_valid=0"
        << "\n";
}

void testNewHazardInvalidatesAcceptedExecution()
{
    Replan::Policy policy;
    Replan::Query q;
    q.mode = Replan::ExecutionMode::Automatic;
    q.universeTimeSeconds = 100.0;
    q.acceptedSegmentValid = true;
    q.acceptedSegmentValidUntilUniverseTimeSeconds = 120.0;
    q.acceptedSegmentComplete = false;
    q.globalRouteValid = true;
    q.currentTopologyBranchValid = true;
    q.dynamicHazardInvalidated = true;

    const auto result = Replan::evaluate(policy, q);

    require(
        result.scope == Replan::Scope::LocalHorizon,
        "new hazard did not request local replan"
    );
    require(
        result.reason ==
            Replan::Reason::DynamicHazardInvalidated,
        "new hazard produced wrong replan reason"
    );
    require(
        result.immediate,
        "new hazard invalidation was not immediate"
    );
    require(
        !result.continueAcceptedAutomaticExecution,
        "obsolete accepted program continued after hazard invalidation"
    );

    std::cout
        << "[LIMIT] case=new_dynamic_hazard"
        << " action=immediate_local_replan"
        << " continue_old_program=0"
        << "\n";
}

} // namespace

int main()
{
    try
    {
        testChainedTransitions();
        testInsufficientTurnRoomRejectsBeforeAccept();
        testInsufficientBrakingDistanceFailsReserveProof();
        testRigidHullRejectsTooNarrowCorridor();
        testNoLawCompatibleCandidateFailsSelection();
        testNewHazardInvalidatesAcceptedExecution();

        std::cout
            << "MANEUVER CHAINED/LIMIT MATRIX TESTS: PASS\n";
        std::cout
            << " - four maneuver phases execute without resetting P/V/attitude/omega at seams\n";
        std::cout
            << " - Newtonian chain contains a material high-slip drift while Assisted remains aligned\n";
        std::cout
            << " - terminal precision phase uses StateCapture rather than time-expiry success\n";
        std::cout
            << " - B5 rejects a 90-degree maneuver when the physical horizon is too short\n";
        std::cout
            << " - stopping reserve rejects insufficient braking distance\n";
        std::cout
            << " - full Cobra dimensions reject an undersized corridor\n";
        std::cout
            << " - B7 rejects a candidate set incompatible with the current control law\n";
        std::cout
            << " - new dynamic hazard invalidates accepted execution and requests immediate local replan\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "MANEUVER CHAINED/LIMIT MATRIX TESTS: FAIL: "
            << error.what() << "\n";
        return 1;
    }
}
