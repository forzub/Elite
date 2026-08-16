#!/usr/bin/env bash
# Shared MinGW/MSYS2 test/build toolchain validation.
# This file only defines helpers; sourcing it does not mutate PATH.

elite_resolve_python() {
    local candidate

    if [[ -n "${ELITE_PYTHON_BIN:-}" ]]; then
        if command -v "${ELITE_PYTHON_BIN}" >/dev/null 2>&1 \
            && "${ELITE_PYTHON_BIN}" -c 'import sys; raise SystemExit(0 if sys.version_info.major == 3 else 1)' >/dev/null 2>&1; then
            export ELITE_PYTHON_BIN
            return 0
        fi
        unset ELITE_PYTHON_BIN
    fi

    for candidate in python3 python; do
        if command -v "${candidate}" >/dev/null 2>&1 \
            && "${candidate}" -c 'import sys; raise SystemExit(0 if sys.version_info.major == 3 else 1)' >/dev/null 2>&1; then
            ELITE_PYTHON_BIN="${candidate}"
            export ELITE_PYTHON_BIN
            return 0
        fi
    done

    return 1
}

elite_require_python() {
    if elite_resolve_python; then
        return 0
    fi

    echo "[TOOLCHAIN][FAIL] no working Python 3 interpreter is available on PATH." >&2
    echo "[TOOLCHAIN][FAIL] A Windows Store/App Execution Alias named 'python' does not count." >&2
    return 1
}

elite_require_build_toolchain() {
    local command_name
    local failed=0

    for command_name in cmake ninja ctest g++; do
        if ! command -v "${command_name}" >/dev/null 2>&1; then
            echo "[TOOLCHAIN][FAIL] missing command: ${command_name}" >&2
            failed=1
        fi
    done

    if (( failed )); then
        return 1
    fi

    if ! cmake --version >/dev/null 2>&1; then
        echo "[TOOLCHAIN][FAIL] cmake exists on PATH but cannot execute." >&2
        return 1
    fi
    if ! ninja --version >/dev/null 2>&1; then
        echo "[TOOLCHAIN][FAIL] ninja exists on PATH but cannot execute." >&2
        return 1
    fi
    if ! ctest --version >/dev/null 2>&1; then
        echo "[TOOLCHAIN][FAIL] ctest exists on PATH but cannot execute." >&2
        return 1
    fi
    if ! g++ --version >/dev/null 2>&1; then
        echo "[TOOLCHAIN][FAIL] g++ exists on PATH but cannot execute." >&2
        return 1
    fi

    return 0
}

elite_require_ready_toolchain() {
    local failed=0

    elite_require_python || failed=1
    elite_require_build_toolchain || failed=1

    if (( failed )); then
        echo "[TOOLCHAIN][FAIL] ready suite was not started; project test results are INVALID." >&2
        echo "[TOOLCHAIN] Run from the shell where the MinGW toolchain, CMake/Ninja and Python 3 are actually available." >&2
        echo "[TOOLCHAIN] MSYSTEM=${MSYSTEM:-<unset>}" >&2
        echo "[TOOLCHAIN] PATH=${PATH}" >&2
        return 1
    fi

    echo "[TOOLCHAIN] python=$(${ELITE_PYTHON_BIN} -c 'import sys; print(sys.executable)')"
    echo "[TOOLCHAIN] cmake=$(command -v cmake)"
    echo "[TOOLCHAIN] ninja=$(command -v ninja)"
    echo "[TOOLCHAIN] ctest=$(command -v ctest)"
    echo "[TOOLCHAIN] g++=$(command -v g++)"
}
