#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MAIN = (ROOT / "benchmarks/navigation_space_costed/main.cpp").read_text(encoding="utf-8")
README = (ROOT / "benchmarks/navigation_space_costed/README.md").read_text(encoding="utf-8")
CMAKE = (ROOT / "benchmarks/navigation_space_costed/CMakeLists.txt").read_text(encoding="utf-8")
RUNNER = (ROOT / "benchmarks/navigation_space_costed/run_mingw64.sh").read_text(encoding="utf-8")
CURRENT_TASK = (ROOT / "CURRENT_TASK.md").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")


for scenario in (
    '"open_1k"',
    '"open_5k"',
    '"open_10k"',
    '"hub_1k"',
    '"hub_5k"',
    '"hub_10k"',
):
    require(scenario in MAIN, f"costed benchmark scenario missing: {scenario}")

for marker in (
    "queryCostedCorridor",
    "distanceOnly",
    "clearanceAware",
    "distanceMedianMs",
    "distanceP95Ms",
    "clearanceMedianMs",
    "clearanceP95Ms",
    "distanceRegionsVisited",
    "distancePortalsExamined",
    "clearanceRegionsVisited",
    "clearancePortalsExamined",
    "distancePathRegions",
    "clearancePathRegions",
    "distanceCost",
    "clearanceCost",
    "narrowPortal",
):
    require(marker in MAIN, f"costed benchmark marker missing: {marker}")

require("navigation_space_costed_benchmark" in CMAKE,
        "costed benchmark target missing from CMake")
require("EliteNavigationSpace" in CMAKE,
        "costed benchmark does not link the isolated NavigationSpace library")
require("ELITE_TEST_BUILD_ROOT" in RUNNER,
        "costed benchmark runner does not use canonical build layout")
require("navigation_space_costed_benchmark.csv" in MAIN,
        "costed benchmark CSV output is not pinned")

for marker in (
    "distance_only",
    "clearance_aware",
    "regions visited",
    "portals examined",
    "region-path length",
    "meter-equivalent cost",
):
    require(marker in README.lower(),
            f"costed benchmark documentation missing: {marker}")

require("NAV-V2-SPACE-1" in CURRENT_TASK,
        "CURRENT_TASK is not on NAV-V2-SPACE-1")
require("costed" in CURRENT_TASK.lower(),
        "CURRENT_TASK does not mention the active costed-corridor gate")

print("NAVIGATION SPACE COSTED BENCHMARK CONTRACT: PASS")
print(" - open/hub 1k/5k/10k topology scales are pinned")
print(" - distance-only and clearance-aware policies share one published snapshot")
print(" - median/p95 plus graph-work diagnostics are recorded")
print(" - reduced-clearance portals exercise real policy-dependent alternatives")
print(" - benchmark remains behind the isolated EliteNavigationSpace boundary")
print(" - canonical MinGW build layout is used")
