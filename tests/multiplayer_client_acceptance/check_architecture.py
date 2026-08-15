#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
HARNESS = ROOT / "src/game/diagnostics/MultiplayerClientAcceptanceHarness.cpp"
GAME_CLIENT = ROOT / "src/game/client/GameClient.cpp"
CLIENT_WORLD = ROOT / "src/game/client/ClientWorldState.cpp"
MAIN = ROOT / "src/main.cpp"
CMAKE = ROOT / "CMakeLists.txt"
APPLICATION = ROOT / "src/core/Application.cpp"
WINDOW_CPP = ROOT / "src/window/Window.cpp"
WINDOW_H = ROOT / "src/window/Window.h"


def fail(message: str) -> None:
    print(f"[FAIL] multiplayer client acceptance architecture: {message}", file=sys.stderr)
    raise SystemExit(1)


def read(path: Path) -> str:
    if not path.is_file():
        fail(f"missing file: {path.relative_to(ROOT)}")
    return path.read_text(encoding="utf-8", errors="replace")


harness = read(HARNESS)
game_client = read(GAME_CLIENT)
client_world = read(CLIENT_WORLD)
main = read(MAIN)
cmake = read(CMAKE)
application = read(APPLICATION)
window_cpp = read(WINDOW_CPP)
window_h = read(WINDOW_H)

for token in (
    "game::server::ServerRuntime runtime(",
    "LocalLoopbackTransport transportA;",
    "LocalLoopbackTransport transportB;",
    "GameClient clientA(transportA);",
    "GameClient clientB(transportB);",
    "runtime.attachPlayerSessionTransport(",
    "clientA.beginSynchronization();",
    "clientB.beginSynchronization();",
    "clientA.prepareGameplayFrame(FrameSeconds);",
    "clientB.prepareGameplayFrame(FrameSeconds);",
    "clientA.submitInput(*controlA);",
    "clientB.submitInput(*controlB);",
    "runtime.advance(FrameSeconds);",
    "clientA.lastAcknowledgedControlTick()",
    "clientB.lastAcknowledgedControlTick()",
    "clientA.world().isLocalControlledEntity(shipAId)",
    "clientB.world().isLocalControlledEntity(shipBId)",
):
    if token not in harness:
        fail(f"two-client production-path token disappeared: {token}")


for token in (
    "game::server::ServerRuntime runtime(worldParams, debugChannel);",
    "makeAcceptanceIdentity(2001u, 1u)",
    "makeAcceptanceIdentity(2002u, 2u)",
    "std::abs(worldDistance - 50.0)",
    "renderDistanceA",
    "renderDistanceB",
):
    if token not in harness:
        fail(f"dedicated two-slot bootstrap acceptance token disappeared: {token}")

for token in (
    "requireAccountReconnectReturnsSamePersistentPlayer()",
    "same account obtained two concurrent gameplay sessions",
    "reconnected account was rebound to a different PlayerId",
    "reconnected account was rebound to a different ShipInstanceId",
    "clientA.playerIdentityId()",
    "clientB.playerIdentityId()",
    "clientA.controlledShipInstanceId()",
    "clientB.controlledShipInstanceId()",
    "clientA.localControlledEntityId()",
    "clientB.localControlledEntityId()",
):
    if token not in harness:
        fail(f"persistent two-client identity assertion disappeared: {token}")

for token, source in (
    ("bool ownsForegroundInput() const;", window_h),
    ("GetForegroundWindow()", window_cpp),
    ("foregroundPid == GetCurrentProcessId()", window_cpp),
    ("const bool ownsForegroundInput = m_window->ownsForegroundInput();", application),
    ("if (ownsForegroundInput)", application),
    ("Input::instance().reset();", application),
):
    if token not in source:
        fail(f"graphical-client foreground input ownership token disappeared: {token}")

for forbidden in (
    "GameServer server",
    "GameSimulation simulation",
    ".world().predict(",
    ".enqueueClientMessage(",
    ".publishSessionWelcomeImmediately(",
    ".publishSnapshotImmediately(",
):
    if forbidden in harness:
        fail(f"harness bypasses a production client/session seam via: {forbidden}")

for token in (
    "m_lastAcknowledgedControlTick = acknowledgedControlTick;",
    "m_world.setLocalControlledEntity(m_playerId);",
):
    if token not in game_client:
        fail(f"GameClient lost client-owned acknowledgement/identity seam: {token}")

for token in (
    "state.acknowledgedControlTick = s.acknowledgedControlTick;",
    "isLocalControlledEntity(ship.id)",
):
    if token not in client_world:
        fail(f"ClientWorldState lost replicated two-client evidence seam: {token}")

if "--self-test-multiplayer-client" not in main:
    fail("EliteGame no longer exposes the multiplayer client acceptance self-test")

if "src/game/diagnostics/MultiplayerClientAcceptanceHarness.cpp" not in cmake:
    fail("multiplayer client acceptance harness disappeared from EliteGame build")

print("[PASS] two real GameClient state machines are acceptance-tested on one ServerRuntime")
