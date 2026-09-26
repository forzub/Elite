#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8")


def fail(message: str) -> None:
    print(f"Local-flight-control architecture check failed: {message}", file=sys.stderr)
    raise SystemExit(1)


state = read("src/game/navigation/DynamicMotionState.h")
flight_state_machine = read(
    "src/game/navigation/LocalFlightControlStateMachine.h"
)
system = read("src/game/navigation/DynamicMotionSystem.cpp")
mapper = read("src/game/ship/controller/PlayerInputMapper.cpp")
client = read("src/game/client/GameClient.cpp")
space = read("src/game/SpaceState.cpp")
shared = read("src/game/shared/SharedShipPhysics.cpp")
simulation = read("src/game/simulation/GameSimulation.cpp")
prediction = read("src/game/client/ClientHubTacticalPrediction.h")
params = read("src/game/ship/core/ShipParams.h")
dynamics = read("src/game/ship/core/ShipDynamics.h")
propulsion = read("src/game/ship/ShipPropulsionState.h")
compiler = read("src/game/navigation/OrdinaryPhysicalManeuverCompiler.cpp")
controller = read("src/game/ship/ShipController.cpp")
impulse = read("src/game/ship/physics/ShipImpulseSystem.cpp")
cobra = read("src/game/ship/descriptors/EliteCobraMk1.cpp")
cockpit = read("src/game/ship/descriptors/EliteCobraMk1_Cockpit.cpp")
player_view = read("src/game/ship/view/PlayerShipView.cpp")

if "LocalFlightControlLaw::Assisted" not in state.split(
        "LocalFlightControlLaw localControlLaw", 1
    )[1].split(";", 1)[0]:
    fail("fresh DynamicMotionState no longer defaults to Assisted")

control_state = read("src/game/ship/core/ShipControlState.h")
if "requestedLocalControlLaw =\n        game::navigation::LocalFlightControlLaw::Assisted" not in control_state:
    fail("fresh ShipControlState law request no longer defaults to Assisted")

game_client_h = read("src/game/client/GameClient.h")
game_client_cpp = read("src/game/client/GameClient.cpp")
if "m_pendingLocalControlLaw =\n        game::navigation::LocalFlightControlLaw::Assisted" not in game_client_h:
    fail("client pending law latch no longer defaults to Assisted")
if "m_pendingLocalControlLaw =\n        game::navigation::LocalFlightControlLaw::Assisted" not in game_client_cpp:
    fail("client synchronization reset no longer restores Assisted default")

for token in (
    "LocalFlightControlLaw localControlLaw",
    "VelocityAlignmentMode velocityAlignmentMode",
    "mainEngineAccelerationMps2",
    "manoeuvreAccelerationMps2",
    "manoeuvreGasPressure01",
    "manoeuvreGasDepleted",
):
    if token not in state:
        fail(f"persistent motion state lost: {token}")

for token in (
    "LocalFlightControlLaw::Newtonian",
    "game::ship::controlledSpeedLimitMps(params)",
    "game::ship::forwardMainAccelerationLimitMps2(params)",
    "game::ship::reverseMainAccelerationLimitMps2(params)",
    "VelocityAlignmentMode::BrakeToStop",
    "motion.localVelocityMps",
):
    if token not in system:
        fail(f"shared local motion law lost: {token}")

for token in (
    "else if (reverseMainAccel > 1.0e-9)",
    "motion.mainEngineAccelerationMps2 = -f * brakeAccel",
    "-f * (mainThrustCommand * reverseMainAccel)",
):
    if token not in system:
        fail(f"Newtonian surviving-fore-main fallback lost: {token}")

# Ship-specific numerical policy is intentionally centralized in ShipDynamics.
# DynamicMotionSystem must consume these accessors instead of reopening raw
# ShipParams fields and inventing a second interpretation.
for token in (
    "controlledSpeedLimitMps",
    "params.maxCombatSpeed",
    "effectiveLinearGs",
    "params.maxLinearGs > 0.0f",
    "params.maxGs",
    "forwardMainAccelerationLimitMps2",
    "reverseMainAccelerationLimitMps2",
):
    if token not in dynamics:
        fail(f"central ship-dynamics policy lost: {token}")

for token in (
    "manoeuvreAccelerationLimit",
    "controlledMainAcceleration + actualManoeuvreLocalAcceleration",
    "requestedMainLocalAcceleration + actualManoeuvreLocalAcceleration",
    "manoeuvreGasPressure01",
    "manoeuvreGasUsePerSecond",
    "manoeuvreGasRechargePerSecond",
    "manoeuvreGasRestartFraction",
    "yieldAxisToManualRcs",
):
    if token not in system:
        fail(f"law-independent keypad RCS contract lost: {token}")

# Ordinary/main propulsion retains the ship-profile control envelope. The only
# deliberate exception is the tiny gas-limited keypad RCS in Newtonian mode; it
# may accumulate delta-v past maxCombatSpeed without turning the main engine
# into a second J-like propulsion path.
for forbidden in (
    "const double maxCombatSpeed =\n        500.0",
    "maxTacticalAccel =\n        49.0",
):
    if forbidden in system:
        fail(f"hard-coded player-only motion limit returned: {forbidden}")

for token in (
    "const bool f10Down = keys.isKeyPressed(GLFW_KEY_F10)",
    "CtrlF10State::ReleaseDebounce",
    "kCtrlF10ReleaseDebounceSamples",
    "GLFW_KEY_HOME",
    "GLFW_KEY_INSERT",
    "GLFW_KEY_END",
    "localControlLawCommandValid",
    "LocalFlightControlStateMachine::next",
):
    if token not in mapper:
        fail(f"production input mapping lost: {token}")

for token in (
    "currentLocalControlLaw",
    "playerIt->second.transform.motion.localControlLaw",
):
    if token not in (mapper + space):
        fail(f"flight-law mapping lost current-state derivation: {token}")

for token in (
    "m_hasPendingLocalControlLawCommand = true",
    "m_pendingLocalControlLaw",
    "m_latestControl.localControlLawCommandValid = false",
    "consumeLocalControlLawCommand",
    "m_hasPendingLocalControlLawCommand = false",
):
    if token not in client:
        fail(f"fixed-step discrete-command latch lost: {token}")

for token in (
    "defaultLaw()",
    "LocalFlightControlLaw::Assisted",
    "next(",
    "transition(",
    "motion.localControlLaw = requested",
    "velocityAlignmentOwnsAttitude",
    "motion.targetForwardSpeedMps",
):
    if token not in flight_state_machine:
        fail(f"central flight state machine lost: {token}")

for token in (
    "control.localControlLawCommandValid",
    "control.requestedLocalControlLaw",
    "LocalFlightControlStateMachine::transition",
    "control.velocityAlignmentCommand",
):
    if token not in shared:
        fail(f"shared server/client attitude path lost state transition: {token}")

if "motion.localControlLaw = control.requestedLocalControlLaw" in shared:
    fail("SharedShipPhysics bypassed the flight state machine")

if "LocalFlightControlStateMachine::defaultLaw()" not in space:
    fail("SpaceState input fallback bypassed the authoritative Assisted default")

if "velocityAlignmentOwnsAttitude" not in controller:
    fail("ShipController reopened flight-law attitude ownership outside state machine")

for token in (
    "shipPtr->core().effectivePhysics()",
    "const ShipParams effectivePhysics",
):
    if token not in simulation:
        fail(f"server local motion lost damage-aware propulsion profile: {token}")

if "game::ship::effectiveShipPhysics" not in space:
    fail("manual docking planning no longer consumes current engine health")

if "const ShipParams& params" not in prediction:
    fail("client prediction no longer consumes the same per-ship physics profile")

if "float maxGs" not in params:
    fail("ship profile lost the shared acceleration envelope")

for token in (
    "manoeuvreThrusterAccel",
    "manoeuvreGasUsePerSecond",
    "manoeuvreGasRechargePerSecond",
    "manoeuvreGasRestartFraction",
    "forwardMainEngineAvailable",
    "reverseMainEngineAvailable",
    "forwardMainEngineAccelerationMps2",
    "reverseMainEngineAccelerationMps2",
):
    if token not in params:
        fail(f"ship profile lost propulsion parameter: {token}")

for token in (
    "propulsionBankOperational",
    "applyRuntimeMainPropulsionState",
    "Main propulsion is deliberately binary",
):
    if token not in propulsion:
        fail(f"binary runtime propulsion contract lost: {token}")

for forbidden in (
    "operationalFraction",
    "staticForwardAuthority * aftFraction",
    "staticReverseAuthority * foreFraction",
):
    if forbidden in propulsion:
        fail(f"proportional main-engine derating returned: {forbidden}")

for token in (
    "game::ship::angularAccelerationLimitRadPerSec2(params)",
    "game::ship::angularLoadRateLimitRadPerSec(params)",
    "game::ship::pitchRateLimitRadPerSec(params)",
    "game::ship::yawRateLimitRadPerSec(params)",
    "game::ship::rollRateLimitRadPerSec(params)",
):
    if token not in controller:
        fail(f"shared angular safety envelope lost: {token}")

for token in (
    "angularAccelerationLimitRadPerSec2",
    "angularLoadRateLimitRadPerSec",
    "params.angularAccel",
    "params.maxGs",
    "params.turnRadius",
):
    if token not in dynamics:
        fail(f"central angular dynamics policy lost: {token}")

# Normal-flight limits constrain propulsion/RCS authority, never physical state.
# External collision/explosion impulses are allowed to create linear and angular
# overspeed, which real engines then have to remove over time.
for forbidden in (
    "motion.localVelocityMps =\n            clampMagnitude",
    "ship.pitchRate = glm::clamp",
    "ship.yawRate = glm::clamp",
    "ship.rollRate = glm::clamp",
):
    if forbidden in system or forbidden in controller:
        fail(f"physical velocity/rate hard clamp returned: {forbidden}")

for token in (
    "limitPropulsionAccelerationToControlledSpeed",
    "allowedRadius = std::max(controlledSpeedLimit, currentSpeed)",
    "game::ship::controlledSpeedLimitMps(params)",
    "game::ship::forwardMainAccelerationLimitMps2(params)",
):
    if token not in system:
        fail(f"controlled-speed propulsion boundary lost: {token}")

for token in (
    "glm::cross(leverArmWorldMeters, impulseWorldNewtonSeconds)",
    "deltaAngularVelocityBodyRadPerSec",
    "ship.motion.localVelocityMps +=",
    "ship.yawRate +=",
):
    if token not in impulse:
        fail(f"rigid-body collision impulse response lost: {token}")

for token in (
    "maxLinearGs",
    "massKg",
    "pitchInertiaKgM2",
    "yawInertiaKgM2",
    "rollInertiaKgM2",
):
    if token not in params:
        fail(f"ship mass property lost: {token}")

if "desc.physics.maxCombatSpeed         = 500.0f" not in cobra:
    fail("Cobra controlled local speed envelope is no longer 500 m/s")
if "desc.physics.maxGs                  = 5.0f" not in cobra:
    fail("Cobra angular/load envelope is no longer the accepted 5 g")
if "desc.physics.maxLinearGs            = 7.5f" not in cobra:
    fail("Cobra linear acceleration envelope is no longer the accepted 7.5 g")
for token in (
    "desc.physics.manoeuvreThrusterAccel = 2.0f",
    "desc.physics.manoeuvreGasUsePerSecond = 0.20f",
    "desc.physics.manoeuvreGasRechargePerSecond = 0.08f",
    "desc.physics.manoeuvreGasRestartFraction = 0.20f",
    "desc.physics.forwardMainEngineAvailable = true",
    "desc.physics.reverseMainEngineAvailable = true",
    "desc.physics.forwardMainEngineAccelerationMps2 = 73.549875f",
    "desc.physics.reverseMainEngineAccelerationMps2 = 73.549875f",
    '"ship_fore_engine_L"',
    '"ship_fore_engine_R"',
    "desc.mainPropulsion.aftEngineModuleIds",
    "desc.mainPropulsion.foreEngineModuleIds",
):
    if token not in cobra:
        fail(f"Cobra propulsion topology disappeared: {token}")

for token in (
    "LocalFlightControlLaw::Assisted",
    "maxForwardMainAccelerationMps2",
    "maxReverseMainAccelerationMps2",
    "usesReverseMain",
    "requiredHullForward = -thrustDirection",
):
    if token not in compiler:
        fail(f"B5 failed-engine maneuver support lost: {token}")


for token, text, label in (
    ("manoeuvre_gas_fill", cockpit, "cockpit geometry"),
    ("manoeuvre_gas_fill", player_view, "cockpit state update"),
    ("manoeuvreGasPressure01", player_view, "cockpit state update"),
    ("ship.transform.motion.manoeuvreGasPressure01", space, "replicated cockpit feed"),
):
    if token not in text:
        fail(f"manoeuvre-gas cockpit contract lost in {label}: {token}")

print("Local-flight-control architecture check passed.")
