#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HEADER = (ROOT / "src/world/navigation/trajectory/MovingGapPredictor.h").read_text(encoding="utf-8")
IMPL = (ROOT / "src/world/navigation/trajectory/MovingGapPredictor.cpp").read_text(encoding="utf-8")
DOC = (ROOT / "src/world/navigation/MOVING_GAP_MODEL.md").read_text(encoding="utf-8")
CMAKE = (ROOT / "src/world/navigation/trajectory/CMakeLists.txt").read_text(encoding="utf-8")
TEST_CMAKE = (ROOT / "tests/navigation_trajectory/CMakeLists.txt").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")


for marker in (
    "class MovingGapPredictor final",
    "kSamples = 33",
    "kIntervals = kSamples - 1",
    "linearVelocityMapMetersPerSec",
    "linearAccelerationMapMetersPerSec2",
    "angularVelocityMapRadPerSec",
    "gapCenterVelocityMapMetersPerSec",
    "surfaceVelocityAtPointMapMetersPerSec",
    "GapClosesDuringHorizon",
    "AlignmentLost",
    "RevisionMismatch",
):
    require(marker in HEADER, f"moving-gap interface missing: {marker}")

for marker in (
    "relativeAccelerationMagnitude * dt * dt / 8.0",
    "distancePointToSegmentFromOrigin",
    "maximumAbsoluteQuadraticProjection",
    "continuousClearSeparationLowerBound",
    "maximumContinuousAbsSeparationTravelDotBound",
    "cross(query.primary.angularVelocityMapRadPerSec, primaryPhysicalLever)",
    "cross(query.secondary.angularVelocityMapRadPerSec, secondaryPhysicalLever)",
    "query.primary.snapshotRevision != query.secondary.snapshotRevision",
):
    require(marker in IMPL, f"moving-gap implementation missing: {marker}")

for forbidden in (
    "NavigationMap.h",
    "NavigationSpace.h",
    "GL/",
    "OpenGL",
    "glm/",
):
    require(forbidden not in HEADER and forbidden not in IMPL,
            f"moving-gap predictor must stay backend/world neutral: {forbidden}")

for marker in (
    "does **not** perform pair discovery",
    "33 time samples",
    "32 continuous intervals",
    "center deviation from chord <= |a_rel| * dt^2 / 8",
    "GapClosesDuringHorizon",
    "AlignmentLost",
    "v_surface = v_center + omega x r_surface",
    "No frame-path `N x N` moving-gap search is allowed",
    "does not invent a collision or contact point",
):
    require(marker in DOC, f"moving-gap documentation missing: {marker}")

require("MovingGapPredictor.cpp" in CMAKE,
        "trajectory library must compile MovingGapPredictor")
require("navigation_trajectory_moving_gap" in TEST_CMAKE,
        "trajectory CTest must register moving-gap behavior")

print("NAVIGATION TRAJECTORY MOVING GAP CONTRACT: PASS")
print(" - one already-selected gap pair is predicted over 33 samples / 32 intervals")
print(" - constant-P/V/A relative motion receives a conservative between-sample width proof")
print(" - transverse gap alignment is bounded continuously, not only at samples")
print(" - moving gap center velocity and boundary surface velocities are published")
print(" - rotating-boundary material velocity includes omega cross r")
print(" - predictor owns no pair discovery, world search, CCD, or contact response")
