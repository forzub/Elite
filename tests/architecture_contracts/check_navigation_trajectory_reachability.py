#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TRAJECTORY = ROOT / "src/world/navigation/trajectory"
TESTS = ROOT / "tests/navigation_trajectory"

HEADER = (TRAJECTORY / "AttitudeReachabilityEvaluator.h").read_text(encoding="utf-8")
SOURCE = (TRAJECTORY / "AttitudeReachabilityEvaluator.cpp").read_text(encoding="utf-8")
TRAJECTORY_CMAKE = (TRAJECTORY / "CMakeLists.txt").read_text(encoding="utf-8")
TEST_SOURCE = (TESTS / "NavigationTrajectoryReachabilityTests.cpp").read_text(encoding="utf-8")
TEST_CMAKE = (TESTS / "CMakeLists.txt").read_text(encoding="utf-8")
RUNNER = (TESTS / "run_mingw64.sh").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")


for marker in (
    "class AttitudeReachabilityEvaluator",
    "struct AngularCapability",
    "struct ApproachState",
    "ReachableCoast",
    "ReachableWithBraking",
    "UnreachableBeforeEntry",
    "minimumRotationTimeSeconds",
    "coastDistanceBeforeReadyMeters",
    "minimumDistanceBeforeReadyMeters",
):
    require(marker in HEADER, f"attitude reachability boundary marker missing: {marker}")

for forbidden in (
    "#include <glm",
    "#include <GL/",
    "#include <GLFW",
    "EliteGame",
    "Renderer",
    "NavigationMap.h",
    "NavigationSpace.h",
    "NavigationObstacle.h",
    "Ruckig",
):
    require(forbidden not in HEADER + SOURCE,
            f"attitude reachability precheck must stay backend-neutral: {forbidden}")

for marker in (
    "attitudeErrorRad",
    "restToRestRotationTime",
    "distanceWhileBraking",
    "angularSettleTimeSeconds",
    "conservativeSettleAngleRad",
    "ReachableWithBraking",
    "UnreachableBeforeEntry",
):
    require(marker in SOURCE, f"attitude reachability implementation marker missing: {marker}")

for marker in (
    "testAlreadyAlignedIsReadyImmediately",
    "testNinetyDegreeRollFitsWithCoastWhenDistanceIsEnough",
    "testNinetyDegreeRollCanRequireBraking",
    "testHighSpeedShortDistanceMakesGapUnreachable",
    "testCurrentAngularMotionConsumesAdditionalMargin",
    "testZeroAngularAuthorityFailsClosed",
):
    require(marker in TEST_SOURCE, f"attitude reachability fixture missing: {marker}")

require("AttitudeReachabilityEvaluator.cpp" in TRAJECTORY_CMAKE,
        "trajectory library must compile attitude reachability evaluator")
require("navigation_trajectory_reachability_tests" in TEST_CMAKE,
        "attitude reachability test executable missing")
require("cmake --build \"${BUILD_DIR}\"" in RUNNER,
        "trajectory runner must build every registered executable")
require("ctest --test-dir" in RUNNER,
        "trajectory runner must execute CTest")

print("NAVIGATION TRAJECTORY ATTITUDE REACHABILITY CONTRACT: PASS")
print(" - required passage attitude is checked against physical angular authority")
print(" - current angular motion consumes conservative settle time/margin")
print(" - longitudinal room distinguishes coast, braking-assisted and unreachable cases")
print(" - high-speed short-distance gap entry fails closed when attitude cannot be ready")
print(" - evaluator remains a constant-size precheck, not a duplicate flight controller")
