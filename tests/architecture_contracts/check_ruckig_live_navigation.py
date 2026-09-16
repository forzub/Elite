#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TRAJECTORY = (ROOT / "src/world/navigation/TrajectoryGenerator.cpp").read_text(encoding="utf-8")
TUNNEL = (ROOT / "src/world/navigation/GuidanceTunnel.cpp").read_text(encoding="utf-8")
ROUTE_API = (ROOT / "src/game/navigation/RuckigRoutePlanner.h").read_text(encoding="utf-8")
RUCKIG_SOLVER = (ROOT / "src/game/navigation/RuckigTrajectorySolver.cpp").read_text(encoding="utf-8")
SMOOTHER = (ROOT / "src/world/navigation/SmoothPathOptimizer.cpp").read_text(encoding="utf-8")
ROOT_CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
TEST_CMAKE = (ROOT / "tests/navigation_guidance/CMakeLists.txt").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")


require("RuckigRoutePlanner::plan" in TRAJECTORY,
        "runtime trajectory path does not delegate to the canonical Ruckig route planner")
require("RuckigTrajectorySolver::solve" in TRAJECTORY,
        "Ruckig route planner does not use the state-to-state Ruckig solver")
require("RuckigTrajectorySolver::solve" in TUNNEL,
        "rolling guidance reconnect does not use the Ruckig solver")
require("segmentClearOfNavigationObstacles" in TRAJECTORY,
        "route trajectory is missing swept canonical obstacle validation")
require("segmentClearOfNavigationObstacles" in TUNNEL,
        "rolling reconnect is missing swept canonical obstacle validation")
require("SmoothPathOptimizer" not in TRAJECTORY,
        "legacy spline optimizer returned to live route trajectory generation")
require("SmoothPathOptimizer" not in TUNNEL,
        "legacy spline optimizer returned to live rolling guidance reconnect")
require("consume an already collision-free coarse polyline" in ROUTE_API,
        "Ruckig route backend ownership contract is missing")

# Ruckig's scalar axes must not define arbitrary world-space path geometry. The
# state-to-state primitive is solved in a deterministic leg-aligned basis so a
# stopped diagonal leg follows the collision-free coarse chord instead of
# bowing away merely because XYZ have different scalar profiles.
for marker in (
    "struct MotionBasis",
    "makeMotionBasis(",
    "basis.toLocal(relativePosition0World)",
    "basis.toWorld(toVec3(relativePosition))",
):
    require(marker in RUCKIG_SOLVER,
            f"leg-aligned Ruckig solve marker missing: {marker}")

require("SmoothPathOptimizer retired; use the canonical Ruckig navigation backend" in SMOOTHER,
        "legacy smoother implementation is no longer fail-closed in production")
require("ELITE_LEGACY_SMOOTH_PATH_TEST_COMPAT" in SMOOTHER,
        "retired smoother lacks an explicitly test-only compatibility boundary")
require("ELITE_LEGACY_SMOOTH_PATH_TEST_COMPAT" not in ROOT_CMAKE,
        "production build enables the legacy smoother compatibility implementation")
require("ELITE_LEGACY_SMOOTH_PATH_TEST_COMPAT" in TEST_CMAKE,
        "legacy smoother compatibility is not isolated to the old navigation test target")

print("RUCKIG LIVE NAVIGATION CONTRACT: PASS")
print(" - coarse topology remains outside the Ruckig motion backend")
print(" - route trajectory and rolling reconnect both use Ruckig")
print(" - Ruckig state-to-state solves use a leg-aligned basis, not arbitrary world XYZ")
print(" - both products retain canonical swept obstacle validation")
print(" - SmoothPathOptimizer is absent from live paths and fails closed in production")
