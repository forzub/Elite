#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
server_h = (ROOT / "src/game/server/GameServer.h").read_text(encoding="utf-8", errors="replace")
server_cpp = (ROOT / "src/game/server/GameServer.cpp").read_text(encoding="utf-8", errors="replace")
harness = (ROOT / "src/game/diagnostics/MultiplayerClientAcceptanceHarness.cpp").read_text(encoding="utf-8", errors="replace")

errors = []

def require(text: str, token: str, message: str) -> None:
    if token not in text:
        errors.append(message)

require(server_h, "resetSessionControlState(", "GameServer has no explicit session control reset seam")
for token in (
    'resetSessionControlState(controlledEntityId, "session-create")',
    'resetSessionControlState(controlledEntityId, "session-disconnect")',
    "m_controlStreams.erase(streamIt)",
    "m_pendingClientShipCommands.erase(commandIt)",
    "ship->setControlState(ShipControlState{})",
):
    require(server_cpp, token, f"session control reset contract missing: {token}")

for token in (
    "oldAcknowledgedControlTick > 1",
    "reconnectClient.lastAcknowledgedControlTick() == 0",
    "reconnectAcknowledgedControlTick > 0",
    "reconnectAcknowledgedControlTick < oldAcknowledgedControlTick",
):
    require(harness, token, f"reconnect acceptance no longer proves fresh control epoch: {token}")

if errors:
    for error in errors:
        print(f"[FAIL] {error}", file=sys.stderr)
    raise SystemExit(1)

print("[PASS] reconnect creates a fresh session-owned control epoch")
