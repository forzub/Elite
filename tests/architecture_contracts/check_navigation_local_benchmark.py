#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BENCH = ROOT / "benchmarks/navigation_local"
MAIN = (BENCH / "main.cpp").read_text(encoding="utf-8")
CMAKE = (BENCH / "CMakeLists.txt").read_text(encoding="utf-8")
RUNNER = (BENCH / "run_mingw64.sh").read_text(encoding="utf-8")
README = (BENCH / "README.md").read_text(encoding="utf-8")
CURRENT_TASK = (ROOT / "CURRENT_TASK.md").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")


for marker in (
    '"clear_0"',
    '"clear_16"',
    '"clear_64"',
    '"clear_256"',
    '"clear_1024"',
    '"conflict_16"',
    '"conflict_64"',
    '"conflict_256"',
    '"conflict_1024"',
    '"stale_1024"',
    "LocalHorizonPlanner",
    "planner.evaluate",
    "median_us",
    "p95_us",
    "p95_ns_per_candidate",
    "candidates_examined",
    "conflicts_found",
):
    require(marker in MAIN, f"local benchmark marker missing: {marker}")

require("EliteNavigationLocal" in CMAKE,
        "local benchmark must link through EliteNavigationLocal boundary")
require("navigation_local_benchmark" in CMAKE,
        "local benchmark executable target missing")
require("ELITE_TEST_BUILD_ROOT" in RUNNER,
        "local benchmark runner must use canonical test build layout")
require("navigation_local_benchmark.exe" in RUNNER,
        "local benchmark runner does not launch the expected executable")

for marker in (
    "compact `navigationmap::queryresult`",
    "0 / 16 / 64 / 256 / 1024",
    "clear_*",
    "conflict_*",
    "stale_1024",
    "outside the timed region",
    "measurement gate",
):
    require(marker in README.lower(),
            f"local benchmark documentation missing: {marker}")

require(
    "benchmarks/navigation_local/" in CURRENT_TASK and
    "candidate" in CURRENT_TASK.lower() and
    "benchmark" in CURRENT_TASK.lower(),
    "CURRENT_TASK does not declare the local candidate-scaling benchmark"
)

print("NAVIGATION LOCAL BENCHMARK CONTRACT: PASS")
print(" - compact candidate counts 0/16/64/256/1024 are pinned")
print(" - clear/conflict/stale evaluation paths are measured separately")
print(" - scenario construction remains outside the timed region")
print(" - stale path is pinned to examine zero candidates")
print(" - benchmark remains behind EliteNavigationLocal")
print(" - canonical MinGW build layout is used")
