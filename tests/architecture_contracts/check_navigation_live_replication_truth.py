#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

NAV_SNAPSHOT = (ROOT / "src/game/simulation/NavigationExecutionSnapshot.h").read_text(encoding="utf-8")
SHIP_SNAPSHOT = (ROOT / "src/game/simulation/ShipSnapshot.h").read_text(encoding="utf-8")
SIM_CPP = (ROOT / "src/game/simulation/GameSimulation.cpp").read_text(encoding="utf-8")
WIRE_SCHEMA = (ROOT / "src/game/network/WireDataSchema.h").read_text(encoding="utf-8")
WIRE_CODEC = (ROOT / "src/game/network/WireDataCodec.h").read_text(encoding="utf-8")
CLIENT_H = (ROOT / "src/game/client/ClientWorldState.h").read_text(encoding="utf-8")
CLIENT_CPP = (ROOT / "src/game/client/ClientWorldState.cpp").read_text(encoding="utf-8")
REPLICATED = (ROOT / "src/game/navigation/ReplicatedNavigationExecutionState.h").read_text(encoding="utf-8")
WORKSPACE = (ROOT / "src/game/navigation/ClientNavigationWorkspace.h").read_text(encoding="utf-8")
SPACE = (ROOT / "src/game/SpaceState.cpp").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")


for marker in (
    "struct NavigationExecutionSnapshot",
    "intentRevision",
    "activeTargetRevision",
    "idealLinearAccelerationDemandMapMps2",
    "idealAngularAccelerationDemandMapRadPerSec2",
    "executedLinearAccelerationDemandMapMps2",
    "executedAngularAccelerationDemandMapRadPerSec2",
    "reactionBlocked",
    "decisionSampled",
    "queuedCommandApplied",
    "pendingCommandCount",
):
    require(marker in NAV_SNAPSHOT, f"replicated navigation execution DTO missing: {marker}")

require(
    "NavigationExecutionSnapshot        navigationExecution" in SHIP_SNAPSHOT,
    "ShipSnapshot does not carry navigation execution truth",
)

for marker in (
    "m_npcNavigationExecutionSnapshots.find(id)",
    "replicated.intentRevision = execution.intentRevision",
    "replicated.activeTargetRevision",
    "replicated.executedLinearAccelerationDemandMapMps2",
    "replicated.executedAngularAccelerationDemandMapRadPerSec2",
):
    require(marker in SIM_CPP, f"GameSimulation publication seam missing: {marker}")

for marker in (
    "game::simulation::NavigationExecutionSnapshot,",
    "v.navigationExecution",
    "v.intentRevision",
    "v.activeTargetRevision",
    "v.executedLinearAccelerationDemandMapMps2",
    "v.executedAngularAccelerationDemandMapRadPerSec2",
):
    require(marker in WIRE_SCHEMA, f"wire schema missing navigation execution field: {marker}")

require(
    "SimulationSnapshotWireSchemaVersion = 8u" in WIRE_CODEC,
    "simulation snapshot wire schema version was not bumped for navigation execution",
)

require(
    "NavigationExecutionSnapshot   navigationExecution" in CLIENT_H,
    "ClientShipState does not retain replicated navigation execution",
)

require(
    CLIENT_CPP.count("state.navigationExecution = s.navigationExecution;") >= 2,
    "client hydration/update paths do not both copy navigation execution",
)

for marker in (
    "class ReplicatedNavigationExecutionState final",
    "void replace(",
    "const game::simulation::NavigationExecutionSnapshot* find(",
    "const std::unordered_map<",
):
    require(marker in REPLICATED, f"read-only replicated navigation state missing: {marker}")

for forbidden in (
    "LocalGuidancePlanner",
    "DockingPathPlanner",
    "TrajectoryGenerator",
    "GeometricPathPlanner",
    "NavigationRuntimeControlBridge",
    "ShipControlState",
):
    require(
        forbidden not in REPLICATED,
        f"replicated execution state must not become a client planner/control layer: {forbidden}",
    )

require(
    "void syncReplicatedNavigationExecution(" in WORKSPACE,
    "client workspace missing explicit replicated-navigation ingress",
)
require(
    "const ReplicatedNavigationExecutionState&" in WORKSPACE and
    "replicatedNavigationExecution() const noexcept" in WORKSPACE,
    "client workspace missing const replicated-navigation inspection",
)
require(
    "ReplicatedNavigationExecutionState& replicatedNavigationExecution()" not in WORKSPACE,
    "client workspace exposes mutable replicated server truth",
)

for marker in (
    "std::vector<game::navigation::ReplicatedNavigationExecution>",
    "shipState.navigationExecution.valid",
    "entry.execution = shipState.navigationExecution",
    "syncReplicatedNavigationExecution(",
):
    require(marker in SPACE, f"SpaceState replicated navigation sync missing: {marker}")

for forbidden in (
    "LocalGuidancePlanner::",
    "DockingPathPlanner::",
    "TrajectoryGenerator::",
):
    # Only inspect the narrow replicated-sync block, not the rest of SpaceState
    # which legitimately owns manual/player guidance.
    sync_start = SPACE.find(
        "std::vector<game::navigation::ReplicatedNavigationExecution>"
    )
    sync_end = SPACE.find("m_perfDockingTunnelBuilds", sync_start)
    require(sync_start >= 0 and sync_end > sync_start,
            "could not isolate replicated-navigation sync block")
    sync_block = SPACE[sync_start:sync_end]
    require(
        forbidden not in sync_block,
        f"replicated server execution is being replanned on the client: {forbidden}",
    )

print("NAVIGATION LIVE REPLICATION/GUIDANCE TRUTH CONTRACT: PASS")
print(" - exact server-executed navigation revisions/demands are embedded in ShipSnapshot")
print(" - wire schema v8 serializes that execution truth end to end")
print(" - ClientWorldState retains the replicated product for each ship")
print(" - ClientNavigationWorkspace exposes server execution as read-only state")
print(" - SpaceState syncs that state without invoking a client navigation planner")
print(" - existing manual/player guidance remains separate from server-executed NPC truth")
