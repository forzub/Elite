#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8")


def fail(message: str) -> None:
    print(f"[FAIL] headless EliteServer executable boundary: {message}", file=sys.stderr)
    raise SystemExit(1)


cmake = read("CMakeLists.txt")
server_main = read("src/server_main.cpp")
endpoints = read("src/game/server/HeadlessServerEndpoints.h")
ship_cpp = read("src/game/ship/Ship.cpp")
cobra_cpp = read("src/game/ship/descriptors/EliteCobraMk1.cpp")
run_all = read("tests/run_all_mingw64.sh")

for required in (
    'option(ELITE_BUILD_CLIENT',
    'option(ELITE_BUILD_SERVER',
    'if(ELITE_BUILD_CLIENT)',
    'add_executable(EliteServer',
    'src/server_main.cpp',
    'src/game/server/GameServer.cpp',
    'src/game/server/ServerRuntime.cpp',
    'src/game/simulation/GameSimulation.cpp',
):
    if required not in cmake:
        fail(f"CMake lost required headless target seam: {required}")

start = cmake.find("add_executable(EliteServer")
if start < 0:
    fail("EliteServer target not found")
end = cmake.find("endif()", start)
if end < 0:
    fail("could not isolate EliteServer target")
server_block = cmake[start:end]

for forbidden in (
    "src/render/",
    "src/ui/",
    "src/window/",
    "src/input/",
    "src/game/client/",
    "src/game/presentation/",
    "glad/",
    "GLFW",
    "opengl32",
    "Freetype",
    "webview",
):
    if forbidden in server_block:
        fail(f"EliteServer target regained client/render dependency: {forbidden}")

for forbidden in (
    '#include "src/input/Input.h"',
    '#include "src/render/',
    "GLFW",
):
    if forbidden in ship_cpp:
        fail(f"shared authoritative Ship.cpp still drags client/render code: {forbidden}")

if "target_compile_definitions(EliteGame PRIVATE ELITE_CLIENT_PRESENTATION=1)" not in cmake:
    fail("graphical client no longer explicitly opts into client presentation descriptor data")

if "src/game/ship/descriptors/EliteCobraMk1_Cockpit.cpp" in server_block:
    fail("EliteServer target links client-only Cobra cockpit geometry")

for required in (
    "#if defined(ELITE_CLIENT_PRESENTATION)",
    "cp.geometry = createCockpitGeometry(desc);",
    "desc.cockpit = cp;",
):
    if required not in cobra_cpp:
        fail(f"Cobra cockpit presentation seam missing: {required}")

for required in (
    "class HeadlessServerTransport final : public IServerTransport",
    "class HeadlessDebugChannel final : public game::debug::IServerDebugChannel",
    "publishSessionWelcomeImmediately",
    "publishSnapshotImmediately",
):
    if required not in endpoints:
        fail(f"headless process endpoint seam missing: {required}")

for required in (
    "game::server::ServerRuntime runtime",
    "--self-test",
    "runtime.advance(step)",
    "runtime.attachPlayerSessionTransport(",
    "headless-server boot + two-session authoritative routing smoke",
):
    if required not in server_main:
        fail(f"EliteServer main does not exercise real authoritative runtime: {required}")

for required in (
    "HEADLESS SERVER BUILD",
    "-DELITE_BUILD_CLIENT=OFF",
    "--target EliteServer",
    "./EliteServer.exe --self-test",
):
    if required not in run_all:
        fail(f"ready harness does not prove headless-only build: {required}")

print("[PASS] standalone EliteServer config/build boundary excludes client/render/UI dependencies")
