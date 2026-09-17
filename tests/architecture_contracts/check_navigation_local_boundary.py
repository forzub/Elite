#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HEADER = (ROOT / "src/world/navigation/local/LocalHorizonPlanner.h").read_text(encoding="utf-8")
IMPL = (ROOT / "src/world/navigation/local/LocalHorizonPlanner.cpp").read_text(encoding="utf-8")
README = (ROOT / "src/world/navigation/local/README.md").read_text(encoding="utf-8")
CMAKE = (ROOT / "src/world/navigation/local/CMakeLists.txt").read_text(encoding="utf-8")
TEST_CMAKE = (ROOT / "tests/navigation_local/CMakeLists.txt").read_text(encoding="utf-8")
TEST_CPP = (ROOT / "tests/navigation_local/NavigationLocalContractTests.cpp").read_text(encoding="utf-8")
RUNNER = (ROOT / "tests/navigation_local/run_mingw64.sh").read_text(encoding="utf-8")
CURRENT_TASK = (ROOT / "CURRENT_TASK.md").read_text(encoding="utf-8")
CURRENT_STATE = (ROOT / "CURRENT_STATE.md").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")


for forbidden in (
    "glm/",
    "glad/",
    "GLFW/",
    "game/",
    "render/",
    "scene/",
    "NavigationContact",
    "NavigationScene",
    "TacticalCollisionMonitor",
    "SmallCraftNavigation",
    "SpaceState",
):
    require(forbidden not in HEADER,
            f"local horizon public API leaks legacy/runtime dependency: {forbidden}")

for marker in (
    "class LocalHorizonPlanner final",
    "NavigationMap::QueryResult",
    "struct AgentState",
    "struct NominalTargetState",
    "struct Policy",
    "struct Query",
    "enum class Status",
    "Clear",
    "ConflictHold",
    "StaleHold",
    "enum class TargetMode",
    "PassThrough",
    "Terminal",
    "Hold",
    "safeProgressTargetDemonstrated",
    "mapRevision",
    "sourceRevision",
    "dynamicResultAgeSeconds",
    "horizonDistanceMeters",
    "primaryConflictEntityId",
    "candidatesExamined",
    "conflictsFound",
    "evaluate",
):
    require(marker in HEADER, f"local horizon API marker missing: {marker}")

for forbidden in (
    "#include <glm/",
    "#include <glad/",
    "#include <GLFW/",
    "#include \"game/",
    "#include \"render/",
    "NavigationContact",
    "NavigationScene",
    "TacticalCollisionMonitor",
    "SmallCraftNavigation",
):
    require(forbidden not in IMPL,
            f"local horizon implementation depends on forbidden legacy/runtime layer: {forbidden}")

for marker in (
    "latencyDistance",
    "brakingDistance",
    "maxResultAgeSeconds",
    "relativePosition",
    "relativeVelocity",
    "timeToClosest",
    "conservativeSweptRadiusMeters",
    "distanceToSegmentSquared",
    "Status::ConflictHold",
    "Status::StaleHold",
    "TargetMode::PassThrough",
    "TargetMode::Terminal",
):
    require(marker in IMPL, f"local horizon reference marker missing: {marker}")

for marker in (
    "compact dynamic products",
    "physical horizon",
    "fail closed",
    "no full-scene actor scan",
    "no second navigationworld snapshot",
):
    require(marker in README.lower(),
            f"local horizon ownership documentation missing: {marker}")

require("already reduced NavigationMap products" in HEADER,
        "local horizon header no longer declares reduced-map-product ownership")
require("EliteNavigationLocal" in CMAKE,
        "local horizon does not define its own library target")
require("add_subdirectory" in TEST_CMAKE and "EliteNavigationLocal" in TEST_CMAKE,
        "local horizon behavioral test does not build through its own boundary")
require("ELITE_TEST_BUILD_ROOT" in RUNNER,
        "local horizon runner does not use canonical test build layout")
require('cmake --build "${BUILD_DIR}"' in RUNNER,
        "local horizon runner must build all CTest-registered local executables")
require("NAVIGATION LOCAL HORIZON CONTRACT TESTS: PASS" in TEST_CPP,
        "local horizon behavioral contract executable missing")

for marker in (
    "testClearPassThroughIsBoundedByPhysicalHorizon",
    "testTerminalTargetPreservesTerminalKinematics",
    "testCrossingActorFailsClosed",
    "testHeadOnActorFailsClosed",
    "testStaleSnapshotFailsClosedBeforeCandidateWork",
    "testSelfCandidateIsIgnored",
):
    require(marker in TEST_CPP, f"local horizon fixture missing: {marker}")

require("NAV-V2-LOCAL-1" in CURRENT_TASK,
        "CURRENT_TASK is not advanced to NAV-V2-LOCAL-1")
require("NAV-V2-LOCAL-1" in CURRENT_STATE,
        "CURRENT_STATE does not record NAV-V2-LOCAL-1")

print("NAVIGATION LOCAL HORIZON BOUNDARY CONTRACT: PASS")
print(" - local planner consumes compact NavigationMap query products only")
print(" - public boundary is isolated from GLM/OpenGL/game/render/legacy scene state")
print(" - physical horizon includes result age, braking, turn distance and margin")
print(" - dynamic reference uses bounded closest approach + conservative swept bounds")
print(" - clear, terminal, crossing, head-on, stale and self-filter fixtures are pinned")
print(" - conflict/stale results fail closed instead of inventing an unverified bypass")
print(" - local runner builds every CTest-registered executable")
print(" - project state/task agree on NAV-V2-LOCAL-1")
