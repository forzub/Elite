#!/usr/bin/env python3
"""Run every Python `check_*.py` gate in the repository and compare the result
against the explicit allow-list of known reds (tools/gates_known_red.json).

Usage (from anywhere):
    python tools/run_gates.py                 # run all gates
    python tools/run_gates.py -k system_map   # only gates whose path contains the text
    python tools/run_gates.py -v              # print the output of every failing gate
    python tools/run_gates.py --timeout 120   # per-gate timeout in seconds (default 60)
    python tools/run_gates.py -j 8            # parallel workers (default: CPU count)

Every gate runs with the repository root as cwd and PYTHONIOENCODING=utf-8.

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
    args = parser.parse_args()

    gates = [g for g in discover() if args.filter in g]
    allow = load_allow_list()

    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        results = list(pool.map(lambda g: run_gate(g, args.timeout), gates))

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

    missing = [] if args.filter else sorted(g for g in allow if g not in set(gates))

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
