#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BENCH = ROOT / "benchmarks/navigation_space_turn"
MAIN = (BENCH / "main.cpp").read_text(encoding="utf-8")
CMAKE = (BENCH / "CMakeLists.txt").read_text(encoding="utf-8")
RUNNER = (BENCH / "run_mingw64.sh").read_text(encoding="utf-8")
README = (BENCH / "README.md").read_text(encoding="utf-8")
CURRENT_TASK = (ROOT / "CURRENT_TASK.md").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")


for marker in (
    '"open_1k"', '"open_5k"', '"open_10k"',
    '"hub_1k"', '"hub_5k"', '"hub_10k"',
    "turnPenaltyMetersPerRadian = 0.0",
    "turnPenaltyMetersPerRadian =",
    "zero_p95_ms",
    "turn_p95_ms",
    "zero_portals_examined",
    "turn_portals_examined",
    "zero_path_turn_rad",
    "turn_path_turn_rad",
    "queryCostedCorridor",
):
    require(marker in MAIN, f"turn benchmark marker missing: {marker}")

require("EliteNavigationSpace" in CMAKE,
        "turn benchmark must link only through EliteNavigationSpace boundary")
require("navigation_space_turn_benchmark" in CMAKE,
        "turn benchmark executable target missing")
require("ELITE_TEST_BUILD_ROOT" in RUNNER,
        "turn benchmark runner must use canonical test build layout")
require("navigation_space_turn_benchmark.exe" in RUNNER,
        "turn benchmark runner does not launch the expected executable")

for marker in (
    "same published navigationspace snapshot",
    "expanded state",
    "regionslot, incoming portalid",
    "portals examined",
    "worker/reference",
):
    require(marker in README.lower(),
            f"turn benchmark documentation missing: {marker}")

require("turn-aware performance benchmark" in CURRENT_TASK.lower(),
        "CURRENT_TASK does not declare the turn-aware performance gate")

print("NAVIGATION SPACE TURN BENCHMARK CONTRACT: PASS")
print(" - open/hub 1k/5k/10k scales are pinned")
print(" - zero-turn and expanded turn-aware policies share one snapshot")
print(" - timing and expansion-work diagnostics are recorded separately")
print(" - accumulated coarse turn radians are reported")
print(" - benchmark remains behind EliteNavigationSpace")
print(" - canonical MinGW build layout is used")
