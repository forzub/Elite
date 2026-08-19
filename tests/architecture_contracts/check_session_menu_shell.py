#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
def read(rel): return (ROOT / rel).read_text(encoding="utf-8", errors="replace")
def fail(message):
    print(f"[FAIL] ESC session menu shell: {message}", file=sys.stderr)
    raise SystemExit(1)

app_h = read("src/core/Application.h")
app = read("src/core/Application.cpp")
window_h = read("src/window/Window.h")
window = read("src/window/Window.cpp")
local = read("src/assets/webui/local_session_menu.html")
remote = read("src/assets/webui/multiplayer_session_menu.html")
space = read("src/game/SpaceState.cpp")
cmake = read("CMakeLists.txt")
main_state = read("src/ui/MainMenuState.cpp")

if (ROOT / "src/assets/webui/session_menu.html").exists():
    fail("legacy mixed local/remote session_menu.html still exists")

for token in ("SessionMenu", "showSessionMenu", "resumeSessionFromMenu", "applySessionMenuView", "returnSessionToMainMenu", "m_activeSessionKind"):
    if token not in app_h + app: fail(f"native session-menu lifecycle missing: {token}")
for token in ("local_session_menu.html", "multiplayer_session_menu.html", 'webCommand == "session_menu_ready"', 'webCommand == "session_resume"', 'webCommand == "session_escape"', 'webCommand == "session_return_main"', 'webCommand == "session_quit"'):
    if token not in app: fail(f"GameWebView session-menu bridge missing: {token}")
for token in ("consumeEscapePressed", "GetAsyncKeyState(VK_ESCAPE)", "WM_KEYDOWN", "VK_ESCAPE", "GetAncestor(foreground, GA_ROOT)", "IsChild(gameHwnd, foreground)"):
    if token not in window_h + window + app: fail(f"focus-independent native Escape capture missing: {token}")
if "const bool escapePressed = m_window->consumeEscapePressed();" not in app:
    fail("Windows Escape still competes with stale GLFW key-state instead of using one native latch")

# Exact local surface: no network/session-account commands.
for token in ("session_menu.resume", "session_menu.save", "session_menu.load", "session_menu.main_menu", "session_menu.quit", "applyLocalSessionMenuState"):
    if token not in local: fail(f"local ESC surface missing: {token}")
for forbidden in ("session_menu.disconnect", "session_menu.sign_out", "applyMultiplayerSessionMenuState", "session_menu.new_game", "session_menu.settings"):
    if forbidden in local: fail(f"local ESC surface contains unrelated action: {forbidden}")

# Exact multiplayer surface: no local save/load/main action.
for token in ("session_menu.resume", "session_menu.disconnect", "session_menu.sign_out", "session_menu.quit", "session_menu.remote_world_continues", "applyMultiplayerSessionMenuState"):
    if token not in remote: fail(f"multiplayer ESC surface missing: {token}")
for forbidden in ("session_menu.save", "session_menu.load", "session_menu.main_menu", "session_menu.new_game", "session_menu.settings", "applyLocalSessionMenuState"):
    if forbidden in remote: fail(f"multiplayer ESC surface contains unrelated action: {forbidden}")

for button_id in ("local-save", "local-load"):
    pos = local.find(f'id="{button_id}"')
    if pos < 0 or "disabled" not in local[pos:pos+220]: fail(f"{button_id} falsely exposed as functional")
pos = remote.find('data-i18n="session_menu.sign_out"')
if pos < 0 or "disabled" not in remote[max(0,pos-180):pos+180]: fail("Sign out exposed before revocation backend")

# While the WebView owns focus, the page forwards Escape into the same native
# idempotent command. The native Window async latch is the complementary path
# when gameplay/parent focus owns the key; whichever source wins first starts
# one transition and the other becomes a no-op.
for page_name, page in (("local", local), ("remote", remote)):
    for token in ("addEventListener('keydown'", "event.key !== 'Escape'", "event.repeat", "session_escape", "stopImmediatePropagation"):
        if token not in page:
            fail(f"{page_name} session page lacks WebView-focus Escape forwarding: {token}")
if "serviceUiTransitionPending()" not in app:
    fail("dual Escape sources are not deduplicated through the native transition latch")

for token in ("elite-ui-boot", "waitForDocumentDependencies", "settlePreparedLayout", "session_menu_prepared"):
    if token not in local or token not in remote: fail(f"session page anti-flash preparation missing: {token}")
if 'webCommand == "session_menu_prepared"' not in app or "presentPreparedGameUi" not in app:
    fail("native session menu can be shown before the hidden document is prepared")

if "ConfirmExitState.cpp" in cmake or (ROOT / "src/ui/ConfirmExitState.cpp").exists(): fail("legacy ConfirmExitState survived")
if "ConfirmExit" in space: fail("SpaceState still pushes legacy confirm-exit modal")
if "HtmlUiPanelId::MainMenu" in main_state: fail("duplicate legacy MainMenu HtmlUiManager path survived")

print("[PASS] local/remote ESC menus are separate and Escape is focus-independent")
