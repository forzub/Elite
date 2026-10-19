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
    "player.controlAuthorityTelemetry",
    "phase=awaiting-server-capability",
    "phase=capability-ready",
    "authoritative motor telemetry lost during docking",
    "applyAuthorityToControlParams",
    "m_dockPlannedAssistedAuthority = liveAssistedAuthority",
    "hasSignificantAssistedAuthorityLoss",
    "accepted-trajectory-invalidated",
    "required_curve_a_mps2",
    "phase=entry-rebind-retry",
]
for token in required:
    assert token in space, f"missing restored Assisted docking-entry contract: {token}"

# Assisted-only moving-start lead and moving-worker handoff contracts.
# Newtonian keeps the original conservative generic lead policy.
policy = (
    ROOT / "src/game/navigation/OrdinaryRoutePolicy.h"
).read_text(encoding="utf-8")
composer = (
    ROOT / "src/game/navigation/traffic/SegmentedRouteComposer.cpp"
).read_text(encoding="utf-8")
route_request = (
    ROOT / "src/game/navigation/planner/RoutePlannerApi.h"
).read_text(encoding="utf-8")
handoff_policy = (
    ROOT / "src/game/navigation/planner/BallisticEntryHandoffPolicy.h"
).read_text(encoding="utf-8")
for name in [
    "requiredStartLeadMeters",
    "requiredAssistedStartLeadMeters",
    "requiredGuidanceTunnelRadiusMeters",
]:
    assert name in policy
assert "assistedCompactStartLead = assisted" in space
assert "bool assistedCompactStartLead = false;" in route_request
assert composer.count("requiredAssistedStartLeadMeters(") == 2
assert composer.count("requiredStartLeadMeters(") == 2
assert "BallisticEntryHandoffPolicy::" in space
assert "canReuseStraightPrefix(" in handoff_policy
assert "const bool mustRebind =" in space
assert "(!job->entryRebind && !forecastOnPrefix)" in space
assert "const double leadSeconds = connectorLeadSeconds;" in space
assert "firstTurnTravelSeconds," not in space[
    space.index("const double leadSeconds = connectorLeadSeconds;"):
    space.index("m_dockLateEntry = {};", space.index(
        "const double leadSeconds = connectorLeadSeconds;"))
], "first turn time leaked back into connector projection"
assert "entrySolveDurationMs" in space
assert "m_dockObservedConnectorWorkerSeconds" in header
assert "first_turn_remaining_m=" in space

# The terminal free-leg optimizer must not turn corridor manoeuvre room
# back into a nonnegotiable kilometre-scale straight. Keep the original
# moving-ship ballistic prefix and the BLUE hard portal straight unchanged.
route_planner = (
    ROOT / "src/game/navigation/planner/RoutePlanner.cpp"
).read_text(encoding="utf-8")
for marker in [
    "assistedHardStartStraightMeters(",
    "input.assistedUseExactProtectedStart",
    "hard_start_m=",
    "hard_goal_m=",
    "manoeuvre_lead_m=",
]:
    assert marker in route_planner, f"Assisted optimizer handoff lost: {marker}"
assert "bool assistedUseExactProtectedStart = false;" in route_request
assert "requiredDockingAxisMinimumMeters(" in policy
assert "assistedHardStartStraightMeters(" in policy
for marker in [
    "finalRequest.assistedUseExactProtectedStart =",
    "finalRequest.initialProtectedStraightMeters =",
    "requiredDockingAxisMinimumMeters(",
    "free_leg_total_ms=",
    "final_goal_hard_m=",
    "final_exit_speed_mps=",
]:
    assert marker in composer, f"segmented terminal geometry drift: {marker}"
assert "inbound_start_hard_m=" in composer
assert 'planner_diagnostics=' in space
# Explicit mode must remain confined to Assisted's *final* free leg.
assert "legRequest.assistedUseExactProtectedStart =" not in composer
assert ("finalRequest.assistedCompactStartLead &&" in composer and
        "finalRequest.useContinuousAssistedOptimizer;" in composer)

# The original literal "one-shot handoff" error was retired when the
# connector gained an explicit, bounded late-entry recovery budget. Verify
# the LIVE control flow, rather than requiring a removed diagnostic string.
retry_start = space.index("++m_dockLateEntry.rebindAttempts;")
retry_exhaustion = space.index("phase=entry-rebind-exhausted", retry_start)
retry_fail = space.index(
    'fail("dock entry connector handoff did not converge");',
    retry_exhaustion,
)
retry_log = space.index("phase=entry-rebind-retry", retry_fail)
retry_region = space[retry_start:retry_log]
assert "constexpr std::uint32_t MaxEntryConnectorMisses = 4;" in retry_region
assert "if (m_dockLateEntry.rebindAttempts >=" in retry_region
assert "MaxEntryConnectorMisses)" in retry_region
assert "return;" in space[retry_fail:retry_log]
assert retry_start < retry_exhaustion < retry_fail < retry_log

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

# The control loop must read a dedicated server-authored capability
# snapshot; module graphs are intentionally sparse in normal replication.
ship_snapshot = (
    ROOT / "src/game/simulation/ShipSnapshot.h"
).read_text(encoding="utf-8")
client_snapshot = (
    ROOT / "src/game/client/ClientWorldState.h"
).read_text(encoding="utf-8")
server = (
    ROOT / "src/game/simulation/GameSimulation.cpp"
).read_text(encoding="utf-8")
client_receive = (
    ROOT / "src/game/client/ClientWorldState.cpp"
).read_text(encoding="utf-8")
wire = (
    ROOT / "src/game/network/WireDataSchema.h"
).read_text(encoding="utf-8")
schema = (
    ROOT / "src/game/network/WireDataCodec.h"
).read_text(encoding="utf-8")
for src in [ship_snapshot, client_snapshot, server, client_receive, wire]:
    assert "controlAuthorityTelemetry" in src
assert "ship.core().controlAuthorityTelemetry()" in server
assert client_receive.count(
    "state.controlAuthorityTelemetry = s.controlAuthorityTelemetry;"
) == 2
assert "game::ship::telemetry::ShipControlAuthorityTelemetry," in wire
assert "SimulationSnapshotWireSchemaVersion = 18u;" in schema
assert "m_dockReplicatedAssistedPrograms" not in space
assert "readReplicatedAssistedControlAuthority" not in space

# A virtual pilot must not turn installed motor damage into ideal geometry.
assert "reservedCurveLateralAccelerationMps2" in space
assert "ConcurrentTurnAxisFraction" in space
client_follow = (
    ROOT / "src/game/navigation/autopilot/DedicatedClientRouteAutopilot.h"
).read_text(encoding="utf-8")
assert "maximumSustainableCurveSpeedMps" in client_follow
assert "ConcurrentTurnAxisFraction" in client_follow
motor_read = (
    ROOT / "src/game/ship/telemetry/ReplicatedAssistedControlAuthority.h"
).read_text(encoding="utf-8")
for marker in [
    "state->engineImpactShock01",
    "state->engineTemperatureK",
    "state->emitterIntactArea01",
    "cache.matches(model, mounted, physicalParams)",
    "cache.build(model, mounted, physicalParams)",
]:
    assert marker in motor_read, f"missing installed motor projection: {marker}"
authority_api = (
    ROOT / "src/game/ship/telemetry/ShipControlAuthorityTelemetry.h"
).read_text(encoding="utf-8")
assert "p.forwardMainEngineAvailable && forward > 1.0e-6" in authority_api
assert "p.reverseMainEngineAvailable && reverse > 1.0e-6" in authority_api

print("ASSISTED DOCKING ENTRY CONTRACT: PASS")
print(" - safe-course selects one direction; it is not a trajectory follower.")
print(" - safe-course emits ordinary Assisted hull controls; FA-on remains authoritative.")
print(" - planning coast uses neutral physical controls, never a direct acceleration bypass.")
print(" - expensive downstream route is retained.")
print(" - live handoff uses one projected entry bind based on first-turn travel time.")
print(" - repeated entry-rebind loops are bounded and explicitly reported.")
