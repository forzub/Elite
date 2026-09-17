#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HEADER = (ROOT / "src/world/navigation/trajectory/DockingTerminalEvaluator.h").read_text(encoding="utf-8")
IMPL = (ROOT / "src/world/navigation/trajectory/DockingTerminalEvaluator.cpp").read_text(encoding="utf-8")
DOC = (ROOT / "src/world/navigation/DOCKING_TERMINAL_MODEL.md").read_text(encoding="utf-8")
CMAKE = (ROOT / "src/world/navigation/trajectory/CMakeLists.txt").read_text(encoding="utf-8")
TEST_CMAKE = (ROOT / "tests/navigation_trajectory/CMakeLists.txt").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")


for marker in (
    "class DockingTerminalEvaluator final",
    "SurfaceSemantic",
    "requiredShipSurface",
    "requiredDockSurface",
    "maxPositionErrorMeters",
    "maxRelativeLinearSpeedMetersPerSec",
    "maxNormalAlignmentErrorRad",
    "maxRollAlignmentErrorRad",
    "maxRelativeAngularSpeedRadPerSec",
    "PortSemanticMismatch",
    "RollAlignmentMismatch",
    "relativeAngularSpeedRadPerSec",
):
    require(marker in HEADER, f"docking terminal interface missing: {marker}")

for marker in (
    "v_port = v_origin + omega x r",
    "cross(dock.angularVelocityMapRadPerSec, offset)",
    "cross(ship.angularVelocityMapRadPerSec, offset)",
    "desiredShipNormal",
    "result.rollAlignmentErrorRad",
    "result.relativeAngularSpeedRadPerSec",
    "Status::Capturable",
):
    require(marker in IMPL, f"docking terminal implementation missing: {marker}")

for forbidden in (
    "NavigationMap.h",
    "NavigationSpace.h",
    "GL/",
    "OpenGL",
    "glm/",
    "TacticalCollisionMonitor",
):
    require(forbidden not in HEADER and forbidden not in IMPL,
            f"docking terminal evaluator must stay backend/world neutral: {forbidden}")

for marker in (
    "terminal docking 6DoF",
    "ship surface = Bottom",
    "dock surface = Bottom",
    "ship mating normal = -dock mating normal",
    "ship referenceUp   =  dock referenceUp",
    "v_port = v_origin + omega x r_port",
    "180-degree rolled ship",
    "relative angular velocity",
    "not collision physics",
):
    require(marker in DOC, f"docking terminal documentation missing: {marker}")

require("DockingTerminalEvaluator.cpp" in CMAKE,
        "trajectory library must compile docking terminal evaluator")
require("navigation_trajectory_docking_terminal" in TEST_CMAKE,
        "trajectory CTest must register docking terminal behavior")

print("NAVIGATION TRAJECTORY DOCKING TERMINAL CONTRACT: PASS")
print(" - docking capture is an explicit terminal 6DoF relative-frame problem")
print(" - ship and dock ports carry explicit surface semantics and local mating frames")
print(" - moving/rotating dock prediction includes v_port = v_origin + omega cross r")
print(" - bottom-to-bottom normals must oppose while roll/reference-up must align")
print(" - 180-degree rolled arrival is rejected even with correct face normals")
print(" - relative position, linear velocity, attitude and angular rate are capture gates")
print(" - collision response and authoritative latch remain outside the evaluator")
