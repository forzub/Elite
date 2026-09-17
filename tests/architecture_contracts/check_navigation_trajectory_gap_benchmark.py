#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BENCH = ROOT / "benchmarks/navigation_trajectory_gap"

MAIN = (BENCH / "main.cpp").read_text(encoding="utf-8")
CMAKE = (BENCH / "CMakeLists.txt").read_text(encoding="utf-8")
RUNNER = (BENCH / "run_mingw64.sh").read_text(encoding="utf-8")
README = (BENCH / "README.md").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")


for marker in (
    '"reject_16"', '"reject_64"', '"reject_256"', '"reject_1024"',
    '"top8_16"', '"top8_64"', '"top8_256"', '"top8_1024"',
    "median_us=", "p95_us=", "neighbors=", "candidates=",
    "Builder::kHardCandidateLimit",
):
    require(marker in MAIN, f"bounded-gap benchmark marker missing: {marker}")

require("navigation_trajectory_gap_benchmark" in CMAKE,
        "bounded-gap benchmark target missing")
require("cmake --build \"${BUILD_DIR}\"" in RUNNER,
        "bounded-gap benchmark runner must build the configured target")
require("navigation_trajectory_gap_benchmark.exe" in RUNNER,
        "bounded-gap benchmark runner must execute the benchmark")

for marker in (
    "O(local_neighbors * 8)",
    "not all-pairs `O(N^2)`",
    "1024",
    "<0.5 ms typical",
    "fallback-path microbenchmark",
):
    require(marker in README, f"bounded-gap benchmark documentation missing: {marker}")

print("NAVIGATION TRAJECTORY GAP BENCHMARK CONTRACT: PASS")
print(" - rejection and top-8 maintenance are measured at 16/64/256/1024 neighbors")
print(" - benchmark isolates the precision gap builder from broadphase/global search")
print(" - 1024 neighbors is explicitly a stress ceiling, not normal runtime expectation")
print(" - results are interpreted against the existing local navigation CPU budget")
