#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")

def require(cond: bool, msg: str) -> None:
    if not cond:
        raise SystemExit(f"[FAIL] {msg}")

CONTROL = read("src/game/ship/core/ShipControlState.h")
PILOT = read("src/game/navigation/autopilot/PredictivePilot.h")
CLIENT_AUTO = read("src/game/navigation/autopilot/ClientRouteAutopilot.h")
SPACE = read("src/game/SpaceState.cpp")
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
    "class PredictivePilot final",
    "effectivePitchAuthorityRadPerSec2",
    "assistedCourseResponseSeconds",
    "choosePredictiveAxisInput(",
    "navigationAccelerationDemandValid = false",
    "navigationVelocityTargetValid = false",
    "navigationPrecisionTranslationOnly = false",
):
    require(marker in PILOT, f"PredictivePilot V2 contract missing: {marker}")

for marker in (
    "Execution owns one continuous spatial reference assembled from all",
    "Authoritative spatial geometry.",
    "sampleContinuousReference(",
    "Advance strictly along the already accepted centerline.",
    "ManeuverTrackingController::track(",
    "sampleRouteCurveAtProgress(",
):
    require(
        marker in CLIENT_AUTO,
        f"client continuous-route execution contract missing: {marker}",
    )

for forbidden in ("TrajectoryFollower", "RouteFollower::follow(", "steeringRay"):
    require(
        forbidden not in CLIENT_AUTO,
        f"client autopilot regained retired/private route mechanism: {forbidden}",
    )

for marker in (
    "class ClientRouteAutopilot final",
    "sampleContinuousReference(",
    "ManeuverTrackingController::track(",
    "PredictivePilot::make(",
    "ReferenceMode::SpatialCorridor",
    "ShipControlState",
):
    require(marker in CLIENT_AUTO, f"client virtual-pilot contract missing: {marker}")

for marker in (
    "ClientAutopilot::stabilize(",
    "ClientAutopilot::start(",
    "ClientAutopilot::update(",
    "m_client->submitInput(m_clientAutopilotControl)",
    "control_path=ShipControlState",
):
    require(marker in SPACE, f"SpaceState client execution path missing: {marker}")

for forbidden in (
    "PredictivePilot",
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
print(" - client owns ClientRouteAutopilot/ManeuverTrackingController/PredictivePilot execution")
print(" - virtual pilot emits the same ShipControlState surface as Human input")
print(" - server is navigation-blind and applies ordinary numbered controls through ShipControlExecutionGateway")
print(" - V2 follows the accepted continuous centerline without a retired private follower")
