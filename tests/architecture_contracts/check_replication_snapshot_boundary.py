#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8")


def fail(message: str) -> None:
    print(
        f"Replication-snapshot architecture check failed: {message}",
        file=sys.stderr,
    )
    raise SystemExit(1)


simulation_h = read("src/game/simulation/GameSimulation.h")
simulation_cpp = read("src/game/simulation/GameSimulation.cpp")
server_cpp = read("src/game/server/GameServer.cpp")

if "SimulationSnapshot buildReplicationSnapshot(std::uint64_t serverTick);" not in simulation_h:
    fail("GameSimulation lost the explicit replication publication seam")

if "SimulationSnapshot                  m_snapshot" in simulation_h:
    fail("GameSimulation again owns a continuously rebuilt snapshot cache")

if "const SimulationSnapshot& GameSimulation::snapshot() const" in simulation_cpp:
    fail("legacy continuously rebuilt GameSimulation::snapshot() returned")

try:
    update_body = simulation_cpp.split("void GameSimulation::update(", 1)[1].split(
        "SimulationSnapshot GameSimulation::buildReplicationSnapshot", 1
    )[0]
except IndexError:
    fail("could not isolate GameSimulation::update/buildReplicationSnapshot boundary")

# The fixed simulation step owns authoritative state evolution only. Replication
# DTO materialization belongs to the server publication cadence so dirty flags
# cannot be consumed by a snapshot that is never published.
for forbidden in (
    "ShipSnapshot s;",
    "ObjectSnapshot o;",
    "buildModuleSnapshot(",
    "buildStructuralLinkSnapshot(",
    "HitVolumeSnapshotBuilder::build(",
    "staticSnapshotPayloadDirty = false",
):
    if forbidden in update_body:
        fail(f"replication work leaked back into GameSimulation::update: {forbidden}")

if server_cpp.count("m_simulation.buildReplicationSnapshot(") < 2:
    fail("GameServer no longer builds initial and cadence-controlled snapshots explicitly")

for forbidden in (
    "m_simulation.snapshot()",
    "m_simulation.setTick(",
):
    if forbidden in server_cpp:
        fail(f"legacy simulation-owned snapshot lifecycle returned: {forbidden}")

if "m_shipGraphPayloadPublicationsRemaining" not in simulation_cpp:
    fail("ship graph resend lifetime is no longer publication-based")

if "m_shipGraphPayloadPublicationsRemaining[id] = 2" not in simulation_cpp:
    fail("ship graph publication redundancy contract changed unexpectedly")

print("Replication-snapshot architecture check passed.")
