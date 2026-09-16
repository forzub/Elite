#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HEADER = (ROOT / "src/world/navigation/space/NavigationSpace.h").read_text(encoding="utf-8")
IMPL = (ROOT / "src/world/navigation/space/NavigationSpace.cpp").read_text(encoding="utf-8")
README = (ROOT / "src/world/navigation/space/README.md").read_text(encoding="utf-8")
SPACE_CMAKE = (ROOT / "src/world/navigation/space/CMakeLists.txt").read_text(encoding="utf-8")
TEST_CMAKE = (ROOT / "tests/navigation_space/CMakeLists.txt").read_text(encoding="utf-8")
TEST_CPP = (ROOT / "tests/navigation_space/NavigationSpaceContractTests.cpp").read_text(encoding="utf-8")
RUNNER = (ROOT / "tests/navigation_space/run_mingw64.sh").read_text(encoding="utf-8")
CURRENT_STATE = (ROOT / "CURRENT_STATE.md").read_text(encoding="utf-8")
CURRENT_TASK = (ROOT / "CURRENT_TASK.md").read_text(encoding="utf-8")
NAV_V2 = (ROOT / "NAVIGATION_WORLD_V2.md").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")


for forbidden in (
    "glad/",
    "GLFW/",
    "glm/",
    "game/",
    "render/",
    "scene/",
    "SpaceState",
    "ClientWorldState",
):
    require(forbidden not in HEADER,
            f"public NavigationSpace API leaks external dependency: {forbidden}")

for marker in (
    "class NavigationSpace final",
    "class Impl;",
    "std::unique_ptr<Impl>",
    "AgentEnvelope",
    "RegionInput",
    "PortalInput",
    "StaticSpaceUpdate",
    "LocalPatch",
    "replaceStaticWorld",
    "applyLocalPatch",
    "invalidateBounds",
    "queryPoint",
    "queryCorridor",
    "spaceRevision",
    "sourceRevision",
):
    require(marker in HEADER, f"NavigationSpace public API marker missing: {marker}")

for marker in (
    "std::map<RegionId",
    "std::map<PortalId",
    "requiredClearance",
    "pointClearance",
    "regionCapacity",
    "std::queue<RegionId>",
    "invalidated",
    "std::map<RegionId, std::vector<PortalId>> adjacency",
    "buildAdjacency",
    "impl_->adjacency.find(current)",
):
    require(marker in IMPL, f"NavigationSpace CPU reference marker missing: {marker}")

require("for (const auto& portalEntry : impl_->portals)" not in IMPL,
        "corridor BFS regressed to scanning the complete portal map per visited region")

for forbidden in (
    "#include <glad/",
    "#include <GLFW/",
    "#include <glm/",
    "#include \"game/",
    "#include \"render/",
    "#include \"scene/",
):
    require(forbidden not in IMPL,
            f"NavigationSpace implementation directly depends on forbidden runtime layer: {forbidden}")

require("EliteNavigationSpace" in SPACE_CMAKE,
        "NavigationSpace does not define its own library target")
require("add_subdirectory" in TEST_CMAKE and "EliteNavigationSpace" in TEST_CMAKE,
        "NavigationSpace behavioral test does not build through its own CMake boundary")
require("ELITE_TEST_BUILD_ROOT" in RUNNER,
        "NavigationSpace runner does not use canonical test build layout")
require("NAVIGATION SPACE CONTRACT TESTS: PASS" in TEST_CPP,
        "NavigationSpace behavioral contract executable missing")

for marker in (
    "only production-facing api",
    "free-space regions",
    "portals",
    "agent envelope",
    "local invalidation",
    "cpu reference",
    "hub / local-domain rule",
):
    require(marker in README.lower(),
            f"NavigationSpace boundary documentation missing: {marker}")

require("NAV-V2-SPACE-1" in CURRENT_TASK,
        "CURRENT_TASK is not advanced to NAV-V2-SPACE-1")
require("NAV-V2-SPACE-1" in CURRENT_STATE,
        "CURRENT_STATE does not record NAV-V2-SPACE-1")
require("NAV-V2-SPACE-1" in NAV_V2 and "Hybrid backend contract" in NAV_V2,
        "NavigationWorld v2 contract does not record static-space stage/hybrid ownership")

print("NAVIGATION SPACE BOUNDARY CONTRACT: PASS")
print(" - one backend-neutral API owns persistent static navigation topology")
print(" - public header is isolated from GLM/OpenGL/game/render state")
print(" - CPU reference uses deterministic free-space regions + portals")
print(" - corridor traversal uses private per-region portal adjacency")
print(" - agent-envelope clearance and narrow-portal admission are explicit")
print(" - local invalidation + transactional patching are owned by the block")
print(" - project state/task agree on NAV-V2-SPACE-1")
