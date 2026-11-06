#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HEADER = (ROOT / "src/world/navigation/map/NavigationMap.h").read_text(encoding="utf-8")
IMPL = (ROOT / "src/world/navigation/map/NavigationMap.cpp").read_text(encoding="utf-8")
README = (ROOT / "src/world/navigation/map/README.md").read_text(encoding="utf-8")
MAP_CMAKE = (ROOT / "src/world/navigation/map/CMakeLists.txt").read_text(encoding="utf-8")
TEST_CMAKE = (ROOT / "tests/navigation_map/CMakeLists.txt").read_text(encoding="utf-8")
TEST_CPP = (ROOT / "tests/navigation_map/NavigationMapContractTests.cpp").read_text(encoding="utf-8")
FRAME_BOUNDARY = (ROOT / "src/game/navigation/NavigationFrameBoundary.h").read_text(encoding="utf-8")
ARCH = (ROOT / "src/world/navigation/NAVIGATION_PLANNING_ARCHITECTURE.md").read_text(encoding="utf-8")


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
    # Only #include lines count: comments may name the game/navigation adapter.
    include_lines = "\n".join(
        line for line in HEADER.splitlines() if line.lstrip().startswith("#include")
    )
    require(forbidden not in include_lines,
            f"public NavigationMap API leaks external dependency: {forbidden}")

for marker in (
    "class NavigationMap final",
    "class Impl;",
    "std::unique_ptr<Impl>",
    "DynamicWorldUpdate",
    "replaceDynamicWorld",
    "queryCorridor",
    "querySphere",
    "sourceRevision",
    "mapRevision",
):
    require(marker in HEADER, f"NavigationMap public API marker missing: {marker}")

require("std::unordered_map<CellCoord" in IMPL,
        "CPU reference does not own its sparse spatial index internally")
# NavigationMap became NavLocal-only (33d1830a): system/world -> NavLocal
# conversion is owned by the game-side NavigationFrameBoundary adapter.
require("pointToMap" not in IMPL and "WorkingFrame" not in HEADER,
        "NavigationMap regained its own working-frame conversion")
require("NavLocal-only" in HEADER and "NavLocal" in FRAME_BOUNDARY,
        "NavLocal frame conversion is no longer owned by NavigationFrameBoundary")
require("conservativeSweptRadiusMeters" in IMPL,
        "NavigationMap does not own conservative dynamic prediction")
require("cells.find" in IMPL,
        "NavigationMap query path is not using its internal spatial index")

for forbidden in (
    "#include <glad/",
    "#include <GLFW/",
    "#include <glm/",
    "#include \"game/",
    "#include \"render/",
    "#include \"scene/",
):
    require(forbidden not in IMPL,
            f"NavigationMap implementation directly depends on forbidden runtime layer: {forbidden}")

require("EliteNavigationMap" in MAP_CMAKE,
        "NavigationMap does not define its own library target")
require("add_subdirectory" in TEST_CMAKE and "EliteNavigationMap" in TEST_CMAKE,
        "NavigationMap behavioral test does not build the block through its own CMake boundary")
require("NAVIGATION MAP CONTRACT TESTS: PASS" in TEST_CPP,
        "NavigationMap behavioral contract executable missing")

for marker in (
    "only production-facing API",
    "does not retain references",
    "CPU reference backend",
    "GPU backend",
    "Hub remains Hub-local",
):
    require(marker.lower() in README.lower(),
            f"NavigationMap boundary documentation missing: {marker}")

# Legacy CURRENT_TASK/CURRENT_STATE stage markers are no longer checked
# (PROJECT_STATE.md is the only living state file, AGENTS.md).
require("## NavigationMap boundary" in ARCH,
        "navigation architecture does not define the NavigationMap block boundary")

print("NAVIGATION MAP BOUNDARY CONTRACT: PASS")
print(" - one public API owns the ship-centered working set")
print(" - spatial cells/prediction/backend state remain private")
print(" - callers publish snapshots by value and receive compact query products by value")
print(" - CPU reference is isolated from game/render/OpenGL dependencies")
print(" - GPU backend can replace internals without changing planner call sites")
