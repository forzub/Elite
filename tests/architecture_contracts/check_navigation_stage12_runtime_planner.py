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
LOCAL_H = (ROOT / "src/world/navigation/local/LocalAvoidancePlanner.h").read_text(encoding="utf-8")
LOCAL_CPP = (ROOT / "src/world/navigation/local/LocalAvoidancePlanner.cpp").read_text(encoding="utf-8")
SERVER_RUNTIME_H = (ROOT / "src/game/server/ServerRuntime.h").read_text(encoding="utf-8")
SERVER_RUNTIME_CPP = (ROOT / "src/game/server/ServerRuntime.cpp").read_text(encoding="utf-8")
SERVER_MAIN = (ROOT / "src/server_main.cpp").read_text(encoding="utf-8")


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
    "mapIntentToWorld",
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
    "testAdjustedTargetPreservesNominalConflictIdentity",
    "testNavigationMapCrossingConflictProducesBrakingHold",
    "testMapIntentTransformsIntoWorldControlFrame",
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
    "m_navigationRuntimeLabMap->querySphere",
    "Planner::plan(",
    "Planner::mapIntentToWorld(",
    "navigationWorkingFrame",
):
    require(marker in SIM_CPP, f"GameSimulation runtime planner integration missing: {marker}")

require(
    "NpcNavigationIntentController::buildIntent" in SIM_CPP,
    "ordinary NPC fallback path must remain present while the lab is isolated",
)

require(
    "outIntent = m_navigationRuntimeLabLastPlan.intent" not in SIM_CPP,
    "map-space planner intent must not cross directly into the Stage-11 world-space control seam",
)

require(
    "isNavigationRuntimeLabShip(shipId)" in SIM_CPP and
    "return SimulationMode::Active;" in SIM_CPP,
    "stage-12 proving actor must stay Active so activation cadence cannot contaminate the navigation result",
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
    "nominalPrimaryConflictEntityId",
    "nominalConflictsFound",
):
    require(marker in LOCAL_H, f"local avoidance nominal conflict diagnostic missing: {marker}")

require(
    "nominal.primaryConflictEntityId" in LOCAL_CPP and
    "nominal.conflictsFound" in LOCAL_CPP,
    "local avoidance must preserve the nominal conflict before adjusted-target probing",
)

for marker in (
    "executionCount",
    "minimumConservativeClearanceMeters",
    "maximumStraightLineDeviationMeters",
    "maximumExecutedLateralDemandMps2",
    "maximumAppliedEngineAccelerationMps2",
    "maximumAppliedLateralAccelerationMps2",
    "maximumRelativeSpeedMps",
    "lastExecutedLinearDemandMapMps2",
    "passedObstaclePlane",
):
    require(marker in LAB_H, f"live navigation physical observation missing: {marker}")

for marker in (
    "minimumConservativeClearanceMeters",
    "maximumStraightLineDeviationMeters",
    "maximumExecutedLateralDemandMps2",
    "maximumAppliedEngineAccelerationMps2",
    "maximumAppliedLateralAccelerationMps2",
    "maximumRelativeSpeedMps",
    "nominalPrimaryConflictEntityId",
):
    require(marker in SIM_CPP, f"GameSimulation live navigation evidence missing: {marker}")

require(
    "navigationRuntimeLabObservation() const noexcept" in SERVER_RUNTIME_H and
    "m_server->navigationRuntimeLabObservation()" in SERVER_RUNTIME_CPP,
    "read-only Stage-12 observation must cross the ServerRuntime boundary",
)

require(
    "copyAuthoritativePublishedSnapshot" in SERVER_RUNTIME_H and
    "outSnapshot = m_server->snapshot()" in SERVER_RUNTIME_CPP,
    "live self-test must compare against a copied authoritative published snapshot, not mutable per-step state",
)

for marker in (
    "--self-test-navigation",
    "runNavigationRuntimeSelfTest",
    "obstaclePrimaryConflictSeen",
    "adjustedTargetSeen",
    "lateralExecutedDemandSeen",
    "minimumConservativeClearanceMeters > 0.0",
    "findShipSnapshotByInstanceId",
    "NavigationExecutionSnapshot",
    "replicationErrorMps2",
    "canonicalReplicationErrorMps2",
    "copyAuthoritativePublishedSnapshot",
    "sparsePacket.metadata.serverTick",
    "authoritativePublished.metadata.serverTick",
):
    require(marker in SERVER_MAIN, f"authoritative navigation self-test missing: {marker}")

require(
    "MaxSimulatedSeconds = 120.0" in SERVER_MAIN,
    "navigation self-test must remain bounded in simulated time",
)

require(
    "querySphere(dynamicQuery)" in SIM_CPP and
    "localHorizonMeters" in SIM_CPP and
    "LabTurnDistanceMeters" in SIM_CPP,
    "live broadphase must cover the complete bounded avoidance fan rather than only the nominal corridor",
)

require(
    "routeVectorWorld" in SIM_CPP and
    "routeDirectionWorld" in SIM_CPP,
    "executed lateral-demand diagnostics must compare world-space vectors in one frame",
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
print(" - authoritative GameSimulation isolates one Active stage-12 lab actor on real NAV STRESS hit volumes")
print(" - live self-test pins CUBE 08 -> adjusted target -> executed lateral demand -> positive clearance")
print(" - bounded NavigationMap sphere covers the complete local avoidance fan")
print(" - sparse packet is compared with authoritative publication at the exact same server tick")
print(" - canonical sparse hydration must match the same authoritative execution truth")
print(" - non-identity working-frame regression pins map intent -> world control transform")
print(" - live lateral-demand diagnostics compare vectors in world space")
print(" - self-test reports pilot demand separately from physically applied acceleration")
