#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]

def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")

def fail(message: str) -> None:
    print(f"[FAIL] client service UI polish: {message}", file=sys.stderr)
    raise SystemExit(1)

css = read("src/assets/webui/elite_ui.css")
kit = read("src/assets/webui/elite_ui.js")
menu = read("src/assets/webui/main_menu.html")
loading = read("src/assets/webui/loading.html")
app = read("src/core/Application.cpp")
app_h = read("src/core/Application.h")
window = read("src/window/Window.cpp")
webview = read("src/ui/browser/GameWebView.cpp")
main_strings = read("src/assets/localization/ui/common/main_menu.json")
loading_strings = read("src/assets/localization/ui/common/loading.json")


for token in (
    "fadeOutDocument",
    "restoreDocument",
    "VIEW_TRANSITION_MS",
    "elite-view--leaving",
    "elite-ui-leaving",
):
    if token not in kit + css:
        fail(f"global service-screen transition primitive missing: {token}")

for token in (
    "beginServiceUiTransition",
    "updateServiceUiTransition",
    "m_serviceUiTransitionCompletion",
    "beginServiceUiTransition(finishReadySession)",
):
    if token not in app + app_h:
        fail(f"native cross-document fade sequencing missing: {token}")

for token in (
    "--elite-action-gap",
    "font-size: clamp(11px, 2.2vh, 17px)",
    "min-height: clamp(38px, 7.2vh, 56px)",
    "@media (max-height: 680px)",
):
    if token not in css:
        fail(f"height-responsive layout contract missing: {token}")

for token in (
    "glfwSetWindowSizeLimits",
    "800,",
    "600,",
):
    if token not in window:
        fail(f"desktop minimum-window usability envelope missing: {token}")

if "m_gameWebView.setBounds(0, 0, width, height)" not in app:
    fail("service/session WebView does not use the full resized client window")

for token in (
    'id="local-new-game-dialog"',
    'id="local-player-name"',
    "encodeURIComponent(name)",
    'NewLocalGamePrefix = "new_local_game|"',
    "decodeWebComponent",
    "normalizeLocalPlayerDisplayName",
    'state["localPlayerName"]',
):
    if token not in menu + app:
        fail(f"local New Game name flow missing: {token}")

# Local-only names must not be forced through multiplayer AccountHandle/public
# moderation. Structural length/control validation is allowed; content policy is not.
local_block_start = app.find('NewLocalGamePrefix = "new_local_game|"')
local_block_end = app.find('if (webCommand == "load_local_game")', local_block_start)
local_block = app[local_block_start:local_block_end]
for forbidden in ("isValidAccountHandle", "Moderation", "moderation"):
    if forbidden in local_block:
        fail(f"local player name incorrectly uses public-account moderation: {forbidden}")

for token in (
    'id="cancelConnection"',
    'window.gameCommand("session_cancel")',
    "connectionCancelable()",
    'sessionKind !== "remote"',
    '"&session=remote"',
    '"&session=local"',
    'webCommand == "session_cancel"',
    "cancelPendingSessionStart",
    "showMultiplayerConnectionForm();",
):
    if token not in loading + app + app_h:
        fail(f"remote connection cancellation flow missing: {token}")

for token in (
    '"main.local_player_name"',
    '"main.local_player_name_help"',
    '"main.local_player_name_required"',
):
    if token not in main_strings:
        fail(f"local player-name localization missing: {token}")
for token in ('"loading.stage.connecting"', '"loading.cancel_connection"'):
    if token not in loading_strings:
        fail(f"connection-cancel localization missing: {token}")

for token in (
    "elite-ui-boot",
    "waitForDocumentDependencies",
    "settlePreparedLayout",
    "main_menu_prepared",
):
    if token not in menu + kit:
        fail(f"main service shell preparation handshake missing: {token}")
for token in (
    "presentPreparedGameUi",
    'webCommand == "main_menu_prepared"',
    'webCommand == "loading_ui_prepared"',
    "m_gameUi.isPrepared",
):
    if token not in app + app_h:
        fail(f"native prepared-before-show boundary missing: {token}")
if "glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE)" not in window:
    fail("top-level GLFW window can expose an unpainted startup background")
if "setVisible(false);" not in webview or "controller surface is white" not in webview:
    fail("WebView2 child can remain visible while a new document is unpainted")
if "ready: () => readyPromise" not in read("src/assets/webui/game_i18n.js"):
    fail("localization runtime exposes no readiness promise for service reveal")

print("[PASS] service UI scales by height, avoids language flash, supports local naming and remote-only cancel")
