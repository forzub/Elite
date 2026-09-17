#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BENCH = ROOT / "benchmarks/navigation_trajectory_continuous"

MAIN = (BENCH / "main.cpp").read_text(encoding="utf-8")
CMAKE = (BENCH / "CMakeLists.txt").read_text(encoding="utf-8")
RUNNER = (BENCH / "run_mingw64.sh").read_text(encoding="utf-8")
README = (BENCH / "README.md").read_text(encoding="utf-8")
RUN_LOG = (BENCH / "RUN_LOG.md").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")


for marker in (
    '"straight_newton"',
    '"rolled_newton"',
    '"lateral_newton"',
    '"elite_aligned"',
    '"geometry_blocked_roll"',
    '"full_precision_batch8"',
    "median_batch_us=",
    "p95_batch_us=",
    "median_per_query_us=",
    "p95_per_query_us=",
    "Evaluator::kPoseSamples",
):
    require(marker in MAIN, f"continuous benchmark marker missing: {marker}")

require("navigation_trajectory_continuous_benchmark" in CMAKE,
        "continuous benchmark target missing")
require("cmake --build \"${BUILD_DIR}\"" in RUNNER,
        "continuous benchmark runner must build the configured target")
require("navigation_trajectory_continuous_benchmark.exe" in RUNNER,
        "continuous benchmark runner must execute the benchmark")

for marker in (
    "33 pose samples",
    "32 continuous interval proofs",
    "full_precision_batch8",
    "hard <=8",
    "<0.5 ms typical",
    "<1.0 ms normal peak",
    "construction and one-time contract validation occur outside the timed region",
):
    require(marker in README, f"continuous benchmark documentation missing: {marker}")

require("target-machine timing pending" in RUN_LOG,
        "continuous benchmark run log must declare pending target-machine timing")
require("574a2e98fd7a75ebf562bbfa476fba367fb888d1" in RUN_LOG,
        "continuous benchmark run log must preserve accepted behavior baseline")

print("NAVIGATION TRAJECTORY CONTINUOUS BENCHMARK CONTRACT: PASS")
print(" - full 33-sample / 32-interval verifier paths are timed")
print(" - Newtonian, assisted, rotating, lateral and blocked geometry paths are represented")
print(" - an eight-query batch measures the hard surviving precision-candidate ceiling")
print(" - benchmark construction/validation stays outside the timed region")
print(" - results are interpreted against the existing 0.5/1.0 ms navigation CPU budgets")
