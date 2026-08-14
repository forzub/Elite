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
server = read("src/game/server/GameServer.cpp")
runner = read("src/game/server/ServerRunner.cpp")
envelope_text = envelope
client = read("src/game/client/ClientWorldState.cpp")
merge = read("src/game/network/ReplicationSnapshotMerge.h")
snapshot = read("src/game/simulation/SimulationSnapshot.h")

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
    "buildShipReplicationInterestPlan" in server
    and "m_sessions.controlledEntity(sessionId)" in server,
    "GameServer does not compose replication interest from destination session authority",
)
require(
    "lastShipInterestPlan" in runner
    and "shipReplicationInterestPlanForSession" in runner,
    "ServerRunner does not retain a separate interest plan per transport/session binding",
)
require(
    "shipInterest" not in envelope_text,
    "detailed/server interest metadata must not leak through the client replication envelope",
)
require(
    "ReplicatedEntitySetMode::FullAuthoritativeSet" in server,
    "M6 must remain full-presence until sparse cadence is enabled in a later stage",
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
    "canonical.replication.entitySetMode = Mode::FullAuthoritativeSet" in merge,
    "retained history snapshots must be canonical full-presence samples",
)

print("[PASS] per-session replication interest is separated from retain/update/remove protocol semantics")
