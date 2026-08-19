#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]

def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")

def fail(message: str) -> None:
    print(f"[FAIL] presentation transition atomicity: {message}", file=sys.stderr)
    raise SystemExit(1)

app = read("src/core/Application.cpp")
app_h = read("src/core/Application.h")
space = read("src/game/SpaceState.cpp")
space_h = read("src/game/SpaceState.h")
renderer = read("src/game/system_map/SystemMapRenderer.cpp")
renderer_h = read("src/game/system_map/SystemMapRenderer.h")
renderer_common = read("src/game/system_map/SystemMapRendererCommon.inl")
map_transition = read("src/game/system_map/MapTransitionController.h")
kit = read("src/assets/webui/elite_ui.js")
css = read("src/assets/webui/elite_ui.css")
menu = read("src/assets/webui/main_menu.html")
loading = read("src/assets/webui/loading.html")
panel = read("src/assets/webui/system_map_panel.html")
local_session = read("src/assets/webui/local_session_menu.html")
remote_session = read("src/assets/webui/multiplayer_session_menu.html")
webview = read("src/ui/browser/GameWebView.cpp")
window = read("src/window/Window.cpp")

# Cross-document service navigation must be completed by the outgoing document,
# not by a guessed 180ms C++ deadline.
for token in (
    "service_ui_fade_out_complete|",
    "completeServiceUiTransition",
    "m_serviceUiTransitionSerial",
    "m_serviceUiTransitionFailSafeDeadline",
):
    if token not in app + app_h:
        fail(f"acknowledged service transition contract missing: {token}")
if "m_serviceUiTransitionDeadline" in app + app_h:
    fail("legacy fixed-deadline service transition state still exists")
if "m_serviceUiTransitionDeadline = glfwGetTime() + 0.18" in app:
    fail("normal service navigation still depends on a guessed 180ms timer")

# navigate() acceptance is not presentation readiness. Loading and SystemMap use
# explicit DOM/payload acknowledgements, with the map panel prepared while the
# WebView remains native-hidden.
for token in (
    "loading_ui_ready",
    "system_map_panel_ready",
    "system_map_panel_prepared",
    "m_systemMapPanelPrepared",
    "m_systemMapPanelNavigationPending",
    "m_systemMapPanelStateRequested",
):
    if token not in app + app_h + panel:
        fail(f"page-specific prepare-before-present seam missing: {token}")
if 'navigateGameUi(GameUiMode::SystemMap);\n                m_gameUi.markLoaded(GameUiMode::SystemMap)' in app:
    fail("SystemMap panel is marked ready immediately after navigate()")
for token in (
    "setLoadingUiProgress",
    "m_loadingUiProgress",
    "!m_gameUi.isLoaded(GameUiMode::Loading)",
):
    if token not in app + app_h:
        fail(f"cached loading-state presentation seam missing: {token}")

# First presentation is assembled while hidden: no default Home route and no
# placeholder map panel may be visible before native state is applied.
if "mainMenuShell.show('home')" in menu:
    fail("main menu still exposes a default Home route before native route state")
for token in (
    "waitForDocumentDependencies",
    "settleLayout",
    "animate: !firstPresentation",
    "revealPreparedDocument",
):
    if token not in menu + kit:
        fail(f"prepare-before-present service shell primitive missing: {token}")
for page_name, page in (
    ("main", menu),
    ("loading", loading),
    ("system-map panel", panel),
    ("local session", local_session),
    ("multiplayer session", remote_session),
):
    if "elite-ui-boot" not in page:
        fail(f"{page_name} document is not boot-hidden")
if "systemMapPanelPresented" not in panel or "system_map_panel_prepared" not in panel:
    fail("SystemMap panel can expose placeholder content before authoritative payload preparation")

# Same-document routing must finish one transition before exposing a superseded
# destination. The old serial/early-return implementation could leave the only
# active view at opacity:0 during rapid native state updates.
for token in (
    "pendingRequest",
    "drainRequests",
    "A newer route that arrived while the old route was fading wins",
):
    if token not in kit:
        fail(f"serialized last-destination-wins navigation missing: {token}")
if "if (serial !== transitionSerial) return" in kit:
    fail("navigation can still abandon a half-applied transition on serial mismatch")

# Visual completion follows CSS transitionend, with timeout only as a browser
# fault fallback. Route scrollbars must not kick the outer service frame.
for token in ("waitForCssTransition", "transitionend"):
    if token not in kit:
        fail(f"CSS transition completion primitive missing: {token}")
for token in ("elite-panel--shell", "scrollbar-gutter: stable", "overflow: hidden"):
    if token not in menu + css:
        fail(f"stable service-frame layout contract missing: {token}")

# F9-F12 first-open prepares the requested destination and its side panel while
# gameplay remains the visible owner. Never expose a default Galaxy first.
for token in (
    "beginPlayerNavigationMapEntry",
    "playerNavigationMapEntryTargetReady",
    "armPlayerNavigationMapEntryPresentation",
    "consumePreparedPlayerNavigationMapEntry",
    "prepareSystemMapUiForEntry",
):
    if token not in app + app_h + space + space_h:
        fail(f"prepare-before-present map entry seam missing: {token}")

hotkey_start = app.find("auto requestNavigationLevel")
hotkey_end = app.find("bool stateChangedFromWebView", hotkey_start)
hotkey_block = app[hotkey_start:hotkey_end]
if "openGameUi(GameUiMode::SystemMap);" in hotkey_block:
    fail("F9-F12 still opens SystemMap before the requested map level is prepared")
if "m_window->swapBuffers();" in hotkey_block:
    fail("F9-F12 still swaps an unrendered/stale back buffer")
if "m_systemMapRenderer.mode() == SystemMapRenderer::Mode::Galaxy" not in space:
    fail("SystemMap visibility entry still assumes every prepared target is Galaxy")

# Presentation ownership changes only after the outgoing frame has actually been
# rendered/captured and swapped. Capturing an arbitrary next-frame back buffer is
# explicitly forbidden.
for token in (
    "capturePresentationSource",
    "m_playerNavigationMapEntrySourceCaptured",
    "openPreparedSystemMapAfterSwap",
    "closePreparedSystemMapAfterSwap",
    "beginPresentationCrossfade",
):
    if token not in app + space + renderer_h + renderer_common:
        fail(f"frame-owned map presentation transaction missing: {token}")
if "m_presentationFadeCapturePending" in renderer + renderer_h + renderer_common:
    fail("first map entry still captures an unspecified next-frame back buffer")
if "captureMapTransitionSnapshot(viewport);\n        m_presentation" in renderer:
    fail("map renderer still assumes the pre-render back buffer contains gameplay")

# Map close is symmetric: panel fades first, the current map frame is captured,
# then gameplay gets one fully covered warmup frame before the map dissolves.
for token in (
    "requestSystemMapClose",
    "beginPlayerNavigationMapExit",
    "consumePreparedPlayerNavigationMapExit",
):
    if token not in app + app_h + space + space_h:
        fail(f"symmetric map exit transition missing: {token}")
if "mode == GameUiMode::SystemMap" not in app[app.find("beginServiceUiTransition"):]:
    fail("SystemMap side panel does not participate in acknowledged fade-out")

# Visible map-to-map transitions keep the outgoing map opaque through one full
# incoming render frame before crossfade starts.
for token in (
    "AwaitingIncomingFrame",
    "needsIncomingWarmup",
    "incomingFrameRendered",
):
    if token not in map_transition + renderer:
        fail(f"incoming-map warmup phase missing: {token}")
if "drawMapTransitionSnapshot(\n            viewport,\n            1.0f" not in renderer:
    fail("incoming map is not kept fully covered during its warmup frame")
for token in (
    "PresentationCrossfadePhase::AwaitingIncomingFrame",
    "drawPresentationCrossfadeOverlay",
):
    if token not in renderer_common + renderer_h + space:
        fail(f"gameplay/map first-frame cover missing: {token}")


# Native HWND visibility is part of presentation atomicity. CSS cannot hide
# WebView2's white controller surface before the first document paints. The
# top-level GLFW window starts hidden/primed dark; each WebView navigation is
# physically hidden until a page-specific prepared acknowledgement arrives.
for token in (
    "glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE)",
    "void Window::show()",
):
    if token not in window:
        fail(f"startup native-surface preparation missing: {token}")
for token in (
    "controller surface is white",
    "Cross-document navigation is prepared off-screen",
    "setVisible(false);",
):
    if token not in webview:
        fail(f"WebView native-hide boundary missing: {token}")
for token in (
    "main_menu_prepared",
    "loading_ui_prepared",
    "session_menu_prepared",
    "presentPreparedGameUi",
):
    if token not in app + menu + loading + local_session + remote_session:
        fail(f"prepared-before-native-show handshake missing: {token}")

# The target-agnostic map side panel stays warm during gameplay. Re-navigating
# HTML/fonts on every F9-F12 entry made the transition latency browser-bound.
for token in (
    "prewarmSystemMapPanel",
    "m_systemMapPanelPrewarmPending",
    "preserveSystemMapPanel",
):
    if token not in app + app_h:
        fail(f"persistent hidden SystemMap panel prewarm missing: {token}")

print("[PASS] menus/maps use prepared hidden destinations and frame-owned crossfades")
