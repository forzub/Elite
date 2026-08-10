#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
HARNESS = ROOT / "src/game/diagnostics/ClientAcceptanceHarness.cpp"
MAPPER = ROOT / "src/game/ship/controller/PlayerInputMapper.cpp"


def fail(message: str) -> None:
    print(f"[FAIL] client acceptance architecture: {message}", file=sys.stderr)
    raise SystemExit(1)


harness = HARNESS.read_text(encoding="utf-8")
mapper = MAPPER.read_text(encoding="utf-8")

required_harness_tokens = (
    "game::host::LocalGameSession",
    "client.prepareGameplayFrame",
    "client.submitInput",
    "session.advance",
    "client.update",
    "requestGalaxyMapSnapshot",
    "requestSystemMapSnapshot",
    "requestDetailMapSnapshot",
    "requestHubMapSnapshot",
    "acknowledgedControlTick",
)

for token in required_harness_tokens:
    if token not in harness:
        fail(f"production-path token disappeared: {token}")

for forbidden in (
    ".world().predict(",
    "SharedShipPhysics::integrate(",
    "DynamicMotionSystem::applyHubTacticalInput(",
    "GameServer server",
    "GameSimulation simulation",
):
    if forbidden in harness:
        fail(f"harness bypasses the client/server path via: {forbidden}")

if "updateFromKeyState(control, keys)" not in mapper:
    fail("runtime PlayerInputMapper no longer shares the injectable mapping path")

print("[PASS] client acceptance architecture guard")
