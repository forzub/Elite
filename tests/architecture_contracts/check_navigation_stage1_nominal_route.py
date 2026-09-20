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


header = read("src/game/navigation/NominalRoutePlanner.h")
impl = read("src/game/navigation/NominalRoutePlanner.cpp")
runtime = read("tools/navigation_runtime/NavigationScenarioRuntime.cpp")
runtime_h = read("tools/navigation_runtime/NavigationScenarioRuntime.h")
viewer = read("tools/navigation_runtime/NavigationRuntimeViewer.cpp")
trace_h = read("tools/navigation_runtime/NavigationTrace.h")
tool_cmake = read("tools/navigation_runtime/CMakeLists.txt")
test = read("tests/navigation_runtime/NominalRoutePlannerTests.cpp")
readme = read("tools/navigation_runtime/README.md")

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
    "Program makeProgramChunk("
)

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
    "Follower::follow",
    "vehicle.bridge.step",
    "SharedShipPhysics::integrate",
    "DynamicMotionSystem::applySystemAccelerationDemand",
    "DynamicMotionSystem::updateLocalFrameMotion",
    "last_execution.log",
):
    require(required in execute, f"Stage-2 execution path missing {required}")

for required in (
    "calculatedRoute.routePoints",
    "TrajectoryGenerator::generate",
):
    require(
        required in trajectory_builder,
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
    "ЗАПУСТИТЬ ПОЛЁТ",
    "UiAction::Execute",
    "executionPerformed",
    "diagnosticLines",
    "retainedRoute",
    "retainedRouteDiagnostics",
    "restoreRetainedRouteForNewExecutionSettings",
):
    require(marker in viewer, f"two-stage viewer workflow missing {marker}")

for required in (
    "EliteNavigationRouteToolCore",
    "EliteNavigationExecutionToolCore",
    "TrajectoryFollower.cpp",
    "NavigationRuntimeControlBridge.cpp",
    "SharedShipPhysics.cpp",
    "DynamicMotionSystem.cpp",
    "TrajectoryGenerator.cpp",
    "EliteNavigationRuckig",
):
    require(required in tool_cmake, f"viewer build missing {required}")

require(
    "NavigationRuntimePlanner.cpp" not in tool_cmake,
    "viewer execution target reintroduced periodic/global runtime planner ownership",
)

for marker in (
    "testStaticWallProducesDetour",
    "testRequiredWaypointIsPreserved",
    "testDynamicRevisionDoesNotInvalidateNominalRoute",
    "dynamic-only revision incorrectly rebuilt global route",
):
    require(marker in test, f"Stage-1 regression missing {marker}")

for marker in (
    "Stage 1",
    "Stage 2",
    "dynamic-world revision",
    "swept-hull",
):
    require(marker in readme, f"documentation missing {marker}")

print("NAVIGATION STATIC ROUTE + TWO-STAGE EXECUTION CONTRACT: PASS")
print(" - Stage 1 alone builds the retained static start -> finish route")
print(" - dynamic revision cannot rebuild the nominal global route")
print(" - Stage 2 consumes cached route points and cannot invoke a global planner")
print(" - Stage 2 uses trajectory -> Follower -> pilot bridge -> authoritative physics")
print(" - exact swept-hull tunnel proof remains a later Stage-2 acceptance layer")
