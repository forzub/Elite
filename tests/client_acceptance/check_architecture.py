#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
HARNESS = ROOT / "src/game/diagnostics/ClientAcceptanceHarness.cpp"
MAPPER = ROOT / "src/game/ship/controller/PlayerInputMapper.cpp"
GAME_CLIENT = ROOT / "src/game/client/GameClient.cpp"
APPLICATION = ROOT / "src/core/Application.cpp"
SPACE = ROOT / "src/game/SpaceState.cpp"
SPACE_HEADER = ROOT / "src/game/SpaceState.h"
SPACE_INIT = ROOT / "src/game/SpaceState-Init.cpp"
HUD_PRESENTATION = ROOT / "src/game/presentation/ClientHudPresentation.cpp"
GALAXY_NAV_PRESENTATION = ROOT / "src/game/presentation/GalaxyNavigationPresentation.cpp"
SKY_LABEL_PRESENTATION = ROOT / "src/game/presentation/StarSystemLabelPresentation.cpp"
MAP_PANEL_PRESENTATION = ROOT / "src/game/presentation/SystemMapPanelPresentation.cpp"
IN_SESSION_RENDERER = ROOT / "src/ui/presentation/InSessionPresentationRenderer.cpp"
GALAXY_MAP_RENDERER = ROOT / "src/game/system_map/GalaxyMapRenderer.cpp"
SYSTEM_MAP_COMMON = ROOT / "src/game/system_map/SystemMapRendererCommon.inl"
STARFIELD_RENDERER = ROOT / "src/render/starfield/GalaxyStarfieldRenderer.cpp"
SCENE_RENDERER = ROOT / "src/scene/SceneRenderer.cpp"
CMAKE = ROOT / "CMakeLists.txt"
FLIGHT_INDICATOR_RENDERER = ROOT / "src/render/cockpit/FlightVectorIndicatorRenderer.cpp"


def fail(message: str) -> None:
    print(f"[FAIL] client acceptance architecture: {message}", file=sys.stderr)
    raise SystemExit(1)


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


harness = read(HARNESS)
mapper = read(MAPPER)
game_client = read(GAME_CLIENT)
application = read(APPLICATION)
space = read(SPACE)
space_header = read(SPACE_HEADER)
space_init = read(SPACE_INIT)
hud_presentation = read(HUD_PRESENTATION)
galaxy_nav_presentation = read(GALAXY_NAV_PRESENTATION)
sky_label_presentation = read(SKY_LABEL_PRESENTATION)
map_panel_presentation = read(MAP_PANEL_PRESENTATION)
in_session_renderer = read(IN_SESSION_RENDERER)
galaxy_map_renderer = read(GALAXY_MAP_RENDERER)
system_map_common = read(SYSTEM_MAP_COMMON)
starfield_renderer = read(STARFIELD_RENDERER)
scene_renderer = read(SCENE_RENDERER)
cmake = read(CMAKE)
flight_indicator_renderer = read(FLIGHT_INDICATOR_RENDERER)

# The headless suite must continue to exercise the production session path.
required_harness_tokens = (
    "game::host::LocalGameSession",
    "client.prepareGameplayFrame",
    "client.submitInput",
    "session.advance",
    "client.update",
    "requestGalaxyMapSnapshot",
    "composeSystemMapSnapshot",
    "composeDetailMapSnapshot",
    "composeHubMapSnapshot",
    "acknowledgedControlTick",
    "buildPlayerHudTelemetry",
    "buildFlightVectorIndicatorPresentation",
    "applyPlayerHudTelemetry",
    "buildSystemMapPanelPresentation",
    "resolveGalaxyPlayerMarkerPosition",
    "buildGameSystemSkyLabel",
    "CoordinateDisplayService",
    "directTargetForFunctionKey",
    "F1-F12 DIRECT SELECTOR + F9-F12 GENERATION ROUTING",
    "constellationOverlayEnabled",
)

for token in required_harness_tokens:
    if token not in harness:
        fail(f"production-path token disappeared: {token}")

for stale_map_rpc in (
    "requestSystemMapSnapshot",
    "requestDetailMapSnapshot",
    "requestHubMapSnapshot",
):
    if stale_map_rpc in harness:
        fail(f"client acceptance harness still exercises removed map RPC: {stale_map_rpc}")

for forbidden in (
    ".world().predict(",
    "SharedShipPhysics::integrate(",
    "DynamicMotionSystem::applyHubTacticalInput(",
    "DynamicMotionSystem::applyLocalFrameInput(",
    "GameServer server",
    "GameSimulation simulation",
):
    if forbidden in harness:
        fail(f"harness bypasses the client/server path via: {forbidden}")

if "updateFromKeyState(control, keys, currentLocalControlLaw)" not in mapper:
    fail("runtime PlayerInputMapper no longer shares the injectable mapping path")

# F1-F12 are one protected player-facing presentation keyboard. Flight is an
# explicit peer target (F1-F4), services are F5-F8 and maps are F9-F12. The
# same selector is a no-op; a later selector replaces a still-pending request.
window = read(ROOT / "src/window/Window.cpp")
function_router = read(ROOT / "src/ui/presentation/PresentationFunctionKeyRouter.cpp")

for token in (
    "pollFunctionKeyPress", "FunctionKeyPress", "WM_KEYDOWN", "WM_SYSKEYDOWN",
    "VK_F1", "VK_F12", "pressedSincePoll", "0x0001",
):
    if token not in window:
        fail(f"message-backed F-key edge capture lost: {token}")

for token in (
    "directTargetForFunctionKey(functionKey)",
    "requestPresentationTarget(*target)",
):
    if token not in application:
        fail(f"Application no longer consumes direct F1-F12 target events: {token}")

for token in (
    "case 9: return GameUiTarget::forNavigation(NavigationPresentationView::Galaxy)",
    "case 10: return GameUiTarget::forNavigation(NavigationPresentationView::System)",
    "case 11: return GameUiTarget::forNavigation(NavigationPresentationView::Detail)",
    "case 12: return GameUiTarget::forNavigation(NavigationPresentationView::Local)",
):
    if token not in function_router:
        fail(f"F9-F12 direct route lost: {token}")

for token in (
    "preparePlayerNavigationMapLevel",
    "m_gameUi.armSceneTarget(requested)",
    "commitPreparedPresentationAfterSwap",
):
    if token not in application + space + space_header:
        fail(f"F9-F12 one-surface prepare-to-commit chain lost: {token}")
for obsolete in (
    "m_mapPanelExpectedStateSerial",
    "m_mapPanelPreparedStateSerial",
    "system_map_panel_state_prepared|",
    "m_mapPanelWebView",
):
    if obsolete in application + space + space_header:
        fail(f"obsolete F9-F12 WebView readiness path returned: {obsolete}")

for forbidden in (
    "GameUiNavigationAction::Close",
    "navigationAction(",
    "closeGameUi()",
):
    if forbidden in application:
        fail(f"function-key routing regressed to toggle/gameplay fallback: {forbidden}")

for token in (
    "PlayerNavigationMapLevel::Galaxy",
    "PlayerNavigationMapLevel::System",
    "PlayerNavigationMapLevel::Detail",
    "PlayerNavigationMapLevel::Local",
    "DetailSceneKind::SpatialVolume",
):
    if token not in space:
        fail(f"map hotkey level classifier lost: {token}")

# Ctrl+F10 remains available to the production flight mapper. The application
# latches every physical F-key edge, but plain F10 navigation is only selected
# inside the !ctrlDown && !altDown block, so releasing Ctrl cannot create a
# second map command from the same physical press.
for token in (
    "if (sessionReady && !ctrlDown && !altDown)",
    "functionKey == 11 && ctrlDown && !altDown",
    "resolveF12HotkeyAction(ctrlDown, altDown, sessionReady)",
):
    if token not in application:
        fail(f"modified F-key chord separation lost: {token}")

for token in (
    "const bool f10Down = keys.isKeyPressed(GLFW_KEY_F10)",
    "CtrlF10State::ReleaseDebounce",
    "kCtrlF10ReleaseDebounceSamples",
    "localControlLawCommandValid",
    "requestedLocalControlLaw",
):
    if token not in mapper:
        fail(f"Ctrl+F10 flight-law mapping lost: {token}")

for token in (
    "m_hasPendingLocalControlLawCommand = true",
    "m_latestControl.localControlLawCommandValid = false",
    "consumeLocalControlLawCommand",
    "m_hasPendingLocalControlLawCommand = false",
):
    if token not in game_client:
        fail(f"render/fixed-step command latch lost: {token}")

for token in (
    "CoordinateDisplayService::instance()",
    ".cycle()",
    "functionKey == 11 && ctrlDown && !altDown",
):
    if token not in application:
        fail(f"Ctrl+F11 coordinate-format path no longer uses: {token}")

for token in (
    "functionKey == 12",
    "resolveF12HotkeyAction",
    "F12HotkeyAction::ToggleConstellations",
    "toggleConstellationOverlay()",
    "F12HotkeyAction::CycleSkyCulture",
    "cycleSkyCulture()",
    "F12HotkeyAction::CycleUiLanguage",
    "cycleUiLanguage()",
):
    if token not in application:
        fail(f"F12 service-chord path no longer uses: {token}")

for token in (
    "m_constellationOverlayEnabled",
    "setConstellationOverlayEnabled",
):
    if token not in space:
        fail(f"constellation overlay runtime path no longer uses: {token}")

for token in (
    "CoordinateDisplayService::instance()",
    ".format()",
    "formatDisplayName()",
    '" [CTRL+F11]"',
):
    if token not in system_map_common:
        fail(f"map coordinate footer stopped exposing current format state: {token}")

# HUD speed must be canonical travel-frame-relative velocity; legacy mirrors
# are explicitly forbidden because they diverge from DynamicMotionState.
if "motion.localVelocityMps" not in hud_presentation:
    fail("HUD no longer reads canonical travel-frame relative velocity")
for forbidden in (
    "renderTransform.localVelocity",
    "renderTransform.forwardVelocity",
):
    if forbidden in hud_presentation:
        fail(f"HUD regressed to legacy velocity mirror: {forbidden}")

for token in (
    "buildFlightVectorIndicatorPresentation",
    "motion.localVelocityMps",
    "shipModelToIndicatorBasis",
    "displayUnitsPerMps",
    "speedUnitLabel",
):
    if token not in hud_presentation:
        fail(f"flight-vector instrument presentation contract lost: {token}")

for token in (
    "buildFlightVectorIndicatorPresentation(",
    "m_flightVectorIndicatorRenderer.render(",
):
    if token not in space:
        fail(f"cockpit flight-vector instrument left production render path: {token}")

for token in (
    "shipModelToIndicatorBasis",
    "speedFraction01",
    "presentation.speedText",
    "presentation.modeText",
):
    if token not in flight_indicator_renderer:
        fail(f"flight-vector cockpit renderer lost presentation field: {token}")

for forbidden in (
    "localVelocityMps",
    "worldVelocityMps",
    "referenceVelocityMps",
):
    if forbidden in flight_indicator_renderer:
        fail(f"flight-vector renderer started owning motion semantics: {forbidden}")

if "src/render/cockpit/FlightVectorIndicatorRenderer.cpp" not in cmake:
    fail("flight-vector cockpit renderer disappeared from production build")

# The old WebView map-command router was removed with the in-session map panel.
# F9-F12 and direct map picking now exercise the production map routes without
# serializing commands through a browser bridge.
for forbidden in (
    "parseSystemMapUiCommand",
    "dispatchSystemMapUiCommand",
    "ISystemMapUiTarget",
):
    if forbidden in application + space + space_header:
        fail(f"obsolete browser map-command router returned: {forbidden}")

# HUD data must be built from ClientWorldState and applied to the same five UIText IDs
# created by SpaceState initialization. The architecture guard catches a renamed/missing
# visible field even before the C++ runtime scenario gets a chance to run.
for token in (
    "buildPlayerHudTelemetry(",
    "applyPlayerHudTelemetry(",
):
    if token not in space:
        fail(f"SpaceState HUD no longer uses tested presentation seam: {token}")

for token in (
    '"main_coord_cell"',
    '"main_coord_x"',
    '"main_coord_y"',
    '"main_coord_z"',
    '"main_coord_v"',
):
    if token not in space_init:
        fail(f"production HUD binding disappeared from SpaceState init: {token}")
    if token not in hud_presentation:
        fail(f"HUD presenter no longer writes production binding: {token}")

# Keep formatting/data ownership in the tested presenter, not duplicated in SpaceState.
for token in (
    '"%s %lld %lld %lld"',
    '"%s %.1f m/s  %s"',
    "textProfile.cellLabel.c_str()",
    "textProfile.relativeVelocityLabel.c_str()",
):
    if token in space:
        fail(f"HUD formatting escaped the tested presenter back into SpaceState: {token}")
    if token not in hud_presentation:
        fail(f"HUD formatting contract disappeared from presenter: {token}")

# Map snapshots -> typed native panel presentation is also a tested seam.
# The in-session side panel must not serialize through JSON or a WebView HWND.
for token in (
    "buildSystemMapPanelPresentation(input)",
    "renderInSessionPresentationOverlay()",
):
    if token not in space:
        fail(f"SpaceState native map-panel seam missing: {token}")

for token in (
    "SystemMapPanelPresentation",
    "SystemMapPanelSystemItem",
    "panel.currentSystemId",
    "panel.currentSystemName",
    "panel.selectedSystemId",
    "panel.systems",
    "panel.canOpenDetail",
    "panel.canOpenHub",
    "panel.systemLayerIsSpace",
):
    if token not in map_panel_presentation + harness:
        fail(f"typed native map panel contract disappeared: {token}")

for token in (
    "testNativeSystemMapPanelActionContract",
    "resolveSystemMapPanelAction",
    "SystemMapPanelCommandType::OpenSelectedGalaxyTarget",
    "SystemMapPanelCommandType::SelectSystem",
    "SystemMapPanelCommandType::LoadedSystem",
    "SystemMapPanelCommandType::SelectedDetail",
    "SystemMapPanelCommandType::Hub",
    "buildSystemMapPanelNavigationActions",
):
    if token not in map_panel_presentation + harness:
        fail(f"native map panel action contract disappeared: {token}")

for token in (
    "rebuildUnboundSpatialDetailMap",
    "empty-sector Details did not compose without a celestial system",
    "const int selectedId = m_loadedSystemMapId",
    "setSystemMapLoadedSystemMode()",
):
    if token not in game_client + space + harness + read(ROOT / "src/game/client/ClientDetailMapBridge.h"):
        fail(f"selected empty-space Details regression guard disappeared: {token}")

for forbidden in (
    "SystemMapPanelActionType::Close",
    "layout.closeButton",
    "ToggleMode",
):
    if forbidden in map_panel_presentation + in_session_renderer:
        fail(f"obsolete fixed-slot/close map-panel behavior returned: {forbidden}")

for token in (
    "renderSystemMapPanel(",
    "solidRectPx(",
    "PanelRatio",
):
    if token not in in_session_renderer:
        fail(f"native map panel renderer missing: {token}")

for obsolete_path in (
    ROOT / "src/assets/webui/system_map_panel.html",
    ROOT / "src/assets/webui/service_shell.html",
    ROOT / "src/assets/webui/service_panel.js",
):
    if obsolete_path.exists():
        fail(f"obsolete in-session browser asset survived: {obsolete_path.relative_to(ROOT)}")

for forbidden in (
    "buildSystemMapPanelPayload",
    "window.setSystemMapPanel",
    "system_map_panel_state_prepared|",
    "m_mapPanelWebView",
):
    if forbidden in application + space + map_panel_presentation + in_session_renderer:
        fail(f"obsolete map-panel browser seam returned: {forbidden}")

# The Galaxy player marker and the side-panel distance must share one production
# position resolver. Otherwise a ship can move correctly while map navigation
# silently keeps drawing/calculating from the system center.
for token in (
    "navigation.currentSystemId",
    "navigation.systemLocalAu",
    "AuPerLightYear",
    "toGalacticLy",
):
    if token not in galaxy_nav_presentation:
        fail(f"Galaxy player-marker resolver lost navigation input: {token}")

if "resolveGalaxyPlayerMarkerPosition(" not in galaxy_map_renderer:
    fail("GalaxyMapRenderer no longer uses the tested player-marker resolver")

for token in (
    "resolveGalaxyPlayerMarkerPosition(*input.galaxy, *input.navigation)",
    "candidate.positionLy - playerMarker.positionLy",
    "item.distanceFromPlayerLy",
):
    if token not in map_panel_presentation:
        fail(f"map-panel player-distance path no longer uses live player position: {token}")

# Gameplay sky labels must preserve the authored game-system name even when a
# game system is merged onto an astronomical catalog star with its own name.
if starfield_renderer.count("star.gameSystemName = system.name;") < 2:
    fail("starfield merge no longer assigns authored game-system names to both merged and added stars")

for token in (
    "buildGameSystemSkyLabel(",
    "star.gameSystemName",
):
    if token not in scene_renderer:
        fail(f"SceneRenderer sky labels no longer use tested game-system naming seam: {token}")

for token in (
    "gameSystemName",
    "fallbackName",
    "fallbackId",
    '<< " ly"',
):
    if token not in sky_label_presentation:
        fail(f"game-system sky-label presenter lost contract: {token}")

for source in (
    "src/game/presentation/ClientHudPresentation.cpp",
    "src/game/presentation/GalaxyNavigationPresentation.cpp",
    "src/game/presentation/StarSystemLabelPresentation.cpp",
    "src/game/presentation/SystemMapPanelPresentation.cpp",
):
    if source not in cmake:
        fail(f"production presenter is not compiled by EliteGame: {source}")

print("[PASS] client acceptance architecture guard")
