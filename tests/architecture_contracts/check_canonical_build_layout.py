#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]


def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8")


def fail(message: str) -> None:
    print(f"[FAIL] canonical build layout: {message}", file=sys.stderr)
    raise SystemExit(1)


cmake = read("CMakeLists.txt")
layout = read("tests/helpers/build_layout.sh")
toolchain = read("tests/helpers/toolchain.sh")
run_all = read("tests/run_all_mingw64.sh")
network = read("tests/network_process_acceptance/run_mingw64.sh")
runtime = read("tests/runtime_standalone/run_mingw64.sh")
build_script = read("build_mingw64.sh")
cleanup_script = read("clean_build_layout_mingw64.sh")

if 'option(ELITE_BUILD_SERVER "Build the headless EliteServer executable" OFF)' not in cmake:
    fail("default client configure must not create a second EliteServer in build/")

for token in (
    'ELITE_CLIENT_BUILD_DIR="${ROOT_DIR}/build"',
    'ELITE_SERVER_BUILD_DIR="${ROOT_DIR}/build/headless_server"',
    'ELITE_TEST_BUILD_ROOT="${ROOT_DIR}/build/tests"',
    'ELITE_TEST_LOG_DIR="${ROOT_DIR}/build/test-logs"',
    'elite_cleanup_legacy_build_layout()',
    'elite_build_canonical_client()',
    'elite_build_canonical_server()',
    'elite_assert_unique_runtime_binaries()',
    '"${ROOT_DIR}/build/EliteServer.exe"',
):
    if token not in layout:
        fail(f"shared build-layout helper lost required invariant: {token}")

for token in (
    'elite_resolve_python()',
    'elite_require_python()',
    'elite_require_build_toolchain()',
    'elite_require_ready_toolchain()',
    'A Windows Store/App Execution Alias',
):
    if token not in toolchain:
        fail(f"shared toolchain helper lost required invariant: {token}")

if 'elite_require_ready_toolchain' not in run_all:
    fail("ready runner must fail once at toolchain preflight before subsystem tests")
if 'elite_require_ready_toolchain' not in build_script:
    fail("canonical developer build must validate its toolchain before cleanup/build")

for token in (
    'elite_cleanup_legacy_build_layout',
    'elite_build_canonical_server',
    'elite_build_canonical_client',
    'elite_assert_unique_runtime_binaries',
    'ELITE_CANONICAL_BUILDS_READY=1',
):
    if token not in run_all:
        fail(f"ready harness does not enforce canonical runtime binaries: {token}")

for text, name in ((network, "network process acceptance"), (runtime, "standalone runtime")):
    for token in (
        'SERVER_DIR="${ELITE_SERVER_BUILD_DIR}"',
        'CLIENT_DIR="${ELITE_CLIENT_BUILD_DIR}"',
        'elite_require_canonical_runtime_binaries',
    ):
        if token not in text:
            fail(f"{name} lost canonical runtime path: {token}")

for forbidden in (
    "ELITE_TEST_SERVER_DIR",
    "ELITE_TEST_CLIENT_DIR",
    "ready_headless_server",
    "network_process_server\"",
):
    if forbidden in run_all + network + runtime:
        fail(f"runtime acceptance regained alternate executable path: {forbidden}")

for token in (
    'elite_resolve_python()',
    'elite_require_python()',
    'elite_require_build_toolchain()',
    'elite_require_ready_toolchain()',
    'A Windows Store/App Execution Alias',
):
    if token not in toolchain:
        fail(f"shared toolchain helper lost required invariant: {token}")

if 'elite_require_ready_toolchain' not in run_all:
    fail("ready runner must fail once at toolchain preflight before subsystem tests")
if 'elite_require_ready_toolchain' not in build_script:
    fail("canonical developer build must validate its toolchain before cleanup/build")

for token in (
    'elite_cleanup_legacy_build_layout',
    'elite_build_canonical_runtime',
    'elite_require_canonical_runtime_binaries',
):
    if token not in build_script:
        fail(f"developer build entry point missing canonical step: {token}")

if 'elite_cleanup_legacy_build_layout' not in cleanup_script:
    fail("explicit one-time cleanup entry point is missing")

shell_files = list((ROOT / "tests").rglob("*.sh"))
legacy_path_patterns = (
    r"build/ready_headless_server",
    r"build/network_process_server(?:/|\")",
    r"build/architecture_contract_tests",
    r"build/feature_contract_tests",
    r"build/interaction_activation_tests",
    r"build/presentation_pipeline_tests",
    r"build/system_map_behavior_tests",
    r"build/world_runtime_tests",
)
for path in shell_files:
    rel = path.relative_to(ROOT)
    if rel == Path("tests/helpers/build_layout.sh"):
        # The cleanup helper is the one intentional place that names legacy
        # generated directories so it can delete them.
        continue
    text = path.read_text(encoding="utf-8")
    for pattern in legacy_path_patterns:
        if re.search(pattern, text):
            fail(f"legacy build directory still referenced by {rel}: {pattern}")

for rel in (
    "tests/architecture_contracts/run_mingw64.sh",
    "tests/feature_contracts/run_mingw64.sh",
    "tests/interaction_activation/run_mingw64.sh",
    "tests/presentation_pipeline/run_mingw64.sh",
    "tests/system_map/run_mingw64.sh",
    "tests/world_runtime/run_mingw64.sh",
    "tests/world_runtime/run_clock_sync.sh",
):
    text = read(rel)
    if 'ELITE_TEST_BUILD_ROOT' not in text:
        fail(f"test build is not namespaced under build/tests: {rel}")

print("[PASS] canonical build layout prevents alternate/stale runtime binaries and namespaces test scratch builds")
