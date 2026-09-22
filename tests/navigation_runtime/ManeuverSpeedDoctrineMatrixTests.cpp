#include "src/game/navigation/AcceptedManeuverProgram.h"
#include "src/game/navigation/TrajectoryFollower.h"
#include "src/game/navigation/NavigationRuntimeControlBridge.h"
#include "src/game/navigation/DynamicMotionSystem.h"
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
using Decision = game::ship::controller::ManeuverDecisionController;
using Law = game::navigation::LocalFlightControlLaw;

constexpr double kPi = 3.14159265358979323846;
constexpr double kDt = 0.02;
constexpr double kDistanceMeters = 180.0;
constexpr double kInitialSpeedMps = 6.0;
constexpr double kObstacleRadiusMeters = 16.0;
const glm::dvec3 kObstacleCenter {90.0, 0.0, 0.0};
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

double smooth5(double u)
{
    return 10.0 * u * u * u -
        15.0 * u * u * u * u +
        6.0 * u * u * u * u * u;
}

double smooth5Derivative(double u)
{
    return
        30.0 * u * u -
        60.0 * u * u * u +
        30.0 * u * u * u * u;
}

double smooth5Integral(double u)
{
    return
        2.5 * std::pow(u, 4.0) -
        3.0 * std::pow(u, 5.0) +
        std::pow(u, 6.0);
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
    profile.execution.deterministicSeed = 0x51D0C7A1ull;
    return profile;
}

struct PathSample
{
    glm::dvec3 position {0.0};
    glm::dvec3 velocity {0.0};
    glm::dvec3 acceleration {0.0};
    Basis basis {};
};

struct FixtureSpec
{
    std::uint64_t id = 0;
    const char* name = "";
    double lateralOffsetMeters = 0.0;
    double finalSpeedMps = kInitialSpeedMps;
    bool fixedForward = false;

    Decision::CandidateFamily decisionFamily =
        Decision::CandidateFamily::LocalVisibility;
    Decision::ControlLawRequirement lawRequirement =
        Decision::ControlLawRequirement::Any;

    double escapeReserve01 = 0.0;
    double threatExposure = 0.0;
    double criticalRisk01 = 0.05;
};

double durationSeconds(const FixtureSpec& spec)
{
    return
        2.0 * kDistanceMeters /
        (kInitialSpeedMps + spec.finalSpeedMps);
}

PathSample samplePath(const FixtureSpec& spec, double t)
{
    const double T = durationSeconds(spec);
    const double u =
        std::clamp(t / T, 0.0, 1.0);

    const double deltaV =
        spec.finalSpeedMps - kInitialSpeedMps;

    PathSample result;
    result.position.x =
        kInitialSpeedMps * T * u +
        deltaV * T * smooth5Integral(u);
    result.position.y =
        spec.lateralOffsetMeters * lateralBump(u);

    result.velocity.x =
        kInitialSpeedMps +
        deltaV * smooth5(u);
    result.velocity.y =
        spec.lateralOffsetMeters *
        lateralBumpDerivative(u) / T;

    result.acceleration.x =
        deltaV * smooth5Derivative(u) / T;
    result.acceleration.y =
        spec.lateralOffsetMeters *
        lateralBumpSecondDerivative(u) /
        (T * T);

    if (spec.fixedForward)
        result.basis = basisForForward(glm::dvec3(1.0, 0.0, 0.0));
    else
        result.basis = basisForForward(result.velocity);

    return result;
}

double hullClearanceToObstacle(
    const glm::dvec3& center,
    const Basis& basis
)
{
    double minimum =
        std::numeric_limits<double>::infinity();

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

                minimum = std::min(
                    minimum,
                    glm::length(corner - kObstacleCenter) -
                        kObstacleRadiusMeters
                );
            }
        }
    }

    return minimum;
}

struct BuiltFixture
{
    FixtureSpec spec {};
    Program program {};
    Decision::Candidate candidate {};

    double plannedMinimumClearanceMeters =
        std::numeric_limits<double>::infinity();
    double plannedPeakSpeedMps = 0.0;
    double plannedPeakAccelerationMps2 = 0.0;
    double plannedMaximumSlipDeg = 0.0;
};

BuiltFixture buildFixture(
    const FixtureSpec& spec,
    std::uint64_t revision
)
{
    BuiltFixture result;
    result.spec = spec;

    const double T = durationSeconds(spec);

    Program p;
    p.valid = true;
    p.revision = revision;
    p.objectiveRevision = 9100u;
    p.family =
        spec.fixedForward
            ? Program::ManeuverFamily::DriftPass
            : Program::ManeuverFamily::PrecisionTransit;
    p.acceptedAtUniverseTimeSeconds = 0.0;
    p.validUntilUniverseTimeSeconds = T + 8.0;
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
    const double sampleDt = T / denom;

    for (std::size_t i = 0; i < Program::kMaxSamples; ++i)
    {
        const double t =
            T * static_cast<double>(i) / denom;
        const PathSample s = samplePath(spec, t);

        orientations[i] = quaternionForBasis(s.basis);

        auto& out = p.samples[i];
        out.timeOffsetSeconds = t;
        out.positionMapMeters = s.position;
        out.velocityMapMetersPerSecond = s.velocity;
        out.linearAccelerationFeedForwardMapMps2 =
            s.acceleration;
        out.forwardMap = s.basis.forward;
        out.rightMap = s.basis.right;
        out.upMap = s.basis.up;
    }

    if (spec.fixedForward)
    {
        for (auto& s : p.samples)
        {
            s.angularVelocityMapRadPerSecond = {0.0, 0.0, 0.0};
            s.angularAccelerationFeedForwardMapRadPerSec2 =
                {0.0, 0.0, 0.0};
        }
    }
    else
    {
        std::array<glm::dvec3, Program::kMaxSamples> omega {};
        for (std::size_t i = 0; i < Program::kMaxSamples; ++i)
        {
            if (i == 0)
            {
                omega[i] =
                    angularVelocityBetween(
                        orientations[i],
                        orientations[i + 1],
                        sampleDt
                    );
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
            p.samples[i].
                angularAccelerationFeedForwardMapRadPerSec2 =
                alpha;
        }
    }

    constexpr int DenseSamples = 1024;
    for (int i = 0; i <= DenseSamples; ++i)
    {
        const double t =
            T * static_cast<double>(i) /
            static_cast<double>(DenseSamples);
        const PathSample s = samplePath(spec, t);

        result.plannedMinimumClearanceMeters =
            std::min(
                result.plannedMinimumClearanceMeters,
                hullClearanceToObstacle(
                    s.position,
                    s.basis
                )
            );
        result.plannedPeakSpeedMps =
            std::max(
                result.plannedPeakSpeedMps,
                glm::length(s.velocity)
            );
        result.plannedPeakAccelerationMps2 =
            std::max(
                result.plannedPeakAccelerationMps2,
                glm::length(s.acceleration)
            );
        result.plannedMaximumSlipDeg =
            std::max(
                result.plannedMaximumSlipDeg,
                angleRad(
                    s.velocity,
                    s.basis.forward
                ) * 180.0 / kPi
            );
    }

    p.proof.mapRevision = 1;
    p.proof.spaceRevision = 1;
    p.proof.minimumClearanceMeters =
        result.plannedMinimumClearanceMeters;
    p.proof.minimumLinearAuthorityReserveMps2 = 0.05;
    p.proof.minimumAngularAuthorityReserveRadPerSec2 = 0.05;

    result.program = p;

    Decision::Candidate c;
    c.candidateId = spec.id;
    c.family = spec.decisionFamily;
    c.controlLawRequirement = spec.lawRequirement;
    c.valid = true;
    c.collisionFree =
        result.plannedMinimumClearanceMeters > 0.0;
    c.contactExpected = false;
    c.progressesObjective = true;
    c.stopsOrBrakes = false;

    c.timeToObjectiveSeconds = T;
    c.entrySpeedMps = kInitialSpeedMps;
    c.exitSpeedMps = spec.finalSpeedMps;
    c.minimumClearanceMeters =
        result.plannedMinimumClearanceMeters;
    c.escapeReserve01 = spec.escapeReserve01;

    c.peakClosingNormalSpeedMps = 0.0;
    c.normalImpactEnergyProxyJ = 0.0;
    c.criticalDamageRisk01 = spec.criticalRisk01;
    c.missionDamageCost01 = 0.0;
    c.expendableDamageCost01 = 0.0;
    c.threatExposure = spec.threatExposure;

    result.candidate = c;
    return result;
}

const std::array<FixtureSpec, 6>& fixtureSpecs()
{
    static const std::array<FixtureSpec, 6> specs {{
        {
            1, "precision",
            38.0, 6.0, false,
            Decision::CandidateFamily::PrecisionPassage,
            Decision::ControlLawRequirement::Any,
            0.80, 5.0, 0.05
        },
        {
            2, "balanced",
            32.0, 9.0, false,
            Decision::CandidateFamily::LocalVisibility,
            Decision::ControlLawRequirement::Any,
            0.95, 4.0, 0.05
        },
        {
            3, "fast",
            27.0, 12.0, false,
            Decision::CandidateFamily::LocalVisibility,
            Decision::ControlLawRequirement::Any,
            0.70, 6.0, 0.05
        },
        {
            4, "newtonian_drift_dash",
            25.0, 14.0, true,
            Decision::CandidateFamily::LocalVisibility,
            Decision::ControlLawRequirement::NewtonianOnly,
            0.60, 7.0, 0.05
        },
        {
            5, "low_threat_escape",
            34.0, 10.0, false,
            Decision::CandidateFamily::ExtendedVisibilityRecovery,
            Decision::ControlLawRequirement::Any,
            0.75, 1.0, 0.05
        },
        {
            6, "reckless_shortcut",
            18.0, 18.0, false,
            Decision::CandidateFamily::EmergencyContact,
            Decision::ControlLawRequirement::Any,
            0.40, 0.20, 0.90
        }
    }};
    return specs;
}

const char* lawName(Law law)
{
    return law == Law::Newtonian
        ? "newtonian"
        : "assisted";
}

const char* doctrineName(Decision::Doctrine doctrine)
{
    switch (doctrine)
    {
        case Decision::Doctrine::PrecisionRetrieval:
            return "precision_retrieval";
        case Decision::Doctrine::Extreme:
            return "extreme";
        case Decision::Doctrine::CombatEscape:
            return "combat_escape";
        case Decision::Doctrine::Rational:
        default:
            return "rational";
    }
}

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
        frame.frameId = "speed-doctrine";
        frame.originMeters = {0.0, 0.0, 0.0};
        frame.localToWorldBasis = glm::dmat3(1.0);
        frame.valid = true;

        transform.motion.mode =
            game::navigation::MotionMode::HubTactical;
        transform.motion.systemId = 1;
        transform.motion.travelFrame = frame;
        transform.motion.localControlLaw = law;
        transform.motion.localPositionMeters = {0.0, 0.0, 0.0};
        transform.motion.localVelocityMps =
            {kInitialSpeedMps, 0.0, 0.0};
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
            "speed/doctrine bridge reset failed"
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

Basis actualBasis(const Vehicle& v)
{
    return {
        glm::dvec3(v.transform.forward()),
        glm::dvec3(v.transform.right()),
        glm::dvec3(v.transform.up())
    };
}

struct ExecutionMetrics
{
    bool valid = true;
    std::size_t trackingExceededTicks = 0;
    double minimumActualClearanceMeters =
        std::numeric_limits<double>::infinity();
    double peakSpeedMps = 0.0;
    double maximumSlipDeg = 0.0;
    double maximumForwardTrackingErrorDeg = 0.0;
    double finalPositionErrorMeters = 0.0;
    double finalVelocityErrorMps = 0.0;
    double finalForwardErrorDeg = 0.0;
    double simulatedSeconds = 0.0;
};

ExecutionMetrics execute(
    Law law,
    const BuiltFixture& fixture
)
{
    Vehicle v(law);
    ExecutionMetrics m;

    const Program& p = fixture.program;
    const double endTime =
        p.samples[
            static_cast<std::size_t>(p.sampleCount - 1)
        ].timeOffsetSeconds;

    while (v.timeSeconds < endTime - 1.0e-9)
    {
        const auto follower =
            Follower::follow(
                p,
                v.timeSeconds,
                agentState(v),
                game::navigation::ManeuverTrackingController::Policy {}
            );

        if (follower.status == Follower::Status::InvalidInput)
        {
            m.valid = false;
            break;
        }

        if (follower.trackingErrorExceeded)
            ++m.trackingExceededTicks;

        m.maximumForwardTrackingErrorDeg =
            std::max(
                m.maximumForwardTrackingErrorDeg,
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

        const glm::dvec3 velocity =
            v.transform.motion.localVelocityMps;
        const double speed = glm::length(velocity);

        m.peakSpeedMps =
            std::max(m.peakSpeedMps, speed);

        if (speed > 0.25)
        {
            m.maximumSlipDeg =
                std::max(
                    m.maximumSlipDeg,
                    angleRad(
                        velocity,
                        glm::dvec3(v.transform.forward())
                    ) * 180.0 / kPi
                );
        }

        m.minimumActualClearanceMeters =
            std::min(
                m.minimumActualClearanceMeters,
                hullClearanceToObstacle(
                    v.transform.motion.localPositionMeters,
                    actualBasis(v)
                )
            );
    }

    const auto& terminal =
        p.samples[
            static_cast<std::size_t>(p.sampleCount - 1)
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
    m.simulatedSeconds = v.timeSeconds;

    return m;
}

const BuiltFixture& findFixture(
    const std::array<BuiltFixture, 6>& fixtures,
    std::uint64_t id
)
{
    for (const auto& fixture : fixtures)
    {
        if (fixture.spec.id == id)
            return fixture;
    }
    throw std::runtime_error("selected fixture id not found");
}

std::uint64_t expectedSelection(
    Law law,
    Decision::Doctrine doctrine
)
{
    switch (doctrine)
    {
        case Decision::Doctrine::PrecisionRetrieval:
            return 1;
        case Decision::Doctrine::Extreme:
            return
                law == Law::Newtonian
                    ? 4
                    : 3;
        case Decision::Doctrine::CombatEscape:
            return 5;
        case Decision::Doctrine::Rational:
        default:
            return 2;
    }
}

void testSpeedDoctrineMatrix()
{
    std::array<BuiltFixture, 6> fixtures;
    for (std::size_t i = 0; i < fixtures.size(); ++i)
    {
        fixtures[i] =
            buildFixture(
                fixtureSpecs()[i],
                9200u + static_cast<std::uint64_t>(i)
            );

        require(
            fixtures[i].plannedMinimumClearanceMeters > 0.0,
            std::string("fixture is not collision-free: ") +
                fixtures[i].spec.name
        );

        // All ordinary candidate programs must remain within the shared
        // 2 m/s2 transverse manoeuvre authority. The total vector may be
        // larger because positive tangent acceleration belongs to the main
        // engine.
        require(
            fixtures[i].plannedPeakAccelerationMps2 < 3.2,
            std::string("fixture acceleration unexpectedly high: ") +
                fixtures[i].spec.name
        );
    }

    const std::array<Decision::Doctrine, 4> doctrines {{
        Decision::Doctrine::Rational,
        Decision::Doctrine::PrecisionRetrieval,
        Decision::Doctrine::Extreme,
        Decision::Doctrine::CombatEscape
    }};

    const std::array<Law, 2> laws {{
        Law::Newtonian,
        Law::Assisted
    }};

    for (const Law law : laws)
    {
        for (const auto doctrine : doctrines)
        {
            std::array<Decision::Candidate, 6> candidates;
            for (std::size_t i = 0; i < fixtures.size(); ++i)
                candidates[i] = fixtures[i].candidate;

            Decision::Context context;
            context.doctrine = doctrine;
            context.progress =
                Decision::ProgressRequirement::MustProgress;
            context.controlLaw = law;
            context.allowExpectedContact = false;
            context.allowSacrificialComponentLoss = false;
            context.maximumPreferredCriticalDamageRisk01 = 0.20;

            const auto selection =
                Decision::select(
                    context,
                    candidates.data(),
                    candidates.size()
                );

            require(
                selection.valid,
                std::string("doctrine selection invalid for ") +
                    lawName(law) + "/" + doctrineName(doctrine)
            );

            const std::uint64_t expected =
                expectedSelection(law, doctrine);

            require(
                selection.selectedCandidateId == expected,
                std::string("unexpected candidate for ") +
                    lawName(law) + "/" + doctrineName(doctrine)
            );

            require(
                selection.selectedCandidateId != 6,
                "hard critical-risk envelope failed to reject reckless shortcut"
            );

            if (law == Law::Assisted)
            {
                require(
                    selection.selectedCandidateId != 4,
                    "Assisted selected Newtonian-only drift dash"
                );
            }

            const BuiltFixture& selected =
                findFixture(
                    fixtures,
                    selection.selectedCandidateId
                );

            const ExecutionMetrics exec =
                execute(law, selected);

            require(
                exec.valid,
                std::string("selected program execution invalid for ") +
                    lawName(law) + "/" + doctrineName(doctrine)
            );
            require(
                exec.trackingExceededTicks == 0,
                std::string("tracking envelope exceeded for ") +
                    lawName(law) + "/" + doctrineName(doctrine)
            );
            require(
                exec.minimumActualClearanceMeters > 0.25,
                std::string("selected program lost obstacle clearance for ") +
                    lawName(law) + "/" + doctrineName(doctrine)
            );
            require(
                exec.finalPositionErrorMeters <= 1.5,
                std::string("selected program final position miss for ") +
                    lawName(law) + "/" + doctrineName(doctrine)
            );
            require(
                exec.finalVelocityErrorMps <= 0.75,
                std::string("selected program final velocity miss for ") +
                    lawName(law) + "/" + doctrineName(doctrine)
            );
            require(
                exec.finalForwardErrorDeg <= 5.0,
                std::string("selected program final attitude miss for ") +
                    lawName(law) + "/" + doctrineName(doctrine)
            );

            if (selection.selectedCandidateId == 4)
            {
                require(
                    exec.maximumSlipDeg >= 20.0,
                    "Newtonian drift dash did not produce material slip"
                );
            }

            std::cout
                << std::fixed << std::setprecision(6)
                << "[DOCTRINE]"
                << " law=" << lawName(law)
                << " doctrine=" << doctrineName(doctrine)
                << " selected=" << selected.spec.name
                << " candidate_id=" << selected.spec.id
                << " planned_time_s="
                << durationSeconds(selected.spec)
                << " planned_clearance_m="
                << selected.plannedMinimumClearanceMeters
                << " planned_peak_speed_mps="
                << selected.plannedPeakSpeedMps
                << " planned_peak_accel_mps2="
                << selected.plannedPeakAccelerationMps2
                << " planned_max_slip_deg="
                << selected.plannedMaximumSlipDeg
                << " actual_min_clearance_m="
                << exec.minimumActualClearanceMeters
                << " actual_peak_speed_mps="
                << exec.peakSpeedMps
                << " actual_max_slip_deg="
                << exec.maximumSlipDeg
                << " final_pos_error_m="
                << exec.finalPositionErrorMeters
                << " final_velocity_error_mps="
                << exec.finalVelocityErrorMps
                << " final_forward_error_deg="
                << exec.finalForwardErrorDeg
                << " tracking_envelope_exceeded_ticks="
                << exec.trackingExceededTicks
                << " simulated_s="
                << exec.simulatedSeconds
                << "\n";
        }
    }

    const auto& precision = fixtures[0];
    const auto& balanced = fixtures[1];
    const auto& fast = fixtures[2];
    const auto& dash = fixtures[3];
    const auto& escape = fixtures[4];
    const auto& reckless = fixtures[5];

    require(
        precision.plannedMinimumClearanceMeters >
            balanced.plannedMinimumClearanceMeters,
        "precision path must actually have more clearance than balanced"
    );
    require(
        balanced.plannedMinimumClearanceMeters >
            fast.plannedMinimumClearanceMeters,
        "balanced path must actually have more clearance than fast"
    );
    require(
        durationSeconds(dash.spec) <
            durationSeconds(fast.spec),
        "Newtonian drift dash must actually be faster than common fast path"
    );
    require(
        escape.spec.threatExposure <
            balanced.spec.threatExposure,
        "escape candidate must actually have lower threat annotation"
    );
    require(
        reckless.spec.criticalRisk01 >
            0.20 &&
        durationSeconds(reckless.spec) <
            durationSeconds(dash.spec),
        "reckless shortcut must be faster but outside preferred critical-risk envelope"
    );
}

} // namespace

int main()
{
    try
    {
        testSpeedDoctrineMatrix();

        std::cout << "MANEUVER SPEED/DOCTRINE MATRIX TESTS: PASS\n";
        std::cout << " - Rational selects the balanced proved program\n";
        std::cout << " - PrecisionRetrieval selects the largest-clearance proved program\n";
        std::cout << " - Extreme selects the fastest law-compatible proved program\n";
        std::cout << " - CombatEscape selects the lowest-threat proved program\n";
        std::cout << " - Assisted filters the Newtonian-only drift dash before doctrine ranking\n";
        std::cout << " - critical-risk filtering rejects a faster reckless shortcut above doctrine\n";
        std::cout << " - every selected AcceptedManeuverProgram executes through follower, PilotSkill and real physics\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "MANEUVER SPEED/DOCTRINE MATRIX TESTS: FAIL: "
            << error.what() << "\n";
        return 1;
    }
}
