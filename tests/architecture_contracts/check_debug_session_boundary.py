#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]


def read(rel: str) -> str:
    path = ROOT / rel
    if not path.exists():
        fail(f"missing required file: {rel}")
    return path.read_text(encoding="utf-8", errors="replace")


def fail(message: str) -> None:
    print(f"Debug-session boundary architecture check failed: {message}", file=sys.stderr)
    raise SystemExit(1)


iface = read("src/game/debug/IDebugSessionControl.h")
server_iface = read("src/game/debug/IServerDebugChannel.h")
local_h = read("src/game/debug/LocalDebugSessionControl.h")
local_cpp = read("src/game/debug/LocalDebugSessionControl.cpp")
runtime_h = read("src/game/server/ServerRuntime.h")
runtime_cpp = read("src/game/server/ServerRuntime.cpp")
host_h = read("src/game/host/LocalGameHost.h")
host_cpp = read("src/game/host/LocalGameHost.cpp")
space_cpp = read("src/game/SpaceState.cpp")
acceptance = read("src/game/diagnostics/ClientAcceptanceHarness.cpp")

# The application-side facade must expose copied diagnostic values, never a
# reference into authoritative storage that would become a data race once the
# runtime has its own worker thread.
if "const SimulationSnapshot& snapshot() const" in iface:
    fail("IDebugSessionControl still exposes a reference to server-owned snapshot memory")

for required in (
    "virtual SimulationSnapshot snapshot() const = 0",
    "virtual std::uint64_t snapshotRevision() const = 0",
    "virtual std::uint64_t stateRevision() const = 0",
    "virtual void refreshStructureSnapshot() = 0",
):
    if required not in iface:
        fail(f"application debug facade lost copied/revisioned state contract: {required}")

# Mutating debug operations can no longer report synchronous GameServer results;
# they are requests whose effects are observed through a later diagnostic copy.
for method in (
    "destroyShipModule",
    "restoreShipModule",
    "resetShipStructure",
    "detachShipModule",
    "hangShipModule",
    "reevaluateShipStructure",
    "setShipStructuralLinkHealth",
):
    if re.search(rf"virtual\s+bool\s+{method}\s*\(", iface):
        fail(f"debug command {method} regained a synchronous authoritative return value")

for required in (
    "class IServerDebugChannel",
    "virtual bool receiveCommand(DebugCommand& outCommand) = 0",
    "virtual void publishSnapshot(const SimulationSnapshot& snapshot) = 0",
    "virtual void publishState(const DebugSessionState& state) = 0",
):
    if required not in server_iface:
        fail(f"server-side debug message endpoint is incomplete: {required}")

for pattern, label in (
    (r'#include\s+["<].*GameServer', "GameServer include"),
    (r'\bGameServer\s*[&*]', "GameServer reference/pointer"),
    (r'#include\s+["<].*ServerRuntime', "ServerRuntime include"),
    (r'\bServerRuntime\s*[&*]', "ServerRuntime reference/pointer"),
):
    if re.search(pattern, local_h) or re.search(pattern, local_cpp):
        fail(f"local debug bridge regained direct authoritative ownership knowledge: {label}")

for required in (
    "public IDebugSessionControl",
    "public IServerDebugChannel",
    "std::queue<DebugCommand> m_commands",
    "m_snapshot = snapshot",
    "return m_snapshot",
):
    if required not in local_h and required not in local_cpp:
        fail(f"local debug bridge no longer separates tool/server endpoints: {required}")

if "public game::debug::IDebugSessionControl" in runtime_h:
    fail("ServerRuntime once again implements the client/tool debug interface")

for required in (
    "game::debug::IServerDebugChannel& m_debugChannel",
    "m_debugChannel.receiveCommand(command)",
    "m_debugChannel.publishSnapshot(snapshot)",
    "m_debugChannel.publishState(makeDebugState())",
):
    if required not in runtime_h and required not in runtime_cpp:
        fail(f"ServerRuntime no longer routes debug through the server endpoint: {required}")

for required in (
    "std::unique_ptr<game::debug::LocalDebugSessionControl> m_debugControl",
    "return *m_debugControl",
    "*m_debugControl",
):
    if required not in host_h and required not in host_cpp:
        fail(f"LocalGameHost debug composition is incomplete: {required}")

if "return *m_runtime" in host_cpp:
    fail("LocalGameHost still exposes ServerRuntime as an application debug facade")

# UI code must wait for a new copied revision after server-mutating debug
# commands. Immediate command->read patterns would become stale once threaded.
for required in (
    "requestStructureDebugStateRefresh()",
    "m_debugSession->refreshStructureSnapshot()",
    "flushPendingDebugUiState()",
    "m_debugSession->snapshotRevision() >",
    "m_debugSession->stateRevision() >",
    "const auto snapshot = m_debugSession->snapshot()",
):
    if required not in space_cpp:
        fail(f"SpaceState still assumes synchronous debug/server access: {required}")

for required in (
    "const std::uint64_t previousRevision = debug.snapshotRevision()",
    "debug.refreshSnapshot()",
    "debug.snapshotRevision() > previousRevision",
):
    if required not in acceptance:
        fail(f"client acceptance no longer exercises copied async debug snapshots: {required}")

print("[PASS] asynchronous debug/control ownership boundary")
