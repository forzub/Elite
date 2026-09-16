#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BENCH = (ROOT / "benchmarks/navigation_space/main.cpp").read_text(encoding="utf-8")
CMAKE = (ROOT / "benchmarks/navigation_space/CMakeLists.txt").read_text(encoding="utf-8")
RUNNER = (ROOT / "benchmarks/navigation_space/run_mingw64.sh").read_text(encoding="utf-8")
README = (ROOT / "benchmarks/navigation_space/README.md").read_text(encoding="utf-8")
CURRENT_STATE = (ROOT / "CURRENT_STATE.md").read_text(encoding="utf-8")
CURRENT_TASK = (ROOT / "CURRENT_TASK.md").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")


for marker in (
    "open_1k",
    "open_5k",
    "open_10k",
    "hub_1k",
    "hub_5k",
    "hub_10k",
    "replaceStaticWorld",
    "queryPoint",
    "queryCorridor",
    "invalidateBounds",
    "applyLocalPatch",
    "pointRegionsExamined",
    "corridorPortalsExamined",
    "invalidatedRegions",
    "navigation_space_cpu_benchmark.csv",
):
    require(marker in BENCH, f"static-space benchmark marker missing: {marker}")

require("EliteNavigationSpace" in CMAKE,
        "benchmark does not link the isolated NavigationSpace library")
require("ELITE_TEST_BUILD_ROOT" in RUNNER,
        "benchmark runner does not use canonical test build layout")
require("navigation_space_benchmark" in RUNNER,
        "benchmark runner does not execute the static-space target")

for marker in (
    "free-space",
    "portals",
    "point",
    "corridor",
    "invalidate",
    "patch",
    "1k / 5k / 10k",
    "diagnostic",
):
    require(marker.lower() in README.lower(),
            f"static-space benchmark documentation missing: {marker}")

require("NAV-V2-SPACE-1" in CURRENT_STATE,
        "CURRENT_STATE is not on NAV-V2-SPACE-1")
require("NAV-V2-SPACE-1" in CURRENT_TASK,
        "CURRENT_TASK is not on NAV-V2-SPACE-1")

print("NAVIGATION SPACE BENCHMARK CONTRACT: PASS")
print(" - deterministic open/hub 1k/5k/10k topology scales are pinned")
print(" - replace/point/corridor/invalidation/local-patch costs are measured separately")
print(" - scan diagnostics expose which internal index is needed first")
print(" - benchmark stays behind the isolated EliteNavigationSpace boundary")
print(" - canonical MinGW build layout is used")
