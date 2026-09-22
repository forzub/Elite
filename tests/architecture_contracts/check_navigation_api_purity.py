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
compiler_h = read("src/game/navigation/OrdinaryPhysicalManeuverCompiler.h")
compiler_cpp = read("src/game/navigation/OrdinaryPhysicalManeuverCompiler.cpp")
sampler = read("src/game/navigation/ManeuverProgramSampler.cpp")
tracker = read("src/game/navigation/ManeuverTrackingController.cpp")
follower = read("src/game/navigation/TrajectoryFollower.cpp")
bridge = read("src/game/navigation/NavigationRuntimeControlBridge.cpp")
ruckig = read("src/game/navigation/RuckigTrajectorySolver.cpp")
vehicle_adapter = read("src/game/navigation/NavigationVehicleProfileAdapters.h")
capability_adapter = read("src/game/navigation/ManeuverCapabilityAdapters.h")
ownership_doc = read("src/game/navigation/NAVIGATION_COMMAND_OWNERSHIP.md")
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
require("request.policy = settings.trajectory;" in runtime_cpp,
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
    ("GeometricPathPlanner", geo_cpp),
    ("RuckigTrajectorySolver", ruckig),
    ("OrdinaryPhysicalManeuverCompiler", compiler_cpp),
    ("ManeuverProgramSampler", sampler),
    ("ManeuverTrackingController", tracker),
    ("TrajectoryFollower", follower),
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
        "GameSimulation",
    ):
        require(forbidden not in source,
                f"{name} pure kernel leaked ambient/stateful dependency {forbidden}")

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
