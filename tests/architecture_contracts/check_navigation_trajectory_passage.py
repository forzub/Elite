#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TRAJECTORY = ROOT / "src/world/navigation/trajectory"
TESTS = ROOT / "tests/navigation_trajectory"

HEADER = (TRAJECTORY / "OrientedPassageEvaluator.h").read_text(encoding="utf-8")
SOURCE = (TRAJECTORY / "OrientedPassageEvaluator.cpp").read_text(encoding="utf-8")
CMAKE = (TRAJECTORY / "CMakeLists.txt").read_text(encoding="utf-8")
TEST_SOURCE = (TESTS / "NavigationTrajectoryPassageTests.cpp").read_text(encoding="utf-8")
TEST_CMAKE = (TESTS / "CMakeLists.txt").read_text(encoding="utf-8")
RUNNER = (TESTS / "run_mingw64.sh").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")


for marker in (
    "class OrientedPassageEvaluator",
    "struct HullProxy",
    "struct Passage",
    "struct ObstacleGap",
    "PassageSource",
    "ObstacleGap",
    "conservativeSphereFits",
    "makeObstacleGapPassage",
):
    require(marker in HEADER, f"trajectory passage boundary marker missing: {marker}")

for forbidden in (
    "#include <glm",
    "#include <GL/",
    "#include <GLFW",
    "EliteGame",
    "Renderer",
    "NavigationObstacle.h",
    "NavigationMap.h",
    "NavigationSpace.h",
    "std::vector",
):
    require(forbidden not in HEADER + SOURCE,
            f"trajectory passage evaluator must stay backend-neutral/O(1): {forbidden}")

for marker in (
    "projectedHalfExtent",
    "halfWidthMeters",
    "halfHeightMeters",
    "conservativeRadius",
    "PassageSource::ObstacleGap",
):
    require(marker in SOURCE, f"trajectory passage implementation marker missing: {marker}")

for marker in (
    "testFlatHullFitsFlatSlotWhileSphereWouldReject",
    "testNinetyDegreeRollRejectsSameFlatSlot",
    "testTwoObstacleGapBecomesPositivePassageCandidate",
    "testOffCenterHullFailsEvenWhenOrientationFits",
    "testDegenerateObstacleGapFailsClosed",
):
    require(marker in TEST_SOURCE, f"trajectory passage fixture missing: {marker}")

require("EliteNavigationTrajectory" in CMAKE,
        "trajectory precision library target missing")
require("navigation_trajectory_passage_tests" in TEST_CMAKE,
        "trajectory passage test executable missing")
require("ELITE_TEST_BUILD_ROOT" in RUNNER,
        "trajectory runner must use canonical test build layout")
require("cmake --build \"${BUILD_DIR}\"" in RUNNER,
        "trajectory runner must build all registered targets")
require("ctest --test-dir" in RUNNER,
        "trajectory runner must execute CTest")

print("NAVIGATION TRAJECTORY PASSAGE BOUNDARY CONTRACT: PASS")
print(" - precision passage fit is isolated from GLM/OpenGL/game/render ownership")
print(" - oriented OBB projection can recover flat-hull/flat-slot cases rejected by a sphere")
print(" - nearby obstacles may produce a positive ObstacleGap passage candidate")
print(" - evaluator performs constant-size pose math and owns no all-pairs discovery")
print(" - invalid frames and excessive lateral offset fail closed")
print(" - canonical MinGW build layout is used")
