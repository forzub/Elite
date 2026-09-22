#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")


def fail(message: str) -> None:
    print(f"[FAIL] navigation API purity: {message}", file=sys.stderr)
    raise SystemExit(1)


def require(condition: bool, message: str) -> None:
    if not condition:
        fail(message)


runtime_h = read("tools/navigation_runtime/NavigationScenarioRuntime.h")
runtime_cpp = read("tools/navigation_runtime/NavigationScenarioRuntime.cpp")
geo_h = read("src/world/navigation/GeometricPathPlanner.h")
geo_cpp = read("src/world/navigation/GeometricPathPlanner.cpp")
trajectory_h = read("src/world/navigation/TrajectoryGenerator.h")
ship_dynamics = read("src/game/ship/core/ShipDynamics.h")
ship_controller = read("src/game/ship/ShipController.cpp")
dynamic_motion = read("src/game/navigation/DynamicMotionSystem.cpp")
ship_params_h = read("src/game/ship/core/ShipParams.h")
pilot_h = read("src/world/navigation/control/PilotSkillExecutor.h")
pilot_cpp = read("src/world/navigation/control/PilotSkillExecutor.cpp")
compiler_h = read("src/game/navigation/OrdinaryPhysicalManeuverCompiler.h")
compiler_cpp = read("src/game/navigation/OrdinaryPhysicalManeuverCompiler.cpp")
sampler = read("src/game/navigation/ManeuverProgramSampler.cpp")
tracker = read("src/game/navigation/ManeuverTrackingController.cpp")
follower = read("src/game/navigation/TrajectoryFollower.cpp")
bridge = read("src/game/navigation/NavigationRuntimeControlBridge.cpp")
ruckig = read("src/game/navigation/RuckigTrajectorySolver.cpp")
vehicle_adapter = read("src/game/navigation/NavigationVehicleProfileAdapters.h")
capability_adapter = read("src/game/navigation/ManeuverCapabilityAdapters.h")
nominal_h = read("src/game/navigation/NominalRoutePlanner.h")
nominal_cpp = read("src/game/navigation/NominalRoutePlanner.cpp")
trajectory_cpp = read("src/world/navigation/TrajectoryGenerator.cpp")
ruckig_h = read("src/game/navigation/RuckigTrajectorySolver.h")
sampler_h = read("src/game/navigation/ManeuverProgramSampler.h")
tracker_h = read("src/game/navigation/ManeuverTrackingController.h")
follower_h = read("src/game/navigation/TrajectoryFollower.h")
bridge_h = read("src/game/navigation/NavigationRuntimeControlBridge.h")
phase_gate_h = read("src/game/navigation/ManeuverPhaseGate.h")
phase_gate_cpp = read("src/game/navigation/ManeuverPhaseGate.cpp")
replan_h = read("src/game/navigation/NavigationExecutionReplanPolicy.h")
replan_cpp = read("src/game/navigation/NavigationExecutionReplanPolicy.cpp")
ownership_doc = read("src/game/navigation/NAVIGATION_COMMAND_OWNERSHIP.md")
runtime_planner_h = read("src/game/navigation/NavigationRuntimePlanner.h")
runtime_planner_cpp = read("src/game/navigation/NavigationRuntimePlanner.cpp")
e2e = read("tests/navigation_runtime/NavigationScenarioRuntimeE2ETests.cpp")

# ---------- Public snapshot/API boundary ----------
for token in (
    "struct ScenarioDefinition",
    "loadScenarioDefinition(",
    "const ScenarioDefinition& scenario",
    "struct RetainedStaticRoute",
    "vehicleCapabilityRevision",
    "const RetainedStaticRoute& retainedRoute",
    "VehicleDynamicsProfile",
    "ScenarioNavigationPolicy",
    "TrajectoryGenerationPolicy trajectory",
    "WorldParams worldPhysics",
    "ScenarioFrameDefinition",
):
    require(token in runtime_h, f"runtime API missing explicit boundary token {token!r}")

# Stage-1 and Stage-2 calculation must consume one already parsed snapshot.
calc_start = runtime_cpp.find("ScenarioRunResult calculateScenario(")
exec_start = runtime_cpp.find("ScenarioRunResult executeCalculatedRoute(")
require(calc_start >= 0 and exec_start > calc_start, "cannot locate runtime calculate/execute bodies")
calc_body = runtime_cpp[calc_start:exec_start]
exec_body = runtime_cpp[exec_start:]

for name, body in (("calculateScenario", calc_body), ("executeCalculatedRoute", exec_body)):
    for forbidden in (
        "std::ifstream",
        "loadScenarioDefinition(",
        "parseScenarioDefinitionFile(",
        "nlohmann::json",
        "current_path()",
        "EliteCobraMk1",
        "cobraParams(",
    ):
        require(forbidden not in body, f"{name} reaches outside its value API via {forbidden!r}")

# Side effects must be opt-in through the explicit I/O policy.
for token in (
    "ScenarioRuntimeIoPolicy",
    "writeRouteDiagnostics",
    "writeExecutionDiagnostics",
    "writeExecutionTelemetry",
):
    require(token in runtime_h or token in runtime_cpp, f"explicit runtime I/O contract missing {token}")

require("std::filesystem::current_path" not in runtime_cpp,
        "runtime resolves ambient current working directory")

for forbidden in (
    "WorldParams world {};",
    'frame.systemId = 1;',
    'frame.frameId = "navigation-runtime-stage2";',
    "frame.originMeters = {0.0, 0.0, 0.0};",
):
    require(
        forbidden not in runtime_cpp,
        f"runtime manufactures hidden environment/frame input: {forbidden}",
    )

for token in (
    "vehicleInit.world = scenario.worldPhysics",
    "vehicleInit.frame = scenario.frame",
    "vehicleInit.startPositionMapMeters = scenario.startPosition",
    "vehicleInit.startVelocityMapMps =",
    "ExecutionVehicle vehicle(vehicleInit, vehicleInput)",
):
    require(token in runtime_cpp,
            f"runtime does not compose explicit execution init data: {token}")

# ---------- Active-path API surface ----------
for token in (
    "struct Request",
    "static Plan plan(const Request& request)",
):
    require(token in nominal_h, f"NominalRoutePlanner API missing explicit request seam {token}")

for token in (
    "struct TrajectoryGenerationRequest",
    "NavigationVehicleProfile vehicle",
    "TrajectoryGenerationPolicy policy",
    "static TrajectoryGenerationResult generate(",
):
    require(token in trajectory_h, f"TrajectoryGenerator API missing explicit input {token}")

for token in (
    "struct RuckigProgressRequest",
    "struct RuckigTrajectoryRequest",
    "solveProgress(",
):
    require(token in ruckig_h, f"Ruckig solver API missing explicit request {token}")

for token in (
    "const AcceptedManeuverProgram& program",
    "double universeTimeSeconds",
    "const AgentState& agent",
    "const ManeuverTrackingController::Policy& trackingPolicy",
):
    require(token in follower_h, f"TrajectoryFollower API missing explicit input {token}")

for token in (
    "const AcceptedManeuverProgram& program",
    "const AcceptedManeuverProgram::ReferenceSample& reference",
    "const AgentState& agent",
    "const Policy& policy",
):
    require(token in tracker_h, f"ManeuverTrackingController API missing explicit input {token}")

for token in (
    "const AcceptedManeuverProgram& program",
    "double universeTimeSeconds",
):
    require(token in sampler_h, f"ManeuverProgramSampler API missing explicit input {token}")

for token in (
    "const AcceptedManeuverProgram& program",
    "double universeTimeSeconds",
    "TrajectoryFollower::Status followerStatus",
    "const Policy& policy",
):
    require(token in phase_gate_h, f"ManeuverPhaseGate API missing explicit input {token}")

for token in (
    "const Policy& policy",
    "const Query& query",
):
    require(token in replan_h, f"NavigationExecutionReplanPolicy API missing explicit input {token}")

# ---------- Stage-1 geometric policy ----------
for token in (
    "minimumSupportMarginMeters",
    "supportMarginObstacleRadiusFactor",
    "sphereRadialSamples",
    "capsuleRadialSamples",
    "[[nodiscard]] bool valid() const noexcept",
):
    require(token in geo_h, f"geometric planner API hides policy {token}")

for forbidden in (
    "std::max(8, params.sphereRadialSamples)",
    "std::max(8, params.capsuleRadialSamples)",
    "std::max(0.25, params.supportMarginMeters)",
    "conservativeRadiusMeters() * 0.03",
):
    require(forbidden not in geo_cpp,
            f"geometric planner reintroduced hidden behavior constant: {forbidden}")

for token in (
    "geometricMinimumSupportMarginMeters",
    "geometricSupportMarginObstacleRadiusFactor",
):
    require(token in runtime_h and token in runtime_cpp,
            f"runtime does not pass complete geometric policy: {token}")

# ---------- Trajectory backend policy ----------
require("TrajectoryGenerationPolicy policy" in trajectory_h,
        "trajectory request no longer owns explicit generation policy")
require("request.policy = trajectoryPolicy;" in runtime_cpp,
        "Stage 2 silently relies on TrajectoryGenerationPolicy defaults")
require("settings.trajectory.valid()" in runtime_cpp,
        "Stage 2 does not validate trajectory policy at API boundary")

# ---------- Vehicle dynamics single source of truth ----------
for token in (
    "forwardMainAccelerationLimitMps2",
    "reverseMainAccelerationLimitMps2",
    "manoeuvreAccelerationLimitMps2",
    "angularLoadRateLimitRadPerSec",
    "angularAccelerationLimitRadPerSec2",
    "pitchRateLimitRadPerSec",
    "yawRateLimitRadPerSec",
    "rollRateLimitRadPerSec",
):
    require(token in ship_dynamics, f"canonical ShipDynamics helper missing {token}")

for forbidden in (
    "float angularAccelerationEnvelope(",
    "float angularRateEnvelope(",
    "StandardGravityMps2 =",
):
    require(forbidden not in ship_controller,
            f"ShipController keeps a competing physics truth: {forbidden}")

for token in (
    "game::ship::angularAccelerationLimitRadPerSec2(params)",
    "game::ship::pitchRateLimitRadPerSec(params)",
    "game::ship::yawRateLimitRadPerSec(params)",
    "game::ship::rollRateLimitRadPerSec(params)",
):
    require(token in ship_controller,
            f"ShipController does not consume canonical angular limit {token}")

for token in (
    "stopSpeedEpsilonMps",
    "brakeAlignmentCosine",
    "assistedMinimumTargetSpeedChangeRateMps2",
    "assistedTargetSpeedChangeRateFractionPerSecond",
    "fallbackThrottleResponsePerSecond",
):
    require(token in ship_params_h,
            f"ShipParams API missing low-level motion input {token}")
    require(token in dynamic_motion,
            f"DynamicMotionSystem does not consume explicit ShipParams input {token}")

for forbidden in (
    "constexpr double StopSpeedEpsilonMps",
    "constexpr double BrakeAlignmentCos",
    "std::max(50.0, maxSpeed * 0.6)",
    "positiveOr(static_cast<double>(params.throttleAccel), 1.0)",
):
    require(forbidden not in dynamic_motion,
            f"DynamicMotionSystem reintroduced hidden behavior constant {forbidden}")

for source_name, source in (
    ("NavigationVehicleProfileAdapters", vehicle_adapter),
    ("ManeuverCapabilityAdapters", capability_adapter),
    ("DynamicMotionSystem", dynamic_motion),
):
    for forbidden in (
        "params.maxLinearGs",
        "params.manoeuvreThrusterAccel",
    ):
        require(forbidden not in source,
                f"{source_name} reinterprets raw vehicle field {forbidden}")

# ---------- Physical compiler policy ----------
for token in (
    "struct Policy",
    "minimumPrimitiveSeconds",
    "directPrimitiveSeconds",
    "burnRampMinimumSeconds",
    "burnRampMaximumSeconds",
    "burnRampFractionOfRawBurn",
):
    require(token in compiler_h, f"maneuver compiler API hides timing policy {token}")

for forbidden in (
    "kMinimumPrimitiveSeconds",
    "std::min(1.0, q.maximumProgramSeconds)",
    "std::min(0.20, std::max(0.02",
):
    require(forbidden not in compiler_cpp,
            f"maneuver compiler reintroduced hidden behavioral constant {forbidden}")

# ---------- Pure calculation kernels ----------
for name, source in (
    ("NominalRoutePlanner", nominal_cpp),
    ("GeometricPathPlanner", geo_cpp),
    ("TrajectoryGenerator", trajectory_cpp),
    ("RuckigTrajectorySolver", ruckig),
    ("OrdinaryPhysicalManeuverCompiler", compiler_cpp),
    ("ManeuverProgramSampler", sampler),
    ("ManeuverTrackingController", tracker),
    ("TrajectoryFollower", follower),
    ("ManeuverPhaseGate", phase_gate_cpp),
    ("NavigationExecutionReplanPolicy", replan_cpp),
):
    for forbidden in (
        "std::ifstream",
        "std::ofstream",
        "std::filesystem",
        "current_path()",
        "std::chrono",
        "system_clock",
        "steady_clock",
        "random_device",
        "std::rand",
        "EliteCobraMk1",
        "ShipDescriptor",
        "NavigationScenarioRuntime",
        "scenario.json",
        "GameSimulation",
        "std::cout",
        "std::cerr",
    ):
        require(forbidden not in source,
                f"{name} pure kernel leaked ambient/stateful dependency {forbidden}")

# Pilot executor is deliberately stateful, but cadence/queue behavior must
# remain part of its explicit profile rather than private constants.
for token in (
    "maximumStepSeconds",
    "integrationSubstepsPerResponsePeriod",
    "maximumIntegrationSubsteps",
    "maximumPendingCommands",
):
    require(token in pilot_h, f"pilot execution API hides cadence policy {token}")
    require(token in pilot_cpp, f"pilot executor ignores explicit cadence policy {token}")

for forbidden in (
    "kMaximumStepSeconds",
    "kMaxIntegrationSubsteps",
    "kMaxPendingCommands",
):
    require(forbidden not in pilot_h + pilot_cpp,
            f"pilot executor reintroduced hidden behavior constant {forbidden}")

# A fixed array capacity is an implementation/storage bound, not behavior.
require("kPendingCommandStorageCapacity" in pilot_h,
        "pilot queue storage bound is no longer explicit as implementation capacity")

# Bridge is deliberately stateful, but its state/time/profile must be explicit.
for token in (
    "NavigationRuntimeControlBridge(",
    "const PilotSkillProfile& profile",
    "double timeSeconds",
    "double deltaSeconds",
    "const Intent& intent",
):
    require(token in read("src/game/navigation/NavigationRuntimeControlBridge.h"),
            f"control bridge state boundary missing explicit {token}")

# ---------- Runtime helper API narrowness ----------
# Orchestration may compose Scenario/Settings, but calculation helpers below it
# must receive exactly the data they use. This catches "reach into a giant
# context object" regressions before they become a second source of truth.
for required in (
    "struct ResolvedRunKinematics",
    "ResolvedRunKinematics resolveRunKinematics(",
    "double routePlanningClearanceMeters(\n    double planningSpeedMps,",
    "const Basis& initialBasis,",
    "const glm::dvec3& initialAngularVelocityMapRadPerSec,",
    "const Endpoint& terminal,",
    "double preferredClearanceMeters,",
    "double solverSpeedCeilingMps",
    "struct ExecutionVehicleInit",
    "ExecutionVehicle(\n        const ExecutionVehicleInit& init,",
):
    require(required in runtime_cpp, f"runtime helper API is not explicit/narrow: {required}")

for forbidden in (
    "double effectiveStartSpeedMps(",
    "double effectiveFinishSpeedMps(",
    "glm::dvec3 effectiveStartVelocity(",
    "double routePlanningClearanceMeters(\n    const Scenario&",
    "std::vector<ReferenceAttitude> buildReferenceAttitudes(\n    const world::navigation::Trajectory& trajectory,\n    const Scenario&",
    "Program makeProgramPhase(\n    const world::navigation::Trajectory& trajectory,\n    const std::vector<ReferenceAttitude>& attitudes,\n    std::size_t first,\n    std::size_t last,\n    std::uint64_t revision,\n    const Scenario&",
    "ExecutionVehicle(\n        const Scenario&",
):
    require(forbidden not in runtime_cpp, f"runtime calculation helper reaches across broad API: {forbidden}")

# Scenario file parser owns ScenarioDefinition mutation. No anonymous 'terminal'
# alias may leak into parser/preview/Stage-1 orchestration.
parser = function_slice(
    runtime_cpp,
    "ScenarioDefinition parseScenarioDefinitionFile(",
    "TraceStaticObstacle traceObstacle("
)
for token in (
    "scenario.finish.position",
    "scenario.finish.requireForward",
    "scenario.finish.requireUp",
):
    require(token in parser, f"scenario parser lost explicit finish ownership: {token}")

# The active E2E contract must not mention the retired reference-hold/reacquire
# mechanism at all.
for forbidden in (
    "REFERENCE CLOCK HOLD:",
    "reference-hold",
    "reacquiresInsteadOfOutrunningReference",
    "instead of reacquiring",
):
    require(forbidden not in e2e,
            f"E2E still names retired frozen-reference semantics: {forbidden}")

# 'terminal' is an explicit helper parameter only inside the attitude author
# and trajectory-request adapter. It must not leak as an undeclared alias into
# parser/diagnostic/orchestration functions.
attitude_helper = function_slice(
    runtime_cpp,
    "std::vector<ReferenceAttitude> buildReferenceAttitudes(",
    "world::navigation::NavigationVehicleProfile executionVehicleProfile("
)
trajectory_helper = function_slice(
    runtime_cpp,
    "world::navigation::TrajectoryGenerationResult buildExecutionTrajectory(",
    "Program makeProgramPhase("
)
runtime_without_terminal_helpers = runtime_cpp.replace(attitude_helper, "").replace(
    trajectory_helper, ""
)
require(
    "terminal." not in runtime_without_terminal_helpers,
    "undeclared/cross-boundary terminal alias leaked outside explicit helper APIs",
)

# ---------- Retained-route provenance ----------
for token in (
    "retainedRoute.goalRevision == scenario.goalRevision",
    "retainedRoute.staticWorldRevision ==",
    "retainedRoute.vehicleCapabilityRevision ==",
    "retainedRoute.planningSpeedMps -",
    "retainedRoute.additionalClearanceMeters -",
):
    require(token in runtime_cpp,
            f"Stage 2 does not validate retained-route provenance: {token}")

# ---------- Production runtime-planner policy ----------
for token in (
    "staticHoldUrgency01",
    "staleHoldUrgency01",
    "conflictHoldUrgency01",
    "minimumApproachHoldDistanceMeters",
    "maximumLateralCorrectionVelocityAngleRad",
):
    require(token in runtime_planner_h,
            f"NavigationRuntimePlanner hides behavior policy {token}")

for forbidden in (
    "holdIntent(agent, goal, 0.5)",
    "holdIntent(agent, goal, 0.75)",
    "holdIntent(agent, goal, 1.0)",
    "std::max(2.0, portalAllowedCrossTrackMeters)",
    "1.5533430342749532 // 89 degrees.",
):
    require(forbidden not in runtime_planner_cpp,
            f"NavigationRuntimePlanner reintroduced hidden behavior literal {forbidden}")

# ---------- Runtime physical-capability purity ----------
for forbidden in (
    "std::max(0.1, profile.maxForwardAccelerationMps2)",
    "std::max(0.1, profile.maxBrakingAccelerationMps2)",
    "std::max(0.1, profile.maxLateralAccelerationMps2)",
    "std::max(0.1, profile.maxAngularAccelerationRadPerSecond2)",
    "std::max(\n            0.1,\n            game::ship::maximumAngularSpeedRadPerSec(params)",
):
    require(forbidden not in runtime_cpp,
            f"runtime invents physical authority through numeric floor: {forbidden}")

# ---------- Removed legacy contracts ----------
for forbidden in (
    "REFERENCE CLOCK HOLD:",
    "PROGRAM PHASES: ",
    "PHASE HANDOFFS: ",
):
    require(forbidden not in e2e,
            f"E2E still asserts obsolete runtime contract {forbidden}")

require("## The planner does not command individual thrusters" not in ownership_doc,
        "command-ownership document contradicts Planner-owned actuator program")
require("The planner owns the nominal actuator schedule" in ownership_doc,
        "command ownership does not state current actuator-program contract")

print("[PASS] navigation active-path API purity contract")
print(" - scenario file I/O ends before Stage-1/Stage-2 calculations")
print(" - one immutable scenario snapshot crosses both stages")
print(" - retained route has an explicit provenance-checked API product")
print(" - vehicle dynamics/capability projections share canonical helpers")
print(" - geometric, trajectory, tracking and compiler policy are explicit inputs")
print(" - pure planner/follower kernels have no ambient I/O/time/random/world reach-through")
