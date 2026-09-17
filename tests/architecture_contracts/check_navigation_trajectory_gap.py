#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TRAJECTORY = ROOT / "src/world/navigation/trajectory"
TESTS = ROOT / "tests/navigation_trajectory"

HEADER = (TRAJECTORY / "BoundedGapCandidateBuilder.h").read_text(encoding="utf-8")
SOURCE = (TRAJECTORY / "BoundedGapCandidateBuilder.cpp").read_text(encoding="utf-8")
TRAJECTORY_CMAKE = (TRAJECTORY / "CMakeLists.txt").read_text(encoding="utf-8")
TEST_SOURCE = (TESTS / "NavigationTrajectoryGapTests.cpp").read_text(encoding="utf-8")
TEST_CMAKE = (TESTS / "CMakeLists.txt").read_text(encoding="utf-8")
RUNNER = (TESTS / "run_mingw64.sh").read_text(encoding="utf-8")
MODEL = (ROOT / "src/world/navigation/ORIENTED_PASSAGE_MODEL.md").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")


for marker in (
    "class BoundedGapCandidateBuilder",
    "kHardCandidateLimit = 8",
    "struct ObstacleWitness",
    "snapshotRevision",
    "struct Policy",
    "struct Candidate",
    "struct Diagnostics",
    "neighborsExamined",
    "rejectedRevision",
    "rejectedAlignment",
    "rejectedCenterlineOffset",
):
    require(marker in HEADER, f"bounded-gap boundary marker missing: {marker}")

for forbidden in (
    "#include <glm",
    "#include <GL/",
    "#include <GLFW",
    "EliteGame",
    "Renderer",
    "NavigationMap.h",
    "NavigationSpace.h",
    "NavigationObstacle.h",
):
    require(forbidden not in HEADER + SOURCE,
            f"bounded-gap builder must stay backend-neutral: {forbidden}")

require("for (const ObstacleWitness& neighbor : query.neighbors)" in SOURCE,
        "bounded-gap builder must scan one primary conflict against reduced neighbors")
require("for (const ObstacleWitness&" not in SOURCE.replace(
    "for (const ObstacleWitness& neighbor : query.neighbors)", "", 1
), "bounded-gap builder must not add a second obstacle-pair loop")
require("std::min(\n        query.policy.maxCandidates,\n        kHardCandidateLimit" in SOURCE,
        "hard candidate limit must be enforced")
require("insertBounded" in SOURCE and "std::lower_bound" in SOURCE,
        "bounded deterministic top-K insertion missing")
require("neighbor.snapshotRevision != query.primary.snapshotRevision" in SOURCE,
        "mixed-snapshot obstacle pairs must fail closed")
require("separationTravelDot" in SOURCE,
        "gap builder must reject front/back pairs masquerading as slots")

for marker in (
    "testPrimaryConflictAndNeighborBuildOneGap",
    "testBuilderNeverEmitsNeighborNeighborPair",
    "testHardCandidateLimitAndDeterministicOrdering",
    "testMixedSnapshotRevisionFailsClosed",
    "testLongitudinalPairIsNotAVisualSlot",
    "testOverlappingConservativeBoundsDoNotInventGap",
    "testBuiltGapFeedsOrientedPassagePrecision",
    "testIrrelevantGapOutsideCorridorWindowIsRejected",
):
    require(marker in TEST_SOURCE, f"bounded-gap fixture missing: {marker}")

require("BoundedGapCandidateBuilder.cpp" in TRAJECTORY_CMAKE,
        "trajectory library must compile bounded-gap builder")
require("navigation_trajectory_gap_tests" in TEST_CMAKE,
        "bounded-gap test executable missing")
require("cmake --build \"${BUILD_DIR}\"" in RUNNER,
        "trajectory runner must build all registered executables")
require("ctest --test-dir" in RUNNER,
        "trajectory runner must execute CTest")

for marker in (
    "no unbounded all-pairs scan",
    "small candidate budget (initial target <= 4-8)",
    "primary conflict",
    "ObstacleGap",
    "full 6DoF trajectory / swept-body feasibility",
):
    require(marker in MODEL, f"oriented-gap architecture marker missing: {marker}")

print("NAVIGATION TRAJECTORY BOUNDED GAP CONTRACT: PASS")
print(" - one primary conflict is compared with already-reduced local neighbors")
print(" - no unbounded all-pairs obstacle discovery exists in the trajectory builder")
print(" - hard candidate budget is capped at 8 with deterministic top-K ordering")
print(" - mixed snapshot revisions and overlapping conservative bounds fail closed")
print(" - transverse gap geometry is separated from later 6DoF maneuver feasibility")
print(" - emitted ObstacleGap candidates feed the oriented precision evaluator")
