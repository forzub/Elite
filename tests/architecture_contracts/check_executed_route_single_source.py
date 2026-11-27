"""Stabilization stage 4 (B3) barrier: the HUD draws the EXECUTED route, it never builds one.

- The pad-service tunnel comes from ShipSnapshot::executedRoute (the server's points and current point) - the HUD must not
  call the route builders (hubPadRoute / hubPadServiceTransit) nor guess the leg.
- No held / synthesized frames (the 2026-11-27 "held frames + straight row to the finish" fallback): the HUD frames are
  the docking computer's published plan or the server's executed route.
- The server publishes the executed route from the values its pad service steers by (executedRouteOf), and the schema
  carries it."""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
src = lambda p: open(os.path.join(ROOT, "src", p), encoding="utf-8", errors="replace").read()
code = lambda t: "\n".join(l.split("//")[0] for l in t.splitlines())
problems = []
hud = code(src("game/SpaceStateHud.cpp"))
for bad, why in [(r"hubPadRoute\s*\(", "the HUD builds the pad route"),
                 (r"hubPadServiceTransit\s*\(", "the HUD computes the service's transit"),
                 (r"\bserviceLeg\b", "the HUD guesses the service's leg"),
                 (r"HeldFrame|static std::vector<\w+> held\b", "the HUD holds frames of its own"),
                 (r"orderedRouteWaypoints\s*\(", "the HUD synthesizes frames to the route's finish")]:
    if re.search(bad, hud):
        problems.append(f"SpaceStateHud.cpp: {why} ({bad})")
if "executedRoute" not in hud:
    problems.append("SpaceStateHud.cpp: the pad tunnel does not read playerShip.executedRoute")
pad = src("game/simulation/GameSimulationPadLanding.cpp")
if "executedPoints = std::move(pts)" not in pad or "GameSimulation::executedRouteOf" not in pad:
    problems.append("GameSimulationPadLanding.cpp: the service does not publish what it executes")
if "v.executedRoute" not in src("game/network/WireDataSchema.h"):
    problems.append("WireDataSchema.h: ShipSnapshot::executedRoute not on the wire")
dock = src("game/SpaceStateDocking.cpp")
if "keepExecutedRoutePublished" not in dock:
    problems.append("SpaceStateDocking.cpp: the docking computer's route is not kept published while it re-plans")
if problems:
    print("[FAIL] executed route single source (stage 4 B3):")
    for p in problems:
        print("  " + p)
    sys.exit(1)
print("[PASS] executed route single source: the HUD draws the server's / docking computer's executed route, builds none")
