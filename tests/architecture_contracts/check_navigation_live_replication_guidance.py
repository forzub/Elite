#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

SNAPSHOT = (ROOT / "src/game/simulation/NavigationExecutionSnapshot.h").read_text(encoding="utf-8")
SHIP_SNAPSHOT = (ROOT / "src/game/simulation/ShipSnapshot.h").read_text(encoding="utf-8")
SIM = (ROOT / "src/game/simulation/GameSimulation.cpp").read_text(encoding="utf-8")
WIRE_SCHEMA = (ROOT / "src/game/network/WireDataSchema.h").read_text(encoding="utf-8")
WIRE_CODEC = (ROOT / "src/game/network/WireDataCodec.h").read_text(encoding="utf-8")
CLIENT_H = (ROOT / "src/game/client/ClientWorldState.h").read_text(encoding="utf-8")
CLIENT_CPP = (ROOT / "src/game/client/ClientWorldState.cpp").read_text(encoding="utf-8")
REPLICATED = (ROOT / "src/game/navigation/ReplicatedNavigationExecutionState.h").read_text(encoding="utf-8")
WORKSPACE = (ROOT / "src/game/navigation/ClientNavigationWorkspace.h").read_text(encoding="utf-8")
SPACE = (ROOT / "src/game/SpaceState.cpp").read_text(encoding="utf-8")
GUIDANCE = (ROOT / "src/game/presentation/GuidanceHudPresentation.h").read_text(encoding="utf-8")
LOCAL_PLANNER = (ROOT / "src/game/navigation/LocalGuidancePlanner.cpp").read_text(encoding="utf-8")
DOCK_PLANNER = (ROOT / "src/game/navigation/DockingPathPlanner.cpp").read_text(encoding="utf-8")
RUNTIME_CMAKE = (ROOT / "tests/navigation_runtime/CMakeLists.txt").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")


for marker in (
    "struct NavigationExecutionSnapshot",
    "intentRevision",
    "activeTargetRevision",
    "idealLinearAccelerationDemandMapMps2",
    "executedLinearAccelerationDemandMapMps2",
    "executedAngularAccelerationDemandMapRadPerSec2",
    "reactionBlocked",
    "pendingCommandCount",
):
    require(marker in SNAPSHOT, f"replicated execution DTO missing: {marker}")

require(
    "std::variant<" in SHIP_SNAPSHOT and
    "std::monostate" in SHIP_SNAPSHOT and
    "game::simulation::NavigationExecutionSnapshot" in SHIP_SNAPSHOT and
    "navigationExecution" in SHIP_SNAPSHOT,
    "ShipSnapshot does not carry sparse navigation execution truth",
)

for marker in (
    "m_npcNavigationExecutionSnapshots.find(id)",
    "replicated.intentRevision = execution.intentRevision",
    "replicated.activeTargetRevision",
    "replicated.executedLinearAccelerationDemandMapMps2",
    "replicated.executedAngularAccelerationDemandMapRadPerSec2",
):
    require(marker in SIM, f"server snapshot publication missing: {marker}")

for marker in (
    "game::simulation::NavigationExecutionSnapshot,",
    "v.navigationExecution",
):
    require(marker in WIRE_SCHEMA, f"wire schema missing replicated execution: {marker}")

require(
    "SimulationSnapshotWireSchemaVersion = 8u" in WIRE_CODEC,
    "snapshot wire schema version was not bumped for navigation execution",
)
require(
    "struct WireSchema<std::monostate>" in WIRE_SCHEMA,
    "wire schema does not support sparse monostate execution payloads",
)

require(
    "game::simulation::NavigationExecutionSnapshot" in CLIENT_H and
    "navigationExecution" in CLIENT_H,
    "ClientShipState does not retain replicated navigation execution",
)
require(
    CLIENT_CPP.count("std::get_if<") >= 2 and
    CLIENT_CPP.count("game::simulation::NavigationExecutionSnapshot") >= 2 and
    CLIENT_CPP.count("state.navigationExecution = *execution;") >= 2 and
    CLIENT_CPP.count("state.navigationExecution = {};") >= 2,
    "new/existing client ship hydration does not resolve sparse navigation execution",
)

for marker in (
    "struct ReplicatedNavigationExecution",
    "ShipInstanceId shipInstanceId",
    "NavigationAssetRef",
    "m_entityByShipInstance",
    "const ReplicatedNavigationExecution* find(",
):
    require(marker in REPLICATED, f"client replicated execution mirror missing: {marker}")

require(
    "syncReplicatedNavigationExecution(" in WORKSPACE,
    "ClientNavigationWorkspace has no explicit server-truth ingress",
)
require(
    "const ReplicatedNavigationExecutionState&" in WORKSPACE and
    "replicatedNavigationExecution() const noexcept" in WORKSPACE,
    "ClientNavigationWorkspace has no read-only replicated execution view",
)
require(
    "ReplicatedNavigationExecutionState& replicatedNavigationExecution() noexcept" not in WORKSPACE,
    "client planners regained mutable access to replicated server execution truth",
)

for marker in (
    "navigationExecutionSnapshotTick",
    "m_lastNavigationExecutionSnapshotTick",
    "shipState.navigationExecution.valid",
    "entry.shipInstanceId = shipState.instanceId",
    "syncReplicatedNavigationExecution",
):
    require(marker in SPACE, f"SpaceState replicated execution sync missing: {marker}")

require(
    "navigationExecutionSnapshotTick !=" in SPACE and
    "m_lastNavigationExecutionSnapshotTick" in SPACE,
    "client replicated execution mirror is not gated by accepted server snapshot tick",
)

for marker in (
    "hasAuthoritativeExecution",
    "replicatedNavigationExecution().find",
    "authoritativeIntentRevision",
    "authoritativeActiveTargetRevision",
    "authoritativeExecutedLinearAccelerationMapMps2",
    "authoritativeExecutedAngularAccelerationMapRadPerSec2",
):
    require(marker in GUIDANCE, f"guidance presentation does not expose server truth: {marker}")

PLANNING_FACTORY = (ROOT / "src/game/client/ClientNavigationPlanningSnapshotFactory.cpp").read_text(encoding="utf-8")
TRAJECTORY_PREDICTOR = (ROOT / "src/game/navigation/TrajectoryPredictor.cpp").read_text(encoding="utf-8")
TRAJECTORY_SAFETY = (ROOT / "src/game/navigation/TrajectorySafetyEvaluator.cpp").read_text(encoding="utf-8")

for planner, label in (
    (LOCAL_PLANNER, "LocalGuidancePlanner"),
    (DOCK_PLANNER, "DockingPathPlanner"),
    (PLANNING_FACTORY, "ClientNavigationPlanningSnapshotFactory"),
    (TRAJECTORY_PREDICTOR, "TrajectoryPredictor"),
    (TRAJECTORY_SAFETY, "TrajectorySafetyEvaluator"),
):
    for forbidden in (
        "NavigationExecutionSnapshot",
        "ReplicatedNavigationExecution",
        "navigationExecution",
        "replicatedNavigationExecution",
        "authoritativeIntentRevision",
    ):
        require(
            forbidden not in planner,
            f"{label} illegally consumes replicated execution truth: {forbidden}",
        )

require(
    "navigation_replication_truth" in RUNTIME_CMAKE,
    "runtime suite does not register navigation replication truth test",
)

print("NAVIGATION LIVE REPLICATION/GUIDANCE CONTRACT: PASS")
print(" - exact server-executed navigation revision/demand is replicated per ShipSnapshot")
print(" - binary wire schema is explicitly versioned and absent execution costs one sparse variant tag")
print(" - ClientWorldState retains the payload and workspace mirrors it read-only")
print(" - stable ShipInstanceId resolves the current runtime entity execution truth")
print(" - GuidanceHudPresentation exposes the selected executor's authoritative execution")
print(" - local guidance/docking planners do not consume or rewrite replicated execution")
