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

Initial declared WebUI glyph coverage is deliberately broader than the currently translated locale set:

- Latin / extended Latin, Cyrillic and Greek;
- Simplified Chinese, Traditional Chinese (Taiwan and Hong Kong), Japanese and Korean CJK variants;
- Arabic-script UI fallback plus Hebrew;
- major Indic scripts: Devanagari, Bengali, Gujarati, Gurmukhi, Tamil, Telugu, Kannada, Malayalam and Sinhala;
- Thai, Lao, Khmer and Myanmar;
- Armenian, Georgian and Ethiopic;
- symbols and emoji fallback.

The first implementation uses pinned, unmodified Noto binaries fetched from immutable upstream commits by `tools/fetch_ui_fonts.ps1`. `tools/fetch_ui_fonts_mingw64.sh` keeps those fetched binaries out of the current checkout via `.git/info/exclude`; UI tooling must not create, replace or assume the contents of the repository root `.gitignore`. The repository stores the manifest/license metadata, not redistributed font binaries; release/build preparation fetches them into `third_party/fonts/noto`, then `tools/build_ui_pack.py` places them in `elite_ui.pak`. `THIRD_PARTY_LICENSES.md` is the central external-content license/provenance index.

WebView consumes packaged faces through `/ui/fonts/...` from the resource pack. Locale/script fallback order is explicit; Chinese regional variants and Japanese/Korean use separate CJK faces because Han glyph design differs by locale. Native FreeType/HUD text remains a separate follow-up: broad Unicode glyph files alone are not enough for complex-script shaping (Arabic/Indic/etc.), so native global-text support must use a shaping layer (for example HarfBuzz) behind the future shared `FontRegistry` instead of pretending raw FreeType glyph lookup is sufficient.

CI must include glyph-coverage smoke strings such as:

```text
刘柏辛
Капитан Козлодоев
山田太郎
```

The platform guarantees supported scripts, not every Unicode code point in existence. Unsupported scripts must fail validation/display capability explicitly rather than depending on whichever fonts happen to be installed on the host OS.

## Current implementation slice

The first UI-platform slice now establishes the resource and internationalization substrate:

- `src/assets/ui/font_manifest.json` is the authoritative pinned font manifest;
- `tools/fetch_ui_fonts.ps1` downloads the declared binaries and records SHA-256 in a generated local lock file;
- `tools/build_ui_pack.py` builds versioned `ELITEUI1` binary `elite_ui.pak`;
- `UiResourcePack` validates the binary index and `HtmlUiServer` serves pack resources before the filesystem development fallback;
- every current WebUI page imports shared `elite_ui.css`;
- `languages.json` carries locale script/direction metadata beyond the currently enabled translations;
- `game_i18n.js` applies `lang`, `dir` and script metadata so RTL is an architectural property, not page-specific CSS;
- font/license assets are indexed centrally and the pack includes human-readable notices;
- development builds may run before the font cache is fetched, but release/CI packaging enables `ELITE_REQUIRE_BUNDLED_UI_FONTS=ON` so missing declared faces fail closed.

The second UI-platform slice now establishes the reusable service-shell/component layer:

- `elite_ui.js` owns the shared navigation shell, banner/validation, modal/dialog and password-field behaviors; secure generated-password support uses Web Crypto rather than `Math.random()`;
- `elite_ui.css` owns responsive page/panel/button/form/field/banner/dialog/password/consent primitives, focus-visible states, compact-height behavior and RTL-safe layout;
- every current WebUI page imports the same shared CSS/JS runtime; `main_menu.html` is the first page migrated off its private inline component stylesheet;
- native `UiNavigationState` represents which main-menu sub-view is intended, while the page sends `main_menu_ready` before native state is applied, preserving race-free WebView navigation;
- `ClientPreferencesStore` persists only non-secret UX context under the per-user application-data/config root using a versioned bounded JSON document and atomic replacement;
- the last successfully entered multiplayer world records `server endpoint -> AccountHandle` only at the session-ready boundary, and the selected locale is also remembered; auth tokens/passwords/recovery material remain outside this store.

The third client-service-shell slice now composes the visible account/session surfaces on top of that foundation:

- `main_menu.html` owns one native-routed service shell with Home, Multiplayer, password fallback, Registration, Recovery and Account views; `UiNavigationState` carries the route plus non-secret endpoint/account drafts so WebView navigation does not invent a second source of truth. `ACCOUNT` is scoped to the Multiplayer view and is hidden until a successful remote authentication exists in the current client process. `SIGN IN` is a single user action: the client first tries the remembered-device credential and routes to the password view only when the credential is missing/rejected;
- Registration exposes the final DisplayName/AccountHandle/password/recovery/locale/consent shape, but password/recovery/profile/consent controls are explicitly fail-closed while the server still lacks durable credential/profile storage. The only live registration action is labelled as a development AccountHandle + remembered-device credential bootstrap and never transmits password/recovery material;
- Recovery belongs to the password/account flow, but its password-form action remains hidden until M8E.3b can distinguish a real password rejection from remembered-device rejection;
- Recovery is reached from the password/account flow rather than being a peer of the primary multiplayer actions. Explicit Sign out remains visible but disabled until M8E.3b can revoke the server-side device credential safely. `ACCOUNT` is not exposed before a successful multiplayer authentication in the current client process;
- locale selection is live now and persists through `ClientPreferencesStore`;
- Local and multiplayer ESC menus are separate service documents: `local_session_menu.html` contains only Resume / Save / Load / Main / Quit, while `multiplayer_session_menu.html` contains only Resume / Disconnect-to-main / Sign-out-to-main / Quit. Save/Load and Sign out remain fail-closed until their backend slices. New Game and Settings deliberately do not duplicate the in-session menus;
- Local New Game first asks for an unrestricted local-only Unicode player name; no public-name moderation is applied because the value belongs to the local save. The current pre-persistence client carries it as launch/session UX state until M8E.3e gives it a durable local record;
- service/session WebView surfaces use the full client window rather than the cinematic 16:9 render viewport, scale typography/control geometry primarily from viewport height, and the desktop window has an 800×600 minimum usability envelope;
- the loading surface receives explicit `session=local|remote` context. Cancel is exposed only for a remote connection/synchronization attempt and returns to Multiplayer without terminating the client;
- the obsolete `ConfirmExitState` and duplicate MainMenu `HtmlUiManager` command path are removed. GameWebView/Application now own service/session menu interaction, while `MainMenuState` is only the lightweight native render-loop backdrop. A visible service/session WebView keeps ordinary keyboard focus for controls; Win32 foreground ownership recognises WebView2 child/helper HWND ancestry, and each session document also forwards DOM Escape into the same native `session_escape` command. One native transition latch deduplicates both sources. `EliteUiKit` owns same-document view crossfades, while `Application::beginServiceUiTransition()` sequences cross-document/service transitions as fade-out -> state/navigation change -> destination fade-in.

## Presentation transaction contract

A UI or map destination is not allowed to become visible merely because a navigation call was accepted or a renderer mode enum changed. Production presentation follows an explicit **prepare -> present** transaction.

For GameWebView service documents:

```text
outgoing visible document
  -> CSS fade-out completes (`transitionend`)
  -> browser acknowledges `service_ui_fade_out_complete|serial`
  -> native navigation changes document/state
  -> destination remains `elite-ui-boot` / invisible
  -> localization + packaged fonts + native route/state + layout settle
  -> destination is revealed
```

The C++ timeout is fault recovery only; it is not the normal transition clock. `navigate()` acceptance is likewise not DOM readiness. Main menu has no temporary Home route while native state is unresolved, Loading receives its first cached authoritative progress only after `loading_ui_ready`, and hidden documents are discarded when their UI mode closes instead of being treated as reusable presentation state. Same-document service routes use a serialized last-destination-wins transition runner so a superseded async route cannot leave the only active view half-hidden.

Native visibility is part of the same contract, not an implementation detail. On Windows the top-level GLFW HWND is created hidden and receives one defined dark OpenGL frame before `glfwShowWindow`; the embedded WebView2 child is hidden immediately after controller creation and on every cross-document `navigate()`. Service documents emit a page-specific `*_prepared` acknowledgement only after localization/fonts/native state and a forced hidden layout pass; native code then exposes the child HWND and starts the CSS fade-in. This is required because boot CSS cannot suppress WebView2's white controller/default-background frame before a document has painted.

During gameplay the target-agnostic SystemMap side-panel document is prewarmed off-screen and retained across ordinary map close/reopen. F9-F12 therefore does not pay another HTML/CSS/font navigation barrier; only the target-specific authoritative payload must be applied before the outgoing gameplay frame is captured. A service/session document temporarily replaces this hidden panel in the single GameWebView and the panel is prewarmed again when gameplay resumes.

The System Map spans two presentation technologies (OpenGL map + a child-HWND GameWebView side panel), so its transaction has an additional barrier. F9-F12 first resolve the requested Galaxy/System/Detail/Local target while gameplay remains visible. The side panel loads native-hidden and acknowledges a fully applied authoritative payload (`system_map_panel_prepared`). Only then does the outgoing renderer capture the exact frame it just rendered; ownership changes after that frame is swapped. The first incoming OpenGL frame stays fully covered by the captured source, then crossfades. Visible map-to-map mode changes use the same rule: capture old map -> switch state -> render one complete incoming frame under opaque old snapshot -> blend. Map exit is symmetric: panel fades out, current map frame is captured, gameplay renders one complete covered frame, then the map snapshot dissolves.

This deliberately removes several previous shortcuts: guessed native 180 ms navigation deadlines, `markLoaded()` immediately after `navigate()`, default-Galaxy-first F9-F12 opening, the hotkey-only unrendered `swapBuffers()`, and capture of an unspecified next-frame back buffer.

`GameWebView` remains the production service/session UI boundary. `HtmlUiManager`/WebSocket panels remain legacy/developer/debug tooling and must not become a second production main-menu/session navigation path. The System Map side panel is still a separate OS child window; its prepare-before-show barrier guarantees complete content, but a pixel-perfect single-frame alpha blend of that HWND with the OpenGL framebuffer would require either migrating that panel to native/OpenGL UI or a future WebView2 composition/offscreen integration.

Password-backed registration/recovery/sign-out becomes the next server-security integration, not another client-only imitation.

## First shared Web components

The initial reusable component set now covers the surfaces currently being changed most often:

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
