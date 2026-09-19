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

namespace
{

using Program = game::navigation::AcceptedManeuverProgram;
using Follower = game::navigation::TrajectoryFollower;
using Bridge = game::navigation::NavigationRuntimeControlBridge;
using Law = game::navigation::LocalFlightControlLaw;

constexpr double kPi = 3.14159265358979323846;
constexpr double kDt = 0.02;
constexpr double kStandardGravityMps2 = 9.80665;

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
    // Canonical Cobra Mk1 logical dimensions from EliteCobraMk1Descriptor:
    // width 26.0 m, height 5.0 m, length 22.2 m.
    glm::dvec3 halfExtentsBodyMeters {13.0, 2.5, 11.1};

    // Explicit physical actuator sources used by this lab.
    double aftMainAccelerationMps2 = 7.5 * kStandardGravityMps2;
    double assistedForeMainAccelerationMps2 =
        7.5 * kStandardGravityMps2;
    double manoeuvreRcsAccelerationMps2 = 2.0;

    // Existing Cobra angular channel represents bounded main-nozzle
    // vectoring / attitude actuation at the rigid-body level.
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
    profile.execution.deterministicSeed = 0xC0B4A001ull;
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
    profile.execution.deterministicSeed = 0xC0B4C002ull;
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
    profile.execution.deterministicSeed = 0xC0B4B003ull;
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
    Law law,
    double totalSeconds
)
{
    p.valid = true;
    p.revision =
        law == Law::Newtonian ? 5001u : 5002u;
    p.objectiveRevision = 5000u;
    p.family =
        law == Law::Newtonian
            ? Program::ManeuverFamily::FlipAndBurn
            : Program::ManeuverFamily::Brake;

    p.acceptedAtUniverseTimeSeconds = 0.0;
    p.validUntilUniverseTimeSeconds = totalSeconds + 12.0;
    p.completionTriggersReplan = true;

    p.terminalTolerance.positionMeters = 0.75;
    p.terminalTolerance.linearVelocityMps = 0.40;
    p.terminalTolerance.forwardAngleRad = 0.035;
    p.terminalTolerance.angularVelocityRadPerSec = 0.05;

    p.tracking.positionErrorMeters = 12.0;
    p.tracking.linearVelocityErrorMps = 6.0;
    p.tracking.forwardAngleErrorRad = 0.70;
    p.tracking.angularVelocityErrorRadPerSec = 0.70;
    p.tracking.linearFeedbackReserveMps2 = 0.50;
    p.tracking.angularFeedbackReserveRadPerSec2 = 0.30;

    p.capability.revision = 1;
    p.capability.maxForwardAccelerationMetersPerSec2 =
        7.5 * kStandardGravityMps2;
    p.capability.maxReverseAccelerationMetersPerSec2 =
        law == Law::Assisted
            ? 7.5 * kStandardGravityMps2
            : 2.0;
    p.capability.maxLateralAccelerationMetersPerSec2 = 2.0;
    p.capability.maxVerticalAccelerationMetersPerSec2 = 2.0;
    p.capability.maxAngularAccelerationRadPerSec2 = 3.0;
    p.capability.maxAngularSpeedRadPerSec = 2.5;
}

Program makeStraightStopProgram(Law law)
{
    constexpr double distanceMeters = 200.0;
    constexpr double commandedMainAccelerationMps2 = 8.0;
    constexpr double coastOrFlipSeconds = 5.0;

    const double burnSeconds =
        0.5 * (
            -coastOrFlipSeconds +
            std::sqrt(
                coastOrFlipSeconds * coastOrFlipSeconds +
                4.0 * distanceMeters /
                    commandedMainAccelerationMps2
            )
        );

    const double peakSpeed =
        commandedMainAccelerationMps2 * burnSeconds;
    const double accelDistance =
        0.5 * commandedMainAccelerationMps2 *
        burnSeconds * burnSeconds;

    const double coastStartPosition = accelDistance;
    const double brakeStartPosition =
        coastStartPosition +
        peakSpeed * coastOrFlipSeconds;

    const double totalSeconds =
        2.0 * burnSeconds + coastOrFlipSeconds;

    Program p;
    fillCommon(p, law, totalSeconds);
    p.sampleCount = static_cast<std::uint8_t>(Program::kMaxSamples);

    const double denom =
        static_cast<double>(Program::kMaxSamples - 1);

    for (std::size_t i = 0; i < Program::kMaxSamples; ++i)
    {
        const double t =
            totalSeconds * static_cast<double>(i) / denom;

        auto& sample = p.samples[i];
        sample.timeOffsetSeconds = t;

        double along = 0.0;
        double speed = 0.0;
        double accelerationAlong = 0.0;
        double flipAngle = 0.0;
        double omega = 0.0;
        double alpha = 0.0;

        if (t < burnSeconds)
        {
            along =
                0.5 * commandedMainAccelerationMps2 * t * t;
            speed = commandedMainAccelerationMps2 * t;
            accelerationAlong = commandedMainAccelerationMps2;
        }
        else if (t < burnSeconds + coastOrFlipSeconds)
        {
            const double coastT = t - burnSeconds;
            along =
                coastStartPosition + peakSpeed * coastT;
            speed = peakSpeed;
            accelerationAlong = 0.0;

            if (law == Law::Newtonian)
            {
                const double u =
                    std::clamp(
                        coastT / coastOrFlipSeconds,
                        0.0,
                        1.0
                    );
                flipAngle = kPi * smooth5(u);
                omega =
                    kPi * smooth5d1(u) /
                    coastOrFlipSeconds;
                alpha =
                    kPi * smooth5d2(u) /
                    (coastOrFlipSeconds *
                     coastOrFlipSeconds);
            }
        }
        else
        {
            const double brakeT =
                std::min(
                    burnSeconds,
                    t - burnSeconds - coastOrFlipSeconds
                );

            along =
                brakeStartPosition +
                peakSpeed * brakeT -
                0.5 * commandedMainAccelerationMps2 *
                    brakeT * brakeT;
            speed =
                std::max(
                    0.0,
                    peakSpeed -
                    commandedMainAccelerationMps2 * brakeT
                );
            accelerationAlong =
                brakeT < burnSeconds - 1.0e-9
                    ? -commandedMainAccelerationMps2
                    : 0.0;

            if (law == Law::Newtonian)
                flipAngle = kPi;
        }

        sample.positionMapMeters = {0.0, 0.0, -along};
        sample.velocityMapMetersPerSecond = {0.0, 0.0, -speed};
        sample.linearAccelerationFeedForwardMapMps2 =
            {0.0, 0.0, -accelerationAlong};

        const Basis basis =
            law == Law::Newtonian
                ? yawBasis(flipAngle)
                : Basis {};

        sample.forwardMap = basis.forward;
        sample.rightMap = basis.right;
        sample.upMap = basis.up;
        sample.angularVelocityMapRadPerSecond =
            {0.0, omega, 0.0};
        sample.angularAccelerationFeedForwardMapRadPerSec2 =
            {0.0, alpha, 0.0};
    }

    // Pin the exact terminal reference independently of interpolation details.
    auto& last = p.samples[p.sampleCount - 1];
    last.positionMapMeters = {0.0, 0.0, -distanceMeters};
    last.velocityMapMetersPerSecond = {0.0, 0.0, 0.0};
    last.linearAccelerationFeedForwardMapMps2 = {0.0, 0.0, 0.0};

    if (law == Law::Newtonian)
    {
        const Basis backward = yawBasis(kPi);
        last.forwardMap = backward.forward;
        last.rightMap = backward.right;
        last.upMap = backward.up;
    }

    last.angularVelocityMapRadPerSecond = {0.0, 0.0, 0.0};
    last.angularAccelerationFeedForwardMapRadPerSec2 =
        {0.0, 0.0, 0.0};

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
        Law law
    )
        : params(cobraParams(model)),
          bridge(pilot.profile)
    {
        frame.systemId = 1;
        frame.frameId = "rigid-body-corridor-lab";
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
        setTransformBasis(transform, Basis {});

        Bridge::Intent initial;
        initial.revision = 1;
        initial.targetRevision = 0;
        require(
            bridge.reset(0.0, initial),
            "rigid-body corridor bridge reset failed"
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

    // Extend the corridor centerline beyond maneuver endpoints so hull
    // clearance measures transverse envelope rather than artificial spherical
    // end-cap distance at start/finish.
    const glm::dvec3 start(0.0, 0.0, 50.0);
    const glm::dvec3 finish(0.0, 0.0, -250.0);

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
                        pointToSegmentDistance(
                            corner,
                            start,
                            finish
                        )
                    );
            }
        }
    }

    return maximum;
}

struct Metrics
{
    bool completed = false;

    double finalPositionErrorMeters = 0.0;
    double finalSpeedMps = 0.0;
    double finalForwardToRouteDeg = 0.0;

    double maximumCenterCrossTrackMeters = 0.0;
    double maximumHullRequiredHalfWidthMeters = 0.0;
    double maximumTight14ViolationMeters = 0.0;
    double maximumFlipSafe18_5ViolationMeters = 0.0;

    double maximumFlipAngleDeg = 0.0;
    double maximumAngularSpeedRadPerSec = 0.0;

    double peakBrakeAftMainMps2 = 0.0;
    double peakBrakeForeMainMps2 = 0.0;
    double peakBrakeRcsMps2 = 0.0;

    std::size_t trackingEnvelopeExceededTicks = 0;
    double simulatedSeconds = 0.0;
};

Metrics runCase(
    const RigidVehicleModel& model,
    const PilotCase& pilot,
    Law law
)
{
    const Program program = makeStraightStopProgram(law);
    Vehicle v(model, pilot, law);

    const double programEnd =
        program.samples[program.sampleCount - 1].timeOffsetSeconds;
    const double hardStop = programEnd + 10.0;

    // Recompute phase boundary from the same program geometry constants.
    constexpr double distanceMeters = 200.0;
    constexpr double commandedMainAccelerationMps2 = 8.0;
    constexpr double coastOrFlipSeconds = 5.0;
    const double burnSeconds =
        0.5 * (
            -coastOrFlipSeconds +
            std::sqrt(
                coastOrFlipSeconds * coastOrFlipSeconds +
                4.0 * distanceMeters /
                    commandedMainAccelerationMps2
            )
        );
    const double brakeStartSeconds =
        burnSeconds + coastOrFlipSeconds;

    Metrics m;

    const glm::dvec3 routeForward(0.0, 0.0, -1.0);
    const glm::dvec3 start(0.0, 0.0, 0.0);
    const glm::dvec3 finish(0.0, 0.0, -distanceMeters);

    while (v.timeSeconds < hardStop - 1.0e-9)
    {
        const auto follower =
            Follower::follow(
                program,
                v.timeSeconds,
                agentState(v)
            );

        require(
            follower.status != Follower::Status::InvalidInput,
            "rigid-body follower became invalid"
        );

        if (follower.trackingErrorExceeded)
            ++m.trackingEnvelopeExceededTicks;

        const auto bridgeResult =
            v.bridge.step(
                v.timeSeconds + kDt,
                kDt,
                toSystemIntent(follower.intent)
            );

        require(
            bridgeResult.status == Bridge::PilotExecutor::Status::Ok,
            "rigid-body PilotSkill execution failed"
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

        const glm::dvec3 bodyForward(v.transform.forward());

        if (v.timeSeconds >= brakeStartSeconds - 1.0e-9)
        {
            const double longitudinalMain =
                glm::dot(
                    v.transform.motion.mainEngineAccelerationMps2,
                    bodyForward
                );

            m.peakBrakeAftMainMps2 =
                std::max(
                    m.peakBrakeAftMainMps2,
                    std::max(0.0, longitudinalMain)
                );
            m.peakBrakeForeMainMps2 =
                std::max(
                    m.peakBrakeForeMainMps2,
                    std::max(0.0, -longitudinalMain)
                );
            m.peakBrakeRcsMps2 =
                std::max(
                    m.peakBrakeRcsMps2,
                    glm::length(
                        v.transform.motion.manoeuvreAccelerationMps2
                    )
                );
        }

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

        m.maximumCenterCrossTrackMeters =
            std::max(
                m.maximumCenterCrossTrackMeters,
                pointToSegmentDistance(center, start, finish)
            );

        const double hullHalfWidth =
            hullRequiredHalfWidthMeters(v, model);
        m.maximumHullRequiredHalfWidthMeters =
            std::max(
                m.maximumHullRequiredHalfWidthMeters,
                hullHalfWidth
            );
        m.maximumTight14ViolationMeters =
            std::max(
                m.maximumTight14ViolationMeters,
                std::max(0.0, hullHalfWidth - 14.0)
            );
        m.maximumFlipSafe18_5ViolationMeters =
            std::max(
                m.maximumFlipSafe18_5ViolationMeters,
                std::max(0.0, hullHalfWidth - 18.5)
            );

        const double flipAngle =
            angleRad(
                glm::dvec3(v.transform.forward()),
                routeForward
            );
        m.maximumFlipAngleDeg =
            std::max(
                m.maximumFlipAngleDeg,
                flipAngle * 180.0 / kPi
            );

        const double angularSpeed =
            std::sqrt(
                static_cast<double>(v.transform.pitchRate) *
                    static_cast<double>(v.transform.pitchRate) +
                static_cast<double>(v.transform.yawRate) *
                    static_cast<double>(v.transform.yawRate) +
                static_cast<double>(v.transform.rollRate) *
                    static_cast<double>(v.transform.rollRate)
            );
        m.maximumAngularSpeedRadPerSec =
            std::max(
                m.maximumAngularSpeedRadPerSec,
                angularSpeed
            );

        const auto after =
            Follower::follow(
                program,
                v.timeSeconds,
                agentState(v)
            );

        if (after.status == Follower::Status::Complete)
        {
            m.completed = true;
            break;
        }
    }

    m.finalPositionErrorMeters =
        glm::length(
            v.transform.motion.localPositionMeters - finish
        );
    m.finalSpeedMps =
        glm::length(v.transform.motion.localVelocityMps);
    m.finalForwardToRouteDeg =
        angleRad(
            glm::dvec3(v.transform.forward()),
            routeForward
        ) * 180.0 / kPi;
    m.simulatedSeconds = v.timeSeconds;

    std::cout
        << std::fixed << std::setprecision(6)
        << "[RIGID-CORRIDOR]"
        << " pilot=" << pilot.name
        << " law="
        << (law == Law::Newtonian ? "newtonian" : "assisted")
        << " completed=" << (m.completed ? 1 : 0)
        << " final_pos_error_m=" << m.finalPositionErrorMeters
        << " final_speed_mps=" << m.finalSpeedMps
        << " final_forward_to_route_deg=" << m.finalForwardToRouteDeg
        << " max_center_cross_track_m="
        << m.maximumCenterCrossTrackMeters
        << " max_hull_required_half_width_m="
        << m.maximumHullRequiredHalfWidthMeters
        << " tight14_violation_m="
        << m.maximumTight14ViolationMeters
        << " flip_safe18_5_violation_m="
        << m.maximumFlipSafe18_5ViolationMeters
        << " max_flip_angle_deg=" << m.maximumFlipAngleDeg
        << " max_angular_speed_radps="
        << m.maximumAngularSpeedRadPerSec
        << " brake_aft_main_peak_mps2="
        << m.peakBrakeAftMainMps2
        << " brake_fore_main_peak_mps2="
        << m.peakBrakeForeMainMps2
        << " brake_rcs_peak_mps2="
        << m.peakBrakeRcsMps2
        << " tracking_envelope_exceeded_ticks="
        << m.trackingEnvelopeExceededTicks
        << " simulated_s=" << m.simulatedSeconds
        << "\n";

    return m;
}

void testPhysicalRigidBodyCorridor()
{
    const RigidVehicleModel model;

    std::cout
        << std::fixed << std::setprecision(6)
        << "[VEHICLE-MODEL]"
        << " width_m=" << 2.0 * model.halfExtentsBodyMeters.x
        << " height_m=" << 2.0 * model.halfExtentsBodyMeters.y
        << " length_m=" << 2.0 * model.halfExtentsBodyMeters.z
        << " aft_main_mps2=" << model.aftMainAccelerationMps2
        << " assisted_fore_main_mps2="
        << model.assistedForeMainAccelerationMps2
        << " rcs_mps2=" << model.manoeuvreRcsAccelerationMps2
        << " vectoring_angular_accel_radps2="
        << model.vectoringAngularAccelerationRadPerSec2
        << "\n";

    const std::array<PilotCase, 3> pilots {{
        {"expert", expertProfile(), true},
        {"competent", competentProfile(), false},
        {"rookie", rookieProfile(), false}
    }};

    for (const auto& pilot : pilots)
    {
        const Metrics newtonian =
            runCase(model, pilot, Law::Newtonian);
        const Metrics assisted =
            runCase(model, pilot, Law::Assisted);

        if (!pilot.strict)
            continue;

        require(
            newtonian.completed,
            "expert Newtonian rigid-body maneuver did not complete"
        );
        require(
            assisted.completed,
            "expert Assisted rigid-body maneuver did not complete"
        );

        require(
            newtonian.maximumFlipAngleDeg >= 170.0,
            "Newtonian braking never performed the required near-180-degree flip"
        );
        require(
            assisted.maximumFlipAngleDeg <= 5.0,
            "Assisted braking unexpectedly flipped the hull"
        );

        require(
            newtonian.finalForwardToRouteDeg >= 170.0,
            "Newtonian final attitude must remain tail-forward after braking"
        );
        require(
            assisted.finalForwardToRouteDeg <= 5.0,
            "Assisted final attitude must remain nose-forward after reverse thrust"
        );

        require(
            newtonian.peakBrakeAftMainMps2 >= 5.0,
            "Newtonian braking did not use aft main thrust after flip"
        );
        require(
            newtonian.peakBrakeForeMainMps2 <= 1.0e-6,
            "Newtonian braking invented fore/nose main thrust"
        );
        require(
            assisted.peakBrakeForeMainMps2 >= 5.0,
            "Assisted braking did not use fore/nose longitudinal main thrust"
        );

        require(
            assisted.maximumTight14ViolationMeters <= 1.0e-6,
            "Assisted aligned Cobra should fit inside 14 m half-width corridor"
        );
        require(
            newtonian.maximumTight14ViolationMeters >= 2.0,
            "Newtonian Cobra flip should require materially more than 14 m half-width"
        );
        require(
            newtonian.maximumFlipSafe18_5ViolationMeters <= 1.0e-6,
            "18.5 m half-width should contain the expert Newtonian flip envelope"
        );

        require(
            newtonian.maximumHullRequiredHalfWidthMeters >
                assisted.maximumHullRequiredHalfWidthMeters + 2.0,
            "Newtonian flip did not produce the expected wider hull corridor envelope"
        );

        require(
            newtonian.finalPositionErrorMeters <= 1.0 &&
            assisted.finalPositionErrorMeters <= 1.0,
            "expert rigid-body stop missed final position"
        );
        require(
            newtonian.finalSpeedMps <= 0.5 &&
            assisted.finalSpeedMps <= 0.5,
            "expert rigid-body stop retained excessive terminal speed"
        );
    }
}

} // namespace

int main()
{
    try
    {
        testPhysicalRigidBodyCorridor();

        std::cout << "MANEUVER RIGID BODY CORRIDOR TESTS: PASS\n";
        std::cout << " - Cobra dimensions participate in corridor occupancy\n";
        std::cout << " - Newtonian braking is flip + aft-main burn\n";
        std::cout << " - Assisted braking is fore-main reverse thrust without hull flip\n";
        std::cout << " - P/V, full attitude, angular rate and actuator use are measured\n";
        std::cout << " - corridor width is derived from oriented hull corners, not center point\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "MANEUVER RIGID BODY CORRIDOR TESTS: FAIL: "
            << error.what() << "\n";
        return 1;
    }
}
