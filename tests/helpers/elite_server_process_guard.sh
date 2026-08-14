#!/usr/bin/env bash

elite_list_running_server_processes() {
    if command -v powershell.exe >/dev/null 2>&1; then
        MSYS2_ARG_CONV_EXCL='*' powershell.exe -NoProfile -NonInteractive -Command '
            $items = @(Get-Process -Name EliteServer -ErrorAction SilentlyContinue)
            foreach ($p in $items) {
                $path = "<unknown>"
                try { if ($p.Path) { $path = $p.Path } } catch {}
                "PID={0} PATH={1}" -f $p.Id, $path
            }
        ' | tr -d '\r'
        return ${PIPESTATUS[0]}
    fi

    if command -v pgrep >/dev/null 2>&1; then
        pgrep -a -x EliteServer 2>/dev/null || true
        return 0
    fi

    echo "No supported process-inspection tool is available (powershell.exe/pgrep)." >&2
    return 2
}

elite_fail_if_server_running() {
    local context="${1:-test harness}"
    local running

    if ! running="$(elite_list_running_server_processes)"; then
        echo "[FAIL] ${context}: unable to inspect running EliteServer processes." >&2
        return 1
    fi

    if [[ -n "${running}" ]]; then
        echo "[FAIL] ${context}: EliteServer is already running." >&2
        echo "Stop the existing server before starting this test; the harness will not kill developer-owned processes." >&2
        while IFS= read -r line; do
            [[ -n "${line}" ]] && echo "  ${line}" >&2
        done <<<"${running}"
        return 1
    fi

    return 0
}
