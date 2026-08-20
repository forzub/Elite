#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def read(rel):
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")


def fail(message):
    print(f"[FAIL] native compositor handoff: {message}", file=sys.stderr)
    raise SystemExit(1)


app = read("src/core/Application.cpp")
cmake = read("CMakeLists.txt")

for token in (
    "#include <dwmapi.h>",
    "bool flushDesktopCompositor()",
    "DwmFlush()",
):
    if token not in app:
        fail(f"DWM presentation fence missing: {token}")
if "dwmapi" not in cmake:
    fail("EliteGame does not link dwmapi for DwmFlush")

# F1-F12 share one OpenGL surface, so SwapBuffers is the only frame boundary
# among in-session peers. A native compositor handoff remains only when leaving
# an explicit document surface (Main Menu / Loading / ESC) for that GL surface.
swap = app.find("m_window->swapBuffers();")
commit_call = app.find("commitPreparedPresentationAfterSwap", swap)
if swap < 0 or commit_call < 0 or swap > commit_call:
    fail("scene commit is not frame-bound after SwapBuffers")

scene_start = app.find("void Application::commitPreparedPresentationAfterSwap")
scene_end = app.find("\nvoid Application::", scene_start + 1)
scene = app[scene_start:scene_end]
if not scene:
    fail("commitPreparedPresentationAfterSwap body not found")

for token in (
    "m_gameUi.committedTarget().isFullScreenWebDocument()",
    "flushDesktopCompositor()",
    "m_documentWebViews[m_activeDocumentSurface].setVisible(false)",
):
    if token not in scene:
        fail(f"document-to-GL handoff contract missing: {token}")
fence = scene.find("flushDesktopCompositor()")
hide = scene.find("m_documentWebViews[m_activeDocumentSurface].setVisible(false)")
if fence > hide:
    fail("document WebView can be hidden before the destination GL frame reaches DWM")

# WebView -> WebView still uses the prepared front/back document surfaces. The
# incoming child must be shown and raised, then compositor-fenced, before the
# old sibling can retire.
web_start = app.find("void Application::commitFullScreenDocument")
web_end = app.find("\nvoid Application::", web_start + 1)
web = app[web_start:web_end]
if not web:
    fail("commitFullScreenDocument body not found")
show = web.find("incoming.setVisible(true)")
front = web.find("incoming.bringToFront()")
fence = web.find("flushDesktopCompositor()")
hide = web.find("m_documentWebViews[m_activeDocumentSurface].setVisible(false)")
if min(show, front, fence) < 0:
    fail("incoming document show/front/fence sequence is incomplete")
if not (show < front < fence):
    fail("incoming document is not raised before the compositor fence")
if hide >= 0 and fence > hide:
    fail("outgoing document sibling can retire before incoming document reaches DWM")

# The old in-session child surfaces must not reappear in this native handoff.
for forbidden in ("m_mapPanelWebView", "m_serviceWebViews"):
    if forbidden in scene + web:
        fail(f"obsolete F1-F12 WebView surface returned to compositor handoff: {forbidden}")

print("[PASS] native compositor handoff is confined to document boundaries; F1-F12 remain one GL surface")
