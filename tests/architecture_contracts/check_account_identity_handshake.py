from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def fail(message: str) -> None:
    print(f"[FAIL] account identity handshake: {message}")
    sys.exit(1)


session = read("src/game/network/SessionMessage.h")
runtime_h = read("src/game/server/ServerRuntime.h")
runtime_cpp = read("src/game/server/ServerRuntime.cpp")
host_cpp = read("src/game/server/NetworkServerHost.cpp")
remote_cpp = read("src/game/session/RemoteGameSession.cpp")
local_host_cpp = read("src/game/host/LocalGameHost.cpp")
main_cpp = read("src/main.cpp")
app_cpp = read("src/core/Application.cpp")
profile_h = read("src/game/identity/ClientIdentityProfile.h")
profile_cpp = read("src/game/identity/ClientIdentityProfile.cpp")
account_h = read("src/game/server/AccountRegistry.h")
game_server_cpp = read("src/game/server/GameServer.cpp")

for required in (
    "enum class AuthenticationIntent",
    "SignIn = 0",
    "Register = 1",
    "struct SessionHello",
    "std::string accountHandle",
    "AuthToken authToken",
    "AuthenticationIntent intent = AuthenticationIntent::SignIn",
    "struct SessionReject",
    "enum class SessionRejectReason",
    "UnknownAccount",
    "AccountHandleTaken",
    "AlreadyActive",
):
    if required not in session:
        fail(f"authentication protocol is incomplete: missing {required}")

hello_block = session.split("struct SessionHello", 1)[1].split("};", 1)[0]
for forbidden in ("AccountId", "PlayerId", "ShipInstanceId", "EntityId"):
    if forbidden in hello_block:
        fail(f"client identity hello illegally selects {forbidden}")

if "AccountRegistry m_accounts" not in runtime_h:
    fail("ServerRuntime does not own the account/player binding registry")
if "resolveOrRegisterAccount" not in runtime_cpp:
    fail("ServerRuntime does not resolve explicit sign-in/registration intent")
for required in (
    "m_accounts.resolve",
    "hello.accountHandle",
    "m_accounts.bind",
    "authTokenDigest(hello.authToken)",
    "AuthenticationIntent::Register",
    "SessionRejectReason::UnknownAccount",
    "SessionRejectReason::AccountHandleTaken",
    "SessionRejectReason::RegistrationUnavailable",
    "playerIdentities()",
):
    if required not in runtime_cpp:
        fail(f"authoritative authentication/admission seam is incomplete: {required}")

# Unknown SignIn must stop before the only registration allocation loop.
unknown_guard = runtime_cpp.find(
    "if (hello.intent != game::network::AuthenticationIntent::Register)"
)
bind_call = runtime_cpp.find("m_accounts.bind", unknown_guard)
if unknown_guard < 0 or bind_call < 0 or unknown_guard > bind_call:
    fail("registration is not gated behind explicit AuthenticationIntent::Register")
if "SessionRejectReason::UnknownAccount" not in runtime_cpp[unknown_guard:bind_call]:
    fail("unknown SignIn does not receive a typed UnknownAccount rejection")

if "receiveSessionHello(hello)" not in host_cpp:
    fail("TCP host does not wait for client identity before gameplay admission")
if "attachPlayerSessionTransport(" not in host_cpp or "hello" not in host_cpp:
    fail("TCP host does not pass SessionHello into authoritative admission")
if "attachPlayerSessionTransport(*transport)" in host_cpp:
    fail("TCP host still admits connections by first-come PlayerId selection")
for required in (
    "AuthenticationDeadline",
    "MaxPendingAuthenticationConnections",
):
    if required not in host_cpp:
        fail(f"unauthenticated TCP lifecycle is unbounded: missing {required}")

if "sendSessionHello(m_config.identityHello)" not in remote_cpp:
    fail("remote client does not present its account identity after TCP connect")
if "sendSessionHello(identityHello)" not in local_host_cpp:
    fail("local client bypasses the account identity seam")

for required in (
    "ClientIdentityProfileStore",
    "Windows Credential Manager",
    "does NOT store",
    "loadExisting",
    "loadOrCreate",
):
    if required not in profile_h:
        fail(f"client profile contract missing: {required}")

profile_block = profile_h.split("struct ClientIdentityProfile", 1)[1].split("};", 1)[0]
if any(token in profile_block for token in ("AccountId", "PlayerId", "ShipInstanceId")):
    fail("client credential slot stores server-side gameplay identity")
for required in ("CredReadW", "CredWriteW", "fillSecureRandom"):
    if required not in profile_cpp:
        fail(f"Windows OS credential storage is not actually wired: {required}")

if 'arg == "--profile"' not in main_cpp:
    fail("graphical client has no local credential-slot selector")
if "configureClientIdentity(" not in main_cpp or "identityProfile.profileName" not in main_cpp:
    fail("selected local credential slot is not wired into Application")

for required in (
    "ClientIdentityProfileStore::loadExisting",
    "ClientIdentityProfileStore::loadOrCreate",
    "AuthenticationIntent::SignIn",
    "AuthenticationIntent::Register",
    '"multiplayer_signin|"',
    '"multiplayer_register|"',
):
    if required not in app_cpp:
        fail(f"Application lacks explicit sign-in/register flow: {required}")

if "AuthToken authToken" in account_h:
    fail("server AccountRegistry retains raw bearer token instead of only its digest")

for forbidden in (
    "requestedPlayerId",
    "requestedEntityId",
    "requestedShipInstanceId",
):
    if forbidden in account_h or forbidden in session:
        fail(f"client-selectable authority leaked into identity layer: {forbidden}")

if "ShipOwnerRef::player(playerId)" not in game_server_cpp:
    fail("bootstrap player starter ship is not explicitly self-owned")

print("[PASS] explicit account-handle + credential sign-in/register -> server AccountId/PlayerId authority seam")
