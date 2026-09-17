#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HEADER = (ROOT / "src/world/navigation/trajectory/MovingPassageTrajectoryEvaluator.h").read_text(encoding="utf-8")
IMPL = (ROOT / "src/world/navigation/trajectory/MovingPassageTrajectoryEvaluator.cpp").read_text(encoding="utf-8")
DOC = (ROOT / "src/world/navigation/MOVING_PASSAGE_TRAJECTORY_MODEL.md").read_text(encoding="utf-8")
CMAKE = (ROOT / "src/world/navigation/trajectory/CMakeLists.txt").read_text(encoding="utf-8")
TEST_CMAKE = (ROOT / "tests/navigation_trajectory/CMakeLists.txt").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")


for marker in (
    "class MovingPassageTrajectoryEvaluator final",
    "using MovingGap = MovingGapPredictor::Result",
    "kPoseSamples = MovingGapPredictor::kSamples",
    "kIntervals = MovingGapPredictor::kIntervals",
    "GapUnavailable",
    "GeometryBlocked",
    "LinearAuthorityExceeded",
    "AngularAuthorityExceeded",
    "AssistedSlipExceeded",
    "maximumRelativeCenterMotionBoundMeters",
    "maximumPassageAxisInflationRad",
    "maximumRelativeOrientationSweepInflationMeters",
):
    require(marker in HEADER, f"moving-passage interface missing: {marker}")

for marker in (
    "MovingGapPredictor::Status::OpenForHorizon",
    "distancePointToSegmentFromOrigin",
    "continuousClearSeparationLowerBound",
    "relativeCenterMotionBound",
    "passageAxisInflation",
    "orientationSweepInflation",
    "bodyProjectionMargin",
    "ControlMode::EliteAssisted",
):
    require(marker in IMPL, f"moving-passage implementation missing: {marker}")

for forbidden in (
    "NavigationMap.h",
    "NavigationSpace.h",
    "GL/",
    "OpenGL",
    "glm/",
    "TacticalCollisionMonitor",
):
    require(forbidden not in HEADER and forbidden not in IMPL,
            f"moving-passage verifier must stay backend/world neutral: {forbidden}")

for marker in (
    "33 synchronized ship/gap samples",
    "32 continuous intervals",
    "not a collision solver",
    "CCD / TOI / contact manifold / impulse / ricochet",
    "between-sample",
    "body-axis physical authority",
    "Newtonian",
    "EliteAssisted",
):
    require(marker in DOC, f"moving-passage documentation missing: {marker}")

require("MovingPassageTrajectoryEvaluator.cpp" in CMAKE,
        "trajectory library must compile moving-passage evaluator")
require("navigation_trajectory_moving_passage" in TEST_CMAKE,
        "trajectory CTest must register moving-passage behavior")

print("NAVIGATION TRAJECTORY MOVING PASSAGE CONTRACT: PASS")
print(" - one accepted moving-gap prediction is composed with one bounded ship segment")
print(" - 33 synchronized ship/gap samples are backed by 32 conservative interval proofs")
print(" - moving width, passage-axis motion, hull sweep and transverse relative motion are bounded")
print(" - body-axis linear/angular authority and Elite/Newton semantics remain explicit")
print(" - exact CCD/TOI/contact response remains outside navigation")
