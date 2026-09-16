#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "${ROOT_DIR}/tests/helpers/build_layout.sh"
BUILD_DIR="${ELITE_TEST_BUILD_ROOT}/navigation_guidance"
elite_require_build_toolchain

cmake \
    -S "${ROOT_DIR}/tests/navigation_guidance" \
    -B "${BUILD_DIR}" \
    -G Ninja

cmake --build "${BUILD_DIR}" --target \
    ruckig_route_planner_tests \
    guidance_tunnel_local_horizon_tests

ctest \
    --test-dir "${BUILD_DIR}" \
    --output-on-failure \
    -R '^(ruckig_route_planner|guidance_tunnel_local_horizon)$'
