#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")


def fail(message: str) -> None:
    print(f"[FAIL] shared UI component kit: {message}", file=sys.stderr)
    raise SystemExit(1)


kit_js = read("src/assets/webui/elite_ui.js")
kit_css = read("src/assets/webui/elite_ui.css")
menu = read("src/assets/webui/main_menu.html")
app_h = read("src/core/Application.h")
app_cpp = read("src/core/Application.cpp")
nav_h = read("src/ui/platform/UiNavigationState.h")
prefs_h = read("src/ui/platform/ClientPreferencesStore.h")
prefs_cpp = read("src/ui/platform/ClientPreferencesStore.cpp")
cmake = read("CMakeLists.txt")
ui_doc = read("src/ui/UI_PLATFORM_ARCHITECTURE.md")

for token in (
    "createNavigationShell",
    "setBanner",
    "setFieldError",
    "showDialog",
    "bindPasswordFields",
    "generatePassword",
    "crypto.getRandomValues",
):
    if token not in kit_js:
        fail(f"elite_ui.js lost reusable behavior: {token}")

for token in (
    ".elite-screen",
    ".elite-panel",
    ".elite-button",
    ".elite-form",
    ".elite-field",
    ".elite-input",
    ".elite-banner",
    ".elite-dialog",
    ".elite-password-row",
    ".elite-checkbox-row",
    ":focus-visible",
    "@media (max-height: 680px)",
    "@media (max-width: 520px)",
    'html[dir="rtl"]',
):
    if token not in kit_css:
        fail(f"elite_ui.css lost shared/responsive/RTL primitive: {token}")

for html in sorted((ROOT / "src/assets/webui").glob("*.html")):
    # presentation_blank.html is an intentionally non-visible bootstrap page
    # for the second physical WebView surface; it has no UI/component surface.
    if html.name == "presentation_blank.html":
        continue
    text = html.read_text(encoding="utf-8", errors="replace")
    if '/elite_ui.css' not in text or '/elite_ui.js' not in text:
        fail(f"WebUI page does not import the shared UI kit: {html.name}")

for token in (
    'data-elite-view="home"',
    'data-elite-view="multiplayer"',
    "window.EliteUiKit.createNavigationShell",
    "window.applyMainMenuState",
    "role=\"alert\"",
):
    if token not in menu:
        fail(f"main menu is not composed on the shared navigation/component layer: {token}")

if "<style>" in menu:
    fail("main menu still owns a private inline CSS component system")

for token in (
    "UiShellRoute::MultiplayerAuthorization",
    "UiNavigationState m_uiNavigationState",
    "window.applyMainMenuState(",
):
    if token not in app_h + app_cpp:
        fail(f"native menu navigation is not represented as explicit shell state: {token}")

for token in (
    "enum class UiShellRoute",
    "MainMenuHome",
    "MultiplayerAuthorization",
    "transientMessageCode",
):
    if token not in nav_h:
        fail(f"UiNavigationState contract missing: {token}")

for token in (
    "lastServerEndpoint",
    "lastSuccessfulAccountByServer",
    "preferredLocale",
    "rememberSuccessfulMultiplayer",
    "ClientPreferencesStore",
):
    if token not in prefs_h:
        fail(f"non-secret client preference contract missing: {token}")

for forbidden in ("password", "AuthToken", "recoverySecret", "credentialSecret"):
    if forbidden in prefs_h:
        fail(f"secret-shaped field leaked into ClientPreferences API: {forbidden}")

for token in (
    '"schema_version"',
    '"last_server_endpoint"',
    '"last_successful_account_by_server"',
    '"preferred_locale"',
    "MoveFileExW",
    "MaxPreferencesFileBytes",
):
    if token not in prefs_cpp:
        fail(f"ClientPreferencesStore persistence hardening missing: {token}")

if "ClientPreferencesStore.cpp" not in cmake:
    fail("EliteGame does not compile ClientPreferencesStore")

finish_start = app_cpp.find("const auto finishReadySession = [this]()")
if finish_start < 0:
    fail("cannot locate successful-session persistence boundary")
finish_end = app_cpp.find("m_pendingSessionLaunch = GameSessionLaunchKind::None;", finish_start)
if finish_end < 0:
    fail("cannot locate successful-session completion boundary")
finish_tail = app_cpp[finish_start:finish_end]
for token in (
    "rememberSuccessfulMultiplayer",
    "ClientPreferencesStore::save",
    "isValidAccountHandle",
):
    if token not in finish_tail:
        fail(f"last successful account is not persisted only at session-ready boundary: {token}")

if "m_clientPreferences.lastSuccessfulAccountFor(endpoint)" not in app_cpp:
    fail("multiplayer form does not restore the last successful account for the selected server")
if "m_clientPreferences.preferredLocale" not in app_cpp:
    fail("preferred locale is not restored/persisted through ClientPreferencesStore")

for token in (
    "shared component",
    "ClientPreferencesStore",
    "navigation shell",
):
    if token.lower() not in ui_doc.lower():
        fail(f"UI platform document is stale: {token}")

print("[PASS] shared UI kit, explicit navigation shell and non-secret remembered client preferences")
