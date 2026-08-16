from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8")


app_h = read("src/core/Application.h")
app_cpp = read("src/core/Application.cpp")
client_main = read("src/main.cpp")
menu_cpp = read("src/ui/MainMenuState.cpp")
menu_html = read("src/assets/webui/main_menu.html")

for token in [
    "GameSessionLaunchKind",
    "LocalNewGame",
    "RemoteMultiplayer",
    "startLocalGameSession",
    "startRemoteGameSession",
    "requestSessionStart",
]:
    if token not in app_h + app_cpp:
        raise SystemExit(f"session lifecycle split missing: {token}")

for forbidden in [
    "startConfiguredGameSession",
    'webCommand == "new_game"',
    'webCommand == "load_game"',
    "m_pendingNewGameLoad",
    "NewGameLoadStage",
]:
    if forbidden in app_h + app_cpp:
        raise SystemExit(f"ambiguous legacy game lifecycle survived: {forbidden}")

for token in [
    'webCommand == "new_local_game"',
    'webCommand == "multiplayer"',
    '"multiplayer_signin|"',
    '"multiplayer_register|"',
    "prepareRemoteIdentity",
    "parseNetworkEndpoint",
    "GameSessionLaunchKind::LocalNewGame",
    "GameSessionLaunchKind::RemoteMultiplayer",
]:
    if token not in app_cpp:
        raise SystemExit(f"explicit local/multiplayer UI route missing: {token}")

for token in [
    "main.new_local_game",
    "main.load_local_game",
    "main.multiplayer",
    "multiplayer_signin|",
    "multiplayer_register|",
    "server-endpoint",
    "credential-profile",
]:
    if token not in menu_html:
        raise SystemExit(f"main menu lifecycle UI missing: {token}")

for forbidden in ["main.new_game", "sendCommand('new_game')"]:
    if forbidden in menu_html:
        raise SystemExit(f"main menu still exposes ambiguous NEW GAME route: {forbidden}")

for token in [
    '"id", "new_local_game"',
    '"id", "load_local_game"',
    '"id", "multiplayer"',
]:
    if token not in menu_cpp:
        raise SystemExit(f"native menu lifecycle route missing: {token}")

# --connect is a shortcut into an already-existing remote universe. It must
# configure the endpoint before Application::run(), and Application::init()
# must start the explicit remote path without a NEW GAME command.
for token in ["--connect", "configureRemoteServer"]:
    if token not in client_main:
        raise SystemExit(f"remote command-line shortcut missing: {token}")

for token in [
    "--connect shortcut: entering multiplayer",
    "requestSessionStart(GameSessionLaunchKind::RemoteMultiplayer)",
]:
    if token not in app_cpp:
        raise SystemExit(f"--connect no longer bypasses ambiguous main-menu start: {token}")

print("[PASS] local-game lifecycle is separate from remote multiplayer attach")
