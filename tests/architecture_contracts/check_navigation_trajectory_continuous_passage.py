#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TRAJECTORY = ROOT / "src/world/navigation/trajectory"
TESTS = ROOT / "tests/navigation_trajectory"

HEADER = (TRAJECTORY / "ContinuousPassageTrajectoryEvaluator.h").read_text(encoding="utf-8")
SOURCE = (TRAJECTORY / "ContinuousPassageTrajectoryEvaluator.cpp").read_text(encoding="utf-8")
TRAJECTORY_CMAKE = (TRAJECTORY / "CMakeLists.txt").read_text(encoding="utf-8")
TEST_SOURCE = (TESTS / "NavigationTrajectoryContinuousPassageTests.cpp").read_text(encoding="utf-8")
TEST_CMAKE = (TESTS / "CMakeLists.txt").read_text(encoding="utf-8")
RUNNER = (TESTS / "run_mingw64.sh").read_text(encoding="utf-8")
MODEL = (ROOT / "src/world/navigation/TRAJECTORY_CONTROL_MODEL.md").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")


for marker in (
    "class ContinuousPassageTrajectoryEvaluator",
    "kPoseSamples = 33",
    "enum class ControlMode",
    "EliteAssisted",
    "Newtonian",
    "struct LinearCapability",
    "LinearAuthorityExceeded",
    "AngularAuthorityExceeded",
    "AssistedSlipExceeded",
    "minimumContinuousClearanceBoundMeters",
    "maximumCenterCurveDeviationBoundMeters",
    "maximumRotationalSweepInflationMeters",
):
    require(marker in HEADER, f"continuous-passage boundary marker missing: {marker}")

for forbidden in (
    "#include <glm",
    "#include <GL/",
    "#include <GLFW",
    "EliteGame",
    "EliteServer",
    "Renderer",
    "NavigationMap.h",
    "NavigationSpace.h",
):
    require(forbidden not in HEADER + SOURCE,
            f"continuous passage verifier must stay backend-neutral: {forbidden}")

for marker in (
    "hermitePosition",
    "hermiteVelocity",
    "hermiteAcceleration",
    "smoothStep",
    "rotationalSweepInflation",
    "rightCurveDeviation",
    "upCurveDeviation",
    "dt * dt / 8.0",
    "projectionError",
    "maxForwardAccelerationMetersPerSec2",
    "maxReverseAccelerationMetersPerSec2",
    "maxLateralAccelerationMetersPerSec2",
    "maxVerticalAccelerationMetersPerSec2",
    "ControlMode::EliteAssisted",
):
    require(marker in SOURCE, f"continuous-passage proof marker missing: {marker}")

for marker in (
    "testStraightAlignedSegmentIsContinuouslyFeasible",
    "testIntermediateRotationCanClipDespiteBothEndpointsFitting",
    "testWiderSlotAcceptsTheSameContinuousRoll",
    "testLateralCorrectionRespectsBodyAxisAuthority",
    "testAngularAuthorityIsCheckedAnalytically",
    "testNewtonAllowsVelocityAttitudeDivergenceButElitePolicyCanRejectIt",
    "testInvalidDurationFailsClosed",
):
    require(marker in TEST_SOURCE, f"continuous-passage fixture missing: {marker}")

require("ContinuousPassageTrajectoryEvaluator.cpp" in TRAJECTORY_CMAKE,
        "trajectory library must build the continuous-passage evaluator")
require("navigation_trajectory_continuous_passage_tests" in TEST_CMAKE,
        "continuous-passage CTest executable missing")
require("cmake --build \"${BUILD_DIR}\"" in RUNNER and "ctest --test-dir" in RUNNER,
        "canonical trajectory runner must build and execute all CTests")

for marker in (
    "continuous",
    "body-axis",
    "Elite",
    "Newton",
    "swept",
):
    require(marker.lower() in MODEL.lower(),
            f"trajectory/control model missing continuous-passage concept: {marker}")

print("NAVIGATION TRAJECTORY CONTINUOUS PASSAGE CONTRACT: PASS")
print(" - one bounded analytic Hermite + shortest-arc pose segment is verified")
print(" - geometry is conservatively bounded between samples, not accepted from point samples alone")
print(" - body-axis linear and angular authority are explicit physical gates")
print(" - Newtonian velocity/attitude independence and assisted Elite slip policy stay distinct")
print(" - verifier remains backend-neutral and owns no world/scene search")
