from pathlib import Path
import sys as _sys, pathlib as _pl; _sys.path.insert(0, str(_pl.Path(__file__).resolve().parents[1])); import source_units  # noqa: E402,F401  (split classes read as one unit)

ROOT = Path(__file__).resolve().parents[2]

space_cpp = (ROOT / "src/game/SpaceState.cpp").read_text(encoding="utf-8")
space_h = (ROOT / "src/game/SpaceState.h").read_text(encoding="utf-8")
dedicated = (
    ROOT / "src/game/navigation/autopilot/DedicatedClientRouteAutopilot.h"
).read_text(encoding="utf-8")

required_cpp = [
    "AssistedPlanningPipeline",
    "NewtonianPlanningPipeline",
    "AssistedClientRouteAutopilot::start",
    "NewtonianClientRouteAutopilot::start",
    "m_clientDockingAssistedStabilizePilotController",
    "m_clientDockingNewtonianStabilizePilotController",
    "m_clientAssistedRouteAutopilot",
    "m_clientNewtonianRouteAutopilot",
]
for token in required_cpp:
    assert token in space_cpp, f"missing isolated production path: {token}"

for forbidden in [
    "RoutePlanner::plan(request)",
    "m_clientRouteAutopilot",
    "m_clientDockingStabilizePilotController",
    "request.roundTurns = assisted",
    "request.useContinuousAssistedOptimizer = assisted",
]:
    assert forbidden not in space_cpp, f"shared law seam returned: {forbidden}"

required_h = [
    "AssistedAutopilot",
    "NewtonianOptimalController",
    "AssistedClientRouteAutopilot::State",
    "NewtonianClientRouteAutopilot::State",
]
for token in required_h:
    assert token in space_h, f"missing split state/controller: {token}"

assert "AutopilotLawController" not in space_h
assert "game::navigation::autopilot::ClientRouteAutopilot::State" not in space_h

assert "template <LocalFlightControlLaw Law, typename PilotController>" in dedicated
assert "PilotController pilotController {};" in dedicated
assert "AutopilotLawController" not in dedicated

# The production-facing dedicated start/update APIs must not accept a runtime
# law argument. Law is fixed by the type itself.
start_region = dedicated[dedicated.index("[[nodiscard]] static bool start("):]
start_region = start_region[: start_region.index("    [[nodiscard]] static ShipControlState stabilize(")]
assert "LocalFlightControlLaw law," not in start_region

update_region = dedicated[dedicated.index("[[nodiscard]] static Output update("):]
update_region = update_region[: update_region.index("private:")]
assert "LocalFlightControlLaw law," not in update_region

print("FLIGHT LAW HARD ISOLATION CONTRACT: PASS")
print(" - Assisted/Newtonian planners are selected only at the orchestration boundary.")
print(" - Assisted/Newtonian autopilot state and controller state are physically separate.")
print(" - Dedicated autopilot public APIs do not accept a runtime flight-law selector.")
print(" - Legacy shared production state/controller seams are absent from SpaceState.")
