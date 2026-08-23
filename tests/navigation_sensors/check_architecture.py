#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]

def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8")

def fail(msg: str) -> None:
    print(f"[FAIL] navigation sensor architecture: {msg}", file=sys.stderr)
    raise SystemExit(1)

sensor_types = read("src/game/equipment/radar/RadarSensorTypes.h")
session = read("src/game/simulation/ClientSessionSnapshot.h")
ship_snapshot = read("src/game/simulation/ShipSnapshot.h")
ship_core = read("src/game/ship/core/ShipCore.cpp")
server = read("src/game/server/GameServer.cpp")
client_h = read("src/game/client/GameClient.h")
client_cpp = read("src/game/client/GameClient.cpp")
runtime_flags = read("src/game/RuntimeFeatureFlags.h")

if "EntityId" in sensor_types or "EntityID.h" in sensor_types:
    fail("public RadarScanReport leaks authoritative EntityId")

if "RadarScanReport" in ship_snapshot or "navigationSensors" in ship_snapshot:
    fail("player sensor data leaked into shared ShipSnapshot replication")

for token in (
    "TEST_IDEAL_RADAR",
    "m_role == ShipRole::Player",
):
    if token not in ship_core:
        fail(f"player radar slot is not explicitly replaced by the test unit: {token}")

for token in (
    "navigationSensors",
    "ClientNavigationSensorSnapshot",
):
    if token not in session:
        fail(f"session-private sensor channel missing: {token}")

for token in (
    "measurementDue(universeTimeSeconds)",
    "captureMeasurement(",
    "advanceAvailability(universeTimeSeconds)",
    "navigationSensorsForSession(sessionId)",
    "snapshot.session.navigationSensors = {};",
):
    if token not in server:
        fail(f"server radar cadence/session boundary missing: {token}")

for token in (
    "navigationSensors() const",
    "return m_sessionSnapshot.navigationSensors;",
):
    if token not in client_h + client_cpp:
        fail(f"client navigation-sensor consumer seam missing: {token}")

if "RadarSimulationEnabled = true" in runtime_flags:
    fail("legacy all-ships radar simulation was re-enabled instead of using the test unit")

if "RadarHudEnabled = true" in runtime_flags:
    fail("test radar accidentally enabled the legacy radar HUD")

for path in (ROOT / "src/ui").rglob("*.cpp"):
    if "navigationSensors" in path.read_text(encoding="utf-8", errors="replace"):
        fail(f"test radar was wired directly into HUD/UI: {path.relative_to(ROOT)}")

print("[PASS] player test radar -> session-private discrete sensor contract")
