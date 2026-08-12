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
    print(f"Server-worker architecture check failed: {message}", file=sys.stderr)
    raise SystemExit(1)


worker_h = read("src/game/server/ServerWorker.h")
worker_cpp = read("src/game/server/ServerWorker.cpp")
host_h = read("src/game/host/LocalGameHost.h")
host_cpp = read("src/game/host/LocalGameHost.cpp")
loopback_h = read("src/game/network/LocalLoopbackTransport.h")
loopback_cpp = read("src/game/network/LocalLoopbackTransport.cpp")
debug_h = read("src/game/debug/LocalDebugSessionControl.h")
debug_cpp = read("src/game/debug/LocalDebugSessionControl.cpp")
runtime_h = read("src/game/server/ServerRuntime.h")
root_cmake = read("CMakeLists.txt")
space_state_cpp = read("src/game/SpaceState.cpp")
local_session_cpp = read("src/game/host/LocalGameSession.cpp")
runner_h = read("src/game/server/ServerRunner.h")
session_h = read("src/game/session/IGameSession.h")

# LocalGameHost may own a worker handle, but authoritative runtime memory itself
# must exist only on the worker thread.
for required in (
    "class ServerWorker final",
    "std::thread m_thread",
    "std::mutex m_mutex",
    "std::condition_variable m_condition",
    "std::unique_ptr<server::ServerWorker> m_worker",
    "std::make_unique<server::ServerWorker>(",
    "m_worker->advance(",
    "m_worker->fixedStepSeconds()",
):
    if required not in worker_h and required not in worker_cpp and required not in host_h and required not in host_cpp:
        fail(f"server worker ownership/lifecycle contract is incomplete: {required}")

for forbidden in (
    "std::unique_ptr<server::ServerRuntime> m_runtime",
    "std::make_unique<server::ServerRuntime>(",
    "m_runtime->advance(",
    "m_runtime->fixedStepSeconds()",
):
    if forbidden in host_h or forbidden in host_cpp:
        fail(f"LocalGameHost regained direct ServerRuntime ownership/access: {forbidden}")

if re.search(r"\bServerRuntime\s+[A-Za-z_]\w*\s*[;{=]", worker_h):
    fail("ServerWorker stores ServerRuntime as cross-thread member state")

for required in (
    "ServerRuntime runtime(worldParams, transport, debugChannel)",
    "runtime.advance(elapsedSeconds)",
    "runtime.fixedStepSeconds()",
    "m_thread = std::thread(",
    "m_thread.join()",
):
    if required not in worker_cpp:
        fail(f"ServerRuntime is not exclusively constructed/advanced/destroyed inside worker thread: {required}")

# Stage-2 local pacing is deliberately a *single-flight* asynchronous pipeline:
# the caller may overlap the newly submitted server batch with client/render
# work, but a second authoritative batch cannot be queued until the previous one
# has completed. This bounds latency/backlog without dropping fixed-step inputs.
advance_marker = "ServerAdvanceResult ServerWorker::advance(double elapsedSeconds)"
fixed_step_marker = "double ServerWorker::fixedStepSeconds() const"
if advance_marker not in worker_cpp or fixed_step_marker not in worker_cpp:
    fail("could not locate ServerWorker::advance body")
advance_body = worker_cpp.split(advance_marker, 1)[1].split(fixed_step_marker, 1)[0]

for required in (
    "return !m_advancePending || static_cast<bool>(m_failure)",
    "const ServerAdvanceResult completedResult = m_lastAdvanceResult",
    "m_pendingElapsedSeconds = elapsedSeconds",
    "m_advancePending = true",
    "return completedResult",
):
    if required not in advance_body:
        fail(f"single-flight asynchronous pacing contract is incomplete: {required}")

if advance_body.count("m_condition.wait(") != 1:
    fail("ServerWorker::advance must wait only for the previous in-flight batch")

if "m_completedSequence" in advance_body or "m_submittedSequence" in advance_body:
    fail("single-flight pacing retained obsolete per-frame sequence bookkeeping")

submit_pos = advance_body.find("m_advancePending = true")
return_pos = advance_body.find("return completedResult")
if submit_pos < 0 or return_pos < submit_pos:
    fail("ServerWorker does not return only after submitting the current batch")


# Existing debug performance telemetry must continue to mean authoritative
# fixed-simulation CPU time after the call itself becomes asynchronous. Do not
# silently relabel main-thread pipeline wait time as server simulation cost.
for required, text in (
    ("double executionWallSeconds = 0.0", runner_h),
    ("double serverExecutionWallSeconds = 0.0", session_h),
    ("sessionResult.serverExecutionWallSeconds = result.executionWallSeconds", local_session_cpp),
    ("serverAdvance.serverExecutionWallSeconds * 1000.0", space_state_cpp),
    ("std::chrono::steady_clock::now()", worker_cpp),
):
    if required not in text:
        fail(f"asynchronous server performance telemetry contract is incomplete: {required}")

# Both local channel implementations now straddle the app/server threads and
# must protect every shared queue/value domain.
for text, label in (
    (loopback_h, "LocalLoopbackTransport"),
    (debug_h, "LocalDebugSessionControl"),
):
    if "mutable std::mutex m_mutex" not in text:
        fail(f"{label} has no mutex protecting cross-thread local state")

if loopback_cpp.count("std::lock_guard<std::mutex> lock(m_mutex)") < 20:
    fail("LocalLoopbackTransport endpoint methods are not consistently mutex-protected")

if debug_cpp.count("std::lock_guard<std::mutex> lock(m_mutex)") < 10:
    fail("LocalDebugSessionControl endpoint methods are not consistently mutex-protected")

if "rand()" in loopback_cpp or "std::rand()" in loopback_cpp:
    fail("loopback worker still touches process-global C RNG state")

if "std::minstd_rand m_packetLossRng" not in loopback_h:
    fail("loopback packet-loss emulation lost its transport-local RNG")

if "src/game/server/ServerWorker.cpp" not in root_cmake:
    fail("main game target does not compile the authoritative server worker")

# ServerRuntime remains the sole owner of GameServer, while ServerWorker owns
# only the lifetime/thread boundary around ServerRuntime.
if "std::unique_ptr<GameServer> m_server" not in runtime_h:
    fail("ServerRuntime no longer owns GameServer authority")
if "std::thread" in runtime_h:
    fail("thread lifecycle leaked into ServerRuntime instead of staying in ServerWorker")

print("[PASS] authoritative server worker + thread-safe local channel boundary")
