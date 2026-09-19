#include "src/game/navigation/AcceptedManeuverProgram.h"
#include "src/game/navigation/TrajectoryFollower.h"
#include "src/game/navigation/NavigationRuntimeControlBridge.h"
#include "src/game/navigation/DynamicMotionSystem.h"
#include "src/game/shared/SharedShipPhysics.h"
#include "src/game/ship/core/ShipParams.h"
#include "src/world/WorldParams.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace
{

using Program = game::navigation::AcceptedManeuverProgram;
using Follower = game::navigation::TrajectoryFollower;
using Bridge = game::navigation::NavigationRuntimeControlBridge;

constexpr double kPi = 3.14159265358979323846;
constexpr double kDt = 0.02;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
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
    return profile;
}

struct Basis
{
    glm::dvec3 forward {0.0, 0.0, -1.0};
    glm::dvec3 right {1.0, 0.0, 0.0};
    glm::dvec3 up {0.0, 1.0, 0.0};
};

Basis yawBasis(double angle)
{
    Basis b;
    const double c = std::cos(angle);
    const double s = std::sin(angle);

    b.forward = {-s, 0.0, -c};
    b.right = {c, 0.0, -s};
    b.up = {0.0, 1.0, 0.0};
    return b;
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
    p.objectiveRevision = 1;
    p.acceptedAtUniverseTimeSeconds = acceptedAt;
    p.validUntilUniverseTimeSeconds = acceptedAt + duration + 10.0;
    p.completionTriggersReplan = true;

    p.terminalTolerance.positionMeters = 0.75;
    p.terminalTolerance.linearVelocityMps = 0.30;
    p.terminalTolerance.forwardAngleRad = 0.02;
    p.terminalTolerance.angularVelocityRadPerSec = 0.03;

    p.tracking.positionErrorMeters = 10.0;
    p.tracking.linearVelocityErrorMps = 5.0;
    p.tracking.forwardAngleErrorRad = 0.5;
    p.tracking.angularVelocityErrorRadPerSec = 0.5;
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

Program makeLineProgram(
    std::uint64_t revision,
    double acceptedAt,
    const glm::dvec3& start,
    const glm::dvec3& end,
    const Basis& basis,
    double duration
)
{
    Program p;
    fillCommon(p, revision, acceptedAt, duration);
    p.family = Program::ManeuverFamily::Trim;
    p.sampleCount = static_cast<std::uint8_t>(Program::kMaxSamples);

    const glm::dvec3 delta = end - start;
    const double denom =
        static_cast<double>(Program::kMaxSamples - 1);

    for (std::size_t i = 0; i < Program::kMaxSamples; ++i)
    {
        const double u = static_cast<double>(i) / denom;
        auto& s = p.samples[i];

        s.timeOffsetSeconds = duration * u;
        s.positionMapMeters = start + delta * smooth5(u);
        s.velocityMapMetersPerSecond =
            delta * (smooth5d1(u) / duration);
        s.linearAccelerationFeedForwardMapMps2 =
            delta * (smooth5d2(u) / (duration * duration));

        s.forwardMap = basis.forward;
        s.rightMap = basis.right;
        s.upMap = basis.up;
        s.angularVelocityMapRadPerSecond = {0.0, 0.0, 0.0};
        s.angularAccelerationFeedForwardMapRadPerSec2 =
            {0.0, 0.0, 0.0};
    }

    return p;
}

Program makeYawProgram(
    std::uint64_t revision,
    double acceptedAt,
    const glm::dvec3& position,
    double startYaw,
    double endYaw,
    double duration
)
{
    Program p;
    fillCommon(p, revision, acceptedAt, duration);
    p.family = Program::ManeuverFamily::LeadRotateMainBurn;
    p.sampleCount = static_cast<std::uint8_t>(Program::kMaxSamples);

    const double angle = endYaw - startYaw;
    const double denom =
        static_cast<double>(Program::kMaxSamples - 1);

    for (std::size_t i = 0; i < Program::kMaxSamples; ++i)
    {
        const double u = static_cast<double>(i) / denom;
        const double yaw = startYaw + angle * smooth5(u);
        const Basis basis = yawBasis(yaw);

        auto& s = p.samples[i];
        s.timeOffsetSeconds = duration * u;
        s.positionMapMeters = position;
        s.velocityMapMetersPerSecond = {0.0, 0.0, 0.0};
        s.linearAccelerationFeedForwardMapMps2 = {0.0, 0.0, 0.0};

        s.forwardMap = basis.forward;
        s.rightMap = basis.right;
        s.upMap = basis.up;
        s.angularVelocityMapRadPerSecond =
            {0.0, angle * smooth5d1(u) / duration, 0.0};
        s.angularAccelerationFeedForwardMapRadPerSec2 =
            {0.0, angle * smooth5d2(u) / (duration * duration), 0.0};
    }

    return p;
}

struct Vehicle
{
    ShipTransform transform {};
    ShipParams params = labShipParams();
    WorldParams world {};
    game::navigation::KinematicFrame frame {};
    Bridge bridge {expertProfile()};
    double timeSeconds = 0.0;

    Vehicle()
    {
        frame.systemId = 1;
        frame.frameId = "maneuver-execution-lab";
        frame.originMeters = {0.0, 0.0, 0.0};
        frame.localToWorldBasis = glm::dmat3(1.0);
        frame.valid = true;

        transform.motion.mode =
            game::navigation::MotionMode::HubTactical;
        transform.motion.systemId = 1;
        transform.motion.travelFrame = frame;
        transform.motion.localControlLaw =
            game::navigation::LocalFlightControlLaw::Newtonian;
        transform.motion.localPositionMeters = {0.0, 0.0, 0.0};
        transform.motion.localVelocityMps = {0.0, 0.0, 0.0};
        transform.setWorldPositionMeters({0.0, 0.0, 0.0});

        Bridge::Intent initial;
        initial.revision = 1;
        initial.targetRevision = 0;
        require(
            bridge.reset(0.0, initial),
            "execution lab bridge reset failed"
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

double distancePointToSegment(
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

double distancePointToPolyline2(
    const glm::dvec3& p,
    const glm::dvec3& a,
    const glm::dvec3& b,
    const glm::dvec3& c
)
{
    return std::min(
        distancePointToSegment(p, a, b),
        distancePointToSegment(p, b, c)
    );
}

struct ScenarioMetrics
{
    bool completed = false;
    double finalPositionErrorMeters = 0.0;
    double finalSpeedMps = 0.0;
    double maximumCrossTrackMeters = 0.0;
    double maximumOvershootMeters = 0.0;
    double cornerPositionErrorMeters = 0.0;
    double corridorHalfWidthMeters = 0.0;
    double maximumCorridorViolationMeters = 0.0;
    double totalSeconds = 0.0;
};

ScenarioMetrics runStraightScenario()
{
    Vehicle v;
    const glm::dvec3 start(0.0, 0.0, 0.0);
    const glm::dvec3 finish(0.0, 0.0, -100.0);
    const glm::dvec3 direction = glm::normalize(finish - start);
    const double length = glm::length(finish - start);

    const Program p =
        makeLineProgram(
            1,
            0.0,
            start,
            finish,
            Basis {},
            20.0
        );

    ScenarioMetrics m;
    double maxCross = 0.0;
    double maxOvershoot = 0.0;

    const double endTime = 30.0;
    while (v.timeSeconds < endTime - 1.0e-9)
    {
        const glm::dvec3 pos =
            v.transform.motion.localPositionMeters;
        maxCross = std::max(
            maxCross,
            glm::length(
                (pos - start) -
                direction * glm::dot(pos - start, direction)
            )
        );
        maxOvershoot = std::max(
            maxOvershoot,
            std::max(
                0.0,
                glm::dot(pos - start, direction) - length
            )
        );

        const auto follower =
            Follower::follow(p, v.timeSeconds, agentState(v));
        require(
            follower.status != Follower::Status::InvalidInput,
            "straight follower became invalid"
        );

        const auto bridgeResult =
            v.bridge.step(
                v.timeSeconds + kDt,
                kDt,
                toSystemIntent(follower.intent)
            );
        require(
            bridgeResult.status == Bridge::PilotExecutor::Status::Ok,
            "straight bridge execution failed"
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
                bridgeResult.control.navigationLinearAccelerationDemandSystemMps2,
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
            Follower::follow(p, v.timeSeconds, agentState(v));
        if (after.status == Follower::Status::Complete)
        {
            m.completed = true;
            break;
        }
    }

    const glm::dvec3 finalPos =
        v.transform.motion.localPositionMeters;
    m.finalPositionErrorMeters = glm::length(finalPos - finish);
    m.finalSpeedMps =
        glm::length(v.transform.motion.localVelocityMps);
    m.maximumCrossTrackMeters = maxCross;
    m.maximumOvershootMeters = maxOvershoot;
    m.totalSeconds = v.timeSeconds;
    return m;
}

ScenarioMetrics runRightAngleScenario(double corridorHalfWidth)
{
    Vehicle v;

    const glm::dvec3 start(0.0, 0.0, 0.0);
    const glm::dvec3 corner(0.0, 0.0, -100.0);
    const glm::dvec3 finish(100.0, 0.0, -100.0);

    ScenarioMetrics m;
    m.corridorHalfWidthMeters = corridorHalfWidth;

    const Program leg1 =
        makeLineProgram(10, v.timeSeconds, start, corner, Basis {}, 20.0);

    auto runWithCorridor =
        [&](const Program& p)
        {
            const double end =
                p.acceptedAtUniverseTimeSeconds +
                p.samples[p.sampleCount - 1].timeOffsetSeconds +
                8.0;

            while (v.timeSeconds < end - 1.0e-9)
            {
                const glm::dvec3 pos =
                    v.transform.motion.localPositionMeters;
                const double corridorDistance =
                    distancePointToPolyline2(pos, start, corner, finish);
                m.maximumCrossTrackMeters =
                    std::max(m.maximumCrossTrackMeters, corridorDistance);
                m.maximumCorridorViolationMeters =
                    std::max(
                        m.maximumCorridorViolationMeters,
                        std::max(
                            0.0,
                            corridorDistance - corridorHalfWidth
                        )
                    );

                const auto follower =
                    Follower::follow(p, v.timeSeconds, agentState(v));
                require(
                    follower.status != Follower::Status::InvalidInput,
                    "right-angle follower became invalid"
                );

                const auto bridgeResult =
                    v.bridge.step(
                        v.timeSeconds + kDt,
                        kDt,
                        toSystemIntent(follower.intent)
                    );
                require(
                    bridgeResult.status == Bridge::PilotExecutor::Status::Ok,
                    "right-angle bridge execution failed"
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

                const auto after =
                    Follower::follow(p, v.timeSeconds, agentState(v));
                if (after.status == Follower::Status::Complete)
                    return true;
            }
            return false;
        };

    const bool firstComplete = runWithCorridor(leg1);
    m.cornerPositionErrorMeters =
        glm::length(
            v.transform.motion.localPositionMeters - corner
        );

    const Program rotate =
        makeYawProgram(
            11,
            v.timeSeconds,
            corner,
            0.0,
            -0.5 * kPi,
            6.0
        );
    const bool rotateComplete = runWithCorridor(rotate);

    const Basis secondBasis = yawBasis(-0.5 * kPi);
    const Program leg2 =
        makeLineProgram(
            12,
            v.timeSeconds,
            corner,
            finish,
            secondBasis,
            20.0
        );
    const bool secondComplete = runWithCorridor(leg2);

    m.completed =
        firstComplete && rotateComplete && secondComplete;
    m.finalPositionErrorMeters =
        glm::length(
            v.transform.motion.localPositionMeters - finish
        );
    m.finalSpeedMps =
        glm::length(v.transform.motion.localVelocityMps);
    m.totalSeconds = v.timeSeconds;

    const glm::dvec3 secondDir = glm::normalize(finish - corner);
    m.maximumOvershootMeters = std::max(
        0.0,
        glm::dot(
            v.transform.motion.localPositionMeters - corner,
            secondDir
        ) - glm::length(finish - corner)
    );

    return m;
}

std::string arrivalClass(
    const ScenarioMetrics& m,
    double positionTolerance,
    double speedTolerance
)
{
    if (m.finalPositionErrorMeters <= positionTolerance &&
        m.finalSpeedMps <= speedTolerance)
    {
        return "ON_TARGET";
    }

    if (m.maximumOvershootMeters > positionTolerance)
        return "OVERSHOOT";

    return "UNDERSHOOT_OR_UNSETTLED";
}

void printMetrics(
    const char* name,
    const ScenarioMetrics& m,
    double positionTolerance,
    double speedTolerance
)
{
    std::cout
        << "[MOVEMENT] scenario=" << name
        << " arrival=" << arrivalClass(
               m,
               positionTolerance,
               speedTolerance
           )
        << " completed=" << (m.completed ? 1 : 0)
        << " final_pos_error_m=" << m.finalPositionErrorMeters
        << " final_speed_mps=" << m.finalSpeedMps
        << " max_cross_track_m=" << m.maximumCrossTrackMeters
        << " max_overshoot_m=" << m.maximumOvershootMeters
        << " corner_error_m=" << m.cornerPositionErrorMeters
        << " corridor_half_width_m=" << m.corridorHalfWidthMeters
        << " max_corridor_violation_m="
        << m.maximumCorridorViolationMeters
        << " simulated_s=" << m.totalSeconds
        << "\n";
}

void testStraightMovementReading()
{
    const auto m = runStraightScenario();
    printMetrics("straight_100m", m, 3.0, 1.0);

    require(m.completed, "straight program did not reach follower completion");
    require(
        m.finalPositionErrorMeters <= 3.0,
        "straight program missed final position by more than 3 m"
    );
    require(
        m.finalSpeedMps <= 1.0,
        "straight program arrived with more than 1 m/s residual speed"
    );
    require(
        m.maximumCrossTrackMeters <= 1.0,
        "straight program wandered more than 1 m off axis"
    );
    require(
        m.maximumOvershootMeters <= 3.0,
        "straight program overshot finish by more than 3 m"
    );
}

void testRightAngleMovementAndCorridor()
{
    constexpr double corridorHalfWidth = 5.0;
    const auto m = runRightAngleScenario(corridorHalfWidth);
    printMetrics("right_angle_100m_100m", m, 5.0, 1.5);

    require(
        m.completed,
        "right-angle program sequence did not complete all three phases"
    );
    require(
        m.cornerPositionErrorMeters <= 3.0,
        "right-angle route missed the corner stop by more than 3 m"
    );
    require(
        m.finalPositionErrorMeters <= 5.0,
        "right-angle route missed final position by more than 5 m"
    );
    require(
        m.finalSpeedMps <= 1.5,
        "right-angle route arrived too fast"
    );
    require(
        m.maximumCorridorViolationMeters <= 1.0e-6,
        "right-angle route left the 5 m corridor"
    );
}

} // namespace

int main()
{
    try
    {
        testStraightMovementReading();
        testRightAngleMovementAndCorridor();

        std::cout << "MANEUVER PROGRAM EXECUTION LAB: PASS\n";
        std::cout << " - straight stop-to-stop program measures final error, speed and overshoot\n";
        std::cout << " - 90-degree two-leg route measures corner capture and final arrival\n";
        std::cout << " - 5 m polyline corridor is monitored during the complete execution\n";
        std::cout << " - execution uses B9/B10 -> PilotSkill -> SharedShipPhysics/DynamicMotionSystem\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "MANEUVER PROGRAM EXECUTION LAB: FAIL: "
            << error.what() << "\n";
        return 1;
    }
}
