#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")

def require(cond: bool, msg: str) -> None:
    if not cond:
        raise SystemExit(f"[FAIL] {msg}")

CONTROL = read("src/game/ship/core/ShipControlState.h")
# PredictivePilot was removed (a1e7847a, mixed-law pilot) and ClientRouteAutopilot
# became the per-law DedicatedClientRouteAutopilot (AssistedClientRouteAutopilot /
# NewtonianClientRouteAutopilot via DedicatedClientRouteAutopilotLaws.h). The
# runtime law seam for server NPCs is AutopilotLawController.
LAW = read("src/game/navigation/autopilot/AutopilotLawController.h")
CLIENT_AUTO = read("src/game/navigation/autopilot/DedicatedClientRouteAutopilot.h")
SPACE = read("src/game/SpaceState.cpp") + read("src/game/SpaceStateDocking.cpp")
SERVER = read("src/game/server/GameServer.cpp")
SIM = read("src/game/simulation/GameSimulation.cpp")
MOTION = read("src/game/navigation/DynamicMotionSystem.cpp")
SHARED = read("src/game/shared/SharedShipPhysics.cpp")
GATEWAY = read("src/game/ship/control/ShipControlExecutionGateway.cpp")
TEST_CMAKE = read("tests/navigation_runtime/CMakeLists.txt")
CLIENT_TEST = read("tests/navigation_runtime/ClientRouteAutopilotTests.cpp")

for marker in (
    "pitchInput", "yawInput", "rollInput", "targetSpeedRate",
    "forwardInput", "strafeInput", "liftInput", "velocityAlignmentCommand",
):
    require(marker in CONTROL, f"ordinary ShipControlState field missing: {marker}")

for marker in (
    "class AutopilotLawController final",
    "case LocalFlightControlLaw::Assisted:",
    "case LocalFlightControlLaw::Newtonian:",
    "AutopilotControlCommand update(",
):
    require(marker in LAW, f"single flight-law virtual-pilot seam missing: {marker}")

for marker in (
    "class DedicatedClientRouteAutopilot final",
    "sampleContinuousReference(",
    "ManeuverTrackingController::track(",
    "sampleRouteCurveAtProgress(",
    "ShipControlState",
):
    require(
        marker in CLIENT_AUTO,
        f"client continuous-route virtual-pilot contract missing: {marker}",
    )

for forbidden in ("TrajectoryFollower", "RouteFollower::follow(", "steeringRay"):
    require(
        forbidden not in CLIENT_AUTO,
        f"client autopilot regained retired/private route mechanism: {forbidden}",
    )

for marker in (
    "AssistedClientRouteAutopilot::start(",
    "NewtonianClientRouteAutopilot::start(",
    "AssistedClientAutopilot::update(",
    "NewtonianClientAutopilot::update(",
    "m_client->submitInput(m_clientAutopilotControl)",
    "control_path=ShipControlState",
):
    require(marker in SPACE, f"SpaceState client execution path missing: {marker}")

for forbidden in (
    "PredictivePilot",
    "DedicatedClientRouteAutopilot",
    "RouteFollower",
    "AcceptedManeuverProgram",
    "RoutePlanner",
    "TrajectoryGenerator",
    "DockingAutomaticRuntime",
    "beginAutomaticDocking",
):
    require(forbidden not in SERVER,
            f"GameServer regained navigation execution ownership: {forbidden}")

require("submitCommand(controlledEntityId, payload)" in SERVER,
        "server no longer receives ordinary ShipControlState")
require("ship.setControlState(cmd)" in SERVER,
        "server fixed-step control stream no longer drives ship state")

for marker in ("control.pitchInput", "control.yawInput", "control.rollInput"):
    require(marker in SHARED, f"SharedShipPhysics ordinary control path missing: {marker}")

require("applyLocalFrameInput(" in MOTION,
        "ordinary local-frame flight law entry point missing")
require(
    "DynamicMotionSystem::applyLocalFrameInput(" in "".join(GATEWAY.split()),
    "ship control gateway no longer owns ordinary local-frame controls",
)
require("shipPtr->updateTranslationControl(motionControlDt)" in SIM,
        "GameSimulation no longer delegates ordinary controls to ship gateway")
require("DynamicMotionSystem::applyLocalFrameInput(" not in SIM,
        "GameSimulation bypassed ship control gateway")

for marker in (
    "testClientAutopilotEmitsOrdinaryControls",
    "testClientStabilizerUsesOrdinaryControls",
    "navigationAccelerationDemandValid",
    "navigationVelocityTargetValid",
):
    require(marker in CLIENT_TEST, f"client autopilot regression missing: {marker}")

require("client_route_autopilot_tests" in TEST_CMAKE,
        "client route autopilot CTest target is not registered")

print("NAVIGATION LIVE RUNTIME CONTROL CONTRACT: PASS")
print(" - client owns DedicatedClientRouteAutopilot/ManeuverTrackingController execution")
print(" - virtual pilot emits the same ShipControlState surface as Human input")
print(" - server is navigation-blind and applies ordinary numbered controls through ShipControlExecutionGateway")
print(" - V2 follows the accepted continuous centerline without a retired private follower")
