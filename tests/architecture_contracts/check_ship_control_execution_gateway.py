#!/usr/bin/env python3

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")


def fail(message: str) -> None:
    print(
        f"Ship-control execution gateway architecture check failed: {message}",
        file=sys.stderr,
    )
    raise SystemExit(1)


gateway_h = read("src/game/ship/control/ShipControlExecutionGateway.h")
gateway_cpp = read("src/game/ship/control/ShipControlExecutionGateway.cpp")
simulation = read("src/game/simulation/GameSimulation.cpp")
ship_core = read("src/game/ship/core/ShipCore.cpp")
prediction = read("src/game/client/ClientHubTacticalPrediction.h")
client_world = read("src/game/client/ClientWorldState.cpp")
presentation = read(
    "src/game/client/presentation/LocalPredictedPresentation.h"
)
client_message = read("src/game/network/ClientMessage.h")
server = read("src/game/server/GameServer.cpp")

for token in (
    "class ShipControlExecutionGateway final",
    "TranslationPath",
    "selectTranslationPath(",
    "evaluateAttitudeControl(",
    "integrateAttitude(",
    "propagateOrientation(",
    "evaluateTranslationControl(",
    "propagateHubTacticalTranslation(",
):
    if token not in gateway_h:
        fail(f"gateway public seam lost: {token}")

# The gateway is vehicle/runtime code. It must not know transport, server,
# client presentation, UI, navigation planner or world orchestration.
for forbidden in (
    "GameServer",
    "GameClient",
    "SpaceState",
    "ClientWorldState",
    "ClientMessage",
    "RoutePlanner",
    "RouteFollower",
    "PredictivePilot",
    "SystemMap",
    "Renderer",
    "ITransport",
):
    if forbidden in gateway_h or forbidden in gateway_cpp:
        fail(f"gateway leaked upward into orchestration/presentation: {forbidden}")

# Current backends remain private implementation details behind the gateway.
for token in (
    "SharedShipPhysics::evaluateControl(",
    "SharedShipPhysics::integrate(",
    "SharedShipPhysics::propagateOrientation(",
    "DynamicMotionSystem::applyLocalFrameInput(",
    "DynamicMotionSystem::applyNavigationAssistedFlightModel(",
    "DynamicMotionSystem::applySystemAccelerationDemand(",
    "DynamicMotionSystem::applyNavigationPrecisionVelocityTrim(",
    "DynamicMotionSystem::updateLocalFrameMotion(",
):
    if token not in gateway_cpp:
        fail(f"gateway lost current backend delegation: {token}")

# Server simulation owns cadence/frame orchestration only; it cannot interpret
# ShipControlState to select a propulsion/control law.
for forbidden in (
    "DynamicMotionSystem::applyLocalFrameInput(",
    "DynamicMotionSystem::applyNavigationAssistedFlightModel(",
    "DynamicMotionSystem::applySystemAccelerationDemand(",
    "DynamicMotionSystem::applyNavigationPrecisionVelocityTrim(",
    "manualTranslationOverride",
):
    if forbidden in simulation:
        fail(f"GameSimulation bypasses gateway: {forbidden}")

for token in (
    "shipPtr->updateTranslationControl(motionControlDt)",
    "shipPtr->propagateHubTacticalTranslation(dt)",
):
    if token not in simulation:
        fail(f"GameSimulation lost gateway-owned ship execution: {token}")

# ShipCore is the authoritative server-side ship boundary.
if "ShipControlExecutionGateway" not in ship_core:
    fail("ShipCore does not use ShipControlExecutionGateway")
for forbidden in ("SharedShipPhysics::", "DynamicMotionSystem::"):
    if forbidden in ship_core:
        fail(f"ShipCore bypasses gateway backend: {forbidden}")

# Fixed and fractional client prediction must use the same seam.
for rel, body in (
    ("ClientHubTacticalPrediction.h", prediction),
    ("ClientWorldState.cpp", client_world),
    ("LocalPredictedPresentation.h", presentation),
):
    if "ShipControlExecutionGateway" not in body and rel != "ClientWorldState.cpp":
        fail(f"{rel} does not cross ShipControlExecutionGateway")
    for forbidden in (
        "DynamicMotionSystem::applyLocalFrameInput(",
        "DynamicMotionSystem::applyNavigationAssistedFlightModel(",
        "DynamicMotionSystem::applySystemAccelerationDemand(",
        "DynamicMotionSystem::applyNavigationPrecisionVelocityTrim(",
        "SharedShipPhysics::integrate(",
    ):
        if forbidden in body:
            fail(f"{rel} bypasses gateway backend: {forbidden}")

if "ShipControlExecutionGateway::integrateAttitude(" not in client_world:
    fail("fixed client attitude prediction bypasses gateway")

# Network authority contract remains command-based: the client sends ordinary
# ShipControlState, never authoritative force/position results.
if "ShipControlState" not in client_message:
    fail("ClientMessage no longer carries ShipControlState")
for token in (
    "std::is_same_v<PayloadT, ShipControlState>",
    "submitCommand(controlledEntityId, payload)",
):
    if token not in server:
        fail(f"server control ingress contract lost: {token}")

print("Ship-control execution gateway architecture check passed.")
print(" - GameSimulation owns cadence/frame orchestration only")
print(" - ShipCore/client prediction share one execution seam")
print(" - current physics backends are private behind the gateway")
print(" - network still transports ShipControlState, not authoritative motion")
