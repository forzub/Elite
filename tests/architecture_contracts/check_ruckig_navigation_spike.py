#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CMAKE = ROOT / "tests/navigation_ruckig/CMakeLists.txt"
HEADER = ROOT / "src/game/navigation/RuckigTrajectorySolver.h"
SOURCE = ROOT / "src/game/navigation/RuckigTrajectorySolver.cpp"
MAIN_CMAKE = ROOT / "CMakeLists.txt"
LICENSES = ROOT / "THIRD_PARTY_LICENSES.md"
LICENSE_TEXT = ROOT / "src/assets/licenses/RUCKIG-MIT.txt"

PIN = "a8db97a4e9c55e5160a3855f739fa3b270df8e4c"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")


cmake = CMAKE.read_text(encoding="utf-8")
header = HEADER.read_text(encoding="utf-8")
source = SOURCE.read_text(encoding="utf-8")
main_cmake = MAIN_CMAKE.read_text(encoding="utf-8")
licenses = LICENSES.read_text(encoding="utf-8")

require(PIN in cmake, "Ruckig FetchContent is not pinned to the reviewed commit")
for option in (
    "BUILD_EXAMPLES OFF",
    "BUILD_PYTHON_MODULE OFF",
    "BUILD_CLOUD_CLIENT OFF",
    "BUILD_TESTS OFF",
    "BUILD_BENCHMARK OFF",
    "BUILD_SHARED_LIBS OFF",
):
    require(option in cmake, f"missing isolated Ruckig option: {option}")

require("ruckig::ruckig" in cmake, "spike target does not link the upstream Ruckig target")
require("cxx_std_20" in cmake, "Ruckig adapter is not isolated behind its C++20 target")
require(
    "if(MINGW)" in cmake and
    "target_compile_definitions(ruckig PUBLIC _USE_MATH_DEFINES)" in cmake,
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
    "RuckigTrajectorySolver.cpp" not in main_cmake and "ruckig::ruckig" not in main_cmake,
    "isolated spike leaked into the production Elite build before acceptance",
)
require(PIN in licenses, "third-party registry does not record the exact reviewed Ruckig commit")
require(LICENSE_TEXT.exists(), "Ruckig MIT license text is missing")

print("RUCKIG NAVIGATION SPIKE: PASS")
print(f" - pinned community commit: {PIN}")
print(" - cloud client/examples/upstream tests/shared library disabled")
print(" - C++20/Ruckig headers remain private to isolated adapter target")
print(" - MinGW strict-C++ M_PI compatibility is target-local")
print(" - production Elite planner/build remains untouched until spike acceptance")
