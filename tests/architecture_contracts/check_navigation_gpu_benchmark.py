#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BENCH = (ROOT / "benchmarks/navigation_gpu/main.cpp").read_text(encoding="utf-8")
BENCH_CMAKE = (ROOT / "benchmarks/navigation_gpu/CMakeLists.txt").read_text(encoding="utf-8")
BENCH_README = (ROOT / "benchmarks/navigation_gpu/README.md").read_text(encoding="utf-8")
RUNNER = (ROOT / "benchmarks/navigation_gpu/run_mingw64.sh").read_text(encoding="utf-8")
CURRENT_STATE = (ROOT / "CURRENT_STATE.md").read_text(encoding="utf-8")
CURRENT_TASK = (ROOT / "CURRENT_TASK.md").read_text(encoding="utf-8")
ARCH = (ROOT / "src/world/navigation/NAVIGATION_PLANNING_ARCHITECTURE.md").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")


for marker in (
    "GL_COMPUTE_SHADER",
    "glDispatchCompute",
    "GL_SHADER_STORAGE_BARRIER_BIT",
    "glQueryCounter",
    "positionRadius",
    "velocity",
    "acceleration",
    "travelBound",
    "cellCounts",
    "cellSlots",
    "candidatePairCount",
    "corridorActorCount",
    "overflowCount",
    "outOfBoundsCount",
    "readbackBytes",
    "1000, 5000, 10000",
):
    require(marker in BENCH, f"GPU benchmark marker missing: {marker}")

require("OpenGL 4.3+ is required" in BENCH,
        "benchmark does not fail closed below the production compute baseline")
require("referenceOk" in BENCH and "computeReference" in BENCH,
        "benchmark lacks the CPU correctness reference")
require("sizeof(GpuStats)" in BENCH and "32" in BENCH,
        "aggregate readback contract is not pinned")
require("navigation_gpu_benchmark" in BENCH_CMAKE,
        "isolated benchmark CMake target missing")
require("glad/src/gl.c" in BENCH_CMAKE and "find_package(glfw3 CONFIG REQUIRED)" in BENCH_CMAKE,
        "benchmark is not using the repository OpenGL/GLFW stack")
require("${ELITE_ROOT}/glad/include" in BENCH_CMAKE,
        "benchmark does not expose the GLAD 2 include root containing glad/gl.h")
require('"${ELITE_ROOT}/glad"' not in BENCH_CMAKE,
        "benchmark still uses the obsolete pre-GLAD-2 include root")
require("ELITE_TEST_BUILD_ROOT" in RUNNER,
        "benchmark runner does not use the canonical test build layout")

for marker in (
    "ship-centered",
    "Dynamic actor",
    "corridor",
    "readback",
    "collision",
    "Ruckig",
):
    require(marker.lower() in BENCH_README.lower(),
            f"benchmark design documentation missing: {marker}")

require("NAV-V2-MAP-2" in CURRENT_TASK,
        "CURRENT_TASK is not at the active CPU/GPU NavigationMap measurement gate")
require("NAV-V2-MAP-2" in CURRENT_STATE,
        "CURRENT_STATE does not record the active CPU/GPU NavigationMap measurement gate")
require("ship-centered NavigationWorld" in ARCH,
        "navigation architecture lacks the ship-centered NavigationWorld authority")
require("Hub-local" in ARCH,
        "navigation architecture does not preserve Hub-local ownership")
require("GPU" in ARCH and "spatial" in ARCH.lower(),
        "navigation architecture lacks GPU spatial broadphase/prediction")
require("Collision" in ARCH and "Damage" in ARCH,
        "navigation architecture does not separate navigation/collision/damage")

print("NAVIGATION GPU BENCHMARK CONTRACT: PASS")
print(" - isolated OpenGL 4.3 compute benchmark exists")
print(" - GLAD 2 include root is pinned to glad/include")
print(" - 1k/5k/10k ship-centered actor scales are pinned")
print(" - P/V/A prediction, spatial bins, corridor filtering and pair queries are measured")
print(" - overflow/out-of-bounds and 1k CPU-reference correctness are fail-visible")
print(" - project state/task agree on NAV-V2-MAP-2 measurement gate")
