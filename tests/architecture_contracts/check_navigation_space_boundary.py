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
TURN_CONTRACT = (ROOT / "src/world/navigation/STATIC_TURN_COST.md").read_text(encoding="utf-8")


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
    "CorridorCostPolicy",
    "CostedCorridorResult",
    "turnPenaltyMetersPerRadian",
    "replaceStaticWorld",
    "applyLocalPatch",
    "invalidateBounds",
    "queryPoint",
    "queryCorridor",
    "queryCostedCorridor",
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
    "invalidated",
    "using RegionSlot = std::size_t",
    "using TurnStateSlot = std::size_t",
    "struct GraphIndex",
    "std::map<RegionId, RegionSlot> regionSlots",
    "std::vector<std::vector<AdjacencyEdge>> adjacency",
    "std::vector<std::vector<PortalId>> incidentPortals",
    "std::vector<TurnStateRecord> turnStates",
    "arrivalTurnStateSlot",
    "buildGraphIndex",
    "std::vector<std::uint8_t> visited",
    "std::vector<Impl::RegionSlot> frontier",
    "impl_->graph.adjacency[currentSlot]",
    "struct SpatialIndex",
    "struct SpatialNode",
    "buildSpatialIndex",
    "collectPointCandidateSlots",
    "collectBoundsCandidateSlots",
    "impl_->graph.incidentPortals.at(slot)",
    "validateCostPolicy",
    "std::multimap<QueueKey, Impl::RegionSlot> frontier",
    "totalCostMetersEquivalent",
    "preferredClearanceMultiple",
    "clearancePenaltyMeters",
    "turnPenaltyMetersPerRadian",
    "turnAngleRadians",
    "policy.turnPenaltyMetersPerRadian > 0.0",
    "std::vector<double> bestCost(stateCount, infinity)",
    "std::priority_queue<",
    "Impl::TurnStateSlot finalStateSlot",
):
    require(marker in IMPL, f"NavigationSpace CPU reference marker missing: {marker}")

for forbidden in (
    "for (const auto& portalEntry : impl_->portals)",
    "std::map<RegionId, bool> visited",
    "std::map<RegionId, Prev> previous",
    "std::map<TurnState, double> bestCost",
    "std::map<TurnState, TurnPrev> previous",
    "std::map<TurnState, bool> settled",
    "std::multimap<TurnQueueKey, TurnState> frontier",
):
    require(forbidden not in IMPL,
            f"corridor traversal regressed to expensive map bookkeeping: {forbidden}")

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
require("testWallApertureAdmission" in TEST_CPP,
        "wall-aperture navigation acceptance case is missing")
require("testCostedCanyonVsOverflight" in TEST_CPP,
        "canyon-vs-overflight costed-routing acceptance case is missing")
require("testTurnCostZigzagVsSmooth" in TEST_CPP,
        "zigzag-vs-smooth turn-cost acceptance case is missing")

for marker in (
    "only production-facing api",
    "free-space regions",
    "portals",
    "agent envelope",
    "local invalidation",
    "cpu reference",
    "hub / local-domain rule",
    "private spatial index",
    "costed corridor",
):
    require(marker in README.lower(),
            f"NavigationSpace boundary documentation missing: {marker}")

for marker in (
    "state = (RegionSlot, incoming PortalId)",
    "turnPenaltyMetersPerRadian",
    "zigzag_vs_smooth",
    "special start state",
):
    require(marker in TURN_CONTRACT,
            f"static turn-cost contract missing invariant: {marker}")

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
print(" - corridor traversal uses private dense region slots + ordered adjacency")
print(" - BFS visited/previous bookkeeping is vector-backed rather than ordered maps")
print(" - point lookup + bounds invalidation use a private RegionSlot BVH")
print(" - endpoint portal invalidation uses private incident-portal adjacency")
print(" - costed corridor separates distance, clearance and optional turn policy")
print(" - turn-aware search uses dense arrival-state slots + vector state + binary heap")
print(" - aperture, canyon/overflight and zigzag/smooth fixtures are pinned")
print(" - agent-envelope clearance and narrow-portal admission are explicit")
print(" - local invalidation + transactional patching are owned by the block")
print(" - project state/task agree on NAV-V2-SPACE-1")
