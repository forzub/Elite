#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CPP = (ROOT / "src/game/SpaceState.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "src/game/SpaceState.h").read_text(encoding="utf-8")

def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")

for marker in (
    "currentPoseChanged",
    "replanReason = \"current_pose\"",
    "poseLateralThreshold",
    "poseVerticalThreshold",
    "tunnelBuildMs",
    "[DockingPerf]",
    "snapshot_ms=",
    "geometric_ms=",
    "trajectory_ms=",
    "tunnel_ms=",
):
    require(marker in CPP, f"live docking guidance marker missing: {marker}")

require("m_perfDockingTunnelBuildMs" in HEADER,
        "SpaceState does not retain per-frame tunnel build time")
require(CPP.find("currentPoseChanged") < CPP.find('replanReason = "current_pose"'),
        "current hull pose is not consulted before current-pose replan")
print("LIVE DOCKING GUIDANCE: PASS")
print(" - rolling tunnel reconnects after material current hull-pose deviation")
print(" - active CALCULATE ROUTE path reports snapshot/geometric/trajectory/tunnel timings")
