#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")


def require(condition: bool, message: str) -> None:
    if not condition:
        print(f"[FAIL] Stage-1 nominal route: {message}", file=sys.stderr)
        raise SystemExit(1)


header = read("src/game/navigation/NominalRoutePlanner.h")
impl = read("src/game/navigation/NominalRoutePlanner.cpp")
runtime = read("tools/navigation_runtime/NavigationScenarioRuntime.cpp")
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
    require(token in header, f"public contract missing {token}")

require(
    "(void)current.dynamicWorldRevision;" in impl,
    "dynamic revision is no longer explicitly excluded from global-route invalidation",
)
require(
    "GeometricPathPlanner::plan" in impl,
    "Stage-1 production seam is not using the shared geometric backend",
)
require(
    "maxConsideredObstacles = 0" in impl,
    "Stage-1 correctness again depends on an arbitrary obstacle-count cap",
)

for forbidden in (
    "TrajectoryFollower",
    "PilotSkillExecutor",
    "SharedShipPhysics",
    "DynamicMotionSystem",
    "NavigationRuntimePlanner::plan",
    "makeShortProgram",
    "kReplanPeriodSeconds",
):
    require(
        forbidden not in runtime,
        f"route-only Stage-1 runtime leaked execution/replan code: {forbidden}",
    )

for required in (
    "NominalRoutePlanner::plan",
    "scenario.staticObstacles",
    "scenario.shipRoutePoints",
    "loadScenarioPreview",
    "setSceneEndpoints",
    "last_route_plan.log",
    "FOLLOWER: NOT RUN (STAGE 1)",
    '"route_ready"',
    '"static_route_ready"',
):
    require(required in runtime, f"Stage-1 runtime missing {required}")

for marker in (
    "hasSceneEndpoints",
    "sceneStartMapMeters",
    "sceneFinishMapMeters",
):
    require(marker in trace_h, f"scene preview trace contract missing {marker}")

for marker in (
    "appendReferenceGrid",
    "ПОЛЁТ: ЭТАП 2",
    "FOLLOWER: OFF",
    "diagnosticLines",
):
    require(marker in viewer, f"Stage-1 viewer diagnostics missing {marker}")

for forbidden in (
    "TrajectoryFollower.cpp",
    "NavigationRuntimeControlBridge.cpp",
    "SharedShipPhysics.cpp",
    "DynamicMotionSystem.cpp",
    "NavigationRuntimePlanner.cpp",
):
    require(
        forbidden not in tool_cmake,
        f"Stage-1 viewer still links execution stack: {forbidden}",
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
    "does **not** invalidate/rebuild the route",
    "swept-hull",
):
    require(marker in readme, f"Stage-1 documentation missing {marker}")

print("NAVIGATION STAGE-1 NOMINAL ROUTE CONTRACT: PASS")
print(" - one static nominal route product owns start -> finish geometry")
print(" - dynamic revision cannot rebuild the nominal global route")
print(" - viewer Stage 1 contains no follower/pilot/physics/replan execution")
print(" - exact swept-hull tunnel proof remains a Stage-2 responsibility")
