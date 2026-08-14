#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        print(f"[FAIL] {message}", file=sys.stderr)
        raise SystemExit(2)


envelope = read("src/game/network/ReplicationEnvelope.h")
interest = read("src/game/server/ReplicationInterestPolicy.h")
publication = read("src/game/server/ReplicationPublicationPolicy.h")
server = read("src/game/server/GameServer.cpp")
server_h = read("src/game/server/GameServer.h")
runner = read("src/game/server/ServerRunner.cpp")
runner_h = read("src/game/server/ServerRunner.h")
client = read("src/game/client/ClientWorldState.cpp")
merge = read("src/game/network/ReplicationSnapshotMerge.h")
snapshot = read("src/game/simulation/SimulationSnapshot.h")
runtime = read("src/game/server/ServerRuntime.cpp")

require(
    "ReplicationEnvelope replication" in snapshot,
    "SimulationSnapshot lost explicit replication-envelope semantics",
)
require(
    "FullAuthoritativeSet" in envelope and "SparseRetainMissing" in envelope,
    "replication protocol must distinguish full presence from sparse omission",
)
require(
    "removedShipIds" in envelope
    and "removedObjectIds" in envelope
    and "removedHubIds" in envelope,
    "sparse replication needs explicit lifecycle removal lists",
)
require(
    "activation" in interest
    and "NOT a sensor/visibility" in interest,
    "replication interest must stay separate from simulation activation and gameplay visibility",
)
require(
    "controlledEntityId" in interest
    and "candidate.transform.motion.systemId" in interest
    and "distanceMeters" in interest,
    "ship interest must be derived per controlled entity/system/distance",
)
require(
    "ReplicationPublicationState" in publication
    and "lastShipPublicationTimeSeconds" in publication
    and "hasBootstrapBaseline" in publication,
    "M7 lost per-transport sparse publication memory",
)
require(
    "selectReplicationPublications" in publication
    and "targetIntervalSeconds" in publication
    and "shipHydrationIds" in publication,
    "M7 does not consume interest cadence or mark first/re-entry hydration",
)
require(
    "removedObjectIds" in publication and "removedHubIds" in publication,
    "ship-only decimation must still preserve object/hub lifecycle under sparse envelope semantics",
)
require(
    "m_canonicalReplicationSnapshot" in server_h
    and "materializeCanonicalReplicationSnapshot" in server,
    "server lost field-retained canonical hydration state",
)
require(
    "copyHydratedSnapshotForSession" in server
    and "copySparseSnapshotForSession" in server,
    "GameServer does not expose distinct full hydration and sparse packet composition seams",
)
require(
    "ReplicatedEntitySetMode::SparseRetainMissing" in server
    and "selection.shipUpdateIds" in server
    and "selection.shipHydrationIds" in server,
    "production session snapshot composition is not actually sparse/hydration-aware",
)
require(
    "seedTransportReplicationBaseline" in runner_h
    and "replicationPublicationState" in runner_h,
    "ServerRunner binding lost per-session bootstrap/cadence memory",
)
require(
    "selectReplicationPublications" in runner
    and "copySparseSnapshotForSession" in runner,
    "ServerRunner does not consume per-session interest as real sparse publication cadence",
)
require(
    "copyHydratedSnapshotForSession" in runtime
    and "seedTransportReplicationBaseline" in runtime,
    "late join/bootstrap is not hydrated before sparse omission becomes legal",
)
require(
    "shipInterest" not in envelope,
    "detailed/server interest metadata must not leak through the client replication envelope",
)
require(
    "FullAuthoritativeSet" in client
    and "removedShipIds" in client
    and "removedObjectIds" in client
    and "removedHubIds" in client,
    "ClientWorldState still treats every sparse omission as destruction",
)
require(
    "materializeCanonicalReplicationSnapshot" in client
    and "previousCanonical" in client,
    "client presentation history is not materializing retained sparse state",
)
require(
    "materializeGraphSnapshot" in merge
    and "!incoming.hasModules" in merge
    and "!incoming.hasStructuralLinks" in merge,
    "canonical history/hydration loses sparse nested graph fields",
)
require(
    "canonical.replication.entitySetMode = Mode::FullAuthoritativeSet" in merge,
    "retained history snapshots must be canonical full-presence samples",
)

print("[PASS] per-session interest drives real sparse cadence with hydrated re-entry and explicit lifecycle")
