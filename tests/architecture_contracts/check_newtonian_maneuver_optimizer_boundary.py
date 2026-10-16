from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

optimizer = (
    ROOT
    / "src/game/navigation/planner/newtonian/NewtonianManeuverOptimizer.h"
).read_text(encoding="utf-8")

required = [
    "NewtonianManeuverOptimizer",
    "NewtonianManeuverPhaseKind",
    "RotateForThrust",
    "MainBurn",
    "Reorient",
    "TerminalHold",
    "terminalVelocitySquared",
    "mainImpulse",
    "angularTravel",
]

for token in required:
    assert token in optimizer, f"missing Newtonian planner contract token: {token}"

for forbidden in [
    "Assisted",
    "AssistedRoutePlanner",
    "AssistedAutopilot",
    "NewtonianOptimalController",
    "AutopilotControl",
    "SegmentedRouteComposer",
    "CompiledTrafficRoute",
    "RouteVolume",
    "blue_lane",
    "transit_volume",
    "SpaceState",
]:
    assert forbidden not in optimizer, (
        f"Newtonian maneuver optimizer leaked external/law-specific dependency: {forbidden}"
    )

assert "mainAccelerationMps2" in optimizer
assert "lateralMps2" not in optimizer
assert "strafe" not in optimizer.lower()

print("NEWTONIAN MANEUVER OPTIMIZER BOUNDARY: PASS")
print(" - pure planner/functional layer only.")
print(" - no Assisted or autopilot dependency.")
print(" - no BLUE/traffic-volume dependency.")
print(" - aft-main translation is explicit; no fake lateral main authority.")
