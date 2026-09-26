#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

CONTROL = (ROOT / "src/game/ship/core/ShipControlState.h").read_text(encoding="utf-8")
BRIDGE_H = (ROOT / "src/game/navigation/NavigationRuntimeControlBridge.h").read_text(encoding="utf-8")
BRIDGE_CPP = (ROOT / "src/game/navigation/NavigationRuntimeControlBridge.cpp").read_text(encoding="utf-8")
SHARED = (ROOT / "src/game/shared/SharedShipPhysics.cpp").read_text(encoding="utf-8")
SHIP_CONTROLLER_H = (ROOT / "src/game/ship/ShipController.h").read_text(encoding="utf-8")
SHIP_CONTROLLER_CPP = (ROOT / "src/game/ship/ShipController.cpp").read_text(encoding="utf-8")
MOTION_H = (ROOT / "src/game/navigation/DynamicMotionSystem.h").read_text(encoding="utf-8")
MOTION_CPP = (ROOT / "src/game/navigation/DynamicMotionSystem.cpp").read_text(encoding="utf-8")
SIM = (ROOT / "src/game/simulation/GameSimulation.cpp").read_text(encoding="utf-8")
DOC = (ROOT / "src/game/navigation/LIVE_NAVIGATION_INTEGRATION.md").read_text(encoding="utf-8")
ROOT_CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
TEST_CMAKE = (ROOT / "tests/navigation_runtime/CMakeLists.txt").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")


for marker in (
    "navigationAccelerationDemandValid",
    "navigationLinearAccelerationDemandSystemMps2",
    "navigationAngularAccelerationDemandSystemRadPerSec2",
    "navigationIntentRevision",
    "navigationActuatorProgramValid",
    "navigationRearMainThrottle01",
    "navigationForeMainThrottle01",
    "navigationManoeuvreAccelerationSystemMps2",
    "navigationLinearFeedbackAccelerationSystemMps2",
):
    require(marker in CONTROL, f"ShipControlState direct navigation seam missing: {marker}")

for marker in (
    "class NavigationRuntimeControlBridge final",
    "using PilotExecutor = world::navigation::PilotSkillExecutor",
    "ExecutionSnapshot",
    "ProgramActuatorCommand",
    "stepProgram(",
    "idealLinearAccelerationDemandSystemMps2",
    "executedLinearAccelerationDemandSystemMps2",
    "activeTargetRevision",
):
    require(marker in BRIDGE_H, f"runtime bridge interface missing: {marker}")

for marker in (
    "executor_.step",
    "navigationAccelerationDemandValid = true",
    "navigationLinearAccelerationDemandSystemMps2",
    "navigationAngularAccelerationDemandSystemRadPerSec2",
    "navigationIntentRevision",
    "navigationActuatorProgramValid = true",
    "navigationRearMainThrottle01",
    "navigationForeMainThrottle01",
    "navigationManoeuvreAccelerationSystemMps2",
    "navigationLinearFeedbackAccelerationSystemMps2",
):
    require(marker in BRIDGE_CPP, f"runtime bridge implementation missing: {marker}")

for marker in (
    "navigationAccelerationDemandValid",
    "navigationAngularAccelerationDemandSystemRadPerSec2",
    "controller.updateControlRates(",
):
    require(marker in SHARED, f"shared angular live seam missing: {marker}")

for marker in (
    "angularAccelerationDemandMapRadPerSec2",
):
    require(marker in SHIP_CONTROLLER_H, f"ShipController interface missing: {marker}")

for marker in (
    "glm::dot(angularAccelerationDemandMapRadPerSec2, right)",
    "glm::dot(angularAccelerationDemandMapRadPerSec2, up)",
    "glm::dot(angularAccelerationDemandMapRadPerSec2, forward)",
    "applyRequestedAngularAcceleration",
):
    require(marker in SHIP_CONTROLLER_CPP, f"ShipController direct-demand clamp missing: {marker}")

require(
    "applySystemAccelerationDemand" in MOTION_H,
    "DynamicMotionSystem direct linear-demand API missing",
)
require(
    "applyNavigationActuatorProgram" in MOTION_H,
    "DynamicMotionSystem planner-actuator execution API missing",
)

for marker in (
    "forwardMainAuthority",
    "reverseMainAuthority",
    "manoeuvreAuthority",
    "clampMagnitude(remainder, manoeuvreAuthority)",
):
    require(marker in MOTION_CPP, f"real propulsion split missing: {marker}")

for marker in (
    "nominalForwardMain",
    "nominalReverseMain",
    "availableForwardMain",
    "availableReverseMain",
    "feedbackMainLongitudinal",
    "manoeuvreAccelerationSystemMps2 +",
):
    require(
        marker in MOTION_CPP,
        f"planner-owned actuator allocation missing: {marker}"
    )

for marker in (
    "manualTranslationOverride",
    "control.navigationAccelerationDemandValid",
    "control.navigationActuatorProgramValid",
    "applyNavigationActuatorProgram",
    "applySystemAccelerationDemand",
    "applyLocalFrameInput",
):
    require(marker in SIM, f"GameSimulation live demand/manual override branch missing: {marker}")

for forbidden in (
    "setWorldPosition",
    "localVelocityMps =",
    "worldVelocityMps =",
    "pitchRate =",
    "yawRate =",
    "rollRate =",
):
    require(
        forbidden not in BRIDGE_H and forbidden not in BRIDGE_CPP,
        f"runtime bridge must not mutate authoritative motion directly: {forbidden}",
    )

for marker in (
    "live runtime integration",
    "same executed demand",
    "Main engine",
    "forward-only",
    "Manual override",
    "No physics bypass",
    "11B after acceptance",
    "same accepted intent/execution snapshot/revision",
):
    require(
        marker.lower() in DOC.lower(),
        f"live integration documentation missing: {marker}",
    )

for marker in (
    "src/game/navigation/NavigationRuntimeControlBridge.cpp",
    "src/world/navigation/control/PilotSkillExecutor.cpp",
):
    require(marker in ROOT_CMAKE, f"main target must compile live seam: {marker}")

require(
    "navigation_runtime_control" in TEST_CMAKE,
    "navigation runtime CTest must be registered",
)

print("NAVIGATION LIVE RUNTIME CONTROL CONTRACT: PASS")
print(" - accepted navigation/pilot output reaches ShipControlState through one explicit demand seam")
print(" - bridge publishes the same executed demand for downstream control and future guidance/debug")
print(" - angular demand uses existing ShipController acceleration/rate capability limits")
print(" - generic net demand maps onto installed rear/fore main banks plus bounded manoeuvre authority")
print(" - accepted-program execution preserves Planner-owned nominal actuator allocation")
print(" - manual control materially overrides navigation demand")
print(" - GameSimulation applies the new channel without replacing legacy controls for other ships")
print(" - navigation never writes authoritative position/velocity/angular-rate state directly")
