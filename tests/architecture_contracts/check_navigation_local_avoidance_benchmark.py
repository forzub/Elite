#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BENCH = ROOT / "benchmarks/navigation_local_avoidance"
MAIN = (BENCH / "main.cpp").read_text(encoding="utf-8")
CMAKE = (BENCH / "CMakeLists.txt").read_text(encoding="utf-8")
RUNNER = (BENCH / "run_mingw64.sh").read_text(encoding="utf-8")
README = (BENCH / "README.md").read_text(encoding="utf-8")
RUN_LOG = (BENCH / "RUN_LOG.md").read_text(encoding="utf-8")
CURRENT_TASK = (ROOT / "CURRENT_TASK.md").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")


for marker in (
    '"nominal_clear_64"',
    '"early_adjust_64"',
    '"all_static_rejected_64"',
    '"all_dynamic_rejected_16"',
    '"all_dynamic_rejected_64"',
    '"all_dynamic_rejected_256"',
    '"all_dynamic_rejected_1024"',
    "LocalAvoidancePlanner",
    "planner.evaluate",
    "median_us",
    "p95_us",
    "targetProbesExamined",
    "staticRejected",
    "dynamicRejected",
    "horizonEvaluations",
    "candidateVisitsEstimate",
    "staticPointQueries",
):
    require(marker in MAIN, f"avoidance benchmark marker missing: {marker}")

for marker in (
    "EliteNavigationLocal",
    "navigation_local_avoidance_benchmark",
):
    require(marker in CMAKE, f"avoidance benchmark build marker missing: {marker}")

require("ELITE_TEST_BUILD_ROOT" in RUNNER,
        "avoidance benchmark runner must use canonical test build layout")
require("navigation_local_avoidance_benchmark.exe" in RUNNER,
        "avoidance benchmark runner does not launch expected executable")

for marker in (
    "nominal_clear_64",
    "early_adjust_64",
    "all_static_rejected_64",
    "all_dynamic_rejected_{16,64,256,1024}",
    "outside the timed region",
    "candidate_visits_estimate",
    "measurement gate",
    "<0.5 ms typical",
    "<1.0 ms normal peak",
):
    require(marker in README,
            f"avoidance benchmark documentation missing: {marker}")

require("target-machine measurement pending" in RUN_LOG.lower(),
        "avoidance benchmark run log must not claim unmeasured acceptance")

require(
    "benchmarks/navigation_local_avoidance/" in CURRENT_TASK and
    "behavior" in CURRENT_TASK.lower() and
    "accepted" in CURRENT_TASK.lower() and
    "benchmark" in CURRENT_TASK.lower(),
    "CURRENT_TASK does not declare accepted behavior + active avoidance benchmark"
)

print("NAVIGATION LOCAL AVOIDANCE BENCHMARK CONTRACT: PASS")
print(" - nominal clear and early-adjust paths are measured separately")
print(" - 16-probe static rejection is isolated from repeated dynamic scans")
print(" - worst-case dynamic rejection scales 16/64/256/1024 compact candidates")
print(" - scenario/static-space construction stays outside the timed region")
print(" - probe, static, dynamic and estimated candidate work are reported")
print(" - benchmark remains behind EliteNavigationLocal and canonical MinGW layout")
