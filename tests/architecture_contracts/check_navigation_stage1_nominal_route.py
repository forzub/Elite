#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")


def require(condition: bool, message: str) -> None:
    if not condition:
        print(f"[FAIL] static-route/two-stage navigation: {message}", file=sys.stderr)
        raise SystemExit(1)


def function_slice(text: str, name: str, next_name: str | None = None) -> str:
    start = text.find(name)
    require(start >= 0, f"missing function {name}")
    if next_name is None:
        return text[start:]
    end = text.find(next_name, start + len(name))
    require(end > start, f"cannot isolate function {name}")
    return text[start:end]


def compact_cpp(text: str) -> str:
    """Normalize C++ formatting so architecture checks do not depend on line wraps."""
    return "".join(text.split())


header = read("src/game/navigation/NominalRoutePlanner.h")
impl = read("src/game/navigation/NominalRoutePlanner.cpp")
runtime = read("tools/navigation_runtime/NavigationScenarioRuntime.cpp")
runtime_h = read("tools/navigation_runtime/NavigationScenarioRuntime.h")
viewer = read("tools/navigation_runtime/NavigationRuntimeViewer.cpp")
trace_h = read("tools/navigation_runtime/NavigationTrace.h")
tool_cmake = read("tools/navigation_runtime/CMakeLists.txt")
test = read("tests/navigation_runtime/NominalRoutePlannerTests.cpp")
readme = read("tools/navigation_runtime/README.md")
scenario_json = read("tools/navigation_runtime/scenario.json")
trajectory_h = read("src/world/navigation/TrajectoryGenerator.h")
trajectory_cpp = read("src/world/navigation/TrajectoryGenerator.cpp")

for token in (
    "class NominalRoutePlanner",
    "goalRevision",
    "staticWorldRevision",
    "dynamicWorldRevision",
    "requiredWaypointsMapMeters",
    "navigationEnvelopeRadiusMeters",
    "InvalidationReason",
):
    require(token in header, f"public route contract missing {token}")

require(
    "(void)current.dynamicWorldRevision;" in impl,
    "dynamic revision is no longer explicitly excluded from global-route invalidation",
)
require(
    "GeometricPathPlanner::plan" in impl,
    "Stage 1 is not using the shared geometric backend",
)
require(
    "maxConsideredObstacles = 0" in impl,
    "Stage-1 correctness again depends on an arbitrary obstacle-count cap",
)

calculate = function_slice(
    runtime,
    "ScenarioRunResult calculateScenario(",
    "ScenarioRunResult executeCalculatedRoute("
)
execute = function_slice(
    runtime,
    "ScenarioRunResult executeCalculatedRoute("
)
trajectory_builder = function_slice(
    runtime,
    "world::navigation::TrajectoryGenerationResult buildExecutionTrajectory(",
    "Program makeProgramPhase("
)

execute_compact = compact_cpp(execute)
trajectory_builder_compact = compact_cpp(trajectory_builder)

for required in (
    "NominalRoutePlanner::plan",
    "scenario.staticObstacles",
    "scenario.shipRoutePoints",
):
    require(required in calculate, f"Stage-1 calculate path missing {required}")

for forbidden in (
    "TrajectoryFollower",
    "TrajectoryGenerator",
    "SharedShipPhysics",
    "DynamicMotionSystem",
    "NavigationRuntimePlanner::plan",
    "makeShortProgram",
    "kReplanPeriodSeconds",
):
    require(
        forbidden not in calculate,
        f"Stage 1 leaked execution/replan ownership: {forbidden}",
    )

for required in (
    "executeCalculatedRoute",
    "buildExecutionTrajectory(",
    "buildRoutePrograms(",
    "activateProgramPhase(",
    "ManeuverPhaseGate::evaluate",
    "Follower::follow",
    "vehicle.bridge.step",
    "SharedShipPhysics::integrate",
    "DynamicMotionSystem::applySystemAccelerationDemand",
    "DynamicMotionSystem::updateLocalFrameMotion",
    "last_execution.log",
):
    require(
        compact_cpp(required) in execute_compact,
        f"Stage-2 execution path missing {required}",
    )

for required in (
    "calculatedRoute.routePoints",
    "TrajectoryGenerator::generate",
):
    require(
        compact_cpp(required) in trajectory_builder_compact,
        f"Stage-2 trajectory builder missing {required}",
    )

stage2_owned_text = trajectory_builder + "\n" + execute

for forbidden in (
    "NominalRoutePlanner::plan",
    "NavigationRuntimePlanner::plan",
    "GeometricPathPlanner::plan",
):
    require(
        forbidden not in stage2_owned_text,
        f"Stage 2 illegally rebuilds global route through {forbidden}",
    )

for required in (
    "loadScenarioPreview",
    "setSceneEndpoints",
    "last_route_plan.log",
    "routePlanningClearanceMeters",
    "characteristicTurnTimeSeconds",
    "effectiveStartSpeedMps",
    "effectiveFinishSpeedMps",
    "startAcceleration",
    "startPitchRateRadPerSec",
    "startYawRateRadPerSec",
    "startRollRateRadPerSec",
    "request.initialAccelerationMps2 = scenario.startAcceleration",
    '"route_ready"',
    '"static_route_ready"',
):
    require(required in runtime, f"diagnostic runtime missing {required}")

for marker in (
    "executeCalculatedRoute",
    "ScenarioRunSettings",
):
    require(marker in runtime_h, f"runtime public seam missing {marker}")

for marker in (
    "hasSceneEndpoints",
    "sceneStartMapMeters",
    "sceneFinishMapMeters",
):
    require(marker in trace_h, f"scene preview trace contract missing {marker}")

for marker in (
    "appendReferenceGrid",
    "UiAction::Calculate",
    "executionPerformed",
    "diagnosticLines",
    "retainedRoute",
    "retainedRouteDiagnostics",
    "routeInputsDirty",
    "executionInputsDirty",
    "recalculationRequired",
    "shortCalculationLog",
    "РАСЧЕТ ГОТОВ",
):
    require(marker in viewer, f"deterministic calculate/viewer workflow missing {marker}")

for required in (
    "EliteNavigationRouteToolCore",
    "EliteNavigationExecutionToolCore",
    "TrajectoryFollower.cpp",
    "ManeuverPhaseGate.cpp",
    "NavigationRuntimeControlBridge.cpp",
    "SharedShipPhysics.cpp",
    "DynamicMotionSystem.cpp",
    "TrajectoryGenerator.cpp",
    "EliteNavigationRuckig",
    "navigation_runtime_pipeline_tests",
    "NavigationScenarioRuntimeE2ETests.cpp",
):
    require(required in tool_cmake, f"viewer build missing {required}")

require(
    "NavigationRuntimePlanner.cpp" not in tool_cmake,
    "viewer execution target reintroduced periodic/global runtime planner ownership",
)

for marker in (
    '"acceleration"',
    '"pitch_rate_rad_s"',
    '"yaw_rate_rad_s"',
    '"roll_rate_rad_s"',
):
    require(marker in scenario_json, f"scenario initial kinematics missing {marker}")

for forbidden in (
    '"standard_speed_mps"',
    '"extreme_speed_mps"',
):
    require(
        forbidden not in scenario_json,
        f"flight style again owns a nominal speed through {forbidden}",
    )

for forbidden in (
    "styleSpeedMps",
    "scenario.standardSpeedMps",
    "scenario.extremeSpeedMps",
):
    require(
        forbidden not in runtime,
        f"runtime again maps flight style to nominal speed through {forbidden}",
    )

for required in (
    "styleReserveFactor",
    "FlightStyle::Extreme",
    "request.additionalRouteClearanceMeters",
):
    require(
        required in calculate or required in runtime,
        f"speed/style-aware route clearance contract missing {required}",
    )

for marker in (
    "initialAccelerationMps2",
):
    require(marker in trajectory_h, f"trajectory request missing {marker}")
    require(marker in trajectory_cpp, f"trajectory generator ignores {marker}")

require(
    "buildProgramChunks" not in runtime,
    "Stage 2 reintroduced raw consecutive-sample microchunking",
)
require(
    '"PROGRAM CHUNKS:"' not in runtime,
    "Stage-2 diagnostics still expose raw microchunks instead of physical phases",
)
for marker in (
    "buildRoutePrograms",
    "sampleNearestSourceProgress",
    "activateProgramPhase",
    '"PROGRAM PHASES: "',
    '"PHASE HANDOFFS: "',
):
    require(marker in runtime, f"route-leg execution phase contract missing {marker}")

for marker in (
    "PROGRAM_BEFORE_START",
    "FOLLOWER_OR_TRACKER_INVALID",
    "FOLLOWER FAIL PROGRAM",
):
    require(marker in execute, f"Follower boundary diagnostics missing {marker}")

for marker in (
    "testStaticWallProducesDetour",
    "testRequiredWaypointIsPreserved",
    "testDynamicRevisionDoesNotInvalidateNominalRoute",
    "dynamic-only revision incorrectly rebuilt global route",
):
    require(marker in test, f"Stage-1 regression missing {marker}")

e2e_test = read("tests/navigation_runtime/NavigationScenarioRuntimeE2ETests.cpp")
for marker in (
    "testSpeedAndStyleChangeStaticManeuverReserve",
    "higher speed did not move static detour farther from the wall",
    "EXTREME did not cut closer than STANDARD at the same speed",
    "ROUTE PLANNING SPEED: 40.00 M/S",
):
    require(marker in e2e_test, f"speed/style route regression missing {marker}")

for marker in (
    "Stage 1",
    "Stage 2",
    "dynamic-world revision",
    "swept-hull",
):
    require(marker in readme, f"documentation missing {marker}")

print("NAVIGATION STATIC ROUTE + TWO-STAGE EXECUTION CONTRACT: PASS")
print(" - Stage 1 builds the retained static start -> finish route with speed-aware maneuver clearance")
print(" - flight style changes clearance doctrine, never nominal speed")
print(" - dynamic revision cannot rebuild the nominal global route")
print(" - Stage 2 consumes cached route points and cannot invoke a global planner")
print(" - Stage 2 uses trajectory -> Follower -> pilot bridge -> authoritative physics")
print(" - exact swept-hull tunnel proof remains a later Stage-2 acceptance layer")
