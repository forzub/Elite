#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MAIN_CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
RUCKIG_CMAKE = (ROOT / "cmake/EliteRuckigNavigation.cmake").read_text(encoding="utf-8")
PLANNER_H = (ROOT / "src/game/navigation/LocalGuidancePlanner.h").read_text(encoding="utf-8")
PLANNER_CPP = (ROOT / "src/game/navigation/LocalGuidancePlanner.cpp").read_text(encoding="utf-8")
GUIDANCE_CMAKE = (ROOT / "tests/navigation_guidance/CMakeLists.txt").read_text(encoding="utf-8")
LICENSES = (ROOT / "THIRD_PARTY_LICENSES.md").read_text(encoding="utf-8")

PIN = "a8db97a4e9c55e5160a3855f739fa3b270df8e4c"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")


require(PIN in RUCKIG_CMAKE, "runtime Ruckig dependency is not pinned to reviewed commit")
for option in (
    "BUILD_EXAMPLES",
    "BUILD_PYTHON_MODULE",
    "BUILD_CLOUD_CLIENT",
    "BUILD_TESTS",
    "BUILD_BENCHMARK",
    "BUILD_SHARED_LIBS",
):
    require(option in RUCKIG_CMAKE, f"runtime Ruckig build lost isolation option: {option}")
require("set(${_option} OFF CACHE BOOL \"\" FORCE)" in RUCKIG_CMAKE,
        "Ruckig dependency options are not forced OFF during dependency configure")
require("_ELITE_RUCKIG_OLD_${_option}" in RUCKIG_CMAKE and
        "unset(${_option} CACHE)" in RUCKIG_CMAKE,
        "Ruckig generic cache options are not restored after dependency configure")

require("target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)" in RUCKIG_CMAKE,
        "Ruckig C++20 boundary is not private")
require("if(MINGW)" in RUCKIG_CMAKE and
        "target_compile_definitions(ruckig PUBLIC _USE_MATH_DEFINES)" in RUCKIG_CMAKE,
        "MinGW Ruckig portability shim is missing")
require("EliteRuckigNavigation.cmake" in MAIN_CMAKE,
        "main runtime build does not include shared Ruckig build seam")
require(MAIN_CMAKE.count("EliteNavigationRuckig") >= 3,
        "Ruckig runtime target is not created and linked to client/server")
require("EliteNavigationRuckig" in GUIDANCE_CMAKE,
        "navigation guidance regression suite does not exercise production Ruckig seam")

for marker in (
    '#include "src/game/navigation/RuckigTrajectorySolver.h"',
    "RuckigTrajectorySolver::solve",
    "predictLegacyLeg",
    "ruckigLegAttempts",
    "ruckigLegSuccesses",
    "ruckigFallbacks",
    "legacyPredictorCalls",
    "ruckigSolveMicroseconds",
    "legacyFallbackMicroseconds",
):
    require(marker in PLANNER_CPP or marker in PLANNER_H,
            f"Ruckig-first/fallback diagnostic seam missing: {marker}")

require(
    PLANNER_CPP.find("RuckigTrajectorySolver::solve") <
    PLANNER_CPP.find("auto fallback = predictLegacyLeg"),
    "legacy predictor is no longer a fallback behind the Ruckig attempt",
)
require("Runtime dependency — Ruckig" in LICENSES and PIN in LICENSES,
        "third-party registry does not describe production Ruckig dependency")

print("RUCKIG NAVIGATION INTEGRATION: PASS")
print(" - Ruckig-first state-to-state leg generation")
print(" - legacy shooting predictor retained as deterministic fallback")
print(" - per-plan attempts/success/fallback/timing diagnostics exposed")
print(" - generic upstream cache options restored after Ruckig configure")
print(" - client/server and guidance tests share the pinned C++20 adapter target")
