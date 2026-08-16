#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[2]
application = (root / "src/core/Application.cpp").read_text(encoding="utf-8")

anchor = "if (m_pendingSessionLaunch != GameSessionLaunchKind::None)"
if anchor not in application:
    print("[FAIL] loading UI GL isolation: pending-session branch missing", file=sys.stderr)
    raise SystemExit(1)

body = application.split(anchor, 1)[1].split("GameState* state = m_states.current();", 1)[0]

failures = []
if "m_window->swapBuffers()" in body:
    failures.append("pending synchronization still swaps the OpenGL back buffer")
if "std::this_thread::sleep_for(" not in body:
    failures.append("pending-session pump has no bounded yield after removing SwapBuffers")
if "m_window->pollEvents();" not in application:
    failures.append("main loop no longer pumps native events")

if failures:
    for failure in failures:
        print(f"[FAIL] loading UI GL isolation: {failure}", file=sys.stderr)
    raise SystemExit(1)

print("[PASS] loading/session synchronization stays off the OpenGL swap path")
