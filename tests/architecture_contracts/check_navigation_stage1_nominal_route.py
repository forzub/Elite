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
tracking_h = read("src/game/navigation/ManeuverTrackingController.h")
tracking_cpp = read("src/game/navigation/ManeuverTrackingController.cpp")
vehicle_profile_h = read("src/game/navigation/VehicleDynamicsProfile.h")
ship_dynamics_h = read("src/game/ship/core/ShipDynamics.h")
vehicle_adapter_h = read("src/game/navigation/NavigationVehicleProfileAdapters.h")
capability_adapter_h = read("src/game/navigation/ManeuverCapabilityAdapters.h")

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
    "geometric.params = request.geometricPolicy;" in impl,
    "Stage-1 geometric policy is not passed through the nominal-route API",
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
    "bindProgramPageToExecutionClock(",
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
    "retainedRoute.pointsMapMeters",
    "request.policy = settings.trajectory",
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
    "follower.trackingErrorExceeded",
    "trackingLossSeconds",
    "PROGRAM_INVALIDATED_TRACKING_LOSS",
    "REFERENCE CLOCK: MONOTONIC",
    "STORAGE PAGE ADVANCES:",
    "bindProgramPageToExecutionClock",
):
    require(marker in runtime, f"continuous-program/root-log contract missing {marker}")

for forbidden in (
    "activeProgramReferenceDelaySeconds",
    "REFERENCE CLOCK HOLD FRAMES",
    "REFERENCE CLOCK HOLD: ",
    "follower_reacquiring",
    "activateProgramPhase(",
):
    require(
        forbidden not in runtime,
        f"obsolete frozen-reference/page-as-phase behavior returned: {forbidden}",
    )

require(
    "program.acceptedAtUniverseTimeSeconds +=" not in runtime,
    "runtime mutates accepted maneuver time instead of using one monotonic clock",
)

for marker in (
    "angularVelocityGainPerSecond = 3.00",
):
    require(marker in tracking_h, f"attitude damping regression missing {marker}")

for marker in (
    "envelopePositionErrorMeters",
    "effectivePositionError",
    "envelopeVelocityErrorMps",
    "effectiveVelocityError",
):
    require(marker in tracking_cpp, f"FreeTransit effective-envelope contract missing {marker}")

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

for marker in (
    "hasEffectiveRuntimeControlLaw",
    "effectiveRuntimeControlLaw",
    "УПРАВЛЕНИЕ / ВЫБРАНО",
    "УПРАВЛЕНИЕ / ФАКТ",
    "ПИЛОТ: ",
    "ПОВЕДЕНИЕ: ",
):
    require(marker in viewer, f"viewer current-mode diagnostics missing {marker}")

runtime_observer = function_slice(
    viewer,
    "case ViewerActionType::RuntimeControlLawObserved:",
    "void dispatchViewerAction("
)
require(
    "state.controlMode =" not in runtime_observer,
    "runtime playback is again overwriting the requested control-law selector",
)

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

for marker in (
    "buildRoutePrograms",
    "sampleNearestSourceProgress",
    "sequenceStartOffsetSeconds",
    "bindProgramPageToExecutionClock",
    '"PROGRAM STORAGE PAGES: "',
    '"STORAGE PAGE ADVANCES: "',
):
    require(marker in runtime, f"continuous maneuver storage-page contract missing {marker}")

for forbidden in (
    '"PROGRAM PHASES: "',
    '"PHASE HANDOFFS: "',
):
    require(
        forbidden not in runtime,
        f"storage pages are again being described as physical phases: {forbidden}",
    )

for marker in (
    "PROGRAM_PAGE_BEFORE_START",
    "FOLLOWER_OR_TRACKER_INVALID",
    "FOLLOWER FAIL PAGE",
):
    require(marker in execute, f"Follower/page boundary diagnostics missing {marker}")

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
    "testHighSpeedRunReacquiresInsteadOfOutrunningReference",
    "high-speed execution outran its physical ship instead of reacquiring",
):
    require(marker in e2e_test, f"high-speed execution regression missing {marker}")

for marker in (
    "Stage 1",
    "Stage 2",
    "dynamic-world revision",
    "swept-hull",
):
    require(marker in readme, f"documentation missing {marker}")

# Generic vehicle/policy API and single-source-of-truth checks.
for marker in (
    "VehicleDynamicsProfile",
    "bodyHalfExtentsMeters",
    "capabilityRevision",
    "conservativeCollisionRadiusMeters",
):
    require(marker in vehicle_profile_h, f"vehicle dynamics profile missing {marker}")

for marker in (
    "forwardMainAccelerationLimitMps2",
    "reverseMainAccelerationLimitMps2",
    "manoeuvreAccelerationLimitMps2",
    "maximumAngularSpeedRadPerSec",
    "angularAccelerationLimitRadPerSec2",
    "angularLoadRateLimitRadPerSec",
    "pitchRateLimitRadPerSec",
    "yawRateLimitRadPerSec",
    "rollRateLimitRadPerSec",
):
    require(marker in ship_dynamics_h, f"canonical ship-dynamics helper missing {marker}")

for marker in (
    "makeNavigationVehicleProfile",
    "forwardMainAccelerationLimitMps2",
    "reverseMainAccelerationLimitMps2",
):
    require(marker in vehicle_adapter_h, f"navigation vehicle projection missing {marker}")

for marker in (
    "makeManeuverCapabilitySnapshot",
    "reverseMainAccelerationLimitMps2",
):
    require(marker in capability_adapter_h, f"maneuver capability projection missing {marker}")

for marker in (
    "ScenarioNavigationPolicy",
    "ScenarioVehicleParameters",
    "ScenarioDefinition",
    "RetainedStaticRoute",
    "const ScenarioDefinition& scenario",
    "const ScenarioVehicleParameters& vehicle",
):
    require(marker in runtime_h, f"runtime API missing explicit input {marker}")

for forbidden in (
    "ShipParams cobraParams()",
    "kExecutionDt",
    "kTraceSampleSeconds",
    "kTrackingLossInvalidateSeconds",
    "std::filesystem::current_path()",
):
    require(
        forbidden not in runtime,
        f"runtime reintroduced hidden/hard-coded calculation state: {forbidden}",
    )

for marker in (
    "settings.navigation.executionDtSeconds",
    "settings.navigation.trackingLossInvalidateSeconds",
    "settings.navigation.terminalOrientationBlendDistanceMeters",
    "request.policy = settings.trajectory",
    "makeNavigationVehicleProfile",
    "makeManeuverCapabilitySnapshot",
):
    require(marker in runtime, f"runtime did not consume explicit common input {marker}")

require(
    "scenario.staticWorldRevision\n        );" not in runtime,
    "vehicle capability revision is again sourced from world-map revision",
)

print("NAVIGATION STATIC ROUTE + TWO-STAGE EXECUTION CONTRACT: PASS")
print(" - Stage 1 builds the retained static start -> finish route with speed-aware maneuver clearance")
print(" - flight style changes clearance doctrine, never nominal speed")
print(" - dynamic revision cannot rebuild the nominal global route")
print(" - Stage 2 consumes cached route points and cannot invoke a global planner")
print(" - Stage 2 uses trajectory -> Follower -> pilot bridge -> authoritative physics")
print(" - exact swept-hull tunnel proof remains a later Stage-2 acceptance layer")
