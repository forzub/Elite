#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
def read(rel): return (ROOT / rel).read_text(encoding='utf-8', errors='replace')
def fail(msg):
    print(f'[FAIL] client service UI polish: {msg}', file=sys.stderr)
    raise SystemExit(1)

css = read('src/assets/webui/elite_ui.css')
kit = read('src/assets/webui/elite_ui.js')
menu = read('src/assets/webui/main_menu.html')
loading = read('src/assets/webui/loading.html')
app = read('src/core/Application.cpp')
app_h = read('src/core/Application.h')
coord_h = read('src/ui/presentation/GamePresentationCoordinator.h')
native = read('src/ui/presentation/InSessionPresentationRenderer.cpp')
window = read('src/window/Window.cpp')
webview = read('src/ui/browser/GameWebView.cpp')
main_strings = read('src/assets/localization/ui/common/main_menu.json')
loading_strings = read('src/assets/localization/ui/common/loading.json')

for token in ('--elite-action-gap', 'font-size: clamp(11px, 2.2vh, 17px)', 'min-height: clamp(38px, 7.2vh, 56px)', '@media (max-height: 680px)'):
    if token not in css: fail(f'height-responsive layout contract missing: {token}')
for token in ('glfwSetWindowSizeLimits', '800,', '600,'):
    if token not in window: fail(f'minimum-window usability envelope missing: {token}')

# Browser surfaces are document-only: Main Menu, Loading and ESC. They still
# use native client-size geometry and front/back prepare-before-present.
for token in ('m_window->clientSize(clientW, clientH)', 'syncDocumentWebViewBounds', 'm_documentWebViews', 'GetClientRect', 'glfwGetFramebufferSize'):
    if token not in app + app_h + window:
        fail(f'document geometry/presentation separation missing: {token}')
for token in ('prepareFullScreenDocument', 'commitFullScreenDocument', 'acknowledgeDocumentPrepared', 'incoming.bringToFront()'):
    if token not in app + app_h:
        fail(f'prepared front/back document commit missing: {token}')

# F1-F12 must not be browser documents. Services render on the same OpenGL
# surface as Flight/Navigation instead of navigating or showing child HWNDs.
webdoc = coord_h[coord_h.find('constexpr bool isFullScreenWebDocument'):]
for forbidden in ('GameUiMode::Flight', 'GameUiMode::SystemMap', 'GameUiMode::ServicePanel'):
    if forbidden in webdoc.split('}', 1)[0]:
        fail(f'F1-F12 mode leaked back into browser document ownership: {forbidden}')
for token in ('GameUiMode::MainMenu', 'GameUiMode::Loading', 'GameUiMode::SessionMenu'):
    if token not in webdoc.split('}', 1)[0]:
        fail(f'non-session browser document mode disappeared: {token}')
for token in ('renderServicePanel(', 'solidRectPx(', 'definition->titleKey'):
    if token not in native:
        fail(f'native F5-F8 service renderer missing: {token}')
for forbidden in ('service_shell.html', 'service_panel.js', 'm_serviceWebViews', 'beginServiceUiTransition', 'm_serviceUiTransitionCompletion'):
    if forbidden in app + app_h + coord_h:
        fail(f'obsolete service browser path survived: {forbidden}')

for token in ('id="local-new-game-dialog"', 'id="local-player-name"', 'encodeURIComponent(name)', 'NewLocalGamePrefix = "new_local_game|"', 'decodeWebComponent', 'normalizeLocalPlayerDisplayName', 'state["localPlayerName"]'):
    if token not in menu + app: fail(f'local New Game name flow missing: {token}')
local_block_start = app.find('NewLocalGamePrefix = "new_local_game|"')
local_block_end = app.find('if (webCommand == "load_local_game")', local_block_start)
local_block = app[local_block_start:local_block_end]
for forbidden in ('isValidAccountHandle', 'Moderation', 'moderation'):
    if forbidden in local_block: fail(f'local player name uses public moderation: {forbidden}')

for token in ('id="cancelConnection"', 'window.gameCommand("session_cancel")', 'connectionCancelable()', 'sessionKind !== "remote"', '"&session=remote"', '"&session=local"', 'webCommand == "session_cancel"', 'cancelPendingSessionStart', 'showMultiplayerConnectionForm();'):
    if token not in loading + app + app_h: fail(f'remote connection cancellation missing: {token}')

for token in ('"main.local_player_name"', '"main.local_player_name_help"', '"main.local_player_name_required"'):
    if token not in main_strings: fail(f'local name localization missing: {token}')
for token in ('"loading.stage.connecting"', '"loading.cancel_connection"'):
    if token not in loading_strings: fail(f'connection-cancel localization missing: {token}')

for token in ('animatedProgressTarget', 'requestAnimationFrame', 'Math.abs(animatedProgressTarget - targetProgress)', 'makes progress speed FPS-dependent'):
    if token not in loading: fail(f'deterministic loading progress animator missing: {token}')
if 'transition: width' in loading: fail('loading progress still stacks CSS transitions')

for token in ('elite-ui-boot', 'waitForDocumentDependencies', 'settlePreparedLayout', 'main_menu_prepared'):
    if token not in menu + kit: fail(f'main shell preparation handshake missing: {token}')
if 'glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE)' not in window: fail('top-level window can expose unpainted startup')
if 'setVisible(false);' not in webview or 'controller surface is white' not in webview: fail('WebView navigation can expose unpainted controller background')
if 'ready: () => readyPromise' not in read('src/assets/webui/game_i18n.js'): fail('localization runtime has no readiness promise')

print('[PASS] document WebView shell is isolated from native F1-F12 presentation')
