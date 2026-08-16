#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")


def fail(message: str) -> None:
    print(f"[FAIL] authentication/admission: {message}", file=sys.stderr)
    raise SystemExit(1)


session = read("src/game/network/SessionMessage.h")
wire = read("src/game/network/WireProtocol.h")
client_transport = read("src/game/network/ITransport.h")
server_transport = read("src/game/network/IServerTransport.h")
tcp_cpp = read("src/game/network/TcpTransport.cpp")
runtime = read("src/game/server/ServerRuntime.cpp")
host = read("src/game/server/NetworkServerHost.cpp")
client = read("src/game/client/GameClient.cpp")
remote = read("src/game/session/RemoteGameSession.cpp")
application = read("src/core/Application.cpp")
profile = read("src/game/identity/ClientIdentityProfile.cpp")
menu = read("src/assets/webui/main_menu.html")
localization = read("src/assets/localization/ui/common/main_menu.json")
acceptance = read("src/game/diagnostics/MultiplayerClientAcceptanceHarness.cpp")

for token in (
    "AuthenticationIntent",
    "SignIn",
    "Register",
    "SessionRejectReason",
    "UnknownCredential",
    "RegistrationUnavailable",
    "AlreadyActive",
    "struct SessionReject",
):
    if token not in session:
        fail(f"typed authentication protocol missing: {token}")

hello_body = session.split("struct SessionHello", 1)[1].split("};", 1)[0]
for forbidden in ("PlayerId", "EntityId", "ShipInstanceId", "AccountId"):
    if forbidden in hello_body:
        fail(f"SessionHello illegally lets client choose {forbidden}")

for token in (
    "SessionReject = 9",
    "encodeSessionReject",
    "decodeSessionReject",
):
    if token not in wire:
        fail(f"typed rejection is missing from wire protocol: {token}")

if "receiveSessionReject" not in client_transport:
    fail("client transport cannot receive typed authentication rejection")
if "publishSessionRejectImmediately" not in server_transport:
    fail("server transport cannot publish typed authentication rejection")
for token in ("WireMessageKind::SessionReject", "receiveSessionReject"):
    if token not in tcp_cpp:
        fail(f"TCP adapter drops typed rejection: {token}")

for token in (
    "resolveOrRegisterAccount",
    "hello.intent != game::network::AuthenticationIntent::Register",
    "SessionRejectReason::UnknownCredential",
    "m_accounts.bind",
    "transport.publishSessionRejectImmediately(reject)",
):
    if token not in runtime:
        fail(f"server admission contract incomplete: {token}")

# Implicit enrollment must be impossible on a normal SignIn request.
guard = runtime.find("hello.intent != game::network::AuthenticationIntent::Register")
bind = runtime.find("m_accounts.bind", guard)
if guard < 0 or bind < 0:
    fail("registration gate/bind path missing")
if "return {};" not in runtime[guard:bind]:
    fail("unknown SignIn can fall through into registration")

for token in (
    "AuthenticationDeadline",
    "MaxPendingAuthenticationConnections",
    "rejectionSent",
    "RejectionFlushGrace",
):
    if token not in host:
        fail(f"pending/rejected connection lifecycle is not bounded: {token}")

if "m_transport.receiveSessionReject(reject)" not in client:
    fail("GameClient does not consume typed SessionReject before gameplay welcome")
if "m_client->connectionError()" not in remote:
    fail("RemoteGameSession does not preserve typed client rejection reason")

for token in (
    "ClientIdentityProfileStore::loadExisting",
    "ClientIdentityProfileStore::loadOrCreate",
    "AuthenticationIntent::SignIn",
    "AuthenticationIntent::Register",
    '"multiplayer_signin|"',
    '"multiplayer_register|"',
    "showMultiplayerConnectionForm();",
):
    if token not in application:
        fail(f"Application authentication state machine missing: {token}")

if "config.identityHello.intent = game::network::AuthenticationIntent::Register" not in application:
    fail("local private runtime does not explicitly register its own ephemeral account")

for token in ("loadExisting(", "loadOrCreate("):
    if token not in profile:
        fail(f"credential store cannot distinguish sign-in from local credential creation: {token}")

for token in (
    'id="credential-profile"',
    "SIGN IN",
    "REGISTER",
    "multiplayer_signin|",
    "multiplayer_register|",
    "connectionErrorI18nKeys",
):
    if token not in menu:
        fail(f"multiplayer authorization UI missing: {token}")

for token in (
    "main.auth.invalid_credential",
    "main.auth.unknown_credential",
    "main.auth.registration_unavailable",
    "main.auth.already_active",
    "main.auth.session_unavailable",
    "main.auth.bootstrap_failed",
):
    if token not in localization:
        fail(f"typed authentication rejection is not localized: {token}")

if "sessionRejectMessage" in session:
    fail("network protocol header contains presentation-language rejection text")

for token in (
    "UnknownCredential",
    "AlreadyActive",
    "reconnect_ack",
):
    if token not in acceptance:
        fail(f"multiplayer acceptance does not cover auth rejection/reconnect: {token}")

print("[PASS] explicit sign-in/register, typed rejection and bounded admission lifecycle")
