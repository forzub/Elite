#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TRAJECTORY = (ROOT / "src/world/navigation/TrajectoryGenerator.cpp").read_text(encoding="utf-8")
TUNNEL = (ROOT / "src/world/navigation/GuidanceTunnel.cpp").read_text(encoding="utf-8")
ROUTE_API = (ROOT / "src/game/navigation/RuckigRoutePlanner.h").read_text(encoding="utf-8")


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

print("RUCKIG LIVE NAVIGATION CONTRACT: PASS")
print(" - coarse topology remains outside the Ruckig motion backend")
print(" - route trajectory and rolling reconnect both use Ruckig")
print(" - both products retain canonical swept obstacle validation")
print(" - SmoothPathOptimizer is excluded from both live motion paths")
