#!/usr/bin/env bash
# Canonical build/runtime layout shared by developer builds and acceptance tests.
# ROOT_DIR must point at the repository root before this file is sourced.

source "${ROOT_DIR}/tests/helpers/toolchain.sh"

if [[ -z "${ROOT_DIR:-}" ]]; then
    echo "build_layout.sh requires ROOT_DIR to be set" >&2
    return 1 2>/dev/null || exit 1
fi

ELITE_CLIENT_BUILD_DIR="${ROOT_DIR}/build"
ELITE_SERVER_BUILD_DIR="${ROOT_DIR}/build/headless_server"
ELITE_TEST_BUILD_ROOT="${ROOT_DIR}/build/tests"
ELITE_TEST_LOG_DIR="${ROOT_DIR}/build/test-logs"

elite_cleanup_legacy_build_layout() {
    local legacy_dirs=(
        "${ROOT_DIR}/build/ready_headless_server"
        "${ROOT_DIR}/build/network_process_server"
        "${ROOT_DIR}/build/architecture_contract_tests"
        "${ROOT_DIR}/build/feature_contract_tests"
        "${ROOT_DIR}/build/interaction_activation_tests"
        "${ROOT_DIR}/build/presentation_pipeline_tests"
        "${ROOT_DIR}/build/system_map_behavior_tests"
        "${ROOT_DIR}/build/world_runtime_tests"
    )
    local legacy_files=(
        "${ROOT_DIR}/build/EliteServer.exe"
        "${ROOT_DIR}/build/network_process_server.log"
        "${ROOT_DIR}/build/network_process_client.log"
    )

    local path
    for path in "${legacy_dirs[@]}"; do
        if [[ -e "${path}" ]]; then
            echo "[BUILD-LAYOUT] removing obsolete directory: ${path#${ROOT_DIR}/}"
            rm -rf -- "${path}"
        fi
    done

    for path in "${legacy_files[@]}"; do
        if [[ -e "${path}" ]]; then
            echo "[BUILD-LAYOUT] removing obsolete file: ${path#${ROOT_DIR}/}"
            rm -f -- "${path}"
        fi
    done

    # Unknown old build-directory names are not trusted either. Remove any
    # duplicate runtime executable while preserving the one canonical path for
    # each permanent process. Known legacy directories above are removed whole.
    if [[ -d "${ROOT_DIR}/build" ]]; then
        while IFS= read -r path; do
            if [[ "${path}" != "${ELITE_SERVER_BUILD_DIR}/EliteServer.exe" ]]; then
                echo "[BUILD-LAYOUT] removing non-canonical server binary: ${path#${ROOT_DIR}/}"
                rm -f -- "${path}"
            fi
        done < <(find "${ROOT_DIR}/build" -type f -name 'EliteServer.exe' -print 2>/dev/null)

        while IFS= read -r path; do
            if [[ "${path}" != "${ELITE_CLIENT_BUILD_DIR}/EliteGame.exe" ]]; then
                echo "[BUILD-LAYOUT] removing non-canonical client binary: ${path#${ROOT_DIR}/}"
                rm -f -- "${path}"
            fi
        done < <(find "${ROOT_DIR}/build" -type f -name 'EliteGame.exe' -print 2>/dev/null)
    fi

    mkdir -p "${ELITE_TEST_BUILD_ROOT}" "${ELITE_TEST_LOG_DIR}"
}

elite_build_canonical_client() {
    elite_require_build_toolchain
    cmake -S "${ROOT_DIR}" -B "${ELITE_CLIENT_BUILD_DIR}" -G Ninja \
        -DELITE_BUILD_CLIENT=ON \
        -DELITE_BUILD_SERVER=OFF
    cmake --build "${ELITE_CLIENT_BUILD_DIR}" --target EliteGame
}

elite_build_canonical_server() {
    elite_require_build_toolchain
    cmake -S "${ROOT_DIR}" -B "${ELITE_SERVER_BUILD_DIR}" -G Ninja \
        -DELITE_BUILD_CLIENT=OFF \
        -DELITE_BUILD_SERVER=ON
    cmake --build "${ELITE_SERVER_BUILD_DIR}" --target EliteServer
}

elite_build_canonical_runtime() {
    elite_build_canonical_client
    elite_build_canonical_server
}

elite_assert_unique_runtime_binaries() {
    local build_root="${ROOT_DIR}/build"
    local canonical_client="${ELITE_CLIENT_BUILD_DIR}/EliteGame.exe"
    local canonical_server="${ELITE_SERVER_BUILD_DIR}/EliteServer.exe"
    local path
    local failed=0

    if [[ ! -d "${build_root}" ]]; then
        return 0
    fi

    while IFS= read -r path; do
        if [[ "${path}" != "${canonical_server}" ]]; then
            echo "[BUILD-LAYOUT][FAIL] non-canonical EliteServer binary: ${path#${ROOT_DIR}/}" >&2
            failed=1
        fi
    done < <(find "${build_root}" -type f -name 'EliteServer.exe' -print 2>/dev/null)

    while IFS= read -r path; do
        if [[ "${path}" != "${canonical_client}" ]]; then
            echo "[BUILD-LAYOUT][FAIL] non-canonical EliteGame binary: ${path#${ROOT_DIR}/}" >&2
            failed=1
        fi
    done < <(find "${build_root}" -type f -name 'EliteGame.exe' -print 2>/dev/null)

    if (( failed )); then
        echo "[BUILD-LAYOUT][FAIL] remove duplicate runtime binaries before acceptance" >&2
        return 1
    fi
}

elite_require_canonical_runtime_binaries() {
    if [[ ! -x "${ELITE_CLIENT_BUILD_DIR}/EliteGame.exe" ]]; then
        echo "[BUILD-LAYOUT][FAIL] missing canonical client: build/EliteGame.exe" >&2
        return 1
    fi
    if [[ ! -x "${ELITE_SERVER_BUILD_DIR}/EliteServer.exe" ]]; then
        echo "[BUILD-LAYOUT][FAIL] missing canonical server: build/headless_server/EliteServer.exe" >&2
        return 1
    fi
    elite_assert_unique_runtime_binaries
}
