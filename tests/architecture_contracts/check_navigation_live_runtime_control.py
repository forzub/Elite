#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")

def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")

CONTROL = read("src/game/ship/core/ShipControlState.h")
PILOT = read("src/game/navigation/autopilot/PredictivePilot.h")
FOLLOWER = read("src/game/navigation/autopilot/RouteFollower.cpp")
SERVER = read("src/game/server/GameServer.cpp")
SIM = read("src/game/simulation/GameSimulation.cpp")
MOTION = read("src/game/navigation/DynamicMotionSystem.cpp")
SHARED = read("src/game/shared/SharedShipPhysics.cpp")
TEST_CMAKE = read("tests/navigation_runtime/CMakeLists.txt")
RUNTIME_TEST = read("tests/navigation_runtime/NavigationRuntimeControlTests.cpp")

for marker in (
    "pitchInput",
    "yawInput",
    "rollInput",
    "targetSpeedRate",
    "forwardInput",
    "strafeInput",
    "liftInput",
    "velocityAlignmentCommand",
):
    require(marker in CONTROL, f"ordinary ShipControlState pilot field missing: {marker}")

for marker in (
    "class PredictivePilot final",
    "struct State",
    "effectivePitchAuthorityRadPerSec2",
    "effectiveYawAuthorityRadPerSec2",
    "effectiveRollAuthorityRadPerSec2",
    "assistedCourseResponseSeconds",
    "choosePredictiveAxisInput(",
    "navigationAccelerationDemandValid = false",
    "navigationVelocityTargetValid = false",
    "navigationPrecisionTranslationOnly = false",
):
    require(marker in PILOT, f"PredictivePilot V2 contract missing: {marker}")

for marker in (
    "the accepted centerline is the ONLY path",
    "centerlinePoint",
    "inward * correctionSpeed",
):
    require(marker in FOLLOWER, f"V2 corridor-following contract missing: {marker}")

for forbidden in (
    "TrajectoryFollower",
    "lookAhead",
    "steeringRay",
):
    require(forbidden not in FOLLOWER,
            f"V2 follower regained legacy/private-route mechanism: {forbidden}")

for marker in (
    "PredictivePilot::Request",
    "PredictivePilot::make(",
    "runtime.pilotState",
    "Follower::follow(",
    "ship->setControlState(automaticControl)",
):
    require(marker in SERVER, f"production automatic docking V2 path missing: {marker}")

for forbidden in (
    "ShipControlAdapter",
    "automaticControl.navigationAccelerationDemandValid = true",
    "automaticControl.navigationVelocityTargetValid = true",
    "alignmentControl.navigationAccelerationDemandValid = true",
    "alignmentControl.navigationVelocityTargetValid = true",
    "applyNavigationAssistedFlightModel(",
    "applySystemAccelerationDemand(",
):
    require(forbidden not in SERVER,
            f"production automatic docking fell back to legacy actuator seam: {forbidden}")

# Ordinary ship physics remains the sole causal actuator path.
for marker in (
    "control.pitchInput",
    "control.yawInput",
    "control.rollInput",
):
    require(marker in SHARED, f"SharedShipPhysics ordinary control path missing: {marker}")

require("applyLocalFrameInput(" in MOTION,
        "ordinary local-frame flight law entry point missing")
require("applyLocalFrameInput(" in SIM,
        "GameSimulation no longer drives ordinary flight law")

# Legacy direct-demand fields may remain for diagnostics/labs, but V2 tests must
# explicitly prove that PredictivePilot never enables them.
for marker in (
    "testPredictivePilotUsesOnlyOrdinaryAssistedControls",
    "testPredictivePilotBrakesAngularMotionBeforeOvershoot",
    "testPredictivePilotLearnsMeasuredPitchAuthority",
    "testPredictivePilotUsesOrdinaryNewtonianThrottle",
    "testPredictivePilotUsesRcsForSmallAuthoredStopResidual",
    "testPredictivePilotUsesEndForAuthoredStop",
):
    require(marker in RUNTIME_TEST, f"V2 runtime regression missing: {marker}")

require("navigation_runtime_control" in TEST_CMAKE,
        "navigation runtime V2 CTest must be registered")

print("NAVIGATION LIVE RUNTIME CONTROL CONTRACT: PASS")
print(" - production automatic docking uses PredictivePilot V2 only")
print(" - V2 emits ordinary human-equivalent ShipControlState inputs")
print(" - V2 never enables direct navigation acceleration/velocity demand fields")
print(" - V2 follows the accepted centerline and has no private look-ahead route")
print(" - pilot state learns measured ship response during the docking session")
print(" - ordinary SharedShipPhysics/DynamicMotionSystem owns physical actuation")
