#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8")


def require(text: str, token: str, message: str) -> None:
    if token not in text:
        raise SystemExit(message)


cmake = read("CMakeLists.txt")
server_main = read("src/server_main.cpp")
guard_h = read("src/platform/ProcessSingleInstanceGuard.h")
guard_cpp = read("src/platform/ProcessSingleInstanceGuard.cpp")
helper = read("tests/helpers/elite_server_process_guard.sh")
run_all = read("tests/run_all_mingw64.sh")
network = read("tests/network_process_acceptance/run_mingw64.sh")
standalone = read("tests/runtime_standalone/run_mingw64.sh")

require(cmake, "src/platform/ProcessSingleInstanceGuard.cpp", "EliteServer target must link the process singleton guard")
require(server_main, 'ProcessSingleInstanceGuard instanceGuard("EliteServer")', "EliteServer main must acquire singleton ownership before runtime construction")
require(server_main, "another EliteServer instance is already running", "EliteServer must report duplicate-process rejection explicitly")
require(guard_h, "anotherInstanceRunning()", "singleton guard must distinguish duplicate ownership from setup failure")
require(guard_cpp, "CreateMutexA", "Windows singleton ownership must be atomic at OS level")
require(guard_cpp, "ERROR_ALREADY_EXISTS", "Windows singleton guard must detect an existing named mutex")
require(guard_cpp, "LOCK_EX | LOCK_NB", "POSIX singleton ownership must use a non-blocking exclusive file lock")

for name, script in [
    ("ready harness", run_all),
    ("network process acceptance", network),
    ("standalone runtime", standalone),
]:
    require(script, "elite_fail_if_server_running", f"{name} must refuse to start while an external EliteServer process exists")

require(helper, "Get-Process -Name EliteServer", "Windows preflight must inspect running EliteServer processes")
require(helper, "PID={0} PATH={1}", "preflight should report conflicting server PID/path")
require(helper, "will not kill developer-owned processes", "preflight must fail instead of killing a developer server")
require(network, "SECOND_SERVER_OUTPUT", "network acceptance must exercise duplicate server rejection")
require(network, "second EliteServer process rejected by singleton guard", "network acceptance must report singleton coverage")

print("[PASS] test preflight rejects external servers and EliteServer enforces atomic single-instance ownership")
