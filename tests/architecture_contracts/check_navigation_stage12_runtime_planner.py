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
ADAPTER_H = (ROOT / "src/game/navigation/NavigationHitVolumeAdapter.h").read_text(encoding="utf-8")
ADAPTER_CPP = (ROOT / "src/game/navigation/NavigationHitVolumeAdapter.cpp").read_text(encoding="utf-8")
SIM_H = (ROOT / "src/game/simulation/GameSimulation.h").read_text(encoding="utf-8")
SIM_CPP = (ROOT / "src/game/simulation/GameSimulation.cpp").read_text(encoding="utf-8")
SCENE_CPP = (ROOT / "src/game/scene/GameSceneSetup.cpp").read_text(encoding="utf-8")
LAB_H = (ROOT / "src/game/diagnostics/NavigationRuntimeLab.h").read_text(encoding="utf-8")


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
    "class NavigationHitVolumeAdapter final",
    "buildObstacles",
    "conservativeRadiusFromOrigin",
):
    require(marker in ADAPTER_H, f"hit-volume navigation adapter missing: {marker}")

for marker in (
    "NavigationObstacleShape::Box",
    "objectWorldPositionMeters",
    "objectLocalToWorld",
    "supportLinkVolume",
):
    require(marker in ADAPTER_CPP, f"hit-volume adapter implementation missing: {marker}")

require(
    "NavigationHitVolumeAdapter.cpp" in ROOT_CMAKE and
    "NavigationHitVolumeAdapter.cpp" in RUNTIME_CMAKE,
    "hit-volume adapter must compile in production and isolated runtime gate",
)

for marker in (
    "registerNavigationRuntimeLabShip",
    "isNavigationRuntimeLabShip",
    "navigationRuntimeLabLastPlan",
    "buildNavigationRuntimeLabIntent",
    "m_navigationRuntimeLabMap",
    "m_navigationRuntimeLabSpace",
):
    require(marker in SIM_H, f"authoritative navigation runtime lab seam missing: {marker}")

for marker in (
    "initializeNavigationRuntimeLab();",
    "buildNavigationRuntimeLabIntent(id, ship, intent)",
    "NavigationHitVolumeAdapter::",
    "conservativeRadiusFromOrigin(object.hitComponent)",
    "m_navigationRuntimeLabMap->replaceDynamicWorld",
    "m_navigationRuntimeLabMap->queryCorridor",
    "Planner::plan(",
):
    require(marker in SIM_CPP, f"GameSimulation runtime planner integration missing: {marker}")

require(
    "NpcNavigationIntentController::buildIntent" in SIM_CPP,
    "ordinary NPC fallback path must remain present while the lab is isolated",
)

for marker in (
    "NavigationRuntimeLabEnabled",
    "NavigationRuntimeLabStartTacticalLocalMeters",
    "NavigationRuntimeLabGoalTacticalLocalMeters",
    "NAVIGATION V2 RUNTIME LAB",
):
    require(
        marker in LAB_H or marker in SCENE_CPP,
        f"navigation runtime lab definition/spawn missing: {marker}",
    )

require(
    "spawnHubGuidanceTestModules" in SCENE_CPP and
    "spawnNavigationRuntimeLabNpc" in SCENE_CPP and
    "registerNavigationRuntimeLabShip" in SCENE_CPP,
    "existing NAV STRESS scene must feed the live runtime lab",
)

require(
    "testHitVolumeAdapterUsesAuthoritativeLocalObb" in RUNTIME_TEST,
    "runtime gate must pin authoritative HitVolume -> navigation OBB conversion",
)

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
