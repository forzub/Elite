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
system = read("src/game/navigation/DynamicMotionSystem.cpp")
mapper = read("src/game/ship/controller/PlayerInputMapper.cpp")
shared = read("src/game/shared/SharedShipPhysics.cpp")
simulation = read("src/game/simulation/GameSimulation.cpp")
prediction = read("src/game/client/ClientHubTacticalPrediction.h")
params = read("src/game/ship/core/ShipParams.h")
controller = read("src/game/ship/ShipController.cpp")
impulse = read("src/game/ship/physics/ShipImpulseSystem.cpp")
cobra = read("src/game/ship/descriptors/EliteCobraMk1.cpp")

for token in (
    "LocalFlightControlLaw localControlLaw",
    "VelocityAlignmentMode velocityAlignmentMode",
):
    if token not in state:
        fail(f"persistent motion state lost: {token}")

for token in (
    "LocalFlightControlLaw::Newtonian",
    "params.maxCombatSpeed",
    "params.maxGs",
    "VelocityAlignmentMode::BrakeToStop",
    "motion.localVelocityMps",
):
    if token not in system:
        fail(f"shared local motion law lost: {token}")

# Local speed and acceleration must be ship-profile limits, not a second J-like
# unbounded propulsion path hidden in one control law.
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
):
    if token not in mapper:
        fail(f"production input mapping lost: {token}")

if "LocalFlightControlLaw::Assisted" not in shared:
    fail("shared mode-switch path lost Assisted law")

for token in (
    "control.localControlLawCommandValid",
    "control.requestedLocalControlLaw",
    "control.velocityAlignmentCommand",
):
    if token not in shared:
        fail(f"shared server/client attitude path lost command: {token}")

if "shipPtr->core().desc().physics" not in simulation:
    fail("server local motion no longer uses per-ship physics profile")

if "const ShipParams& params" not in prediction:
    fail("client prediction no longer consumes the same per-ship physics profile")

if "float maxGs" not in params:
    fail("ship profile lost the shared acceleration envelope")

for token in (
    "angularAccelerationEnvelope",
    "angularRateEnvelope",
    "params.maxGs",
    "params.turnRadius",
):
    if token not in controller:
        fail(f"shared angular safety envelope lost: {token}")

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
    "massKg",
    "pitchInertiaKgM2",
    "yawInertiaKgM2",
    "rollInertiaKgM2",
):
    if token not in params:
        fail(f"ship mass property lost: {token}")

if "desc.physics.maxCombatSpeed         = 500.0f" not in cobra:
    fail("Cobra controlled local speed envelope is no longer 500 m/s")

print("Local-flight-control architecture check passed.")
