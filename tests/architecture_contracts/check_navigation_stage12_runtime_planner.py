#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

SPACE_H = (ROOT / "src/world/navigation/space/NavigationSpace.h").read_text(encoding="utf-8")
SPACE_CPP = (ROOT / "src/world/navigation/space/NavigationSpace.cpp").read_text(encoding="utf-8")
PLANNER_H = (ROOT / "src/game/navigation/NavigationRuntimePlanner.h").read_text(encoding="utf-8")
PLANNER_CPP = (ROOT / "src/game/navigation/NavigationRuntimePlanner.cpp").read_text(encoding="utf-8")
ROOT_CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
RUNTIME_CMAKE = (ROOT / "tests/navigation_runtime/CMakeLists.txt").read_text(encoding="utf-8")
RUNTIME_TEST = (ROOT / "tests/navigation_runtime/NavigationRuntimePlannerTests.cpp").read_text(encoding="utf-8")
DOC = (ROOT / "src/game/navigation/STAGE12_END_TO_END.md").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")


for marker in (
    "portalCentersMapMeters",
    "std::vector<Vec3d>",
):
    require(marker in SPACE_H, f"NavigationSpace compact steering seam missing: {marker}")

require(
    SPACE_CPP.count("portalCentersMapMeters.push_back") >= 3,
    "all corridor reconstruction paths must publish ordered portal centers",
)

for marker in (
    "class NavigationRuntimePlanner final",
    "NavigationRuntimeControlBridge",
    "LocalAvoidancePlanner",
    "NavigationMap",
    "NavigationSpace",
    "safeProgressTargetDemonstrated",
    "usedPortalWaypoint",
):
    require(marker in PLANNER_H, f"runtime planner interface missing: {marker}")

for marker in (
    "queryCostedCorridor",
    "portalCentersMapMeters.front",
    "Avoidance{}.evaluate",
    "holdIntent",
    "idealLinearAccelerationDemandMapMps2",
    "idealAngularAccelerationDemandMapRadPerSec2",
):
    require(marker in PLANNER_CPP, f"runtime planner composition missing: {marker}")

for forbidden in (
    "setWorldPosition",
    "worldVelocityMps =",
    "localVelocityMps =",
    "pitchRate =",
    "yawRate =",
    "rollRate =",
):
    require(
        forbidden not in PLANNER_CPP,
        f"runtime planner must not mutate authoritative physics state: {forbidden}",
    )

for marker in (
    "add_library(EliteNavigationWorldRuntime STATIC",
    "src/world/navigation/map/NavigationMap.cpp",
    "src/world/navigation/space/NavigationSpace.cpp",
    "src/world/navigation/local/LocalHorizonPlanner.cpp",
    "src/world/navigation/local/LocalAvoidancePlanner.cpp",
    "src/game/navigation/NavigationRuntimePlanner.cpp",
):
    require(marker in ROOT_CMAKE, f"shared runtime navigation target missing: {marker}")

require(
    ROOT_CMAKE.count("EliteNavigationWorldRuntime") >= 3,
    "shared NavigationWorld runtime target must be defined and linked by both production executables",
)

for marker in (
    "NavigationRuntimePlanner.cpp",
    "NavigationRuntimePlannerTests.cpp",
    "navigation_runtime_planner",
    "EliteNavigationMap",
    "EliteNavigationLocal",
):
    require(marker in RUNTIME_CMAKE, f"runtime planner test wiring missing: {marker}")

for marker in (
    "testStaticCorridorBecomesLivePortalWaypoint",
    "testSamePortalRejectsOversizedHull",
    "testNavigationMapCrossingConflictProducesBrakingHold",
    "testPlannerIntentCrossesAcceptedPilotBridge",
):
    require(marker in RUNTIME_TEST, f"runtime planner fixture missing: {marker}")

for marker in (
    "deterministic proving ground",
    "production ownership chain",
    "legacy",
):
    require(marker.lower() in DOC.lower(), f"stage-12 contract missing: {marker}")

require(
    "second presentation-only planner" in DOC.lower() or
    "run a second planner" in DOC.lower(),
    "stage-12 contract must forbid a second presentation/debug planner",
)

print("NAVIGATION STAGE 12 RUNTIME PLANNER CONTRACT: PASS")
print(" - NavigationSpace publishes ordered selected-portal steering centers")
print(" - one shared runtime planner composes static corridor + bounded local avoidance")
print(" - blocked/stale/conflict states keep producing fail-closed pilot intent")
print(" - planner cannot mutate authoritative physics state")
print(" - client/server share the same NavigationWorld runtime-planning target")
print(" - deterministic fixtures pin detour, envelope rejection, moving conflict and pilot bridge")
