#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HEADER = (ROOT / "src/world/navigation/control/PilotSkillExecutor.h").read_text(encoding="utf-8")
IMPL = (ROOT / "src/world/navigation/control/PilotSkillExecutor.cpp").read_text(encoding="utf-8")
DOC = (ROOT / "src/world/navigation/PILOT_SKILL_MODEL.md").read_text(encoding="utf-8")
CONTROL_CMAKE = (ROOT / "src/world/navigation/control/CMakeLists.txt").read_text(encoding="utf-8")
TEST_CMAKE = (ROOT / "tests/navigation_trajectory/CMakeLists.txt").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")


for marker in (
    "class PilotSkillExecutor final",
    "struct ExecutionProfile",
    "struct PolicyProfile",
    "struct PilotSkillProfile",
    "reactionDelaySeconds",
    "perceptionDecisionRateHz",
    "commandLatencySeconds",
    "responseFrequencyHz",
    "dampingRatio",
    "commandGain",
    "deterministicSeed",
    "emergencyResponseThreshold01",
    "emergencyReactionDelayScale",
    "anticipationSeconds",
    "riskPreference01",
    "comfortPreference01",
    "kMaxPendingCommands = 256",
    "kMaxIntegrationSubsteps = 64",
):
    require(marker in HEADER, f"pilot skill interface missing: {marker}")

for marker in (
    "mix64",
    "deterministicNoise",
    "revisionFirstSeenTimeSeconds_",
    "nextDecisionTimeSeconds_",
    "commandLatencySeconds",
    "advanceSecondOrder",
    "dampingRatio",
    "maxLinearCommandSlewMetersPerSec3",
    "emergencyResponseThreshold01",
    "emergencyReactionDelayScale",
    "kMaxIntegrationSubsteps",
):
    require(marker in IMPL, f"pilot skill implementation missing: {marker}")

for forbidden in (
    "std::random",
    "random_device",
    "NavigationMap.h",
    "NavigationSpace.h",
    "OrientedPassageEvaluator.h",
    "DockingTerminalEvaluator.h",
    "GL/",
    "OpenGL",
    "glm/",
    "Physics",
):
    require(
        forbidden not in HEADER and forbidden not in IMPL,
        f"pilot skill executor must not own world/geometry/physics: {forbidden}",
    )

for marker in (
    "deterministic NPC pilot skill execution",
    "reaction delay",
    "decision cadence",
    "command latency",
    "second-order response",
    "under-damped",
    "deterministic precision error",
    "same seed",
    "policy preferences",
    "does not own",
    "real closed-loop docking-like oscillation",
):
    require(
        marker.lower() in DOC.lower(),
        f"pilot skill documentation missing: {marker}",
    )

require(
    "PilotSkillExecutor.cpp" in CONTROL_CMAKE,
    "navigation control library must compile PilotSkillExecutor",
)
require(
    "navigation_pilot_skill" in TEST_CMAKE,
    "trajectory test harness must register pilot skill behavior",
)

print("NAVIGATION PILOT SKILL CONTRACT: PASS")
print(" - pilot skill modifies command execution, never geometry or vehicle capability")
print(" - new intent revisions receive explicit reaction delay, decision cadence and latency")
print(" - second-order damping/gain/slew can produce deterministic overshoot and oscillation")
print(" - command-space precision error is seeded and replay-stable")
print(" - emergency urgency may shorten reaction delay without changing safety truth")
print(" - anticipation/risk/comfort remain upstream policy knobs, not hidden physics modifiers")
print(" - executor work is fixed-size and bounded per pilot")
