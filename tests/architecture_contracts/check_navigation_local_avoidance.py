#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HEADER = (ROOT / "src/world/navigation/local/LocalAvoidancePlanner.h").read_text(encoding="utf-8")
IMPL = (ROOT / "src/world/navigation/local/LocalAvoidancePlanner.cpp").read_text(encoding="utf-8")
TEST = (ROOT / "tests/navigation_local/NavigationLocalAvoidanceTests.cpp").read_text(encoding="utf-8")
CMAKE = (ROOT / "src/world/navigation/local/CMakeLists.txt").read_text(encoding="utf-8")
TEST_CMAKE = (ROOT / "tests/navigation_local/CMakeLists.txt").read_text(encoding="utf-8")
README = (ROOT / "src/world/navigation/local/README.md").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")


for forbidden in (
    "glm/",
    "glad/",
    "GLFW/",
    "game/",
    "render/",
    "scene/",
    "TacticalCollisionMonitor",
    "SmallCraftNavigation",
    "SpaceState",
):
    require(forbidden not in HEADER,
            f"local avoidance public API leaks forbidden dependency: {forbidden}")
    require(forbidden not in IMPL,
            f"local avoidance implementation leaks forbidden dependency: {forbidden}")

for marker in (
    "class LocalAvoidancePlanner final",
    "LocalHorizonPlanner::Query",
    "NavigationMap::QueryResult",
    "NavigationStaticQueryApi",
    "const StaticQueries& staticQueries",
    "NominalClear",
    "AdjustedClear",
    "ConflictHold",
    "StaleHold",
    "StaticHold",
    "offsetCandidatesExamined",
    "lateralGridHalfExtentSamples",
    "staticRejected",
    "dynamicRejected",
    "startRegionId",
):
    require(marker in HEADER, f"local avoidance API marker missing: {marker}")

for marker in (
    # Visible-horizon bypass (9b87e385 / 58089c65): a bounded lateral offset
    # grid replaced the deflection/azimuth probe fan.
    "policy.lateralGridHalfExtentSamples",
    "staticAdditionalClearanceMeters",
    "staticQueries.queryPoint",
    "staticQueries.querySegment",
    "nominalStatic.spaceRevision != start.spaceRevision",
    "nominalStatic.sourceRevision != start.sourceRevision",
    "firstStatic.startRegionId != start.regionId",
    "horizonPlanner.evaluate",
    "TargetMode::PassThrough",
    "Status::AdjustedClear",
):
    require(marker in IMPL, f"local avoidance reference marker missing: {marker}")

require("LocalAvoidancePlanner.cpp" in CMAKE,
        "EliteNavigationLocal does not compile LocalAvoidancePlanner")
require("EliteNavigationSpace" in CMAKE,
        "local avoidance static query API must link the NavigationSpace owner backend")

require(
    "const NavigationSpace&" not in HEADER and
    "const NavigationSpace&" not in IMPL and
    "NavigationSpace::" not in HEADER and
    "NavigationSpace::" not in IMPL,
    "local avoidance calculation boundary must not receive or name the NavigationSpace state owner",
)
require("navigation_local_avoidance_tests" in TEST_CMAKE,
        "local avoidance behavioral test target missing")

for marker in (
    "testNominalClearPassesThroughWithoutOffsetSearch",
    "testCrossingObstacleProjectsToNormalPlaneAndFindsBypass",
    "testHeadOnObstacleCanBypassWithoutMandatoryStop",
    "testNarrowStaticRegionFailsClosedWhenNoOffsetFits",
    "testStaleDynamicResultSkipsOffsetSearch",
    "testNonTraversableStartFailsStaticHold",
    "NAVIGATION LOCAL VISIBLE-HORIZON BYPASS TESTS: PASS",
):
    require(marker in TEST, f"local avoidance fixture missing: {marker}")

for marker in (
    "lateral",
    "head_on",
    "portal-crossing",
    "fail closed",
):
    require(marker in README.lower(),
            f"local avoidance documentation missing: {marker}")

# Legacy CURRENT_TASK/CURRENT_STATE stage markers are no longer checked
# (PROJECT_STATE.md is the only living state file, AGENTS.md).

print("NAVIGATION LOCAL AVOIDANCE BOUNDARY CONTRACT: PASS")
print(" - adjusted-target search remains backend-neutral and bounded")
print(" - static state crosses the calculation boundary only through NavigationStaticQueryApi")
print(" - static endpoint evidence must come from one space/source revision")
print(" - lateral targets require same-region static free-space proof")
print(" - dynamic candidates are rechecked through the accepted LocalHorizonPlanner")
print(" - crossing and head-on blockers may bypass inside the visible horizon; no fit fails closed")
print(" - stale/static-unsafe inputs do not invent a bypass")
