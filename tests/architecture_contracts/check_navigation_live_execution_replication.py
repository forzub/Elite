#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8")

def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")

SNAPSHOT_H = read("src/game/simulation/NavigationExecutionSnapshot.h")
SHIP_SNAPSHOT_H = read("src/game/simulation/ShipSnapshot.h")
SIM_CPP = read("src/game/simulation/GameSimulation.cpp")
WIRE_SCHEMA = read("src/game/network/WireDataSchema.h")
WIRE_CODEC = read("src/game/network/WireDataCodec.h")
CLIENT_H = read("src/game/client/ClientWorldState.h")
CLIENT_CPP = read("src/game/client/ClientWorldState.cpp")
REPLICATED_H = read("src/game/navigation/ReplicatedNavigationExecutionState.h")
WORKSPACE_H = read("src/game/navigation/ClientNavigationWorkspace.h")
SPACE_CPP = read("src/game/SpaceState.cpp")
PRESENTATION_H = read("src/game/presentation/GuidanceHudPresentation.h")
WIRE_TEST = read("tests/architecture_contracts/WireDataPlaneContractTests.cpp")

for marker in (
    "struct NavigationExecutionSnapshot",
    "intentRevision",
    "activeTargetRevision",
    "executedLinearAccelerationDemandMapMps2",
    "executedAngularAccelerationDemandMapRadPerSec2",
    "reactionBlocked",
    "decisionSampled",
    "queuedCommandApplied",
):
    require(marker in SNAPSHOT_H,
            f"replicated navigation execution DTO missing: {marker}")

require(
    "game::simulation::NavigationExecutionSnapshot        navigationExecution;" in SHIP_SNAPSHOT_H,
    "ShipSnapshot does not carry navigationExecution",
)

for marker in (
    "m_npcNavigationExecutionSnapshots.find(id)",
    "s.navigationExecution",
    "replicated.intentRevision = execution.intentRevision",
    "replicated.executedLinearAccelerationDemandMapMps2",
    "replicated.executedAngularAccelerationDemandMapRadPerSec2",
):
    require(marker in SIM_CPP,
            f"GameSimulation does not publish exact execution truth: {marker}")

for marker in (
    "ELITE_WIRE_SCHEMA(\n    game::simulation::NavigationExecutionSnapshot,",
    "v.navigationExecution",
):
    require(marker in WIRE_SCHEMA,
            f"wire schema missing navigation execution: {marker}")

require(
    "SimulationSnapshotWireSchemaVersion = 8u" in WIRE_CODEC,
    "SimulationSnapshot wire schema version was not bumped for navigation execution",
)

require(
    "game::simulation::NavigationExecutionSnapshot   navigationExecution;" in CLIENT_H,
    "ClientShipState does not retain navigation execution",
)
require(
    CLIENT_CPP.count("state.navigationExecution = s.navigationExecution;") >= 2,
    "client hydration/update paths do not both retain navigation execution",
)

for marker in (
    "struct ReplicatedNavigationExecution",
    "ShipInstanceId shipInstanceId",
    "m_entityByShipInstance",
    "find(\n        const NavigationAssetRef& asset",
):
    require(marker in REPLICATED_H,
            f"client replicated execution index missing: {marker}")

require(
    "syncReplicatedNavigationExecution(" in WORKSPACE_H,
    "ClientNavigationWorkspace lacks explicit replicated-execution ingress",
)
require(
    "const ReplicatedNavigationExecutionState&\n    replicatedNavigationExecution() const noexcept" in WORKSPACE_H,
    "ClientNavigationWorkspace lacks const replicated-execution inspection",
)
require(
    "ReplicatedNavigationExecutionState& replicatedNavigationExecution()" not in WORKSPACE_H,
    "client planners gained mutable access to replicated server execution truth",
)

for marker in (
    "navigationExecutionSnapshotTick",
    "m_lastNavigationExecutionSnapshotTick",
    "shipState.navigationExecution.valid",
    "entry.shipInstanceId = shipState.instanceId",
    "syncReplicatedNavigationExecution",
):
    require(marker in SPACE_CPP,
            f"client snapshot-cadence execution sync missing: {marker}")

require(
    "navigationExecutionSnapshotTick !=\n            m_lastNavigationExecutionSnapshotTick" in SPACE_CPP,
    "replicated execution is not gated by accepted server snapshot tick",
)

for marker in (
    "ReplicatedNavigationExecutionHudPresentation",
    "buildReplicatedNavigationExecutionHudPresentation",
    "hasAuthoritativeExecution",
    "authoritativeIntentRevision",
    "authoritativeExecutedLinearAccelerationMapMps2",
    "replicatedNavigationExecution().find",
):
    require(marker in PRESENTATION_H,
            f"HUD/debug read-only execution presentation missing: {marker}")

for marker in (
    "ship.navigationExecution.valid = true",
    "ship.navigationExecution.intentRevision = 9001u",
    "navigation execution intent revision did not round-trip",
    "navigation executed linear demand did not round-trip",
    "navigation executed angular demand did not round-trip",
):
    require(marker in WIRE_TEST,
            f"wire round-trip fixture missing: {marker}")

# Replicated execution is observational truth. It must never become an input
# to a client planner or trajectory generator.
for rel in (
    "src/game/navigation/LocalGuidancePlanner.cpp",
    "src/game/navigation/DockingPathPlanner.cpp",
    "src/game/client/ClientNavigationPlanningSnapshotFactory.cpp",
    "src/game/navigation/TrajectoryPredictor.cpp",
    "src/game/navigation/TrajectorySafetyEvaluator.cpp",
):
    body = read(rel)
    for forbidden in (
        "navigationExecution",
        "replicatedNavigationExecution",
        "NavigationExecutionSnapshot",
        "ReplicatedNavigationExecution",
    ):
        require(
            forbidden not in body,
            f"{rel} illegally consumes replicated execution truth: {forbidden}",
        )

print("NAVIGATION LIVE EXECUTION REPLICATION CONTRACT: PASS")
print(" - exact server intent/execution revisions and demands ride inside ShipSnapshot")
print(" - binary wire schema is versioned and round-trip tested")
print(" - ClientWorldState retains the replicated execution product verbatim")
print(" - client workspace exposes only read-only replicated execution inspection")
print(" - replication mirror refreshes only on new accepted server snapshot ticks")
print(" - HUD/debug presentation reads the same server execution product")
print(" - client planners and trajectory generators do not consume replicated execution as maneuver input")
