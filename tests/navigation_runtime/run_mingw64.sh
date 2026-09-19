#!/usr/bin/env bash
set -uo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "${ROOT_DIR}/tests/helpers/build_layout.sh"
elite_require_build_toolchain

BUILD_DIR="${ELITE_TEST_BUILD_ROOT}/navigation_runtime"

now_ms() {
    date +%s%3N
}

elapsed_ms() {
    local start_ms="$1"
    local end_ms="$2"
    echo $((end_ms - start_ms))
}

TOTAL_START_MS="$(now_ms)"
CONFIGURE_MS="skipped"
BUILD_MS="skipped"
TESTS_MS="skipped"
SCHEDULER_DIAGNOSTIC_MS="skipped"
B5_DIAGNOSTIC_MS="skipped"
EXECUTION_LAB_DIAGNOSTIC_MS="skipped"
CORRIDOR_MATRIX_DIAGNOSTIC_MS="skipped"
RIGID_BODY_CORRIDOR_DIAGNOSTIC_MS="skipped"
CORNER_FAMILY_DIAGNOSTIC_MS="skipped"
CONFIGURE_RC=0
BUILD_RC=0
TESTS_RC=0
SCHEDULER_DIAGNOSTIC_RC=0
B5_DIAGNOSTIC_RC=0
EXECUTION_LAB_DIAGNOSTIC_RC=0
CORRIDOR_MATRIX_DIAGNOSTIC_RC=0
RIGID_BODY_CORRIDOR_DIAGNOSTIC_RC=0
CORNER_FAMILY_DIAGNOSTIC_RC=0

CONFIGURE_START_MS="$(now_ms)"
cmake \
    -S "${ROOT_DIR}/tests/navigation_runtime" \
    -B "${BUILD_DIR}" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release
CONFIGURE_RC=$?
CONFIGURE_END_MS="$(now_ms)"
CONFIGURE_MS="$(elapsed_ms "${CONFIGURE_START_MS}" "${CONFIGURE_END_MS}")"

if [[ "${CONFIGURE_RC}" -eq 0 ]]; then
    BUILD_START_MS="$(now_ms)"
    cmake --build "${BUILD_DIR}"
    BUILD_RC=$?
    BUILD_END_MS="$(now_ms)"
    BUILD_MS="$(elapsed_ms "${BUILD_START_MS}" "${BUILD_END_MS}")"
fi

if [[ "${CONFIGURE_RC}" -eq 0 && "${BUILD_RC}" -eq 0 ]]; then
    TEST_START_MS="$(now_ms)"
    ctest --test-dir "${BUILD_DIR}" --output-on-failure
    TESTS_RC=$?

    if [[ "${TESTS_RC}" -eq 0 ]]; then
        SCHEDULER_DIAGNOSTIC_START_MS="$(now_ms)"
        ctest \
            --test-dir "${BUILD_DIR}" \
            -R navigation_work_scheduler \
            -V
        SCHEDULER_DIAGNOSTIC_RC=$?
        SCHEDULER_DIAGNOSTIC_END_MS="$(now_ms)"
        SCHEDULER_DIAGNOSTIC_MS="$(elapsed_ms "${SCHEDULER_DIAGNOSTIC_START_MS}" "${SCHEDULER_DIAGNOSTIC_END_MS}")"

        if [[ "${SCHEDULER_DIAGNOSTIC_RC}" -ne 0 ]]; then
            TESTS_RC="${SCHEDULER_DIAGNOSTIC_RC}"
        fi

        B5_DIAGNOSTIC_START_MS="$(now_ms)"
        ctest \
            --test-dir "${BUILD_DIR}" \
            -R ordinary_physical_maneuver_compiler \
            -V
        B5_DIAGNOSTIC_RC=$?
        B5_DIAGNOSTIC_END_MS="$(now_ms)"
        B5_DIAGNOSTIC_MS="$(elapsed_ms "${B5_DIAGNOSTIC_START_MS}" "${B5_DIAGNOSTIC_END_MS}")"

        if [[ "${B5_DIAGNOSTIC_RC}" -ne 0 ]]; then
            TESTS_RC="${B5_DIAGNOSTIC_RC}"
        fi

        EXECUTION_LAB_DIAGNOSTIC_START_MS="$(now_ms)"
        ctest \
            --test-dir "${BUILD_DIR}" \
            -R maneuver_program_execution_lab \
            -V
        EXECUTION_LAB_DIAGNOSTIC_RC=$?
        EXECUTION_LAB_DIAGNOSTIC_END_MS="$(now_ms)"
        EXECUTION_LAB_DIAGNOSTIC_MS="$(elapsed_ms "${EXECUTION_LAB_DIAGNOSTIC_START_MS}" "${EXECUTION_LAB_DIAGNOSTIC_END_MS}")"

        if [[ "${EXECUTION_LAB_DIAGNOSTIC_RC}" -ne 0 ]]; then
            TESTS_RC="${EXECUTION_LAB_DIAGNOSTIC_RC}"
        fi

        CORRIDOR_MATRIX_DIAGNOSTIC_START_MS="$(now_ms)"
        ctest \
            --test-dir "${BUILD_DIR}" \
            -R maneuver_corridor_matrix \
            -V
        CORRIDOR_MATRIX_DIAGNOSTIC_RC=$?
        CORRIDOR_MATRIX_DIAGNOSTIC_END_MS="$(now_ms)"
        CORRIDOR_MATRIX_DIAGNOSTIC_MS="$(elapsed_ms "${CORRIDOR_MATRIX_DIAGNOSTIC_START_MS}" "${CORRIDOR_MATRIX_DIAGNOSTIC_END_MS}")"

        if [[ "${CORRIDOR_MATRIX_DIAGNOSTIC_RC}" -ne 0 ]]; then
            TESTS_RC="${CORRIDOR_MATRIX_DIAGNOSTIC_RC}"
        fi

        RIGID_BODY_CORRIDOR_DIAGNOSTIC_START_MS="$(now_ms)"
        ctest \
            --test-dir "${BUILD_DIR}" \
            -R maneuver_rigid_body_corridor \
            -V
        RIGID_BODY_CORRIDOR_DIAGNOSTIC_RC=$?
        RIGID_BODY_CORRIDOR_DIAGNOSTIC_END_MS="$(now_ms)"
        RIGID_BODY_CORRIDOR_DIAGNOSTIC_MS="$(elapsed_ms "${RIGID_BODY_CORRIDOR_DIAGNOSTIC_START_MS}" "${RIGID_BODY_CORRIDOR_DIAGNOSTIC_END_MS}")"

        if [[ "${RIGID_BODY_CORRIDOR_DIAGNOSTIC_RC}" -ne 0 ]]; then
            TESTS_RC="${RIGID_BODY_CORRIDOR_DIAGNOSTIC_RC}"
        fi

        CORNER_FAMILY_DIAGNOSTIC_START_MS="$(now_ms)"
        ctest \
            --test-dir "${BUILD_DIR}" \
            -R maneuver_corner_family_matrix \
            -V
        CORNER_FAMILY_DIAGNOSTIC_RC=$?
        CORNER_FAMILY_DIAGNOSTIC_END_MS="$(now_ms)"
        CORNER_FAMILY_DIAGNOSTIC_MS="$(elapsed_ms "${CORNER_FAMILY_DIAGNOSTIC_START_MS}" "${CORNER_FAMILY_DIAGNOSTIC_END_MS}")"

        if [[ "${CORNER_FAMILY_DIAGNOSTIC_RC}" -ne 0 ]]; then
            TESTS_RC="${CORNER_FAMILY_DIAGNOSTIC_RC}"
        fi
    fi

    TEST_END_MS="$(now_ms)"
    TESTS_MS="$(elapsed_ms "${TEST_START_MS}" "${TEST_END_MS}")"
fi

TOTAL_END_MS="$(now_ms)"
TOTAL_MS="$(elapsed_ms "${TOTAL_START_MS}" "${TOTAL_END_MS}")"

echo "[TIMING] navigation_runtime configure_ms=${CONFIGURE_MS}"
echo "[TIMING] navigation_runtime build_ms=${BUILD_MS}"
echo "[TIMING] navigation_runtime tests_ms=${TESTS_MS}"
echo "[TIMING] navigation_runtime scheduler_scale_diagnostic_ms=${SCHEDULER_DIAGNOSTIC_MS}"
echo "[TIMING] navigation_runtime b5_scale_diagnostic_ms=${B5_DIAGNOSTIC_MS}"
echo "[TIMING] navigation_runtime maneuver_execution_lab_ms=${EXECUTION_LAB_DIAGNOSTIC_MS}"
echo "[TIMING] navigation_runtime corridor_matrix_ms=${CORRIDOR_MATRIX_DIAGNOSTIC_MS}"
echo "[TIMING] navigation_runtime rigid_body_corridor_ms=${RIGID_BODY_CORRIDOR_DIAGNOSTIC_MS}"
echo "[TIMING] navigation_runtime corner_family_matrix_ms=${CORNER_FAMILY_DIAGNOSTIC_MS}"
echo "[TIMING] navigation_runtime total_ms=${TOTAL_MS}"

if [[ "${CONFIGURE_RC}" -ne 0 ]]; then
    echo "[RESULT] navigation_runtime FAIL phase=configure rc=${CONFIGURE_RC}"
    exit "${CONFIGURE_RC}"
fi

if [[ "${BUILD_RC}" -ne 0 ]]; then
    echo "[RESULT] navigation_runtime FAIL phase=build rc=${BUILD_RC}"
    exit "${BUILD_RC}"
fi

if [[ "${TESTS_RC}" -ne 0 ]]; then
    echo "[RESULT] navigation_runtime FAIL phase=tests rc=${TESTS_RC}"
    exit "${TESTS_RC}"
fi

echo "[RESULT] navigation_runtime PASS"
