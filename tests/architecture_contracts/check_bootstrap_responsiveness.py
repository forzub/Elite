#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")


def fail(message: str) -> None:
    print(f"Bootstrap responsiveness contract failed: {message}", file=sys.stderr)
    raise SystemExit(1)


client_h = read("src/game/client/GameClient.h")
client_cpp = read("src/game/client/GameClient.cpp")
assembly_h = read("src/game/geometry/AssemblyMeshLibrary.h")
assembly_cpp = read("src/game/geometry/AssemblyMeshLibrary.cpp")
worker_h = read("src/game/server/ServerWorker.h")
worker_cpp = read("src/game/server/ServerWorker.cpp")
local_host = read("src/game/host/LocalGameHost.cpp")
local_session = read("src/game/host/LocalGameSession.cpp")
application_cpp = read("src/core/Application.cpp")
application_bootstrap = application_cpp[
    application_cpp.find("void Application::updatePendingSessionStart()"):
]
space_h = read("src/game/SpaceState.h")
space_cpp = read("src/game/SpaceState.cpp")

for token in (
    "std::future<void> m_assemblyPreloadFuture",
    "std::optional<SimulationSnapshot> m_deferredAssemblySnapshot",
    "std::launch::async",
    "AssemblyMeshLibrary::isLoaded",
    "m_deferredAssemblySnapshot = snapshot",
):
    if token not in client_h and token not in client_cpp:
        fail(f"client snapshot hydration is not staged off the UI thread: {token}")

if "m_world.applySnapshot(snapshot);" not in client_cpp:
    fail("authoritative snapshot application disappeared from GameClient")

for token in (
    "static std::mutex s_cacheMutex",
    "std::lock_guard<std::mutex> lock(s_cacheMutex)",
):
    if token not in assembly_h and token not in assembly_cpp:
        fail(f"shared CPU assembly cache is not thread-safe: {token}")

# The worker constructor must launch the authoritative thread and return. The
# old m_condition.wait(started) made local NEW GAME synchronous despite having a
# dedicated server worker.
ctor_start = worker_cpp.find("ServerWorker::ServerWorker(")
dtor_start = worker_cpp.find("ServerWorker::~ServerWorker()")
if ctor_start < 0 or dtor_start <= ctor_start:
    fail("could not isolate ServerWorker constructor")
ctor = worker_cpp[ctor_start:dtor_start]
if "m_thread = std::thread(" not in ctor:
    fail("ServerWorker constructor no longer launches authoritative worker")
if "m_condition.wait(" in ctor:
    fail("ServerWorker constructor synchronously waits for authoritative startup")

for token in (
    "bool ServerWorker::ready() const",
    "void ServerWorker::rethrowIfFailed() const",
    "return m_worker->ready()",
    "m_worker->rethrowIfFailed()",
    "if (!m_host->ready())",
):
    if token not in worker_h and token not in worker_cpp and token not in local_host and token not in local_session:
        fail(f"non-blocking local startup handshake is incomplete: {token}")

# OpenGL resource creation must stay on the owning render thread, but the
# heavyweight SpaceState bootstrap must yield back to the Win32/WebView pump
# between major phases instead of monopolizing one UI-thread call for seconds.
for token in (
    "enum class StartupMode",
    "Deferred",
    "bool advanceStartupInitialization()",
    "enum class StartupStage",
):
    if token not in space_h:
        fail(f"SpaceState staged-startup API is incomplete: {token}")

for token in (
    "bool SpaceState::advanceStartupInitialization()",
    "case StartupStage::Shaders:",
    "case StartupStage::SceneRenderer:",
    "case StartupStage::SystemMapRenderer:",
    "case StartupStage::Hud:",
):
    if token not in space_cpp:
        fail(f"SpaceState no longer advances heavyweight startup cooperatively: {token}")

for token in (
    "SessionStartStage::BuildingSpaceState",
    "SpaceState::StartupMode::Deferred",
    "space->advanceStartupInitialization()",
):
    if token not in application_bootstrap:
        fail(f"Application no longer yields between SpaceState startup stages: {token}")

if "m_states.push(std::make_unique<SpaceState>(m_states));" in application_bootstrap:
    fail("Windows/session bootstrap regressed to monolithic SpaceState construction")

print("[PASS] bootstrap CPU work is asynchronous and OpenGL SpaceState startup is cooperatively staged")
