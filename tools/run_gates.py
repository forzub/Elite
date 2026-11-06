#!/usr/bin/env python3
"""Run every Python `check_*.py` gate in the repository and compare the result
against the explicit allow-list of known reds (tools/gates_known_red.json).

Usage (from anywhere):
    python tools/run_gates.py                 # run all gates
    python tools/run_gates.py -k system_map   # only gates whose path contains the text
    python tools/run_gates.py -v              # print the output of every failing gate
    python tools/run_gates.py --timeout 120   # per-gate timeout in seconds (default 60)
    python tools/run_gates.py -j 8            # parallel workers (default: CPU count)

    python tools/run_gates.py --cpp           # also configure / build / run the C++ contract suites
    python tools/run_gates.py --cpp-only      # only the C++ suites (combine with -k to pick one)

Every gate runs with the repository root as cwd and PYTHONIOENCODING=utf-8.

C++ suites (--cpp / --cpp-only; OFF by default so the default Python run stays
fast - a cold build takes minutes). Each suite is a separate CMake project
(see CPP_SUITES below): configured with Ninja / Debug when its build dir has
no CMakeCache.txt, built for its one target, then its executable is run; a
failing configure, build or run is a red. Gate names are `cpp:<target>` and
are allow-listed like the Python gates. Build dirs are transient, under
D:/tmp/elite_* (override the root with ELITE_CPP_GATE_BUILD_ROOT). The MinGW
toolchain directory (default C:/msys64/mingw64/bin, override ELITE_MINGW_BIN)
is put first on PATH for cmake / ninja / g++ and the test executables.

Exit code 1 when:
  * a gate that is NOT in the allow-list fails (new red), or
  * an allow-listed gate now passes (stale allow-list: remove its entry), or
  * an allow-list entry names a gate that no longer exists.
"""
from __future__ import annotations

import argparse
import concurrent.futures
import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ALLOW_LIST = ROOT / "tools" / "gates_known_red.json"
SEARCH_DIRS = ("tests", "tools", "src")
SKIP_PARTS = {".git", "build", "__pycache__", "node_modules"}


def discover() -> list[str]:
    gates: list[str] = []
    for top in SEARCH_DIRS:
        base = ROOT / top
        if not base.is_dir():
            continue
        for path in base.rglob("check_*.py"):
            rel = path.relative_to(ROOT)
            if SKIP_PARTS.intersection(rel.parts):
                continue
            gates.append(rel.as_posix())
    return sorted(gates)


CPP_BUILD_ROOT = Path(os.environ.get("ELITE_CPP_GATE_BUILD_ROOT", "D:/tmp"))
MINGW_BIN = Path(os.environ.get("ELITE_MINGW_BIN", "C:/msys64/mingw64/bin"))
# name -> (CMake source dir relative to ROOT, build dir name under CPP_BUILD_ROOT, target)
CPP_SUITES: dict[str, tuple[str, str, str]] = {
    "cpp:wire_data_plane_contract_tests": (
        "tests/architecture_contracts", "elite_contracts", "wire_data_plane_contract_tests"),
    "cpp:system_map_behavior_tests": (
        "tests/system_map", "elite_sysmap_tests", "system_map_behavior_tests"),
}


def cpp_env() -> dict[str, str]:
    env = dict(os.environ)
    if MINGW_BIN.is_dir():
        env["PATH"] = str(MINGW_BIN) + os.pathsep + env.get("PATH", "")
    return env


def run_cpp_suite(name: str, timeout: float) -> tuple[str, bool, float, str]:
    source, build_name, target = CPP_SUITES[name]
    build_dir = CPP_BUILD_ROOT / build_name
    env = cpp_env()
    started = time.monotonic()
    log: list[str] = []

    def step(label: str, command: list[str]) -> bool:
        remaining = max(1.0, timeout - (time.monotonic() - started))
        # Windows CreateProcess ignores env["PATH"]: resolve the tool here.
        resolved = shutil.which(command[0], path=env.get("PATH"))
        if resolved:
            command = [resolved, *command[1:]]
        try:
            proc = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True,
                                  encoding="utf-8", errors="replace", timeout=remaining)
        except subprocess.TimeoutExpired:
            log.append(f"{label}: TIMEOUT (suite budget {timeout:.0f} s)")
            return False
        except OSError as error:
            log.append(f"{label}: cannot start {command[0]}: {error}")
            return False
        log.append(f"--- {label} (exit {proc.returncode})")
        log.append((proc.stdout or "") + (proc.stderr or ""))
        return proc.returncode == 0

    ok = True
    if not (build_dir / "CMakeCache.txt").exists():
        ok = step("configure", ["cmake", "-S", str(ROOT / source), "-B", str(build_dir), "-G", "Ninja",
                                "-DCMAKE_BUILD_TYPE=Debug"])
    if ok:
        ok = step("build", ["cmake", "--build", str(build_dir), "--target", target])
    if ok:
        exe = build_dir / (target + (".exe" if os.name == "nt" else ""))
        ok = step("run", [str(exe)])
    return name, ok, time.monotonic() - started, "\n".join(log)


def run_gate(gate: str, timeout: float) -> tuple[str, bool, float, str]:
    env = dict(os.environ)
    env["PYTHONIOENCODING"] = "utf-8"
    env["PYTHONDONTWRITEBYTECODE"] = "1"
    started = time.monotonic()
    try:
        proc = subprocess.run(
            [sys.executable, gate],
            cwd=ROOT,
            env=env,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=timeout,
        )
        ok = proc.returncode == 0
        output = (proc.stdout or "") + (proc.stderr or "")
    except subprocess.TimeoutExpired:
        ok = False
        output = f"TIMEOUT after {timeout:.0f} s"
    return gate, ok, time.monotonic() - started, output


def load_allow_list() -> dict[str, dict]:
    if not ALLOW_LIST.exists():
        return {}
    data = json.loads(ALLOW_LIST.read_text(encoding="utf-8"))
    return data.get("known_red", {})


def main() -> int:
    for stream in (sys.stdout, sys.stderr):
        try:
            stream.reconfigure(encoding="utf-8", errors="replace")
        except (AttributeError, ValueError):
            pass
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("-k", dest="filter", default="", help="only gates whose path contains this text")
    parser.add_argument("-v", "--verbose", action="store_true", help="print output of failing gates")
    parser.add_argument("--timeout", type=float, default=60.0, help="per-gate timeout, seconds")
    parser.add_argument("-j", "--jobs", type=int, default=os.cpu_count() or 4)
    parser.add_argument("--cpp", action="store_true", help="also build and run the C++ contract suites")
    parser.add_argument("--cpp-only", action="store_true", help="run only the C++ contract suites")
    parser.add_argument("--cpp-timeout", type=float, default=1800.0,
                        help="per C++ suite budget (configure + build + run), seconds")
    args = parser.parse_args()

    python_gates = [] if args.cpp_only else [g for g in discover() if args.filter in g]
    cpp_gates = [n for n in CPP_SUITES if args.filter in n] if (args.cpp or args.cpp_only) else []
    gates = python_gates + cpp_gates
    allow = load_allow_list()

    with concurrent.futures.ThreadPoolExecutor(max_workers=1) as cpp_pool:
        # The C++ suites build one after another (each build is itself parallel)
        # while the Python gates run.
        cpp_future = cpp_pool.submit(lambda: [run_cpp_suite(n, args.cpp_timeout) for n in cpp_gates])
        with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
            results = list(pool.map(lambda g: run_gate(g, args.timeout), python_gates))
        results += cpp_future.result()

    new_red: list[str] = []
    stale_allow: list[str] = []
    known_red: list[str] = []
    passed = 0
    for gate, ok, seconds, output in results:
        if ok:
            passed += 1
            tag = "PASS"
            if gate in allow:
                tag = "PASS (STALE ALLOW-LIST ENTRY)"
                stale_allow.append(gate)
        elif gate in allow:
            tag = "FAIL (known red)"
            known_red.append(gate)
        else:
            tag = "FAIL (NEW RED)"
            new_red.append(gate)
        print(f"{tag:<30} {seconds:6.1f}s  {gate}")
        if not ok and args.verbose:
            tail = output.strip().splitlines()[-15:]
            for line in tail:
                print(f"        | {line}")

    if args.filter:
        missing = []
    else:
        # a Python entry is judged only when the Python gates ran; a cpp:
        # entry must name a suite in CPP_SUITES
        known = set(python_gates) | set(CPP_SUITES)
        ran_python = not args.cpp_only
        missing = sorted(g for g in allow if g not in known and (ran_python or g.startswith("cpp:")))

    print()
    print(f"{len(gates)} gates: {passed} pass, {len(known_red)} known red, {len(new_red)} new red")
    for gate in known_red:
        entry = allow[gate]
        print(f"  known red: {gate}: {entry.get('reason', '')} [{entry.get('owner', '')}]")
    for gate in new_red:
        print(f"  NEW RED: {gate}")
    for gate in stale_allow:
        print(f"  STALE ALLOW-LIST (gate passes now, remove entry): {gate}")
    for gate in missing:
        print(f"  ALLOW-LIST ENTRY FOR MISSING GATE: {gate}")

    return 1 if (new_red or stale_allow or missing) else 0


if __name__ == "__main__":
    sys.exit(main())
