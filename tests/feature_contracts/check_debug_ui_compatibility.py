#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def fail(message: str) -> None:
    print(f"[FAIL] debug UI compatibility: {message}", file=sys.stderr)
    raise SystemExit(1)


def main() -> int:
    app = (ROOT / "src/core/Application.cpp").read_text(encoding="utf-8")
    server = (ROOT / "src/ui/html/HtmlUiServer.cpp").read_text(encoding="utf-8", errors="replace")
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8", errors="replace")

    required = (
        "PrimaryDebugUiPort = 8090",
        "ProcessSingleInstanceGuard primaryDebugUiLease(",
        '"EliteGamePrimaryDebugUiPort"',
        "primaryDebugUiLease.ownsInstance()",
        "? PrimaryDebugUiPort",
        ": 0",
        "m_gameUiHttpPort = m_htmlUi.start(requestedWebUiPort, webUiRoot);",
        '[DEBUG CONTROL] http://localhost:',
        '<< "/debug_control.html\\n";',
        "startDebugUiCompatibilityRedirect(",
        "CompatibilityPort = 8090",
        "static HtmlUiServer redirectServer",
        "redirectServer.start(CompatibilityPort, webUiRoot)",
        "location.hostname",
        '"/debug_control.html"',
        '"/frustum_debug.html"',
        '"/ship_core.html"',
        '"/structure_debug.html"',
    )
    for token in required:
        if token not in app:
            fail(f"missing contract token: {token}")

    if 'resource = "/debug_control.html"' not in server:
        fail("real process-local server root no longer resolves to debug_control.html")

    client_start = cmake.find("add_executable(EliteGame")
    client_end = cmake.find(")", client_start)
    if client_start < 0 or client_end < 0 or "src/platform/ProcessSingleInstanceGuard.cpp" not in cmake[client_start:client_end]:
        fail("EliteGame does not link the cross-process primary Debug Control lease implementation")

    print("[PASS] first client owns fixed localhost:8090 Debug Control; later clients remain process-local/dynamic")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
