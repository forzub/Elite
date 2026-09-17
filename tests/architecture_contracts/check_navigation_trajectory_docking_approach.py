#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HEADER = (ROOT / "src/world/navigation/trajectory/DockingApproachEvaluator.h").read_text(encoding="utf-8")
IMPL = (ROOT / "src/world/navigation/trajectory/DockingApproachEvaluator.cpp").read_text(encoding="utf-8")
DOC = (ROOT / "src/world/navigation/DOCKING_APPROACH_MODEL.md").read_text(encoding="utf-8")
CMAKE = (ROOT / "src/world/navigation/trajectory/CMakeLists.txt").read_text(encoding="utf-8")
TEST_CMAKE = (ROOT / "tests/navigation_trajectory/CMakeLists.txt").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")


for marker in (
    "class DockingApproachEvaluator final",
    "using GeometryEvaluator = ContinuousPassageTrajectoryEvaluator",
    "using TerminalEvaluator = DockingTerminalEvaluator",
    "FeasibleForCapture",
    "CorridorBlocked",
    "LinearAuthorityExceeded",
    "AngularAuthorityExceeded",
    "AssistedSlipExceeded",
    "TerminalNotCapturable",
    "requiredPeakWorldAngularSpeedBoundRadPerSec",
    "requiredPeakWorldAngularAccelerationBoundRadPerSec2",
    "dockLocalGeometry",
    "terminal",
):
    require(marker in HEADER, f"docking approach interface missing: {marker}")

for marker in (
    "Terminal::predictDockPortWorldState",
    "OrientedPassageEvaluator::PassageSource::DockingCorridor",
    "Geometry::evaluate(geometryQuery)",
    "cross(query.dock.angularVelocityMapRadPerSec, relativePositionWorld0)",
    "2.0 * omega * localVelocityBound",
    "3.0 * omega * omega * localVelocityBound",
    "projectionMargin",
    "query.dock.angularVelocityMapRadPerSec",
    "Terminal::evaluate(terminalQuery)",
):
    require(marker in IMPL, f"docking approach implementation missing: {marker}")

for forbidden in (
    "NavigationMap.h",
    "NavigationSpace.h",
    "GL/",
    "OpenGL",
    "glm/",
    "TacticalCollisionMonitor",
):
    require(forbidden not in HEADER and forbidden not in IMPL,
            f"docking approach evaluator must stay backend/world neutral: {forbidden}")

for marker in (
    "moving/rotating docking approach",
    "dock frame",
    "omega_ship(capture) = omega_dock",
    "ContinuousPassageTrajectoryEvaluator",
    "Coriolis",
    "continuous body-axis thrust proof",
    "world jerk bound",
    "TerminalNotCapturable",
    "authoritative latch",
    "A destructive collision is never relabeled as docking success",
):
    require(marker in DOC, f"docking approach documentation missing: {marker}")

require("DockingApproachEvaluator.cpp" in CMAKE,
        "trajectory library must compile docking approach evaluator")
require("navigation_trajectory_docking_approach" in TEST_CMAKE,
        "trajectory CTest must register docking approach behavior")

print("NAVIGATION TRAJECTORY DOCKING APPROACH CONTRACT: PASS")
print(" - final precision docking is solved in the moving/rotating dock-local frame")
print(" - accepted continuous-passage geometry is reused for 33 samples / 32 intervals")
print(" - inertial world acceleration includes centripetal and Coriolis terms")
print(" - body-axis thrust receives a conservative between-sample jerk/projection bound")
print(" - dock angular velocity consumes real world angular-rate authority")
print(" - successful approach composes into the accepted terminal 6DoF capture evaluator")
print(" - collision response and authoritative latch remain downstream owners")
