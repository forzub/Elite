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
MAP_ROUTER = ROOT / "src/game/ui/SystemMapUiCommandRouter.cpp"
MAP_PANEL_HTML = ROOT / "src/assets/webui/system_map_panel.html"
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
map_router = read(MAP_ROUTER)
map_panel_html = read(MAP_PANEL_HTML)
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
    "requestSystemMapSnapshot",
    "requestDetailMapSnapshot",
    "requestHubMapSnapshot",
    "acknowledgedControlTick",
    "buildPlayerHudTelemetry",
    "buildFlightVectorIndicatorPresentation",
    "applyPlayerHudTelemetry",
    "parseSystemMapUiCommand",
    "dispatchSystemMapUiCommand",
    "buildSystemMapPanelPayload",
    "resolveGalaxyPlayerMarkerPosition",
    "buildGameSystemSkyLabel",
    "CoordinateDisplayService",
    "consumeF9Press",
    "consumeF10Press",
    "consumeF11Press",
    "consumeF12Press",
    "constellationOverlayEnabled",
)

for token in required_harness_tokens:
    if token not in harness:
        fail(f"production-path token disappeared: {token}")

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

# Function-key map layout is now a protected player-facing contract.
for token in (
    "VK_F9",
    "VK_F10",
    "VK_F11",
    "VK_F12",
    "consumeF9Press",
    "consumeF10Press",
    "consumeF11Press",
    "consumeF12Press",
    "setSystemMapGalaxyMode()",
    "setSystemMapPlayerSystemMode()",
    "setSystemMapPlayerDetailMode()",
    "setSystemMapPlayerLocalMode()",
):
    if token not in application:
        fail(f"current F9-F12 navigation layout no longer uses: {token}")

for token in (
    "navigationAction(",
    "isPlayerNavigationMapLevel(level)",
    "GameUiNavigationAction::Close",
    "closeGameUi()",
):
    if token not in application:
        fail(f"same-level map close / cross-level switch contract lost: {token}")

for token in (
    "PlayerNavigationMapLevel::Galaxy",
    "PlayerNavigationMapLevel::System",
    "PlayerNavigationMapLevel::Detail",
    "PlayerNavigationMapLevel::Local",
    "DetailSceneKind::SpatialVolume",
):
    if token not in space:
        fail(f"map hotkey level classifier lost: {token}")

# Ctrl+F10 must remain available to the production flight mapper instead of
# being consumed as an F10 map command. Application still latches physical F10
# so releasing Ctrl first cannot turn one chord into a second map command.
for token in (
    "f10PressedEdge",
    "consumeF10Press(f10Down)",
    "f10PressedEdge && space && !ctrlDown",
):
    if token not in application:
        fail(f"F10/Ctrl+F10 edge separation lost: {token}")

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
    "if (ctrlDown)",
):
    if token not in application:
        fail(f"Ctrl+F11 coordinate-format path no longer uses: {token}")

for token in (
    "consumeF12Press",
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

# Browser command parsing + dispatch must be the exact production seam exercised by acceptance.
for token in (
    "parseSystemMapUiCommand(webCommand)",
    "dispatchSystemMapUiCommand(",
):
    if token not in application:
        fail(f"Application no longer uses tested map UI seam: {token}")

if "public game::ui::ISystemMapUiTarget" not in space_header:
    fail("SpaceState no longer implements the tested map UI action target")

for token in (
    "target->selectSystemMapSystem(command.systemId)",
    "target->setSystemMapCurrentSystemMode()",
    "target->setSystemMapGalaxyMode()",
    "target->setSystemMapHubMode()",
    "target->setSystemMapLoadedDetailMode()",
    "target->setSystemMapDetailMode()",
    "closeSystemMap()",
):
    if token not in map_router:
        fail(f"map command dispatch action disappeared: {token}")

# The actual HTML controls must keep speaking the command vocabulary that the router tests.
for token in (
    '"close_system_map"',
    "`system_map_open_selected:${selectedSystemId}`",
    '"system_map_current_system"',
    '"system_map_galaxy"',
    '"system_map_detail"',
    '"system_map_planet"',
    '"system_map_hub"',
    "`system_map_select:${s.id}`",
):
    if token not in map_panel_html:
        fail(f"system-map panel stopped emitting production command: {token}")

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

# Map snapshots -> JSON -> WebView panel is also a tested presentation seam.
for token in (
    "buildSystemMapPanelPayload(input)",
    '"if (window.setSystemMapPanel) window.setSystemMapPanel("',
):
    if token not in space:
        fail(f"SpaceState map panel no longer uses tested output seam: {token}")

for token in (
    'payload["mode"]',
    'payload["currentSystemId"]',
    'payload["currentSystemName"]',
    'payload["selectedSystemId"]',
    'payload["systems"]',
    'payload["canOpenDetail"]',
    'payload["canOpenHub"]',
):
    if token not in map_panel_presentation:
        fail(f"map panel payload contract disappeared: {token}")

for token in (
    "window.setSystemMapPanel = function(payload)",
    "payload.mode",
    "payload.currentSystemName",
    "payload.systems",
    "payload.currentSystemId",
    "payload.selectedSystemId",
    "payload.canOpenDetail",
    "payload.canOpenHub",
):
    if token not in map_panel_html:
        fail(f"WebView map panel stopped consuming tested payload field: {token}")

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
    "resolveGalaxyPlayerMarkerPosition(*galaxy, *nav)",
    "candidate.positionLy - playerMarker.positionLy",
    'item["distanceFromPlayerLy"]',
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
    "src/game/ui/SystemMapUiCommandRouter.cpp",
):
    if source not in cmake:
        fail(f"production presenter/router is not compiled by EliteGame: {source}")

print("[PASS] client acceptance architecture guard")
