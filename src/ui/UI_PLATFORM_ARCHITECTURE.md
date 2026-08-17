# UI Platform / Resource-Pack Architecture

## Goal

Service pages, account/auth flows, main-menu pages and future in-game WebView surfaces must stop copying CSS/JS/fonts/page chrome independently. The client owns one reusable UI platform with a compiled native boundary plus a versioned binary resource pack.

```text
EliteGame
  -> EliteUiKit (compiled C++ client library)
       -> UiResourceProvider
       -> UiNavigation / message bridge
       -> UiComponent contracts
       -> FontRegistry / glyph coverage policy
  -> elite_ui.pak
       -> manifest + version + content hashes
       -> compiled elite-ui.js / elite-ui.css
       -> page templates / fragments
       -> locale JSON
       -> icons
       -> bundled fonts
```

`elite_ui.pak` is a runtime asset, not gameplay authority and not server state. WebView pages become thin compositions over shared components instead of owning independent copies of form/button/layout logic.

## Bundled-font guarantee

System fonts are never used as the compatibility guarantee. Supported scripts are guaranteed only by fonts redistributed with the game under compatible licenses.

Initial declared glyph coverage:

- Latin + extended Latin (English/Spanish and common European text);
- Cyrillic;
- Simplified/Traditional Han coverage required by supported Chinese localization;
- Japanese kana + required Japanese Han coverage.

WebView consumes packaged WOFF2 faces through the resource provider. Native FreeType/UI text uses packaged native font faces through a shared `FontRegistry`. Locale/script fallback order is explicit; Chinese and Japanese may use different CJK faces because Han glyph design differs by locale.

CI must include glyph-coverage smoke strings such as:

```text
刘柏辛
Капитан Козлодоев
山田太郎
```

The platform guarantees supported scripts, not every Unicode code point in existence. Unsupported scripts must fail validation/display capability explicitly rather than depending on whichever fonts happen to be installed on the host OS.

## First shared Web components

The first reusable component set should cover the surfaces currently being changed most often:

- responsive page shell / safe viewport / scroll fallback;
- title/subtitle block;
- primary/secondary/danger buttons;
- form field + localized help text + validation state;
- password field + show/hide + generated-password action;
- error/warning/info banner;
- modal/dialog layer;
- account selector / remembered-account affordance;
- consent checkbox row;
- in-session ESC menu shell.

The resource pack and component API are versioned together. A page must not silently consume incompatible component assets.

## Registration / sign-in surfaces

`REGISTER` opens a dedicated registration view/modal rather than reusing the compact sign-in form. Target fields:

- display name (Unicode, moderation policy applies);
- stable account handle/login (strict ASCII grammar from authentication contract);
- password + confirmation;
- recovery channel: verified email or phone when external recovery channels exist;
- one-time recovery code fallback until/if no external recovery channel is configured;
- preferred UI locale;
- explicit telemetry/statistics consent and any other optional consent as separate opt-ins;
- required legal/privacy acceptance only where actually needed.

Do not collect demographic or tracking fields merely because storage exists. Optional statistics fields require a concrete product purpose and explicit consent.

`SIGN IN` uses a remembered device credential when one exists for the selected server/account. If no valid device credential exists, the UI requests the password. The password itself is never stored in ordinary client preferences.

## Client preferences versus credentials

`ClientPreferencesStore` is non-secret and may remember:

- last server endpoint;
- last successfully authenticated account handle per server;
- preferred locale;
- ordinary UI preferences.

Secrets stay in the OS credential store:

- remembered-device/refresh credential;
- other local secret material that must not be written to plain JSON.

Normal disconnect/quit retains the remembered device credential. Explicit **Sign out** revokes/removes the remembered device credential and forces password/recovery authentication next time.

## Name moderation boundary

Account handle grammar is deterministic and narrow. Display name is Unicode and passes a separate moderation pipeline:

```text
raw display name
  -> Unicode normalization / case folding for moderation copy
  -> separator/spacing/homoglyph/leetspeak normalization
  -> deterministic rules and deny patterns
  -> optional moderation/ML service
  -> Allow | Reject | SuggestAlternative
```

Patterns such as spaced/obfuscated profanity must not be defeated by trivial insertion of spaces/punctuation or common substitutions.

Do **not** silently mutate the user's persisted display name. If a name such as an obscene or obfuscated variant is rejected, return a localized explanation and optionally one or more sanitized suggestions. The user explicitly chooses the replacement. This keeps account identity, support logs and what the user sees consistent.

The same service boundary can later cover organization names, ship names and public text without coupling moderation logic to authentication.

## Commerce seam

Payment is deliberately not an authentication or gameplay-database concern. Reserve only a service boundary:

```text
CommerceService
  -> IEntitlementProvider
  -> external payment/provider adapter
  -> EntitlementRepository
```

The game server stores provider-neutral product/entitlement/transaction references needed to grant gameplay rights. Raw card/payment-instrument data is never stored by the game server. A future payment provider owns regulated payment processing; gameplay consumes server-verified entitlements.

No payment UI or provider dependency is required until a product need exists.
