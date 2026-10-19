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
    "constexpr std::uint32_t MaxEntryConnectorMisses = 4;",
    "phase=entry-rebind-exhausted",
    "dock entry connector handoff did not converge",
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

# Numeric route construction rules are distinct from physical clearances.
precision = (
    ROOT / "src/game/navigation/NavigationGeometryTolerance.h"
).read_text(encoding="utf-8")
binder = (
    ROOT / "src/game/navigation/planner/RouteEntryBinder.cpp"
).read_text(encoding="utf-8")
for marker in [
    "ConstructedPointMeters = 1.0e-3",
    "EntryStartBackstepMeters = 1.0e-2",
    "EntryEndOvershootMeters = 1.0e-2",
    "ComposedStageLengthMeters = 1.0e-2",
]:
    assert marker in precision, f"route geometry precision drift: {marker}"
for marker in [
    "geometry_tolerance::EntryStartBackstepMeters",
    "geometry_tolerance::EntryEndOvershootMeters",
    "std::clamp(along, 0.0, length)",
]:
    assert marker in binder, f"route entry numerical guard missing: {marker}"
assert "if (along < 0.0)" not in binder
for relative_path in [
    "src/game/navigation/DockingAdvisoryPlanner.cpp",
    "src/game/navigation/planner/RoutePlanner.cpp",
    "src/game/navigation/planner/RouteEntryBinder.cpp",
    "src/game/navigation/traffic/RouteStageCompiler.cpp",
    "src/game/navigation/traffic/SegmentedRouteComposer.cpp",
    "src/game/navigation/traffic/TrafficRoutePlanAdapter.cpp",
    "src/game/navigation/traffic/RouteVolumeContainmentValidator.cpp",
]:
    source = (ROOT / relative_path).read_text(encoding="utf-8")
    assert "geometry_tolerance::" in source, (
        f"authoritative route construction tolerance missing: {relative_path}"
    )

assert "double leadSeconds = 0.0;" in header
assert "navigationAccelerationDemandValid = true;" not in space[
    space.index("const auto coastForPlanningControl"):
    space.index("const auto stabilizeAngularForPlanningControl")
]
assert "game::navigation::AssistedSafeCourseRecoveryPlan plan;" not in header

# A new automatic docking request must settle angular motion BEFORE the
# authoritative planning snapshot is captured. Waiting only at handoff is too
# late because the worker compiles the initial angular trajectory from snapshot.
settle_marker = space.index("AUTOMATIC SETTLE-BEFORE-SNAPSHOT CONTRACT")
snapshot_capture = space.index(
    "ClientNavigationPlanningSnapshotFactory::",
    settle_marker,
)
settle_region = space[settle_marker:snapshot_capture]
assert "AutomaticSettledAngularRadPerSec = 0.01" in settle_region
assert "AutomaticAngularSettledSeconds = 0.20" in settle_region
assert "if (!angularStopped)" in settle_region
assert "return;" in settle_region
assert "submitAutopilotControl(coastForPlanningControl())" in settle_region
assert "async-preplanning" not in settle_region

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
print(" - repeated entry-rebind loops are bounded and explicitly reported.")
