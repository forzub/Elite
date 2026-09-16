#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TEST_CMAKE = ROOT / "tests/navigation_ruckig/CMakeLists.txt"
RUCKIG_CMAKE = ROOT / "cmake/EliteRuckigNavigation.cmake"
HEADER = ROOT / "src/game/navigation/RuckigTrajectorySolver.h"
SOURCE = ROOT / "src/game/navigation/RuckigTrajectorySolver.cpp"
LICENSES = ROOT / "THIRD_PARTY_LICENSES.md"
LICENSE_TEXT = ROOT / "src/assets/licenses/RUCKIG-MIT.txt"

PIN = "a8db97a4e9c55e5160a3855f739fa3b270df8e4c"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")


test_cmake = TEST_CMAKE.read_text(encoding="utf-8")
ruckig_cmake = RUCKIG_CMAKE.read_text(encoding="utf-8")
header = HEADER.read_text(encoding="utf-8")
source = SOURCE.read_text(encoding="utf-8")
licenses = LICENSES.read_text(encoding="utf-8")

require(PIN in ruckig_cmake, "Ruckig FetchContent is not pinned to the reviewed commit")
for option in (
    "BUILD_EXAMPLES",
    "BUILD_PYTHON_MODULE",
    "BUILD_CLOUD_CLIENT",
    "BUILD_TESTS",
    "BUILD_BENCHMARK",
    "BUILD_SHARED_LIBS",
):
    require(option in ruckig_cmake, f"missing Ruckig isolation option: {option}")
require("set(${_option} OFF CACHE BOOL \"\" FORCE)" in ruckig_cmake,
        "Ruckig dependency options are not forced OFF during configure")

require("EliteRuckigNavigation.cmake" in test_cmake,
        "spike test does not use the shared production Ruckig build seam")
require("EliteNavigationRuckig" in test_cmake,
        "spike test does not link the shared Ruckig adapter target")
require("ruckig::ruckig" in ruckig_cmake,
        "shared adapter target does not link upstream Ruckig")
require("cxx_std_20" in ruckig_cmake,
        "Ruckig adapter is not isolated behind its C++20 target")
require(
    "if(MINGW)" in ruckig_cmake and
    "target_compile_definitions(ruckig PUBLIC _USE_MATH_DEFINES)" in ruckig_cmake,
    "MinGW Ruckig M_PI portability shim is missing",
)
require("ruckig/" not in header, "public Elite adapter header leaks Ruckig headers")
require("#include <ruckig/ruckig.hpp>" in source, "private adapter implementation does not include Ruckig")

for seam in (
    "makeTerminalFrame",
    "GravityFieldSystem::sample",
    "input.minimum_duration",
    "maxProperAccelerationMps2",
    "maxProperJerkMps3",
    "trajectory.at_time",
):
    require(seam in source, f"Ruckig adapter lost required seam: {seam}")

require(
    "ScalarEnvelopeAxisScale" in source and
    "properAccelerationAxisBudget" in source and
    "properJerkAxisBudget" in source,
    "Ruckig per-axis box is not conservatively mapped from Elite scalar vector envelope",
)

require(PIN in licenses, "third-party registry does not record the exact reviewed Ruckig commit")
require(LICENSE_TEXT.exists(), "Ruckig MIT license text is missing")

print("RUCKIG NAVIGATION SPIKE: PASS")
print(f" - pinned community commit: {PIN}")
print(" - cloud client/examples/upstream tests/shared library disabled")
print(" - isolated tests reuse the production C++20 adapter target")
print(" - MinGW strict-C++ M_PI compatibility is target-local")
print(" - Elite scalar acceleration/jerk envelope is conservatively mapped to Ruckig axes")
