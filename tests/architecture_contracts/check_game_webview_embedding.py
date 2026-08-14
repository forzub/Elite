from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        print(f"[FAIL] GameWebView embedding contract: {message}", file=sys.stderr)
        raise SystemExit(1)


cpp = read("src/ui/browser/GameWebView.cpp")
hdr = read("src/ui/browser/GameWebView.h")
app = read("src/core/Application.cpp")

require(
    "new webview::webview(false, parentHwnd)" in cpp,
    "WebView must be created directly inside the GLFW HWND",
)
require(
    "w->widget().value()" in cpp,
    "bounds/visibility must target the embedded webview widget, not the parent window",
)
require(
    "CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)" in cpp,
    "embedded WebView2 must initialize STA COM on the GLFW UI thread",
)
require(
    "WEBVIEW2_USER_DATA_FOLDER" in cpp and
    "WebView2Sessions" in cpp and
    "GetCurrentProcessId()" in cpp,
    "each EliteGame process must isolate its WebView2 user-data session",
)
require(
    "SetParent(" not in cpp,
    "a separately-created top-level WebView must never be re-parented after creation",
)
require(
    "WS_OVERLAPPEDWINDOW" not in cpp and
    "SetWindowLongPtr" not in cpp and
    "GetWindowLongPtr" not in cpp,
    "GameWebView must not rewrite Win32 window styles to fake embedding",
)
require(
    "std::thread" not in hdr and "m_thread" not in hdr,
    "WebView lifecycle must share the GLFW UI thread instead of owning a detached message-loop thread",
)
require(
    ".detach()" not in cpp and "\nw.run();" not in cpp and "\nw->run();" not in cpp,
    "embedded WebView lifecycle must be owned by the application/GLFW event loop",
)
require(
    "delete w;" in cpp and "CoUninitialize();" in cpp,
    "shutdown must deterministically destroy the embedded widget and release COM",
)
require(
    "[App] client process pid=" in app and "webui_port=" in app,
    "client process diagnostics must expose PID/HWND/WebUI port for multi-process acceptance",
)
require(
    "[App] main loop exit pid=" in app,
    "client shutdown must log the reason its main loop terminated",
)

print("[PASS] GameWebView is a GLFW-owned embedded WebView2 with deterministic lifecycle")
