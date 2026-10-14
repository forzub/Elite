from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

space = (ROOT / "src/game/SpaceState.cpp").read_text(encoding="utf-8")
header = (ROOT / "src/game/SpaceState.h").read_text(encoding="utf-8")
executor = (
    ROOT / "src/game/navigation/assisted/AssistedSafeCourseExecutor.h"
).read_text(encoding="utf-8")

required = [
    "RESTORED ASSISTED DOCKING-ENTRY CONTRACT",
    "PROJECTED ENTRY CONTRACT -- DO NOT TURN THIS INTO A LOOP",
    "AssistedSafeCourseExecutor::evaluate",
    "submitAutopilotControl(safeOutput.control)",
    "firstTurnTravelSeconds",
    "m_dockLateEntry.leadSeconds",
    "projected Assisted entry bind missed its one-shot handoff",
]
for token in required:
    assert token in space, f"missing restored Assisted docking-entry contract: {token}"

for forbidden in [
    "AssistedSafeCourseRecovery::assess",
    "AssistedSafeCourseRecovery::plan",
    "m_dockAssistedSafeRecovery.plan",
    "safeOutput.request",
]:
    assert forbidden not in space, f"trajectory-style safe-course regression: {forbidden}"

assert "double leadSeconds = 0.0;" in header
assert "navigationAccelerationDemandValid = true;" not in space[
    space.index("const auto coastForPlanningControl"):
    space.index("const auto stabilizeAngularForPlanningControl")
]
assert "game::navigation::AssistedSafeCourseRecoveryPlan plan;" not in header

for token in [
    "DO NOT turn this back into a trajectory follower",
    "turn the VELOCITY VECTOR",
    "deviation from an",
]:
    assert token in executor, f"missing executor contract guard: {token}"

print("ASSISTED DOCKING ENTRY CONTRACT: PASS")
print(" - safe-course selects one direction; it is not a trajectory follower.")
print(" - safe-course emits ordinary Assisted hull controls; FA-on remains authoritative.")
print(" - planning coast uses neutral physical controls, never a direct acceleration bypass.")
print(" - expensive downstream route is retained.")
print(" - live handoff uses one projected entry bind based on first-turn travel time.")
print(" - repeated entry-rebind loops are forbidden.")
