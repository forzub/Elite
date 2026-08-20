#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]

def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")

def fail(message: str) -> None:
    print(f"[FAIL] account/auth polish: {message}", file=sys.stderr)
    raise SystemExit(1)

handle = read("src/game/identity/AccountHandle.h")
session = read("src/game/network/SessionMessage.h")
wire = read("src/game/network/WireProtocol.h")
registry = read("src/game/server/AccountRegistry.h")
runtime_h = read("src/game/server/ServerRuntime.h")
runtime = read("src/game/server/ServerRuntime.cpp")
host_h = read("src/game/server/NetworkServerHost.h")
server_main = read("src/server_main.cpp")
profile = read("src/game/identity/ClientIdentityProfile.cpp")
menu = read("src/assets/webui/main_menu.html")
ui_css = read("src/assets/webui/elite_ui.css")
localization = read("src/assets/localization/ui/common/main_menu.json")
auth_doc = read("src/game/identity/AUTHENTICATION_ARCHITECTURE.md")

for token in (
    "AccountHandleMinLength = 3u",
    "AccountHandleMaxLength = 24u",
    "isValidAccountHandle",
    "lowercase ASCII",
):
    if token not in handle:
        fail(f"shared AccountHandle contract missing: {token}")

for token in (
    "std::string accountHandle",
    "InvalidAccountHandle",
    "AccountHandleTaken",
    "UNKNOWN_ACCOUNT",
):
    if token not in session:
        fail(f"wire/session account-handle contract missing: {token}")

if "WireProtocolVersion = 9u" not in wire:
    fail("SessionHello schema changed without wire protocol version bump")
if "AccountHandleMaxLength" not in wire:
    fail("wire decoder does not bound AccountHandle before authoritative validation")

for token in (
    "std::string accountHandle",
    "findByHandle",
    "ResolveResult::UnknownAccount",
    "ResolveResult::InvalidCredential",
    "void reset() noexcept",
):
    if token not in registry:
        fail(f"AccountRegistry handle/reset contract missing: {token}")

for token in (
    "isValidAccountHandle(hello.accountHandle)",
    "SessionRejectReason::UnknownAccount",
    "SessionRejectReason::AccountHandleTaken",
    "hello.accountHandle",
):
    if token not in runtime:
        fail(f"server does not independently enforce handle semantics: {token}")

for token in (
    "resetAuthenticationStateForDevelopment",
    "connectedPlayerSessionCount() != 0u",
):
    if token not in runtime_h + runtime:
        fail(f"safe development auth reset missing: {token}")
if "resetAuthenticationStateForDevelopment" not in host_h:
    fail("NetworkServerHost does not expose pre-admission auth reset")
for token in ('--reset-auth-state', 'development auth registrations reset'):
    if token not in server_main:
        fail(f"dedicated-server auth reset CLI missing: {token}")

if "sanitizeProfileName" in profile or 'out = "default"' in profile:
    fail("credential store still silently sanitizes/invents account handles")
for token in ('"INVALID_ACCOUNT_HANDLE"', '"LOCAL_CREDENTIAL_MISSING"'):
    if token not in profile:
        fail(f"credential store does not return stable UI error code: {token}")

for token in (
    'data-i18n="main.account_handle"',
    'data-i18n="main.account_handle_rules"',
    'minlength="3"',
    'maxlength="24"',
    "^[a-z0-9][a-z0-9_-]{2,23}$",
    "toLowerCase()",
):
    if token not in menu:
        fail(f"validated authorization UI missing: {token}")

for token in (
    "max-height: calc(100vh - 16px)",
    "overflow-y: auto",
    "@media (max-height: 680px)",
    "clamp(",
):
    if token not in ui_css:
        fail(f"shared responsive UI primitive missing: {token}")

for token in (
    "main.account_handle_rules",
    "main.auth.invalid_account_handle",
    "main.auth.account_handle_taken",
    "main.auth.local_credential_missing",
):
    if token not in localization:
        fail(f"localized account/auth rule missing: {token}")

for token in (
    "IPasswordHasher",
    "AccountRecoveryService",
    "recovery secret",
    "rotate/revoke previous",
    "IRecoveryChannel",
    "TLS",
):
    if token not in auth_doc:
        fail(f"password/recovery architecture is incomplete: {token}")

print("[PASS] stable account handle, dev reset, responsive auth UI and recovery architecture")
