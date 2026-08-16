from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
window_cpp = (ROOT / "src/window/Window.cpp").read_text(encoding="utf-8")
webview_cpp = (ROOT / "src/ui/browser/GameWebView.cpp").read_text(encoding="utf-8")

failures = []

focus_match = re.search(
    r"void\s+Window::focus\s*\(\s*\)\s*\{(?P<body>.*?)\n\}",
    window_cpp,
    re.S,
)
if not focus_match:
    failures.append("Window::focus() was not found")
else:
    body = focus_match.group("body")
    if "GetForegroundWindow" not in body or "GetWindowThreadProcessId" not in body:
        failures.append("Window::focus() must verify foreground process ownership")
    if "foregroundPid != GetCurrentProcessId()" not in body:
        failures.append("Window::focus() must refuse focus when another process is foreground")
    if re.search(r"\bSetForegroundWindow\s*\(", body) or re.search(r"\bBringWindowToTop\s*\(", body):
        failures.append("Window::focus() must not force cross-process foreground activation")

visible_match = re.search(
    r"void\s+GameWebView::setVisible\s*\(bool\s+visible\)\s*\{(?P<body>.*?)\n\}",
    webview_cpp,
    re.S,
)
if not visible_match:
    failures.append("GameWebView::setVisible(bool) was not found")
else:
    body = visible_match.group("body")
    if "SWP_NOACTIVATE" not in body:
        failures.append("GameWebView visibility changes must use SWP_NOACTIVATE")
    if "currentProcessOwnsForegroundWindow()" not in body:
        failures.append("GameWebView may move keyboard focus only inside the foreground client")
    if re.search(r"\bSetForegroundWindow\s*\(", body):
        failures.append("GameWebView visibility must not steal foreground from another client")

if failures:
    for failure in failures:
        print(f"[FAIL] {failure}")
    sys.exit(1)

print("[PASS] multi-process clients cannot steal foreground focus during session bootstrap")
