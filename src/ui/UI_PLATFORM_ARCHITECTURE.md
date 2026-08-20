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
- native `UiNavigationState` represents which main-menu sub-view is intended. Every cross-document WebView navigation now carries a native presentation generation in the URL; page `*_ready|generation` / `*_prepared|generation` acknowledgements are accepted only for that exact target/generation, so a late outgoing document cannot prepare a newer screen;
- `ClientPreferencesStore` persists only non-secret UX context under the per-user application-data/config root using a versioned bounded JSON document and atomic replacement;
- the last successfully entered multiplayer world records `server endpoint -> AccountHandle` only at the session-ready boundary, and the selected locale is also remembered; auth tokens/passwords/recovery material remain outside this store.

The third client-service-shell slice now composes the visible account/session surfaces on top of that foundation:

- `main_menu.html` owns Home, Multiplayer, password fallback, Registration, Recovery and Account views; `UiNavigationState` carries the route plus non-secret endpoint/account drafts so browser navigation does not invent a second source of truth. `ACCOUNT` is scoped to the Multiplayer view and is hidden until a successful remote authentication exists in the current client process. `SIGN IN` is a single user action: the client first tries the remembered-device credential and routes to the password view only when the credential is missing/rejected;
- Registration exposes the final DisplayName/AccountHandle/password/recovery/locale/consent shape, but password/recovery/profile/consent controls remain fail-closed while durable credential/profile storage is absent. Recovery and explicit Sign out remain gated by the same backend boundary;
- locale selection is live and persists through `ClientPreferencesStore`;
- Local and multiplayer ESC menus remain separate WebView documents: `local_session_menu.html` contains Resume / Save / Load / Main / Quit, while `multiplayer_session_menu.html` contains Resume / Disconnect-to-main / Sign-out-to-main / Quit. **Pause is session policy, not presentation policy:** ESC pauses the embedded Local session immediately; Multiplayer never pauses the authoritative world. F1-F12 never pause either session type;
- Local New Game first asks for an unrestricted local-only Unicode player name. The loading document receives explicit `session=local|remote` context and exposes Cancel only for remote connection/synchronization;
- the two `m_documentWebViews` are now reserved exclusively for **non-session documents**: Main Menu, Loading and ESC Session Menu. Their HWND geometry uses the native client area (`GetClientRect`) and they retain front/back prepare-before-present semantics. They are not an F1-F12 presentation mechanism;
- **all F1-F12 are one GLFW/OpenGL presentation surface.** F1-F4 render Flight, F5-F8 render native service surfaces, and F9-F12 render Navigation plus its STAR ATLAS side panel in the same OpenGL frame. Switching any F-key never navigates, shows, hides, resizes or reorders a WebView HWND;
- F5-F8 remain registry-driven (`ServiceUiDefinition`) and keep separate C++ descriptor/source boundaries for Government, Shipyard, Repair & Refit and Trade. Their current placeholder screens are drawn by `InSessionPresentationRenderer` directly into the default framebuffer; there is no `service_shell.html`, document load, font-ready handshake or browser route state;
- F9-F12 keep the client-composed map data architecture. The map itself renders into the left 72% and `InSessionPresentationRenderer::renderSystemMapPanel()` draws the right 28% from typed `SystemMapPanelPresentation` data after post-process. The removed `system_map_panel.html`/JSON/serial handshake can no longer own a second native surface or expose a stale parent region;
- `GamePresentationCoordinator` remains the only player-facing presentation coordinator. It stores committed/requested/scene targets, direct-selector/latest-request-wins semantics and document preparation state only for the three non-session WebView modes. Physical F1-F12 edges remain message-backed in `Window`, so short taps are independent of render/WebView stalls;
- OpenGL backing is an explicit presentation surface. The persistent post-process FBO is fully cleared before every active 3D frame. Service screens do not start post-process; Navigation post-process covers only its map viewport and the native side panel overwrites the remainder in the same frame. No stale render target is permitted to act as a hidden presentation owner;
- DWM fencing remains only on the boundary that genuinely crosses native surfaces (for example ESC/Loading -> an in-session F target or one non-session document -> another). It is no longer part of F1-F12 switching and therefore cannot influence Flight/Service/Navigation peer transitions.

## Presentation transaction contract

A destination is not allowed to become visible merely because a key was accepted or a renderer mode enum changed. Production presentation follows an explicit **request -> prepare -> render -> swap -> commit** transaction. `GameUiMode::None` is boot/terminal shutdown only; during a live session the committed target is Flight, Service, Navigation or SessionMenu.

The application-level direct selector table is:

```text
F1-F4   Flight camera presentations
F5-F8   Native service presentations
F9-F12  Native Navigation presentations
```

Repeating the already requested target is a no-op. A different F-key replaces only an unpublished destination (`latest request wins`); the old committed target remains authoritative until the final requested scene has rendered and swapped. `GamePresentationCoordinator` keeps `committedTarget`, `requestedTarget` and `sceneTarget` separate, but every F1-F12 scene target is rendered into the **same GLFW/OpenGL surface**.

The in-session transaction is therefore intentionally short:

```text
message-backed F-key edge
  -> direct `GameUiTarget` request
  -> target producer prepares state (Navigation may compose local snapshots)
  -> `sceneTarget` is armed
  -> exactly that target renders the complete in-session frame
  -> `SwapBuffers`
  -> requested target becomes committed
```

No F1-F12 branch may call `GameWebView::navigate`, `setVisible`, `bringToFront`, `SetWindowPos`, or a DWM handoff. F5-F8 render their native 2D surface directly; F9-F12 render map and right-side panel in the same frame. Renderer-internal map transitions may still be used for mouse/drill interaction inside the Navigation domain, but direct F9-F12 selectors cancel/avoid outgoing-snapshot crossfades and follow the common frame-bound transaction.

Browser presentation is a separate, narrower domain. Main Menu, Loading and ESC Session Menu use the two persistent `m_documentWebViews` with generation-fenced prepare-before-present. Entering one of these documents parks the scene because the document owns the full client area. Returning from such a document to F1-F12 renders the destination OpenGL frame first, swaps it, then retires the document surface. Browser document z-order/DWM synchronization therefore exists only at this explicit non-session boundary, not between peer F targets.

Native visibility and geometry still obey separate coordinate systems: child WebView geometry is derived from the parent native client area; OpenGL uses framebuffer pixels and its letterboxed viewport. Resize handling may hide/reveal non-session document surfaces, but the active-session F1-F12 image remains a single WGL presentation surface.

Loading progress remains a native-owned monotonic target rather than a CSS transition clock. ESC pause semantics remain outside the presentation coordinator: Local pauses its embedded authoritative session while the ESC document is committed; Multiplayer keeps transport/world advancement; any F1-F12 request releases Local pause and never changes simulation time itself.

Terminal quit hides the top-level/document surfaces before teardown so a last gameplay framebuffer cannot reappear during destruction.

This contract deliberately removes the previous shortcuts and layering: gameplay-as-implicit-background, frame-polled F-key latches, four service documents, persistent map-panel WebView, JSON map-panel readiness generations, service/map child-HWND z-order orchestration, second `playerNavigationMapEntry*` coordination, direct-key map crossfades and stale post-process backing. A regression guard explicitly forbids any WebView presentation dependency in the active-session F1-F12 path.

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
