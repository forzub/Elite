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
constexpr double kCorridorHalfWidthMeters = 5.0;
constexpr std::uint64_t kObjectiveRevision = 100u;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

bool finite(double value)
{
    return std::isfinite(value);
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

ShipParams labShipParams()
{
    ShipParams p {};
    p.maxPitchRate = 1.0f;
    p.maxYawRate = 1.0f;
    p.maxRollRate = 1.0f;
    p.angularAccel = 1.5f;
    p.angularDamping = 0.0f;

    p.maxCombatSpeed = 300.0f;
    p.maxCruiseSpeed = 1000.0f;
    p.throttleAccel = 1.0f;

    p.autoLevelStrength = 0.0f;
    p.strafeAccel = 2.0f;
    p.strafeDamping = 1.0f;
    p.maxStrafeSpeed = 1000.0f;
    p.manoeuvreThrusterAccel = 2.0f;
    p.manoeuvreGasUsePerSecond = 0.0f;
    p.manoeuvreGasRechargePerSecond = 0.0f;

    p.maxGs = 8.0f;
    p.maxLinearGs = 7.5f;
    p.turnRadius = 50.0f;
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
    profile.execution.deterministicSeed = 0xA11CE001ull;
    return profile;
}

Bridge::PilotSkillProfile competentProfile()
{
    Bridge::PilotSkillProfile profile;

    // Mirrors the current production NpcAiSystem baseline.
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
    profile.execution.deterministicSeed = 0xC0A1E7E5ull;
    profile.execution.emergencyResponseThreshold01 = 0.75;
    profile.execution.emergencyReactionDelayScale = 0.35;

    profile.policy.anticipationSeconds = 2.0;
    profile.policy.riskPreference01 = 0.45;
    profile.policy.comfortPreference01 = 0.65;
    return profile;
}

Bridge::PilotSkillProfile rookieProfile()
{
    Bridge::PilotSkillProfile profile;

    // Deliberately worse than production baseline, but still a valid,
    // deterministic pilot rather than random test corruption.
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
    profile.execution.deterministicSeed = 0xBADC0DE5ull;
    profile.execution.emergencyResponseThreshold01 = 0.80;
    profile.execution.emergencyReactionDelayScale = 0.50;

    profile.policy.anticipationSeconds = 1.0;
    profile.policy.riskPreference01 = 0.55;
    profile.policy.comfortPreference01 = 0.45;
    return profile;
}

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
    glm::dmat3 matrix(1.0);
    matrix[0] = basis.right;
    matrix[1] = basis.up;
    matrix[2] = -basis.forward;
    return glm::normalize(glm::quat_cast(matrix));
}

Basis basisForQuaternion(const glm::dquat& q)
{
    const glm::dmat3 matrix = glm::mat3_cast(glm::normalize(q));
    return {
        glm::normalize(-matrix[2]),
        glm::normalize(matrix[0]),
        glm::normalize(matrix[1])
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
    double duration
)
{
    p.valid = true;
    p.revision = revision;
    p.objectiveRevision = kObjectiveRevision;
    p.acceptedAtUniverseTimeSeconds = acceptedAt;
    p.validUntilUniverseTimeSeconds = acceptedAt + duration + 20.0;
    p.completionTriggersReplan = true;

    p.terminalTolerance.positionMeters = 0.85;
    p.terminalTolerance.linearVelocityMps = 0.40;
    p.terminalTolerance.forwardAngleRad = 0.025;
    p.terminalTolerance.angularVelocityRadPerSec = 0.04;

    p.tracking.positionErrorMeters = 12.0;
    p.tracking.linearVelocityErrorMps = 6.0;
    p.tracking.forwardAngleErrorRad = 0.65;
    p.tracking.angularVelocityErrorRadPerSec = 0.65;
    p.tracking.linearFeedbackReserveMps2 = 0.45;
    p.tracking.angularFeedbackReserveRadPerSec2 = 0.25;

    p.capability.revision = 1;
    p.capability.maxForwardAccelerationMetersPerSec2 = 73.5;
    p.capability.maxReverseAccelerationMetersPerSec2 = 2.0;
    p.capability.maxLateralAccelerationMetersPerSec2 = 2.0;
    p.capability.maxVerticalAccelerationMetersPerSec2 = 2.0;
    p.capability.maxAngularAccelerationRadPerSec2 = 1.5;
    p.capability.maxAngularSpeedRadPerSec = 1.0;
}

double lineDurationSeconds(double distanceMeters)
{
    // Quintic max |d2s/du2| is about 5.774. Targeting ~1.10 m/s2
    // feed-forward keeps stop-to-stop braking comfortably below 2 m/s2 RCS.
    return std::max(
        14.0,
        std::sqrt(distanceMeters * 5.774 / 1.10)
    );
}

Program makeLineProgram(
    std::uint64_t revision,
    double acceptedAt,
    const glm::dvec3& start,
    const glm::dvec3& end,
    const Basis& basis
)
{
    const glm::dvec3 delta = end - start;
    const double distance = glm::length(delta);
    const double duration = lineDurationSeconds(distance);

    Program p;
    fillCommon(p, revision, acceptedAt, duration);
    p.family = Program::ManeuverFamily::Trim;
    p.sampleCount = static_cast<std::uint8_t>(Program::kMaxSamples);

    const double denom =
        static_cast<double>(Program::kMaxSamples - 1);

    for (std::size_t i = 0; i < Program::kMaxSamples; ++i)
    {
        const double u = static_cast<double>(i) / denom;
        auto& sample = p.samples[i];

        sample.timeOffsetSeconds = duration * u;
        sample.positionMapMeters = start + delta * smooth5(u);
        sample.velocityMapMetersPerSecond =
            delta * (smooth5d1(u) / duration);
        sample.linearAccelerationFeedForwardMapMps2 =
            delta * (smooth5d2(u) / (duration * duration));

        sample.forwardMap = basis.forward;
        sample.rightMap = basis.right;
        sample.upMap = basis.up;
        sample.angularVelocityMapRadPerSecond = {0.0, 0.0, 0.0};
        sample.angularAccelerationFeedForwardMapRadPerSec2 =
            {0.0, 0.0, 0.0};
    }

    return p;
}

Program makeRotationProgram(
    std::uint64_t revision,
    double acceptedAt,
    const glm::dvec3& position,
    const Basis& startBasis,
    const Basis& targetBasis
)
{
    const glm::dquat startQ = quaternionForBasis(startBasis);
    glm::dquat targetQ = quaternionForBasis(targetBasis);

    if (glm::dot(startQ, targetQ) < 0.0)
        targetQ = -targetQ;

    glm::dquat delta =
        glm::normalize(targetQ * glm::inverse(startQ));
    if (delta.w < 0.0)
        delta = -delta;

    const double w = std::clamp(delta.w, -1.0, 1.0);
    const double angle = 2.0 * std::acos(w);
    const double sinHalf =
        std::sqrt(std::max(0.0, 1.0 - w * w));

    glm::dvec3 axis(0.0, 1.0, 0.0);
    if (sinHalf > 1.0e-8)
    {
        axis = glm::normalize(
            glm::dvec3(delta.x, delta.y, delta.z) / sinHalf
        );
    }

    // Keep angular profile inside the same conservative quintic bounds used
    // by the B5 lead-rotate family, with extra room for low-skill tracking.
    const double duration = std::max(
        3.5,
        std::max(
            angle > 1.0e-9 ? 1.875 * angle / 0.65 : 0.0,
            angle > 1.0e-9 ? std::sqrt(6.0 * angle / 0.90) : 0.0
        )
    );

    Program p;
    fillCommon(p, revision, acceptedAt, duration);
    p.family = Program::ManeuverFamily::LeadRotateMainBurn;
    p.sampleCount = static_cast<std::uint8_t>(Program::kMaxSamples);

    const double denom =
        static_cast<double>(Program::kMaxSamples - 1);

    for (std::size_t i = 0; i < Program::kMaxSamples; ++i)
    {
        const double u = static_cast<double>(i) / denom;
        const double shaped = smooth5(u);
        const glm::dquat q =
            glm::normalize(
                glm::angleAxis(angle * shaped, axis) * startQ
            );
        const Basis basis = basisForQuaternion(q);

        auto& sample = p.samples[i];
        sample.timeOffsetSeconds = duration * u;
        sample.positionMapMeters = position;
        sample.velocityMapMetersPerSecond = {0.0, 0.0, 0.0};
        sample.linearAccelerationFeedForwardMapMps2 = {0.0, 0.0, 0.0};

        sample.forwardMap = basis.forward;
        sample.rightMap = basis.right;
        sample.upMap = basis.up;

        sample.angularVelocityMapRadPerSecond =
            axis * (angle * smooth5d1(u) / duration);
        sample.angularAccelerationFeedForwardMapRadPerSec2 =
            axis * (
                angle * smooth5d2(u) /
                (duration * duration)
            );
    }

    return p;
}

struct Vehicle
{
    ShipTransform transform {};
    ShipParams params = labShipParams();
    WorldParams world {};
    game::navigation::KinematicFrame frame {};
    Bridge bridge;
    double timeSeconds = 0.0;

    Vehicle(
        const Bridge::PilotSkillProfile& pilot,
        Law law,
        const Basis& initialBasis
    )
        : bridge(pilot)
    {
        frame.systemId = 1;
        frame.frameId = "maneuver-corridor-matrix";
        frame.originMeters = {0.0, 0.0, 0.0};
        frame.localToWorldBasis = glm::dmat3(1.0);
        frame.valid = true;

        transform.motion.mode =
            game::navigation::MotionMode::HubTactical;
        transform.motion.systemId = 1;
        transform.motion.travelFrame = frame;
        transform.motion.localControlLaw = law;
        transform.motion.localPositionMeters = {0.0, 0.0, 0.0};
        transform.motion.localVelocityMps = {0.0, 0.0, 0.0};
        transform.setWorldPositionMeters({0.0, 0.0, 0.0});
        setTransformBasis(transform, initialBasis);

        Bridge::Intent initial;
        initial.revision = 1;
        initial.targetRevision = 0;
        require(
            bridge.reset(0.0, initial),
            "corridor matrix bridge reset failed"
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

    const double t = std::clamp(
        glm::dot(p - a, ab) / ab2,
        0.0,
        1.0
    );
    return glm::length(p - (a + ab * t));
}

template <std::size_t N>
double pointToPolylineDistance(
    const glm::dvec3& p,
    const std::array<glm::dvec3, N>& points
)
{
    double best = std::numeric_limits<double>::infinity();
    for (std::size_t i = 1; i < N; ++i)
    {
        best = std::min(
            best,
            pointToSegmentDistance(p, points[i - 1], points[i])
        );
    }
    return best;
}

struct MatrixMetrics
{
    bool valid = true;
    bool completed = true;

    std::size_t completedTranslationLegs = 0;
    std::size_t completedRotations = 0;
    std::size_t trackingEnvelopeExceededTicks = 0;

    double finalPositionErrorMeters = 0.0;
    double finalSpeedMps = 0.0;
    double maximumRouteCrossTrackMeters = 0.0;
    double maximumActiveLegCrossTrackMeters = 0.0;
    double maximumCorridorViolationMeters = 0.0;
    double maximumWaypointErrorMeters = 0.0;
    double maximumOvershootMeters = 0.0;
    double maximumForwardAngleErrorRad = 0.0;
    double simulatedSeconds = 0.0;
};

using RoutePoints = std::array<glm::dvec3, 5>;

struct ProgramRunResult
{
    bool valid = true;
    bool completed = false;
};

ProgramRunResult runProgram(
    Vehicle& v,
    const Program& program,
    const RoutePoints& route,
    MatrixMetrics& metrics,
    const glm::dvec3* activeLegStart,
    const glm::dvec3* activeLegEnd,
    double extraSeconds
)
{
    const double endTime =
        program.acceptedAtUniverseTimeSeconds +
        program.samples[program.sampleCount - 1].timeOffsetSeconds;
    const double hardStop = endTime + extraSeconds;

    while (v.timeSeconds < hardStop - 1.0e-9)
    {
        const glm::dvec3 pos =
            v.transform.motion.localPositionMeters;

        const double routeDistance =
            pointToPolylineDistance(pos, route);
        metrics.maximumRouteCrossTrackMeters =
            std::max(
                metrics.maximumRouteCrossTrackMeters,
                routeDistance
            );
        metrics.maximumCorridorViolationMeters =
            std::max(
                metrics.maximumCorridorViolationMeters,
                std::max(
                    0.0,
                    routeDistance - kCorridorHalfWidthMeters
                )
            );

        if (activeLegStart && activeLegEnd)
        {
            metrics.maximumActiveLegCrossTrackMeters =
                std::max(
                    metrics.maximumActiveLegCrossTrackMeters,
                    pointToSegmentDistance(
                        pos,
                        *activeLegStart,
                        *activeLegEnd
                    )
                );

            const glm::dvec3 leg = *activeLegEnd - *activeLegStart;
            const double legLength = glm::length(leg);
            if (legLength > 1.0e-12)
            {
                const glm::dvec3 direction = leg / legLength;
                const double along =
                    glm::dot(pos - *activeLegStart, direction);
                metrics.maximumOvershootMeters =
                    std::max(
                        metrics.maximumOvershootMeters,
                        std::max(0.0, along - legLength)
                    );
            }
        }

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

        metrics.maximumForwardAngleErrorRad =
            std::max(
                metrics.maximumForwardAngleErrorRad,
                follower.forwardAngleErrorRad
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

        const auto after =
            Follower::follow(
                program,
                v.timeSeconds,
                agentState(v)
            );

        if (after.status == Follower::Status::Complete)
            return {true, true};
    }

    return {true, false};
}

const char* lawName(Law law)
{
    switch (law)
    {
    case Law::Newtonian:
        return "newtonian";
    case Law::Assisted:
        return "assisted";
    }
    return "unknown";
}

struct PilotCase
{
    const char* name = "";
    Bridge::PilotSkillProfile profile {};
    bool strictCorridorAcceptance = false;
};

MatrixMetrics runMatrixCase(
    const PilotCase& pilot,
    Law law
)
{
    const RoutePoints route {{
        {0.0, 0.0, 0.0},
        {0.0, 0.0, -120.0},
        {85.0, 35.0, -190.0},
        {25.0, 100.0, -265.0},
        {120.0, 55.0, -340.0}
    }};

    std::array<Basis, 4> legBases {};
    for (std::size_t i = 0; i < legBases.size(); ++i)
    {
        legBases[i] = basisForForward(
            route[i + 1] - route[i]
        );
    }

    Vehicle v(pilot.profile, law, legBases[0]);
    MatrixMetrics metrics;

    std::uint64_t revision = 1000u;

    for (std::size_t legIndex = 0; legIndex < 4; ++legIndex)
    {
        if (legIndex > 0)
        {
            const Program rotate =
                makeRotationProgram(
                    revision++,
                    v.timeSeconds,
                    route[legIndex],
                    legBases[legIndex - 1],
                    legBases[legIndex]
                );

            const auto rotationResult =
                runProgram(
                    v,
                    rotate,
                    route,
                    metrics,
                    nullptr,
                    nullptr,
                    12.0
                );

            if (!rotationResult.valid)
            {
                metrics.valid = false;
                metrics.completed = false;
                break;
            }
            if (!rotationResult.completed)
            {
                metrics.completed = false;
                break;
            }

            ++metrics.completedRotations;
        }

        const Program line =
            makeLineProgram(
                revision++,
                v.timeSeconds,
                route[legIndex],
                route[legIndex + 1],
                legBases[legIndex]
            );

        const auto lineResult =
            runProgram(
                v,
                line,
                route,
                metrics,
                &route[legIndex],
                &route[legIndex + 1],
                16.0
            );

        if (!lineResult.valid)
        {
            metrics.valid = false;
            metrics.completed = false;
            break;
        }
        if (!lineResult.completed)
        {
            metrics.completed = false;
            break;
        }

        ++metrics.completedTranslationLegs;

        const double waypointError =
            glm::length(
                v.transform.motion.localPositionMeters -
                route[legIndex + 1]
            );
        metrics.maximumWaypointErrorMeters =
            std::max(
                metrics.maximumWaypointErrorMeters,
                waypointError
            );
    }

    metrics.finalPositionErrorMeters =
        glm::length(
            v.transform.motion.localPositionMeters -
            route.back()
        );
    metrics.finalSpeedMps =
        glm::length(v.transform.motion.localVelocityMps);
    metrics.simulatedSeconds = v.timeSeconds;

    require(
        finite(metrics.finalPositionErrorMeters) &&
        finite(metrics.finalSpeedMps) &&
        finite(metrics.maximumRouteCrossTrackMeters) &&
        finite(metrics.maximumActiveLegCrossTrackMeters) &&
        finite(metrics.maximumCorridorViolationMeters) &&
        finite(metrics.maximumWaypointErrorMeters) &&
        finite(metrics.maximumOvershootMeters) &&
        finite(metrics.maximumForwardAngleErrorRad) &&
        finite(metrics.simulatedSeconds),
        "corridor matrix produced non-finite metrics"
    );

    std::cout
        << std::fixed << std::setprecision(6)
        << "[CORRIDOR-MATRIX]"
        << " pilot=" << pilot.name
        << " law=" << lawName(law)
        << " valid=" << (metrics.valid ? 1 : 0)
        << " completed=" << (metrics.completed ? 1 : 0)
        << " legs=" << metrics.completedTranslationLegs << "/4"
        << " rotations=" << metrics.completedRotations << "/3"
        << " final_pos_error_m=" << metrics.finalPositionErrorMeters
        << " final_speed_mps=" << metrics.finalSpeedMps
        << " max_route_cross_track_m="
        << metrics.maximumRouteCrossTrackMeters
        << " max_active_leg_cross_track_m="
        << metrics.maximumActiveLegCrossTrackMeters
        << " corridor_half_width_m="
        << kCorridorHalfWidthMeters
        << " max_corridor_violation_m="
        << metrics.maximumCorridorViolationMeters
        << " max_waypoint_error_m="
        << metrics.maximumWaypointErrorMeters
        << " max_overshoot_m="
        << metrics.maximumOvershootMeters
        << " max_forward_angle_error_deg="
        << metrics.maximumForwardAngleErrorRad * 180.0 / kPi
        << " tracking_envelope_exceeded_ticks="
        << metrics.trackingEnvelopeExceededTicks
        << " simulated_s=" << metrics.simulatedSeconds
        << "\n";

    return metrics;
}


struct LawStressMetrics
{
    double finalSpeedMps = 0.0;
    double finalPositionXMeters = 0.0;
    double maximumSpeedMps = 0.0;
};

LawStressMetrics runLawStress(Law law)
{
    const Basis basis {};
    Vehicle v(expertProfile(), law, basis);

    // Make the controlled-speed boundary reachable quickly while preserving
    // the exact production law semantics. The purpose is not to model a
    // specific ship here; it is to prove that the law selector reaches
    // different propulsion integration branches.
    v.params.maxCombatSpeed = 10.0f;
    v.params.maxCruiseSpeed = 10.0f;
    v.params.manoeuvreThrusterAccel = 2.0f;
    v.params.strafeAccel = 2.0f;

    LawStressMetrics metrics;

    constexpr double duration = 8.0;
    const std::uint64_t revision = 9001u;

    while (v.timeSeconds < duration - 1.0e-9)
    {
        game::navigation::NavigationSystemControlIntent intent;
        intent.revision = kObjectiveRevision + 1u;
        intent.targetRevision = revision;
        intent.idealLinearAccelerationSystemMps2 = {2.0, 0.0, 0.0};
        intent.idealAngularAccelerationSystemRadPerSec2 = {0.0, 0.0, 0.0};

        const auto bridgeResult =
            v.bridge.step(
                v.timeSeconds + kDt,
                kDt,
                intent
            );

        require(
            bridgeResult.status == Bridge::PilotExecutor::Status::Ok,
            "law-stress PilotSkill execution failed"
        );

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

        metrics.maximumSpeedMps =
            std::max(
                metrics.maximumSpeedMps,
                glm::length(v.transform.motion.localVelocityMps)
            );
    }

    metrics.finalSpeedMps =
        glm::length(v.transform.motion.localVelocityMps);
    metrics.finalPositionXMeters =
        v.transform.motion.localPositionMeters.x;

    std::cout
        << std::fixed << std::setprecision(6)
        << "[LAW-STRESS]"
        << " law=" << lawName(law)
        << " final_speed_mps=" << metrics.finalSpeedMps
        << " max_speed_mps=" << metrics.maximumSpeedMps
        << " final_x_m=" << metrics.finalPositionXMeters
        << "\n";

    return metrics;
}

void testNewtonianAndAssistedPhysicsAreActuallyDifferent()
{
    const LawStressMetrics newtonian =
        runLawStress(Law::Newtonian);
    const LawStressMetrics assisted =
        runLawStress(Law::Assisted);

    // Newtonian RCS is a real force and may accumulate delta-v beyond the
    // ordinary controlled-speed envelope. Assisted applies that envelope to
    // combined controlled motion.
    require(
        newtonian.finalSpeedMps > 12.0,
        "Newtonian RCS did not accumulate delta-v beyond 10 m/s envelope"
    );
    require(
        assisted.finalSpeedMps <= 10.05,
        "Assisted law failed to enforce 10 m/s controlled-speed envelope"
    );
    require(
        newtonian.finalSpeedMps >
            assisted.finalSpeedMps + 2.0,
        "Newtonian and Assisted remained physically indistinguishable"
    );
}

void testFourLeg3dCorridorAcrossLawsAndPilots()
{
    const std::array<PilotCase, 3> pilots {{
        {"expert", expertProfile(), true},
        {"competent", competentProfile(), false},
        {"rookie", rookieProfile(), false}
    }};

    const std::array<Law, 2> laws {{
        Law::Newtonian,
        Law::Assisted
    }};

    std::size_t diagnosticRows = 0;

    for (const auto& pilot : pilots)
    {
        for (const Law law : laws)
        {
            const MatrixMetrics metrics =
                runMatrixCase(pilot, law);
            ++diagnosticRows;

            require(
                metrics.valid,
                std::string("corridor matrix invalid for ") +
                    pilot.name + "/" + lawName(law)
            );

            // First target-machine run establishes measured envelopes for
            // production/rookie profiles. Expert is the strict control row.
            if (pilot.strictCorridorAcceptance)
            {
                require(
                    metrics.completed,
                    std::string("expert did not complete 3D corridor in ") +
                        lawName(law)
                );
                require(
                    metrics.completedTranslationLegs == 4 &&
                    metrics.completedRotations == 3,
                    std::string("expert missed route phase in ") +
                        lawName(law)
                );
                require(
                    metrics.maximumCorridorViolationMeters <= 1.0e-9,
                    std::string("expert left 5 m corridor in ") +
                        lawName(law)
                );
                require(
                    metrics.finalPositionErrorMeters <= 1.0,
                    std::string("expert final error exceeded 1 m in ") +
                        lawName(law)
                );
                require(
                    metrics.finalSpeedMps <= 0.6,
                    std::string("expert final speed exceeded 0.6 m/s in ") +
                        lawName(law)
                );
            }
        }
    }

    require(
        diagnosticRows == 6,
        "3D corridor matrix did not execute all 2x3 rows"
    );
}

} // namespace

int main()
{
    try
    {
        testNewtonianAndAssistedPhysicsAreActuallyDifferent();
        testFourLeg3dCorridorAcrossLawsAndPilots();

        std::cout << "MANEUVER CORRIDOR MATRIX TESTS: PASS\n";
        std::cout << " - 4 stop-to-stop segments exercise X/Y/Z translation\n";
        std::cout << " - 3 bounded attitude transitions connect the 3D legs\n";
        std::cout << " - 5 m corridor is measured for every physics tick\n";
        std::cout << " - Newtonian and Assisted run the identical accepted route\n";
        std::cout << " - dedicated law-stress proves the two physics laws diverge at the speed envelope\n";
        std::cout << " - expert, production-baseline and rookie PilotSkill profiles are compared\n";
        std::cout << " - expert rows are strict; lower-skill rows establish first measured envelopes\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "MANEUVER CORRIDOR MATRIX TESTS: FAIL: "
            << error.what() << "\n";
        return 1;
    }
}
