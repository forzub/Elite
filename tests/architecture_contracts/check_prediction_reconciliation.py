#!/usr/bin/env python3

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "src"

failures = []

def fail(path: Path, message: str) -> None:
    failures.append(f"{path.relative_to(ROOT)}: {message}")

queue_h = SRC / "game/server/FixedStepControlQueue.h"
server_h = SRC / "game/server/GameServer.h"
server_cpp = SRC / "game/server/GameServer.cpp"
client_cpp = SRC / "game/client/GameClient.cpp"

for path in (queue_h, server_h, server_cpp, client_cpp):
    if not path.is_file():
        fail(path, "required fixed-step reconciliation contract file is missing")

if queue_h.is_file():
    text = queue_h.read_text(encoding="utf-8", errors="replace")
    for required in (
        "class FixedStepControlQueue",
        "m_queue.push_back(control)",
        "outControl = m_queue.front()",
        "m_queue.pop_front()",
        "m_lastProcessedTick = outControl.controlTick",
        "discardPendingAndAcknowledgeNewest",
    ):
        if required not in text:
            fail(queue_h, f"fixed-step input stream lost ordered consume semantics: {required}")

if server_h.is_file():
    text = server_h.read_text(encoding="utf-8", errors="replace")
    if "m_controlStreams" not in text:
        fail(server_h, "GameServer does not own per-ship fixed-step control streams")
    for forbidden in (
        "m_pendingCommands",
        "m_lastReceivedControlTicks",
        "m_lastProcessedControlTicks",
    ):
        if forbidden in text:
            fail(server_h, f"legacy split control/ack ownership returned: {forbidden}")

if server_cpp.is_file():
    text = server_cpp.read_text(encoding="utf-8", errors="replace")

    update_start = text.find("void GameServer::update(double dt)")
    submit_start = text.find("void GameServer::submitCommand(")
    receive_start = text.find("void GameServer::receiveClientMessage(", submit_start)

    update = text[update_start:submit_start] if update_start >= 0 and submit_start >= 0 else ""
    submit = text[submit_start:receive_start] if submit_start >= 0 and receive_start >= 0 else ""

    for required in (
        "stream.consumeNext(cmd)",
        "ship.setControlState(cmd)",
        "stream.discardPendingAndAcknowledgeNewest()",
    ):
        if required not in update:
            fail(server_cpp, f"authoritative tick does not consume the input stream correctly: {required}")

    for forbidden in (
        "queue.back()",
        "queue.clear()",
        "coalescedControlCommands",
    ):
        if forbidden in update or forbidden in submit:
            fail(server_cpp, f"control samples are being coalesced across simulation steps again: {forbidden}")

    for required in (
        "stream.enqueue(control)",
        "staleControlCommands",
    ):
        if required not in submit:
            fail(server_cpp, f"control enqueue contract is incomplete: {required}")

    for forbidden in (
        "AcceptedAfterDroppingOldest",
        "MaxControlCommandsPerShip",
    ):
        if forbidden in text or forbidden in server_h.read_text(encoding="utf-8", errors="replace"):
            fail(server_cpp, f"fixed-step input history may be silently truncated again: {forbidden}")

    if "lastProcessedTick()" not in text or "acknowledgedControlTick" not in text:
        fail(server_cpp, "snapshot acknowledgement is not derived from the consumed fixed-step stream")

if client_cpp.is_file():
    text = client_cpp.read_text(encoding="utf-8", errors="replace")
    for required in (
        "m_pendingInputs.front().controlTick <= acknowledgedControlTick",
        "m_pendingInputs.pop_front()",
        "replayPendingInputs(",
    ):
        if required not in text:
            fail(client_cpp, f"client replay boundary lost fixed-step acknowledgement semantics: {required}")

if failures:
    print("Prediction/reconciliation architecture check failed:", file=sys.stderr)
    for item in failures:
        print(f"  - {item}", file=sys.stderr)
    sys.exit(1)

print("Prediction/reconciliation architecture check passed.")
