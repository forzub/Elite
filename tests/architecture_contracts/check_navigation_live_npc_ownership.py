#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

AI_H = (ROOT / "src/game/simulation/NpcAiSystem.h").read_text(encoding="utf-8")
AI_CPP = (ROOT / "src/game/simulation/NpcAiSystem.cpp").read_text(encoding="utf-8")
INTENT_H = (ROOT / "src/game/navigation/NpcNavigationIntentController.h").read_text(encoding="utf-8")
INTENT_CPP = (ROOT / "src/game/navigation/NpcNavigationIntentController.cpp").read_text(encoding="utf-8")
SIM_H = (ROOT / "src/game/simulation/GameSimulation.h").read_text(encoding="utf-8")
SIM_CPP = (ROOT / "src/game/simulation/GameSimulation.cpp").read_text(encoding="utf-8")
SHARED = (ROOT / "src/game/shared/SharedShipPhysics.cpp").read_text(encoding="utf-8")
DOC = (ROOT / "src/game/navigation/LIVE_NAVIGATION_INTEGRATION.md").read_text(encoding="utf-8")
ROOT_CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
TEST_CMAKE = (ROOT / "tests/navigation_runtime/CMakeLists.txt").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")


for marker in (
    "struct NpcNavigationGoal",
    "NpcNavigationGoalMode",
    "computeGoal",
    "pilotSkillProfile",
):
    require(marker in AI_H, f"NPC goal/policy interface missing: {marker}")

for forbidden in (
    "computeControl",
    "yawInput",
    "pitchInput",
    "rollInput",
    "forwardInput",
    "targetSpeedRate",
    "ShipControlState control",
):
    require(
        forbidden not in AI_CPP,
        f"NPC AI must no longer be a steering authority: {forbidden}",
    )

for marker in (
    "class NpcNavigationIntentController final",
    "NavigationRuntimeControlBridge::Intent",
    "buildIntent",
):
    require(marker in INTENT_H, f"NPC navigation intent interface missing: {marker}")

for marker in (
    "desiredRelativeWorldVelocity",
    "relativeWorldVelocity",
    "velocityResponsePerSecond",
    "angularDampingPerSecond",
    "idealLinearAccelerationDemandMapMps2",
    "idealAngularAccelerationDemandMapRadPerSec2",
):
    require(marker in INTENT_CPP, f"NPC nominal intent implementation missing: {marker}")

for marker in (
    "m_npcNavigationControlBridges",
    "m_npcNavigationExecutionSnapshots",
    "m_npcNavigationLastExecutionTimeSeconds",
    "npcNavigationExecutionSnapshots",
    "updateNpcNavigationControl",
):
    require(marker in SIM_H, f"per-NPC runtime ownership state missing: {marker}")

for marker in (
    "m_npcAiSystem.computeGoal",
    "NpcNavigationIntentController::buildIntent",
    "bridgeIt->second->step",
    "ship.setControlState(latest.control)",
    "m_npcNavigationExecutionSnapshots[id] = latest.snapshot",
    "kMaximumStepSeconds",
):
    require(marker in SIM_CPP, f"GameSimulation NPC navigation ownership missing: {marker}")

require(
    "m_npcAiSystem.computeControl" not in SIM_CPP,
    "GameSimulation still calls retired NPC direct steering",
)

for forbidden in (
    "setWorldPosition",
    "localVelocityMps =",
    "worldVelocityMps =",
    "pitchRate =",
    "yawRate =",
    "rollRate =",
):
    require(
        forbidden not in INTENT_CPP,
        f"NPC intent controller must not mutate authoritative motion: {forbidden}",
    )

require(
    "hasManualTranslationInput" not in SHARED,
    "accepted 11A unused helper warning was not cleaned up",
)

for marker in (
    "Stage 11A target-machine acceptance",
    "authoritative NPC motion ownership",
    "does not emit control-surface input",
    "Per-NPC runtime bridge",
    "Activation cadence correctness",
    "Same execution truth for diagnostics/guidance",
):
    require(
        marker.lower() in DOC.lower(),
        f"live integration documentation missing: {marker}",
    )

require(
    "src/game/navigation/NpcNavigationIntentController.cpp" in ROOT_CMAKE,
    "main target must compile NPC navigation intent controller",
)
require(
    "NpcNavigationIntentController.cpp" in TEST_CMAKE,
    "runtime tests must compile NPC navigation intent controller",
)

print("NAVIGATION LIVE NPC OWNERSHIP CONTRACT: PASS")
print(" - NpcAiSystem publishes goal/policy only and no longer emits steering controls")
print(" - Navigation v2 converts NPC goals into nominal acceleration intent")
print(" - GameSimulation owns persistent per-NPC pilot/runtime bridge state")
print(" - activation-decimated elapsed time is advanced in bounded exact-time pieces")
print(" - no failure path falls back to the retired direct steering authority")
print(" - the exact executed demand/revision is retained for future replication/guidance")
print(" - navigation intent code never mutates authoritative motion directly")
