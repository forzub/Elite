#include "src/game/navigation/NavigationRuntimeControlBridge.h"
#include "src/game/navigation/NpcNavigationIntentController.h"
#include "src/game/navigation/ReplicatedNavigationExecutionState.h"
#include "src/game/navigation/DynamicMotionSystem.h"
#include "src/game/navigation/autopilot/PredictivePilot.h"
#include "src/game/shared/SharedShipPhysics.h"
#include "src/game/ship/core/ShipParams.h"
#include "src/game/ship/core/ShipTransform.h"
#include "src/world/WorldParams.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace
{

using Bridge = game::navigation::NavigationRuntimeControlBridge;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void requireNear(
    double actual,
    double expected,
    double tolerance,
    const std::string& message
)
{
    if (std::abs(actual - expected) > tolerance)
        throw std::runtime_error(message);
}

Bridge::PilotSkillProfile expertProfile()
{
    Bridge::PilotSkillProfile profile;
    profile.execution.reactionDelaySeconds = 0.0;
    profile.execution.perceptionDecisionRateHz = 100.0;
    profile.execution.commandLatencySeconds = 0.0;
    profile.execution.responseFrequencyHz = 4.0;
    profile.execution.dampingRatio = 1.0;
    profile.execution.commandGain = 1.0;
    profile.execution.maxLinearCommandSlewMetersPerSec3 = 1000.0;
    profile.execution.maxAngularCommandSlewRadPerSec3 = 1000.0;
    return profile;
}

ShipParams capabilityParams()
{
    ShipParams params {};
    params.maxPitchRate = 10.0f;
    params.maxYawRate = 10.0f;
    params.maxRollRate = 10.0f;
    params.angularAccel = 2.0f;
    params.angularDamping = 0.0f;

    params.maxCombatSpeed = 1000.0f;
    params.maxCruiseSpeed = 1000.0f;
    params.throttleAccel = 1.0f;

    params.strafeAccel = 2.0f;
    params.strafeDamping = 1.0f;
    params.maxStrafeSpeed = 1000.0f;
    params.manoeuvreThrusterAccel = 2.0f;

    params.maxGs = 100.0f;
    params.maxLinearGs = 1.0f;
    params.turnRadius = 1.0f;
    return params;
}

void testPredictivePilotUsesOnlyOrdinaryAssistedControls()
{
    ShipParams params = capabilityParams();
    params.assistedMinimumTargetSpeedChangeRateMps2 = 1.0f;
    params.assistedTargetSpeedChangeRateFractionPerSecond = 0.10f;
    params.forwardMainEngineAvailable = true;
    params.reverseMainEngineAvailable = true;
    params.forwardMainEngineAccelerationMps2 = 10.0f;
    params.reverseMainEngineAccelerationMps2 = 10.0f;

    game::navigation::autopilot::PredictivePilot::Request request;
    request.law = game::navigation::LocalFlightControlLaw::Assisted;
    request.desiredVelocityMapMps = {0.0, 20.0, 0.0};
    request.desiredForwardMap = {0.0, 1.0, 0.0};
    request.desiredUpMap = {0.0, 0.0, 1.0};
    request.actualVelocityMapMps = {0.0, 0.0, -5.0};
    request.forwardMap = {0.0, 0.0, -1.0};
    request.rightMap = {1.0, 0.0, 0.0};
    request.upMap = {0.0, 1.0, 0.0};
    request.deltaSeconds = 0.1;

    game::navigation::autopilot::PredictivePilot::State pilotState;
    const ShipControlState control =
        game::navigation::autopilot::PredictivePilot::make(
            request,
            params,
            pilotState
        );

    require(
        !control.navigationAccelerationDemandValid &&
        !control.navigationVelocityTargetValid &&
        !control.navigationPrecisionTranslationOnly,
        "PredictivePilot escaped through the direct navigation actuator seam"
    );
    require(
        control.targetSpeedRate > 0.0f,
        "PredictivePilot did not press the ordinary + speed control"
    );
    require(
        control.pitchInput > 0.0f,
        "PredictivePilot did not turn the hull toward the requested course"
    );
}

void testPredictivePilotBrakesAngularMotionBeforeOvershoot()
{
    ShipParams params = capabilityParams();
    params.angularAccel = 2.0f;
    params.maxPitchRate = 10.0f;

    constexpr double angle = 0.05;
    game::navigation::autopilot::PredictivePilot::Request request;
    request.law = game::navigation::LocalFlightControlLaw::Assisted;
    request.desiredForwardMap =
        {0.0, std::sin(angle), -std::cos(angle)};
    request.desiredUpMap =
        {0.0, std::cos(angle), std::sin(angle)};
    request.forwardMap = {0.0, 0.0, -1.0};
    request.rightMap = {1.0, 0.0, 0.0};
    request.upMap = {0.0, 1.0, 0.0};
    request.pitchRateRadPerSec = 0.8;
    request.deltaSeconds = 0.02;

    game::navigation::autopilot::PredictivePilot::State pilotState;
    const ShipControlState control =
        game::navigation::autopilot::PredictivePilot::make(
            request,
            params,
            pilotState
        );

    require(
        control.pitchInput < 0.0f,
        "PredictivePilot failed to counter-steer inside the angular braking envelope"
    );
}

void testPredictivePilotLearnsMeasuredPitchAuthority()
{
    ShipParams params = capabilityParams();
    params.angularAccel = 2.0f;
    params.maxPitchRate = 10.0f;

    game::navigation::autopilot::PredictivePilot::State state;

    game::navigation::autopilot::PredictivePilot::Request first;
    first.law = game::navigation::LocalFlightControlLaw::Assisted;
    first.desiredForwardMap = {0.0, 1.0, 0.0};
    first.desiredUpMap = {0.0, 0.0, 1.0};
    first.forwardMap = {0.0, 0.0, -1.0};
    first.rightMap = {1.0, 0.0, 0.0};
    first.upMap = {0.0, 1.0, 0.0};
    first.deltaSeconds = 0.1;

    const ShipControlState firstControl =
        game::navigation::autopilot::PredictivePilot::make(
            first,
            params,
            state
        );

    require(
        std::abs(firstControl.pitchInput) >= 0.5f,
        "PredictivePilot learning fixture did not excite pitch control"
    );

    auto second = first;
    second.pitchRateRadPerSec = 0.05;
    (void)game::navigation::autopilot::PredictivePilot::make(
        second,
        params,
        state
    );

    require(
        state.effectivePitchAuthorityRadPerSec2 > 0.0,
        "PredictivePilot did not learn measured pitch authority"
    );
    require(
        state.effectivePitchAuthorityRadPerSec2 <
            game::ship::angularAccelerationLimitRadPerSec2(params),
        "PredictivePilot ignored weaker measured pitch response"
    );
}

void testPredictivePilotUsesOrdinaryNewtonianThrottle()
{
    ShipParams params = capabilityParams();
    params.forwardMainEngineAvailable = true;
    params.reverseMainEngineAvailable = false;
    params.forwardMainEngineAccelerationMps2 = 10.0f;

    game::navigation::autopilot::PredictivePilot::Request request;
    request.law = game::navigation::LocalFlightControlLaw::Newtonian;
    request.desiredVelocityMapMps = {10.0, 0.0, 0.0};
    request.desiredLinearAccelerationMapMps2 = {5.0, 0.0, 0.0};
    request.desiredForwardMap = {1.0, 0.0, 0.0};
    request.desiredUpMap = {0.0, 1.0, 0.0};
    request.forwardMap = {1.0, 0.0, 0.0};
    request.rightMap = {0.0, 0.0, 1.0};
    request.upMap = {0.0, 1.0, 0.0};
    request.deltaSeconds = 0.02;

    game::navigation::autopilot::PredictivePilot::State pilotState;
    const ShipControlState control =
        game::navigation::autopilot::PredictivePilot::make(
            request,
            params,
            pilotState
        );

    const double expectedThrottle =
        5.0 / game::ship::forwardMainAccelerationLimitMps2(params);
    requireNear(
        control.targetSpeedRate,
        expectedThrottle,
        1.0e-6,
        "PredictivePilot did not normalize Newtonian throttle against the real main-engine/load authority"
    );
    require(
        !control.navigationAccelerationDemandValid &&
        !control.navigationVelocityTargetValid,
        "PredictivePilot Newtonian path used direct navigation actuator control"
    );
}

void testPredictivePilotUsesRcsForSmallAuthoredStopResidual()
{
    ShipParams params = capabilityParams();
    params.manoeuvreThrusterAccel = 2.0f;

    game::navigation::autopilot::PredictivePilot::Request request;
    request.law = game::navigation::LocalFlightControlLaw::Assisted;
    request.desiredVelocityMapMps = glm::dvec3(0.0);
    request.actualVelocityMapMps = {0.045, 0.0, 0.0};
    request.desiredForwardMap = {1.0, 0.0, 0.0};
    request.desiredUpMap = {0.0, 1.0, 0.0};
    request.forwardMap = {1.0, 0.0, 0.0};
    request.rightMap = {0.0, 0.0, 1.0};
    request.upMap = {0.0, 1.0, 0.0};
    request.stopRequested = true;
    request.deltaSeconds = 0.02;

    game::navigation::autopilot::PredictivePilot::State pilotState;
    const ShipControlState control =
        game::navigation::autopilot::PredictivePilot::make(
            request,
            params,
            pilotState
        );

    require(
        control.forwardInput < -0.9f,
        "PredictivePilot did not remove a 0.045 m/s STOP residual through ordinary keypad RCS"
    );
    require(
        !control.navigationAccelerationDemandValid &&
        !control.navigationVelocityTargetValid &&
        !control.navigationPrecisionTranslationOnly,
        "PredictivePilot precision STOP bypassed ordinary ship controls"
    );
}

void testPredictivePilotUsesEndForAuthoredStop()
{
    ShipParams params = capabilityParams();
    params.forwardMainEngineAvailable = true;
    params.reverseMainEngineAvailable = true;
    params.forwardMainEngineAccelerationMps2 = 10.0f;
    params.reverseMainEngineAccelerationMps2 = 10.0f;

    game::navigation::autopilot::PredictivePilot::Request request;
    request.law = game::navigation::LocalFlightControlLaw::Assisted;
    request.desiredVelocityMapMps = glm::dvec3(0.0);
    request.actualVelocityMapMps = {8.0, 0.0, 0.0};
    request.desiredForwardMap = {1.0, 0.0, 0.0};
    request.desiredUpMap = {0.0, 1.0, 0.0};
    request.forwardMap = {1.0, 0.0, 0.0};
    request.rightMap = {0.0, 0.0, 1.0};
    request.upMap = {0.0, 1.0, 0.0};
    request.stopRequested = true;
    request.deltaSeconds = 0.02;

    game::navigation::autopilot::PredictivePilot::State pilotState;
    const ShipControlState control =
        game::navigation::autopilot::PredictivePilot::make(
            request,
            params,
            pilotState
        );

    require(
        control.velocityAlignmentCommand ==
            game::navigation::VelocityAlignmentMode::BrakeToStop,
        "PredictivePilot authored STOP did not use ordinary END/autobrake"
    );
    requireNear(
        control.targetSpeedRate,
        0.0,
        1.0e-12,
        "PredictivePilot STOP simultaneously commanded longitudinal trim"
    );
}

void testBridgePublishesOneDirectDemandSample()
{
    Bridge bridge(expertProfile());

    Bridge::Intent initial;
    initial.revision = 1;
    initial.targetRevision = 11;
    require(bridge.reset(0.0, initial),
            "runtime bridge reset must succeed");

    Bridge::Intent intent;
    intent.revision = 2;
    intent.targetRevision = 22;
    intent.idealLinearAccelerationSystemMps2 = {3.0, 0.0, -6.0};
    intent.idealAngularAccelerationSystemRadPerSec2 = {1.0, 0.5, 0.25};

    const auto result = bridge.step(0.01, 0.01, intent);
    require(
        result.status ==
            world::navigation::PilotSkillExecutor::Status::Ok,
        "bridge step must execute through accepted pilot skill"
    );
    require(result.snapshot.valid,
            "successful bridge step must publish a valid execution snapshot");
    require(result.control.navigationAccelerationDemandValid,
            "bridge must use the explicit navigation acceleration channel");
    require(result.control.navigationIntentRevision == intent.revision,
            "control sample must preserve navigation intent revision");
    require(result.snapshot.intentRevision == 2,
            "execution snapshot lost high-level intent revision");
    require(result.snapshot.activeTargetRevision == 22,
            "execution snapshot lost concrete target revision");

    requireNear(
        result.control.navigationLinearAccelerationDemandSystemMps2.x,
        result.snapshot.executedLinearAccelerationDemandSystemMps2.x,
        0.0,
        "control and debug/guidance snapshot must publish the same executed linear demand"
    );
    requireNear(
        result.control.navigationAngularAccelerationDemandSystemRadPerSec2.x,
        result.snapshot.executedAngularAccelerationDemandSystemRadPerSec2.x,
        0.0,
        "control and debug/guidance snapshot must publish the same executed angular demand"
    );

    requireNear(result.control.pitchInput, 0.0, 0.0,
                "autopilot bridge must not synthesize legacy pitch keys");
    requireNear(result.control.yawInput, 0.0, 0.0,
                "autopilot bridge must not synthesize legacy yaw keys");
    requireNear(result.control.rollInput, 0.0, 0.0,
                "autopilot bridge must not synthesize legacy roll keys");
    requireNear(result.control.forwardInput, 0.0, 0.0,
                "autopilot bridge must not synthesize keypad RCS keys");
    requireNear(result.control.targetSpeedRate, 0.0, 0.0,
                "autopilot bridge must not synthesize legacy throttle trim");
}

void testLinearDemandUsesRealMainAndManoeuvreAuthority()
{
    game::navigation::DynamicMotionState motion;
    ShipParams params = capabilityParams();

    // Keep the installed rear-main rating below the common 1g load envelope
    // so this fixture can prove simultaneous main + manoeuvre allocation
    // without asking production physics to violate the total acceleration
    // envelope.
    params.forwardMainEngineAvailable = true;
    params.forwardMainEngineAccelerationMps2 = 5.0f;

    const glm::vec3 forward(0.0f, 0.0f, -1.0f);
    const glm::dvec3 demand(10.0, 0.0, -20.0);

    game::navigation::DynamicMotionSystem::applySystemAccelerationDemand(
        motion,
        params,
        demand,
        forward
    );

    requireNear(
        glm::length(motion.mainEngineAccelerationMps2),
        5.0,
        1.0e-9,
        "main engine demand must respect installed rear-main authority"
    );
    require(
        glm::dot(
            motion.mainEngineAccelerationMps2,
            glm::dvec3(forward)
        ) > 0.0,
        "main engine must remain forward-only"
    );
    requireNear(
        glm::length(motion.manoeuvreAccelerationMps2),
        2.0,
        1.0e-9,
        "remaining vector must use available manoeuvre-thruster authority"
    );

    // Separately pin the shared linear-load envelope. If main already consumes
    // the full 1g budget, secondary RCS must be reduced to zero instead of
    // producing 1g + 2 m/s^2 total acceleration.
    ShipParams saturated = capabilityParams();
    saturated.forwardMainEngineAvailable = true;
    saturated.forwardMainEngineAccelerationMps2 = 100.0f;

    game::navigation::DynamicMotionSystem::applySystemAccelerationDemand(
        motion,
        saturated,
        demand,
        forward
    );

    constexpr double standardGravity = 9.80665;
    requireNear(
        glm::length(motion.mainEngineAccelerationMps2),
        standardGravity,
        1.0e-6,
        "main engine demand must clamp at maxLinearGs authority"
    );
    requireNear(
        glm::length(motion.manoeuvreAccelerationMps2),
        0.0,
        1.0e-12,
        "secondary manoeuvre demand must yield when main consumes the full load envelope"
    );

    const glm::dvec3 reverseDemand(0.0, 0.0, 20.0);

    motion.localControlLaw =
        game::navigation::LocalFlightControlLaw::Newtonian;
    game::navigation::DynamicMotionSystem::applySystemAccelerationDemand(
        motion,
        params,
        reverseDemand,
        forward
    );

    requireNear(
        glm::length(motion.mainEngineAccelerationMps2),
        0.0,
        1.0e-12,
        "Newtonian reverse demand must not invent fore/nose main thrust"
    );
    requireNear(
        glm::length(motion.manoeuvreAccelerationMps2),
        2.0,
        1.0e-9,
        "Newtonian reverse demand may use only real manoeuvre-thruster authority before a hull flip"
    );

    motion.localControlLaw =
        game::navigation::LocalFlightControlLaw::Assisted;
    game::navigation::DynamicMotionSystem::applySystemAccelerationDemand(
        motion,
        params,
        reverseDemand,
        forward
    );

    requireNear(
        glm::length(motion.mainEngineAccelerationMps2),
        0.0,
        1.0e-12,
        "Assisted reverse demand must not invent fore/nose main thrust"
    );
    requireNear(
        glm::length(motion.manoeuvreAccelerationMps2),
        2.0,
        1.0e-9,
        "Assisted reverse demand may use only real manoeuvre-thruster authority before hull rotation"
    );

    const glm::dvec3 lateralDemand(5.0, 0.0, 0.0);
    game::navigation::DynamicMotionSystem::applySystemAccelerationDemand(
        motion,
        params,
        lateralDemand,
        forward
    );

    requireNear(
        glm::length(motion.mainEngineAccelerationMps2),
        0.0,
        1.0e-12,
        "Assisted lateral demand must not invent omnidirectional main thrust"
    );
    requireNear(
        glm::length(motion.manoeuvreAccelerationMps2),
        2.0,
        1.0e-9,
        "Assisted lateral demand must remain bounded by physical RCS authority"
    );
}

void testPrecisionVelocityTrimUsesOnlyPhysicalRcs()
{
    game::navigation::DynamicMotionState motion;
    motion.localControlLaw =
        game::navigation::LocalFlightControlLaw::Newtonian;
    motion.localVelocityMps = {0.03, -0.02, 0.01};

    game::navigation::KinematicFrame frame;
    frame.systemId = 0;
    frame.frameId = "precision-stop";
    frame.valid = true;

    ShipParams params = capabilityParams();
    world::coordinates::WorldPosition worldPosition {};

    const double initialSpeed =
        glm::length(motion.localVelocityMps);

    game::navigation::DynamicMotionSystem::
        applyNavigationPrecisionVelocityTrim(
            motion,
            frame,
            params,
            0.02f,
            glm::dvec3(0.0)
        );

    requireNear(
        glm::length(motion.mainEngineAccelerationMps2),
        0.0,
        1.0e-12,
        "precision STOP woke a main engine"
    );
    requireNear(
        glm::length(motion.assistedStabilizationAccelerationMps2),
        0.0,
        1.0e-12,
        "precision STOP used Assisted stabilization instead of RCS"
    );
    require(
        glm::length(motion.manoeuvreAccelerationMps2) > 0.0,
        "precision STOP produced no physical RCS impulse"
    );

    game::navigation::DynamicMotionSystem::updateLocalFrameMotion(
        motion,
        worldPosition,
        frame,
        params,
        0.02
    );

    require(
        glm::length(motion.localVelocityMps) <
            initialSpeed * 1.0e-6,
        "precision RCS trim failed to remove the residual velocity"
    );
}

void testVehicleBridgePublishesMotionTargetWithoutSelectingEngines()
{
    Bridge bridge(expertProfile());
    Bridge::Intent initial;
    initial.revision = 100;
    require(bridge.reset(0.0, initial), "vehicle bridge reset failed");

    Bridge::Intent intent;
    intent.revision = 101;
    intent.precisionTranslationOnly = true;
    intent.idealLinearAccelerationSystemMps2 = {1.0, 0.0, -4.0};
    const glm::dvec3 targetVelocity {2.0, 1.0, -12.0};
    const auto step = bridge.stepVehicle(
        0.01, 0.01, intent, targetVelocity
    );
    require(step.snapshot.valid && step.control.navigationVelocityTargetValid,
            "vehicle target was not published");
    require(
        step.control.navigationPrecisionTranslationOnly,
        "vehicle bridge lost precision RCS translation ownership"
    );
    requireNear(glm::length(step.control.navigationTargetVelocitySystemMps -
                            targetVelocity), 0.0, 0.0,
                "bridge changed requested vehicle velocity");
    requireNear(step.control.navigationLinearAccelerationDemandSystemMps2.x,
                step.snapshot.executedLinearAccelerationDemandSystemMps2.x,
                1.0e-9, "pilot execution differs from ship command");
}

void testVehicleBridgeRejectsInvalidVelocityAndClock()
{
    Bridge bridge(expertProfile());
    Bridge::Intent intent;
    intent.revision = 17;
    require(bridge.reset(0.0, intent), "vehicle bridge reset failed");
    const glm::dvec3 target {0.0, 0.0, -3.0};
    const auto roundedClock = bridge.stepVehicle(
        0.01005, 0.01, intent, target
    );
    require(roundedClock.status == Bridge::PilotExecutor::Status::Ok,
            "pilot clock rounding rejected vehicle command");
    const auto wrongClock = bridge.stepVehicle(
        0.10, 0.01, intent, target
    );
    require(wrongClock.status == Bridge::PilotExecutor::Status::InvalidInput &&
                wrongClock.failure ==
                    Bridge::StepResult::FailureKind::ExecutorRejected,
            "clock mismatch must report executor rejection");
    const auto invalidVelocity = bridge.stepVehicle(
        0.02005, 0.01, intent,
        {std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0}
    );
    require(!invalidVelocity.snapshot.valid &&
                !invalidVelocity.control.navigationVelocityTargetValid,
            "nonfinite target must never reach ship control");
    const auto recovered = bridge.stepVehicle(
        0.02005, 0.01, intent, target
    );
    require(recovered.status == Bridge::PilotExecutor::Status::Ok,
            "invalid target must not advance the pilot clock");
}

void testAssistedAutopilotUsesCanonicalFlightLaw()
{
    game::navigation::DynamicMotionState motion;
    motion.localControlLaw = game::navigation::LocalFlightControlLaw::Assisted;
    motion.localVelocityMps = {100.0, 0.0, 0.0};

    game::navigation::KinematicFrame frame;
    frame.systemId = 0;
    frame.frameId = "test";
    frame.valid = true;

    ShipParams params = capabilityParams();
    params.maxLinearGs = 7.5f;
    params.strafeAccel = 73.549875f;
    params.strafeDamping = 4.0f;
    params.forwardMainEngineAvailable = true;
    params.reverseMainEngineAvailable = true;
    params.forwardMainEngineAccelerationMps2 = 73.549875f;
    params.reverseMainEngineAccelerationMps2 = 73.549875f;

    world::coordinates::WorldPosition worldPosition {};
    const glm::vec3 forward(0.0f, 0.0f, -1.0f);
    const glm::vec3 right(1.0f, 0.0f, 0.0f);
    const glm::vec3 up(0.0f, 1.0f, 0.0f);

    for (int i = 0; i < 300; ++i)
    {
        game::navigation::DynamicMotionSystem::
            applyNavigationAssistedFlightModel(
                motion,
                frame,
                params,
                0.01f,
                glm::dvec3(0.0, 0.0, -100.0),
                glm::dvec3(0.0),
                forward,
                right,
                up
            );
        game::navigation::DynamicMotionSystem::updateLocalFrameMotion(
            motion,
            worldPosition,
            frame,
            params,
            0.01
        );
    }

    const glm::dvec3 velocity = glm::normalize(motion.localVelocityMps);
    const double courseError = std::acos(std::clamp(
        glm::dot(velocity, glm::dvec3(forward)),
        -1.0,
        1.0
    ));

    require(courseError <= 5.0 * 3.14159265358979323846 / 180.0,
            "Assisted autopilot did not realign VREL to hull nose within 3 seconds");
    requireNear(glm::length(motion.manoeuvreAccelerationMps2), 0.0, 1.0e-12,
                "Assisted autopilot incorrectly spent precision RCS during ordinary transit");
}

void testAssistedVectorTargetKeepsScalarSpeedWhileHullTurns()
{
    game::navigation::DynamicMotionState motion;
    motion.localControlLaw =
        game::navigation::LocalFlightControlLaw::Assisted;
    motion.localVelocityMps = glm::dvec3(0.0);

    game::navigation::KinematicFrame frame;
    frame.systemId = 0;
    frame.frameId = "assisted-steering-speed";
    frame.valid = true;

    ShipParams params = capabilityParams();
    params.throttleAccel = 5.0f;
    params.maxLinearGs = 7.5f;
    params.forwardMainEngineAvailable = true;
    params.reverseMainEngineAvailable = true;
    params.forwardMainEngineAccelerationMps2 = 20.0f;
    params.reverseMainEngineAccelerationMps2 = 20.0f;

    const glm::vec3 forward(0.0f, 0.0f, -1.0f);
    const glm::vec3 right(1.0f, 0.0f, 0.0f);
    const glm::vec3 up(0.0f, 1.0f, 0.0f);

    // This target direction is 120 degrees away from the current nose.
    // Steering owns that direction change. Assisted propulsion must preserve
    // the requested 6 m/s scalar speed instead of projecting it to zero.
    const double angle = 120.0 * 3.14159265358979323846 / 180.0;
    const glm::dvec3 targetVelocity(
        6.0 * std::sin(angle),
        0.0,
        -6.0 * std::cos(angle)
    );

    game::navigation::DynamicMotionSystem::
        applyNavigationAssistedFlightModel(
            motion,
            frame,
            params,
            0.02f,
            targetVelocity,
            glm::dvec3(0.0),
            forward,
            right,
            up
        );

    requireNear(
        motion.targetForwardSpeedMps,
        6.0,
        1.0e-9,
        "Assisted steering vector collapsed scalar speed while hull was turning"
    );
    require(
        glm::dot(
            motion.mainEngineAccelerationMps2,
            glm::dvec3(forward)
        ) > 0.0,
        "Assisted main engine stopped merely because steering target was off-nose"
    );
    requireNear(
        glm::length(motion.manoeuvreAccelerationMps2),
        0.0,
        1.0e-12,
        "Assisted steering-speed regression spent precision RCS"
    );
}

void testAssistedProgramTracksPhysicallyFeasibleAcceleration()
{
    game::navigation::DynamicMotionState motion;
    motion.localControlLaw = game::navigation::LocalFlightControlLaw::Assisted;

    game::navigation::KinematicFrame frame;
    frame.systemId = 0;
    frame.frameId = "test";
    frame.valid = true;

    ShipParams params = capabilityParams();
    params.throttleAccel = 5.0f;
    params.maxLinearGs = 7.5f;
    params.forwardMainEngineAvailable = true;
    params.reverseMainEngineAvailable = true;
    params.forwardMainEngineAccelerationMps2 = 73.549875f;
    params.reverseMainEngineAccelerationMps2 = 73.549875f;

    world::coordinates::WorldPosition worldPosition {};
    const glm::vec3 forward(0.0f, 0.0f, -1.0f);
    const glm::vec3 right(1.0f, 0.0f, 0.0f);
    const glm::vec3 up(0.0f, 1.0f, 0.0f);
    constexpr double plannedAcceleration = 46.0;
    constexpr double dt = 0.02;

    for (int tick = 1; tick <= 17; ++tick)
    {
        const double referenceSpeed = tick * dt * plannedAcceleration;
        game::navigation::DynamicMotionSystem::
            applyNavigationAssistedFlightModel(
                motion, frame, params, static_cast<float>(dt),
                glm::dvec3(0.0, 0.0, -referenceSpeed),
                glm::dvec3(0.0, 0.0, -plannedAcceleration),
                forward, right, up
            );
        game::navigation::DynamicMotionSystem::updateLocalFrameMotion(
            motion, worldPosition, frame, params, dt
        );
    }

    const double actualSpeed = -motion.localVelocityMps.z;
    require(std::abs(actualSpeed - 17 * dt * plannedAcceleration) < 1.0,
            "Assisted speed response lagged a feasible planned burn beyond the tracking envelope");
    require(actualSpeed < 73.549875 * 17 * dt + 1.0e-6,
            "Assisted compensation bypassed the physical main-engine limit");
}

void testAssistedProgramFeedForwardOnlyAssistsNoseAlignment()
{
    game::navigation::DynamicMotionState motion;
    motion.localControlLaw = game::navigation::LocalFlightControlLaw::Assisted;
    motion.localVelocityMps = glm::dvec3(2.0, 0.0, -98.0);

    game::navigation::KinematicFrame frame;
    frame.systemId = 0;
    frame.frameId = "test";
    frame.valid = true;

    ShipParams params = capabilityParams();
    params.throttleAccel = 5.0f;
    params.maxLinearGs = 7.5f;
    params.strafeAccel = 8.0f;
    params.strafeDamping = 1.0f;
    params.forwardMainEngineAvailable = true;
    params.reverseMainEngineAvailable = true;
    params.forwardMainEngineAccelerationMps2 = 73.549875f;
    params.reverseMainEngineAccelerationMps2 = 73.549875f;

    const glm::vec3 forward(0.0f, 0.0f, -1.0f);
    const glm::vec3 right(1.0f, 0.0f, 0.0f);
    const glm::vec3 up(0.0f, 1.0f, 0.0f);

    // Measured VREL is +X of the nose, so the ordinary Assisted stabilizer
    // correctly demands -X. A -X trajectory feed-forward may assist it.
    game::navigation::DynamicMotionSystem::
        applyNavigationAssistedFlightModel(
            motion, frame, params, 0.02f,
            glm::dvec3(0.0, 0.0, -98.0),
            glm::dvec3(-4.0, 0.0, 0.0),
            forward, right, up
        );

    requireNear(
        motion.assistedStabilizationAccelerationMps2.x,
        -6.0,
        1.0e-5,
        "Assisted alignment did not accept same-direction lateral feed-forward"
    );
    requireNear(
        glm::length(motion.manoeuvreAccelerationMps2),
        0.0,
        1.0e-12,
        "Assisted turn improperly spent precision RCS"
    );

    // Opposite feed-forward must not weaken or reverse nose alignment.
    game::navigation::DynamicMotionSystem::
        applyNavigationAssistedFlightModel(
            motion, frame, params, 0.02f,
            glm::dvec3(0.0, 0.0, -98.0),
            glm::dvec3(4.0, 0.0, 0.0),
            forward, right, up
        );

    requireNear(
        motion.assistedStabilizationAccelerationMps2.x,
        -2.0,
        1.0e-5,
        "Assisted feed-forward was allowed to oppose nose alignment"
    );

    // With no sideways VREL, planner feed-forward must not manufacture one.
    motion.localVelocityMps = glm::dvec3(0.0, 0.0, -98.0);
    game::navigation::DynamicMotionSystem::
        applyNavigationAssistedFlightModel(
            motion, frame, params, 0.02f,
            glm::dvec3(0.0, 0.0, -98.0),
            glm::dvec3(4.0, 0.0, 0.0),
            forward, right, up
        );

    requireNear(
        motion.assistedStabilizationAccelerationMps2.x,
        0.0,
        1.0e-12,
        "Assisted feed-forward created sideways VREL from an aligned state"
    );
}

void testAssistedLateralVelocityTargetDoesNotCreateSlip()
{
    game::navigation::DynamicMotionState motion;
    motion.localControlLaw =
        game::navigation::LocalFlightControlLaw::Assisted;
    motion.localVelocityMps = {0.0, 0.0, -20.0};

    game::navigation::KinematicFrame frame;
    frame.systemId = 0;
    frame.frameId = "nose-coupled-target";
    frame.valid = true;

    ShipParams params = capabilityParams();
    params.strafeAccel = 12.0f;
    params.strafeDamping = 2.0f;
    params.maxLinearGs = 7.5f;
    params.forwardMainEngineAvailable = true;
    params.reverseMainEngineAvailable = true;
    params.forwardMainEngineAccelerationMps2 = 20.0f;
    params.reverseMainEngineAccelerationMps2 = 20.0f;

    const glm::vec3 forward(0.0f, 0.0f, -1.0f);
    const glm::vec3 right(1.0f, 0.0f, 0.0f);
    const glm::vec3 up(0.0f, 1.0f, 0.0f);

    game::navigation::DynamicMotionSystem::
        applyNavigationAssistedFlightModel(
            motion,
            frame,
            params,
            0.02f,
            glm::dvec3(5.0, 0.0, -20.0),
            glm::dvec3(0.0),
            forward,
            right,
            up
        );

    requireNear(
        motion.assistedStabilizationAccelerationMps2.x,
        0.0,
        1.0e-12,
        "lateral target velocity created sideways Assisted equilibrium"
    );
    requireNear(
        glm::length(motion.manoeuvreAccelerationMps2),
        0.0,
        1.0e-12,
        "nose-coupled target unexpectedly spent precision RCS"
    );
}

void testAssistedVehicleCorrectsMeasuredLateralMotion()
{
    game::navigation::DynamicMotionState motion;
    motion.localControlLaw = game::navigation::LocalFlightControlLaw::Assisted;
    motion.localVelocityMps = {3.0, 0.0, -20.0};

    game::navigation::KinematicFrame frame;
    frame.systemId = 0;
    frame.frameId = "lateral-response";
    frame.valid = true;

    ShipParams params = capabilityParams();
    params.strafeAccel = 12.0f;
    params.strafeDamping = 2.0f;
    params.maxLinearGs = 7.5f;
    params.forwardMainEngineAvailable = true;
    params.reverseMainEngineAvailable = true;
    params.forwardMainEngineAccelerationMps2 = 20.0f;
    params.reverseMainEngineAccelerationMps2 = 20.0f;

    const glm::vec3 forward(0.0f, 0.0f, -1.0f);
    const glm::vec3 right(1.0f, 0.0f, 0.0f);
    const glm::vec3 up(0.0f, 1.0f, 0.0f);
    game::navigation::DynamicMotionSystem::
        applyNavigationAssistedFlightModel(
            motion, frame, params, 0.02f,
            glm::dvec3(1.0, 0.0, -20.0), glm::dvec3(0.0),
            forward, right, up
        );

    require(motion.assistedStabilizationAccelerationMps2.x < 0.0,
            "ship must react to actual lateral velocity above its target");
    requireNear(glm::length(motion.manoeuvreAccelerationMps2), 0.0, 1.0e-12,
                "vehicle response must not synthesize precision RCS");
}

void testAngularDemandUsesExistingCapabilityClamp()
{
    ShipTransform transform;
    ShipParams params = capabilityParams();
    WorldParams world {};

    ShipControlState control {};
    control.navigationAccelerationDemandValid = true;
    control.navigationAngularAccelerationDemandSystemRadPerSec2 =
        glm::dvec3(100.0, 0.0, 0.0);

    SharedShipPhysics::evaluateControl(
        transform,
        params,
        control,
        world,
        0.1f
    );

    requireNear(
        transform.pitchRate,
        0.2,
        1.0e-5,
        "direct world angular demand must clamp at the existing angular acceleration envelope"
    );
    requireNear(
        transform.yawRate,
        0.0,
        1.0e-6,
        "pure world-right angular demand must not create yaw"
    );
    requireNear(
        transform.rollRate,
        0.0,
        1.0e-6,
        "pure world-right angular demand must not create roll"
    );
}

void testManualAttitudeOverridesNavigationAngularDemand()
{
    ShipTransform transform;
    ShipParams params = capabilityParams();
    WorldParams world {};

    ShipControlState control {};
    control.navigationAccelerationDemandValid = true;
    control.navigationAngularAccelerationDemandSystemRadPerSec2 =
        glm::dvec3(-100.0, 0.0, 0.0);
    control.pitchInput = 1.0f;

    SharedShipPhysics::evaluateControl(
        transform,
        params,
        control,
        world,
        0.1f
    );

    require(
        transform.pitchRate > 0.0f,
        "material manual attitude input must win over navigation angular demand"
    );
    requireNear(
        transform.pitchRate,
        0.036,
        1.0e-5,
        "a short manual tap must use only the initial fraction of angular authority"
    );
    for (int sample = 0; sample < 4; ++sample)
        SharedShipPhysics::evaluateControl(
            transform, params, control, world, 0.1f);
    requireNear(
        transform.pitchRate,
        0.7048,
        1.0e-4,
        "held manual input must regain full angular authority"
    );
}

void testNpcGoalBecomesNavigationIntentWithoutLegacyControl()
{
    game::navigation::NpcNavigationKinematicState state;
    state.relativeSystemVelocityMps = glm::dvec3(0.0);
    state.forwardSystem = glm::dvec3(0.0, 0.0, -1.0);
    state.rightSystem = glm::dvec3(1.0, 0.0, 0.0);
    state.upSystem = glm::dvec3(0.0, 1.0, 0.0);
    state.pitchRateRadPerSec = 0.5;
    state.yawRateRadPerSec = -0.25;
    state.rollRateRadPerSec = 0.10;

    NpcNavigationGoal goal;
    goal.revision = 42;
    goal.mode = NpcNavigationGoalMode::MaintainForwardCruise;
    goal.desiredForwardSpeedMps = 10.0;
    goal.velocityResponsePerSecond = 0.5;
    goal.angularDampingPerSecond = 2.0;

    const auto intent =
        game::navigation::NpcNavigationIntentController::buildIntent(
            state,
            goal
        );

    require(intent.revision == 42,
            "NPC navigation goal revision must become the runtime intent revision");
    requireNear(
        intent.idealLinearAccelerationSystemMps2.x,
        0.0,
        1.0e-12,
        "identity ship forward cruise must not create lateral X acceleration"
    );
    requireNear(
        intent.idealLinearAccelerationSystemMps2.y,
        0.0,
        1.0e-12,
        "identity ship forward cruise must not create vertical acceleration"
    );
    requireNear(
        intent.idealLinearAccelerationSystemMps2.z,
        -5.0,
        1.0e-12,
        "nominal NPC goal must become a physical forward acceleration demand"
    );

    const glm::dvec3 angularDemand(
        intent.idealAngularAccelerationSystemRadPerSec2.x,
        intent.idealAngularAccelerationSystemRadPerSec2.y,
        intent.idealAngularAccelerationSystemRadPerSec2.z
    );

    requireNear(
        glm::dot(angularDemand, state.rightSystem),
        -1.0,
        1.0e-6,
        "NPC nominal intent must oppose positive pitch rate on the ship-right axis"
    );
    requireNear(
        glm::dot(angularDemand, state.upSystem),
        0.5,
        1.0e-6,
        "NPC nominal intent must oppose negative yaw rate on the ship-up axis"
    );
    requireNear(
        glm::dot(angularDemand, state.forwardSystem),
        -0.2,
        1.0e-6,
        "NPC nominal intent must oppose positive roll rate on the ship-forward axis"
    );
}

void testNpcHoldGoalBrakesRelativeVelocity()
{
    game::navigation::NpcNavigationKinematicState state;
    state.relativeSystemVelocityMps = glm::dvec3(4.0, -2.0, 1.0);

    NpcNavigationGoal goal;
    goal.revision = 5;
    goal.mode = NpcNavigationGoalMode::Hold;
    goal.velocityResponsePerSecond = 0.25;

    const auto intent =
        game::navigation::NpcNavigationIntentController::buildIntent(
            state,
            goal
        );

    requireNear(
        intent.idealLinearAccelerationSystemMps2.x,
        -1.0,
        1.0e-12,
        "hold goal must brake actual relative X velocity"
    );
    requireNear(
        intent.idealLinearAccelerationSystemMps2.y,
        0.5,
        1.0e-12,
        "hold goal must brake actual relative Y velocity"
    );
    requireNear(
        intent.idealLinearAccelerationSystemMps2.z,
        -0.25,
        1.0e-12,
        "hold goal must brake actual relative Z velocity"
    );
}

void testReplicatedNavigationExecutionStateIsReadOnlyTruth()
{
    game::navigation::ReplicatedNavigationExecutionState state;

    game::navigation::ReplicatedNavigationExecution ignored;
    ignored.entityId = EntityId{7u};
    ignored.execution.valid = false;

    game::navigation::ReplicatedNavigationExecution accepted;
    accepted.entityId = EntityId{42u};
    accepted.shipInstanceId = 4242u;
    accepted.execution.valid = true;
    accepted.execution.intentRevision = 100u;
    accepted.execution.activeTargetRevision = 99u;
    accepted.execution.executedLinearAccelerationDemandSystemMps2 =
        glm::dvec3(1.0, 2.0, 3.0);

    state.replace({ignored, accepted});

    require(state.all().size() == 1u,
            "replicated navigation state must retain only valid server truth");
    const auto* found = state.find(EntityId{42u});
    require(found != nullptr,
            "replicated navigation state must resolve the authoritative entity");
    require(found->execution.intentRevision == 100u,
            "replicated navigation state must preserve intent revision exactly");
    requireNear(
        found->execution.executedLinearAccelerationDemandSystemMps2.y,
        2.0,
        0.0,
        "replicated navigation state must preserve executed demand exactly"
    );
    require(state.find(EntityId{7u}) == nullptr,
            "invalid replicated navigation rows must not become visible truth");

    const auto* byStableAsset = state.find(
        game::navigation::NavigationAssetRef::ship(4242u)
    );
    require(byStableAsset != nullptr,
            "replicated navigation truth must resolve a stable ship instance");
    require(byStableAsset->entityId == EntityId{42u},
            "stable ship-instance lookup must resolve the authoritative entity");

    require(
        state.find(game::navigation::NavigationAssetRef::ship(9999u)) == nullptr,
        "unknown stable ship instance must not fabricate execution truth"
    );
}

void testBridgeDemandCanReachCapabilityLayerWithoutLegacyKeys()
{
    Bridge bridge(expertProfile());

    Bridge::Intent initial;
    initial.revision = 7;
    require(bridge.reset(0.0, initial),
            "end-to-end bridge reset must succeed");

    Bridge::Intent intent;
    intent.revision = 8;
    intent.idealAngularAccelerationSystemRadPerSec2 =
        {100.0, 0.0, 0.0};

    // Let the expert executor ramp toward the request.
    Bridge::StepResult result;
    for (int i = 1; i <= 20; ++i)
    {
        result = bridge.step(
            0.01 * static_cast<double>(i),
            0.01,
            intent
        );
    }

    require(result.snapshot.valid,
            "bridge must produce a valid executed demand");

    ShipTransform transform;
    const ShipParams params = capabilityParams();
    WorldParams world {};

    SharedShipPhysics::evaluateControl(
        transform,
        params,
        result.control,
        world,
        0.1f
    );

    require(
        transform.pitchRate > 0.0f,
        "bridge direct demand must reach the actual ship angular-control path"
    );
    require(
        transform.pitchRate <= 0.20001f,
        "actual ship capability must remain authoritative downstream of pilot skill"
    );
}

} // namespace

int main()
{
    try
    {
        testPredictivePilotUsesOnlyOrdinaryAssistedControls();
        testPredictivePilotBrakesAngularMotionBeforeOvershoot();
        testPredictivePilotLearnsMeasuredPitchAuthority();
        testPredictivePilotUsesOrdinaryNewtonianThrottle();
        testPredictivePilotUsesRcsForSmallAuthoredStopResidual();
        testPredictivePilotUsesEndForAuthoredStop();
        testBridgePublishesOneDirectDemandSample();
        testLinearDemandUsesRealMainAndManoeuvreAuthority();
        testPrecisionVelocityTrimUsesOnlyPhysicalRcs();
        testVehicleBridgePublishesMotionTargetWithoutSelectingEngines();
        testVehicleBridgeRejectsInvalidVelocityAndClock();
        testAngularDemandUsesExistingCapabilityClamp();
        testManualAttitudeOverridesNavigationAngularDemand();
        testNpcGoalBecomesNavigationIntentWithoutLegacyControl();
        testNpcHoldGoalBrakesRelativeVelocity();
        testReplicatedNavigationExecutionStateIsReadOnlyTruth();
        testBridgeDemandCanReachCapabilityLayerWithoutLegacyKeys();

        std::cout << "NAVIGATION RUNTIME CONTROL TESTS: PASS\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "NAVIGATION RUNTIME CONTROL TESTS: FAIL: "
                  << error.what() << '\n';
        return 1;
    }
}
