#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")


def fail(message: str) -> None:
    print(f"[FAIL] session failure recovery: {message}", file=sys.stderr)
    raise SystemExit(1)


app_cpp = read("src/core/Application.cpp")
menu_html = read("src/assets/webui/main_menu.html")

needle = "if (sessionState == game::session::GameSessionState::Failed)"
if needle not in app_cpp:
    fail("Application lost the explicit failed-session branch")

failed_block = app_cpp.split(needle, 1)[1].split("return;", 1)[0]
for token in (
    "const bool remoteLaunch =",
    "const std::string sessionError = m_gameSession->error();",
    "stopGameSession();",
    "m_gameUi.clearLoaded();",
    "showMultiplayerConnectionForm(message);",
    "showMainMenu();",
):
    if token not in failed_block:
        fail(f"failed session can strand loading UI or lacks recovery path: {token}")

for token in (
    "main_menu_ready",
    "applyMainMenuView();",
    "UiShellRoute::MultiplayerAuthorization",
    "m_uiNavigationState",
    "window.applyMainMenuState(",
):
    if token not in app_cpp:
        fail(f"menu recovery can still race asynchronous WebView navigation: {token}")

for token in (
    "function setConnectionError(code)",
    "DOMContentLoaded",
    "sendCommand('main_menu_ready')",
    "window.applyMainMenuState",
):
    if token not in menu_html:
        fail(f"multiplayer form/page-ready recovery surface missing: {token}")

print("[PASS] failed remote session returns to Multiplayer form after main_menu.html is actually ready")
