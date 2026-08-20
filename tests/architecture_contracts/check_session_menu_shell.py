#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
def read(rel): return (ROOT / rel).read_text(encoding='utf-8', errors='replace')
def fail(msg):
    print(f'[FAIL] ESC session menu shell: {msg}', file=sys.stderr)
    raise SystemExit(1)

app_h = read('src/core/Application.h')
app = read('src/core/Application.cpp')
window_h = read('src/window/Window.h')
window = read('src/window/Window.cpp')
local = read('src/assets/webui/local_session_menu.html')
remote = read('src/assets/webui/multiplayer_session_menu.html')
space = read('src/game/SpaceState.cpp')
cmake = read('CMakeLists.txt')
main_state = read('src/ui/MainMenuState.cpp')

if (ROOT / 'src/assets/webui/session_menu.html').exists():
    fail('legacy mixed local/remote session_menu.html still exists')

for token in ('SessionMenu', 'showSessionMenu', 'resumeSessionFromMenu', 'applySessionMenuView', 'returnSessionToMainMenu', 'm_activeSessionKind'):
    if token not in app_h + app: fail(f'native session-menu lifecycle missing: {token}')
for token in ('local_session_menu.html', 'multiplayer_session_menu.html', 'session_menu_ready|', 'session_resume', 'session_escape', 'session_return_main', 'session_quit'):
    if token not in app: fail(f'GameWebView session-menu bridge missing: {token}')
for token in ('consumeEscapePressed', 'GetAsyncKeyState(VK_ESCAPE)', 'WM_KEYDOWN', 'VK_ESCAPE', 'GetAncestor(foreground, GA_ROOT)', 'IsChild(gameHwnd, foreground)'):
    if token not in window_h + window + app: fail(f'focus-independent Escape capture missing: {token}')

# Local ESC pauses the authoritative embedded session immediately; multiplayer
# never sets that pause. F1-F12 target changes clear it, and non-paused overlays
# still advance SpaceState while publishing neutral manual input.
for token in (
    'setLocalSessionMenuPause(true)',
    'paused && m_activeSessionKind == GameSessionLaunchKind::LocalNewGame',
    'localSessionMenuPaused()',
    'state->prepareFrame(localPaused ? 0.0f : dt)',
    'if (!localPaused)',
    'state->update(dt)',
    'if (target.mode != GameUiMode::SessionMenu)',
    'setLocalSessionMenuPause(false)',
    'presentationConsumesGameplayInput',
    'state->handleInput()',
):
    if token not in app + app_h:
        fail(f'local-only pause / running-overlay policy missing: {token}')
if 'm_activeSessionKind == GameSessionLaunchKind::RemoteMultiplayer' in app[app.find('void Application::setLocalSessionMenuPause'):app.find('void Application::processServiceWebViewCommands')]:
    fail('multiplayer session menu can incorrectly own local pause state')

# Exact local surface.
for token in ('session_menu.resume', 'session_menu.save', 'session_menu.load', 'session_menu.main_menu', 'session_menu.quit', 'applyLocalSessionMenuState'):
    if token not in local: fail(f'local ESC surface missing: {token}')
for forbidden in ('session_menu.disconnect', 'session_menu.sign_out', 'applyMultiplayerSessionMenuState', 'session_menu.new_game', 'session_menu.settings'):
    if forbidden in local: fail(f'local ESC surface contains unrelated action: {forbidden}')

# Exact multiplayer surface explicitly states that the world continues.
for token in ('session_menu.resume', 'session_menu.disconnect', 'session_menu.sign_out', 'session_menu.quit', 'session_menu.remote_world_continues', 'applyMultiplayerSessionMenuState'):
    if token not in remote: fail(f'multiplayer ESC surface missing: {token}')
for forbidden in ('session_menu.save', 'session_menu.load', 'session_menu.main_menu', 'session_menu.new_game', 'session_menu.settings', 'applyLocalSessionMenuState'):
    if forbidden in remote: fail(f'multiplayer ESC surface contains unrelated action: {forbidden}')

for button_id in ('local-save', 'local-load'):
    pos = local.find(f'id="{button_id}"')
    if pos < 0 or 'disabled' not in local[pos:pos+220]: fail(f'{button_id} falsely exposed as functional')
pos = remote.find('data-i18n="session_menu.sign_out"')
if pos < 0 or 'disabled' not in remote[max(0,pos-180):pos+180]: fail('Sign out exposed before revocation backend')

# DOM Escape and native Escape are both safe because target requests are
# idempotent direct selectors rather than a transition toggle latch.
for page_name, page in (('local', local), ('remote', remote)):
    for token in ("addEventListener('keydown'", "event.key !== 'Escape'", 'event.repeat', 'session_escape', 'stopImmediatePropagation'):
        if token not in page: fail(f'{page_name} session page lacks Escape forwarding: {token}')
for token in ('m_requestedTarget == target', 'requestTarget(GameUiTarget target)'):
    if token not in read('src/ui/presentation/GamePresentationCoordinator.cpp') + read('src/ui/presentation/GamePresentationCoordinator.h'):
        fail(f'dual Escape idempotence missing: {token}')

for token in ('elite-ui-boot', 'waitForDocumentDependencies', 'settlePreparedLayout', 'session_menu_prepared'):
    if token not in local or token not in remote: fail(f'session page preparation missing: {token}')
if 'commitFullScreenDocument' not in app or 'acknowledgeDocumentPrepared' not in app:
    fail('session menu can be exposed before prepared front/back commit')

if 'ConfirmExitState.cpp' in cmake or (ROOT / 'src/ui/ConfirmExitState.cpp').exists(): fail('legacy ConfirmExitState survived')
if 'ConfirmExit' in space: fail('SpaceState still pushes legacy confirm-exit modal')
if 'HtmlUiPanelId::MainMenu' in main_state: fail('duplicate legacy MainMenu HtmlUiManager path survived')

print('[PASS] ESC pauses Local only; Multiplayer and F1-F12 keep the world running')
