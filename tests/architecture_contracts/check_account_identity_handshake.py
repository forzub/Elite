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
profile_h = read("src/game/identity/ClientIdentityProfile.h")
profile_cpp = read("src/game/identity/ClientIdentityProfile.cpp")
account_h = read("src/game/server/AccountRegistry.h")
game_server_cpp = read("src/game/server/GameServer.cpp")

for required in (
    "struct SessionHello",
    "AuthToken authToken",
):
    if required not in session:
        fail(f"SessionHello is incomplete: missing {required}")

hello_block = session.split("struct SessionHello", 1)[1].split("};", 1)[0]
for forbidden in ("AccountId", "PlayerId", "ShipInstanceId", "EntityId"):
    if forbidden in hello_block:
        fail(f"client identity hello illegally selects {forbidden}")

if "AccountRegistry m_accounts" not in runtime_h:
    fail("ServerRuntime does not own the account/player binding registry")
if "resolveOrBindAccount" not in runtime_cpp:
    fail("ServerRuntime does not authenticate bearer tokens before session creation")
if "m_accounts.bind" not in runtime_cpp or "m_accounts.resolve" not in runtime_cpp:
    fail("hashed account registry is not used by authoritative admission")
if "authTokenDigest(hello.authToken)" not in runtime_cpp:
    fail("server does not hash the bearer token at the admission boundary")
if "playerIdentities()" not in runtime_cpp:
    fail("new account enrollment does not select from server-owned player identities")

if "receiveSessionHello(hello)" not in host_cpp:
    fail("TCP host does not wait for client identity before gameplay admission")
if "attachPlayerSessionTransport(\n                *connection.transport,\n                hello" not in host_cpp:
    fail("TCP host does not pass SessionHello into authoritative admission")
if "attachPlayerSessionTransport(*transport)" in host_cpp:
    fail("TCP host still admits connections by first-come PlayerId selection")

if "sendSessionHello(m_config.identityHello)" not in remote_cpp:
    fail("remote client does not present its account identity after TCP connect")
if "sendSessionHello(identityHello)" not in local_host_cpp:
    fail("local client bypasses the account identity seam")

for required in (
    "ClientIdentityProfileStore",
    "Windows Credential Manager",
    "does NOT store",
):
    if required not in profile_h:
        fail(f"client profile contract missing: {required}")

profile_block = profile_h.split("struct ClientIdentityProfile", 1)[1].split("};", 1)[0]
if "AccountId" in profile_block or "PlayerId" in profile_block or "ShipInstanceId" in profile_block:
    fail("client credential slot stores server-side gameplay identity")
for required in ("CredReadW", "CredWriteW", "fillSecureRandom"):
    if required not in profile_cpp:
        fail(f"Windows OS credential storage is not actually wired: {required}")

if 'arg == "--profile"' not in main_cpp:
    fail("graphical client has no profile credential selector")
if "configureClientIdentity(identityProfile.sessionHello())" not in main_cpp:
    fail("selected client profile is not wired into game-session creation")

if "AuthToken authToken" in account_h:
    fail("server AccountRegistry retains the raw bearer token instead of only its digest")

for forbidden in (
    "requestedPlayerId",
    "requestedEntityId",
    "requestedShipInstanceId",
):
    if forbidden in account_h or forbidden in session:
        fail(f"client-selectable authority leaked into identity layer: {forbidden}")

if "ShipOwnerRef::player(playerId)" not in game_server_cpp:
    fail("bootstrap player starter ship is not explicitly self-owned")

print("[PASS] OS credential slot -> opaque token -> server hash -> AccountId/PlayerId authority seam")
