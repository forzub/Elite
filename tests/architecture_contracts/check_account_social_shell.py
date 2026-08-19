#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]

def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")

def fail(message: str) -> None:
    print(f"[FAIL] account/social client shell: {message}", file=sys.stderr)
    raise SystemExit(1)

menu = read("src/assets/webui/main_menu.html")
nav = read("src/ui/platform/UiNavigationState.h")
app = read("src/core/Application.cpp")
strings = read("src/assets/localization/ui/common/account_shell.json")

for token in (
    'data-elite-view="multiplayer"',
    'data-elite-view="signin"',
    'data-elite-view="register"',
    'data-elite-view="recovery"',
    'data-elite-view="account"',
    'id="signin-password"',
    'id="signin-remember-device"',
    'id="signin-error"',
    'id="signin-recovery-button"',
    'id="registration-display-name"',
    'id="registration-account-handle"',
    'id="registration-password"',
    'id="registration-recovery-method"',
    'id="registration-locale"',
    'main.telemetry_consent',
    'main.registration.development_warning',
    "REGISTER DEVELOPMENT ACCOUNT",
):
    if token not in menu:
        fail(f"service-shell registration/account surface missing: {token}")

# The compact multiplayer surface has one user-facing sign-in action. Password
# is a fallback route selected by native auth state, not a second competing
# button beside remembered-device sign-in.
multiplayer_start = menu.find('data-elite-view="multiplayer"')
multiplayer_end = menu.find('data-elite-view="signin"', multiplayer_start)
multiplayer = menu[multiplayer_start:multiplayer_end]
if 'data-i18n="main.sign_in_password"' in multiplayer:
    fail("multiplayer authorization exposes a duplicate SIGN IN WITH PASSWORD action")
if 'data-i18n="main.recover_account"' in multiplayer:
    fail("recovery is exposed as a peer action instead of living in password/account flow")
if multiplayer.count('data-i18n="main.sign_in"') != 1:
    fail("multiplayer authorization does not expose exactly one SIGN IN action")
account_button = 'id="multiplayer-account-button"'
account_pos = multiplayer.find(account_button)
if account_pos < 0 or ' hidden' not in multiplayer[account_pos:account_pos + 260]:
    fail("post-auth ACCOUNT action is not scoped to the Multiplayer shell and hidden before auth")
home_start = menu.find('data-elite-view="home"')
home_end = menu.find('data-elite-view="multiplayer"', home_start)
home = menu[home_start:home_end]
if 'data-i18n="main.account"' in home or 'home-account-button' in home:
    fail("server account action leaked into the root/local main-menu surface")

# Recovery is located inside the password flow, but remains fail-closed until
# the future password protocol can distinguish an actual password rejection
# from a rejected remembered-device token.
if "passwordRecoveryAvailable" not in menu + app:
    fail("password-flow recovery availability is not explicit")
if 'state["passwordRecoveryAvailable"] = false' not in app:
    fail("recovery can be exposed before server-side password rejection exists")
if "errorCode !== 'INVALID_CREDENTIAL'" in menu:
    fail("remembered-device rejection incorrectly exposes password recovery")

# The current server has no password/recovery repository yet. The UI may show
# the final form contract, but must not silently pretend those secrets are used.
for element_id in (
    'signin-password',
    'signin-remember-device',
    'registration-password',
    'registration-password-confirm',
    'registration-recovery-method',
    'registration-recovery-contact',
    'recovery-proof',
    'recovery-new-password',
):
    marker = f'id="{element_id}"'
    pos = menu.find(marker)
    if pos < 0 or "disabled" not in menu[pos:pos + 260]:
        fail(f"unsupported security field is not fail-closed: {element_id}")

for forbidden in (
    "multiplayer_register|' + endpoint + '|' + profile + '|'",
    "registration-password').value",
    "registration-recovery-contact').value",
):
    if forbidden in menu:
        fail(f"password/recovery material is being sent through the legacy token-registration command: {forbidden}")

for token in (
    "SignInPassword",
    "showSignInPassword",
    "Registration",
    "Recovery",
    "Account",
    "setConnectionDraft",
    "endpointDraft",
    "accountHandleDraft",
):
    if token not in nav:
        fail(f"native service-shell navigation state missing: {token}")

for token in (
    'MainRoutePrefix = "main_route|"',
    'SetLocalePrefix = "set_ui_locale|"',
    "UiShellRoute::SignInPassword",
    "UiShellRoute::Registration",
    "UiShellRoute::Recovery",
    "UiShellRoute::Account",
    "showRegistrationForm",
    "showPasswordSignInForm",
    'identityError == "LOCAL_CREDENTIAL_MISSING"',
    'message == "INVALID_CREDENTIAL"',
    'state["accountAvailable"]',
    'm_authenticatedRemoteAccountHandle = m_clientIdentityProfileName',
    "setUiLanguage(locale)",
):
    if token not in app:
        fail(f"Application does not own unified auth/account-shell state bridge: {token}")

for token in (
    '"main.signin.title"',
    '"main.signin.pending"',
    '"main.registration.title"',
    '"main.recovery.pending"',
    '"main.account.security_pending"',
    '"main.sign_out"',
):
    if token not in strings:
        fail(f"account-shell localization missing: {token}")

print("[PASS] account/social shell uses one sign-in flow and exposes account only after successful auth")
