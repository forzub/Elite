#!/usr/bin/env python3
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(rel: str) -> str:
    path = ROOT / rel
    if not path.exists():
        fail(f"missing required file: {rel}")
    return path.read_text(encoding="utf-8", errors="replace")


def fail(message: str) -> None:
    print(f"[FAIL] feature contract: {message}", file=sys.stderr)
    raise SystemExit(1)


def require(condition: bool, message: str) -> None:
    if not condition:
        fail(message)


def extract_braced_function(text: str, signature: str) -> str:
    start = text.find(signature)
    if start < 0:
        fail(f"missing function: {signature}")

    brace = text.find("{", start)
    if brace < 0:
        fail(f"function has no body: {signature}")

    depth = 0
    for index in range(brace, len(text)):
        ch = text[index]
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return text[start:index + 1]

    fail(f"unterminated function body: {signature}")
    return ""


def extract_struct_body(text: str, name: str) -> str:
    match = re.search(rf"struct\s+{re.escape(name)}\s*\{{(.*?)\n\}};", text, re.S)
    if not match:
        fail(f"missing struct body: {name}")
    return match.group(1)


def check_debug_control_schema() -> None:
    settings_h = read("src/debug/DebugSettings.h")
    codec_h = read("src/game/debug/DebugControlSettingsCodec.h")
    space_cpp = read("src/game/SpaceState.cpp")
    html = read("src/assets/webui/debug_control.html")
    debug_iface = read("src/game/debug/IDebugSessionControl.h")
    server_runtime = read("src/game/server/ServerRuntime.cpp")
    main_cpp = read("src/main.cpp")

    body = extract_struct_body(settings_h, "DebugRenderSettings")
    fields = [
        name
        for _, name in re.findall(
            r"^\s*(bool|int|float|double|std::string|glm::vec4)\s+"
            r"([A-Za-z_]\w*)\s*(?:=|\{)",
            body,
            re.M,
        )
    ]

    # These are diagnostic/runtime implementation details rather than Debug
    # Control preferences. Adding another field requires an explicit decision
    # here instead of silently leaving a user-facing setting unwired.
    internal_only = {
        "drawSeamDebug",
        "captureSeamDebug",
    }

    require(internal_only.issubset(set(fields)), "internal-only debug field list is stale")
    exposed = [field for field in fields if field not in internal_only]

    encode = extract_braced_function(codec_h, "inline json encodeRenderSettings")
    apply = extract_braced_function(codec_h, "inline void applyRenderSettings")
    push = extract_braced_function(space_cpp, "void SpaceState::pushDebugControlState()")
    apply_space = extract_braced_function(
        space_cpp,
        "void SpaceState::applyDebugControlPayload(const json& payload)",
    )
    html_build = extract_braced_function(html, "function buildPayload()")
    html_apply = extract_braced_function(html, "function applySettingsPayload(p)")

    encoded_keys = set(re.findall(r'payload\["([A-Za-z_]\w*)"\]', encode))
    require(
        encoded_keys == set(exposed),
        "Debug Control codec key set differs from exposed DebugRenderSettings fields: "
        f"missing={sorted(set(exposed) - encoded_keys)}, "
        f"extra={sorted(encoded_keys - set(exposed))}",
    )

    build_keys = set(re.findall(r"^\s*([A-Za-z_]\w*)\s*:", html_build, re.M))
    special_build_keys = {"debugUniverseTimeSimulation", "debugUniverseTimeScale"}
    require(
        build_keys == set(exposed) | special_build_keys,
        "Debug Control HTML payload key set drifted from C++ settings contract: "
        f"missing={sorted((set(exposed) | special_build_keys) - build_keys)}, "
        f"extra={sorted(build_keys - (set(exposed) | special_build_keys))}",
    )

    for field in exposed:
        require(
            re.search(rf'payload\["{re.escape(field)}"\]', encode) is not None,
            f"DebugRenderSettings.{field} is not encoded into Debug Control state",
        )
        require(
            (
                re.search(rf'payload\.value\(\s*"{re.escape(field)}"', apply) is not None
                or re.search(rf'payload\.contains\(\s*"{re.escape(field)}"', apply) is not None
            ),
            f"DebugRenderSettings.{field} is not applied from Debug Control payload",
        )
        require(
            re.search(rf"\b{re.escape(field)}\s*:", html_build) is not None,
            f"DebugRenderSettings.{field} is not emitted by debug_control.html buildPayload()",
        )
        require(
            re.search(rf"\bp\.{re.escape(field)}\b", html_apply) is not None,
            f"DebugRenderSettings.{field} is not restored into debug_control.html controls",
        )

    for field in internal_only:
        require(
            f'payload["{field}"]' not in encode,
            f"internal-only debug field {field} leaked into persisted/UI settings",
        )

    dom_ids = set(re.findall(r'id=["\']([^"\']+)["\']', html))
    referenced_ids = set(
        re.findall(r"document\.getElementById\(['\"]([^'\"]+)['\"]\)", html_build + html_apply)
    )
    missing_ids = sorted(referenced_ids - dom_ids)
    require(not missing_ids, f"Debug Control JS references missing DOM ids: {missing_ids}")

    for command in ("request_snapshot", "apply_settings", "save_defaults", "reset_settings"):
        require(command in html, f"Debug Control HTML lost command '{command}'")
        require(command in space_cpp, f"SpaceState lost Debug Control command route '{command}'")

    for key in ("debugUniverseTimeSimulation", "debugUniverseTimeScale"):
        require(
            re.search(rf"\b{key}\s*:", html_build) is not None,
            f"Debug Control HTML no longer sends {key}",
        )
        require(key in apply_space, f"SpaceState no longer consumes {key}")

    require(
        "debugUniverseTimeSimulation" in push
        and "debugUniverseTimeConfiguredScale" in push,
        "Debug Control state no longer publishes authoritative fast-universe status",
    )

    for method in (
        "universeTimeSimulation() const",
        "universeTimeScale() const",
        "configuredUniverseTimeScale() const",
        "setUniverseTimeSimulation(bool enabled, double timeScale)",
    ):
        require(method in debug_iface, f"debug-session interface lost {method}")

    require(
        "setDebugUniverseTimeSimulation" in server_runtime,
        "ServerRuntime no longer routes universe-time debug control to GameServer",
    )
    require(
        re.search(r'arg\s*==\s*"--self-test-fast-universe"', main_cpp) is not None,
        "real-scene fast-universe smoke command is missing from EliteGame",
    )



def check_debug_control_runtime_wiring() -> None:
    html = read("src/assets/webui/debug_control.html")
    space_cpp = read("src/game/SpaceState.cpp")
    space_init = read("src/game/SpaceState-Init.cpp")
    scene_cpp = read("src/scene/SceneRenderer.cpp")
    policy_h = read("src/scene/SceneRenderPolicy.h")
    prepared_h = read("src/scene/PreparedScene.h")
    traffic_cpp = read("src/game/traffic/StationTrafficSystem.cpp")
    promo_cpp = read("src/game/promo/PromoSceneScenario.cpp")
    system_scene_cpp = read("src/game/system_map/SystemMapSceneRenderer.cpp")
    system_renderer = read("src/game/system_map/SystemMapRendererSystem.inl")
    application_cpp = read("src/core/Application.cpp")
    settings_h = read("src/debug/DebugSettings.h")
    debug_renderer_cpp = read("src/debug/render/DebugRenderer.cpp")
    hit_volume_renderer_cpp = read("src/debug/render/ServerHitVolumeRenderer.cpp")

    checkbox_rows = re.findall(
        r'<div class="([^"]*\brow\b[^"]*)">(.*?)</div>',
        html,
        re.S,
    )
    checkbox_ids = set()
    for classes, body in checkbox_rows:
        if 'type="checkbox"' not in body:
            continue

        id_match = re.search(r'<input\s+id="([^"]+)"\s+type="checkbox"[^>]*>', body, re.S)
        require(id_match is not None, "Debug Control checkbox row lost an input id")
        checkbox_id = id_match.group(1)
        checkbox_ids.add(checkbox_id)

        require(
            "checkboxRow" in classes.split(),
            f"checkbox {checkbox_id} no longer uses left-aligned checkboxRow layout",
        )
        label_match = re.search(
            rf'<label\s+for="{re.escape(checkbox_id)}">.*?</label>',
            body,
            re.S,
        )
        require(
            label_match is not None,
            f"checkbox {checkbox_id} lost its matching label/for association",
        )
        require(
            body.find("<input") < body.find("<label"),
            f"checkbox {checkbox_id} moved back to the right of its label",
        )

    require(len(checkbox_ids) >= 20, "Debug Control checkbox surface unexpectedly shrank")
    require(
        "C++-обработчики добавим следующим шагом" not in html,
        "Debug Control still advertises intentionally unwired visibility controls",
    )

    # Every checkbox must have a concrete runtime consumer, not merely survive
    # the HTML/JSON round trip. This is intentionally explicit: adding a new
    # checkbox requires deciding which client/server subsystem owns its effect.
    checkbox_runtime_consumers = {
        "postProcessEnabled": (application_cpp, "debugRender.postProcessEnabled"),
        "drawMeshes": (scene_cpp, "dbg.shouldDrawMeshes()"),
        "renderStarfield": (space_cpp, "mainPolicy.drawStarfield = dbg.renderStarfield;"),
        "renderShipUi": (space_cpp, "shouldRenderShipUi()"),
        "renderCockpit": (space_cpp, "shouldRenderCockpit()"),
        "renderRearCamera": (space_cpp, "shouldRenderRearCamera()"),
        "showStarLabels": (scene_cpp, "dbg.showStarLabels"),
        "showAllStarLabels": (scene_cpp, "dbg.showAllStarLabels"),
        "showConstellationHover": (scene_cpp, "dbg.showConstellationHover"),
        "renderPlayerShip": (space_cpp, "mainPolicy.drawPlayerShip = dbg.renderPlayerShip;"),
        "renderNpcShips": (space_cpp, "mainPolicy.drawNpcShips = dbg.renderNpcShips;"),
        "renderTrafficShips": (space_cpp, "mainPolicy.drawTrafficShips = dbg.renderTrafficShips;"),
        "renderRealShips": (space_cpp, "mainPolicy.drawRealShips = dbg.renderRealShips;"),
        "renderVisualShips": (space_cpp, "mainPolicy.drawVisualShips = dbg.renderVisualShips;"),
        "renderHubs": (space_cpp, "mainPolicy.drawHubs = dbg.renderHubs;"),
        "renderLargeObjects": (space_cpp, "mainPolicy.drawLargeObjects = dbg.renderLargeObjects;"),
        "renderCelestialBodies": (space_cpp, "mainPolicy.drawCelestial = dbg.renderCelestialBodies;"),
        "renderSystemMapObjects": (system_renderer, "debug::get().render.renderSystemMapObjects"),
        "drawAxes": (settings_h, "return drawAxes || drawWorldAxes;"),
        "drawWorldAxes": (scene_cpp, "dbg.shouldDrawWorldAxes()"),
        "drawObjectAxes": (scene_cpp, "dbg.shouldDrawObjectAxes()"),
        "drawModulePivots": (scene_cpp, "dbg.drawModulePivots"),
        "drawHitVolumes": (hit_volume_renderer_cpp, "if (!dbg.drawHitVolumes)"),
        "publishObjectOrientation": (scene_cpp, "dbg.publishObjectOrientation"),
        "hitVolumesOverlay": (hit_volume_renderer_cpp, "if (dbg.hitVolumesOverlay)"),
        "hideMeshesWhenDrawingHitVolumes": (settings_h, "drawHitVolumes && hideMeshesWhenDrawingHitVolumes"),
        "axesOverlay": (debug_renderer_cpp, "if (dbg.axesOverlay)"),
        "crossesOverlay": (debug_renderer_cpp, "if (dbg.crossesOverlay)"),
        "linesOverlay": (debug_renderer_cpp, "if (dbg.linesOverlay)"),
        "debugUniverseTimeSimulation": (space_cpp, 'payload.value(\n                "debugUniverseTimeSimulation"'),
        "debugControlAutoUpdates": (html, "if (!auto || !auto.checked) return;"),
    }

    require(
        checkbox_ids == set(checkbox_runtime_consumers),
        "Debug Control checkbox runtime-consumer table drifted: "
        f"missing={sorted(checkbox_ids - set(checkbox_runtime_consumers))}, "
        f"extra={sorted(set(checkbox_runtime_consumers) - checkbox_ids)}",
    )

    for checkbox_id, (source, token) in checkbox_runtime_consumers.items():
        require(
            token in source,
            f"Debug Control checkbox {checkbox_id} has no verified runtime consumer",
        )

    main_policy_bindings = {
        "renderPlayerShip": "mainPolicy.drawPlayerShip = dbg.renderPlayerShip;",
        "renderNpcShips": "mainPolicy.drawNpcShips = dbg.renderNpcShips;",
        "renderTrafficShips": "mainPolicy.drawTrafficShips = dbg.renderTrafficShips;",
        "renderRealShips": "mainPolicy.drawRealShips = dbg.renderRealShips;",
        "renderVisualShips": "mainPolicy.drawVisualShips = dbg.renderVisualShips;",
        "renderHubs": "mainPolicy.drawHubs = dbg.renderHubs;",
        "renderLargeObjects": "mainPolicy.drawLargeObjects = dbg.renderLargeObjects;",
        "renderCelestialBodies": "mainPolicy.drawCelestial = dbg.renderCelestialBodies;",
    }
    rear_policy_bindings = {
        key: value.replace("mainPolicy", "policy")
        for key, value in main_policy_bindings.items()
    }

    for field, token in main_policy_bindings.items():
        require(token in space_cpp, f"Debug Control {field} is not wired into main-scene policy")
    for field, token in rear_policy_bindings.items():
        require(token in space_init, f"Debug Control {field} is not wired into rear-camera policy")

    for token in (
        "bool shouldDrawRealShip(ShipRole role) const noexcept",
        "bool shouldDrawVisualShip(game::visual::VisualShipKind kind) const noexcept",
        "bool shouldDrawObject(ObjectType type) const noexcept",
        "return drawTrafficShips;",
        "return drawHubs;",
        "return drawCelestial;",
        "return drawLargeObjects;",
    ):
        require(token in policy_h, f"scene visibility policy lost runtime decision: {token}")

    require(
        "visual.kind = game::visual::VisualShipKind::Traffic;" in traffic_cpp,
        "station traffic is no longer tagged for the traffic visibility filter",
    )
    require(
        "ship.kind = game::visual::VisualShipKind::Promo;" in promo_cpp,
        "promo ships are no longer distinguished from traffic ships",
    )

    for token in (
        "policy.shouldDrawRealShip(item.role)",
        "policy.shouldDrawVisualShip(ship.kind)",
        "policy.shouldDrawObject(item.type)",
        "prepared.debugAssemblies",
        "dbg.publishObjectOrientation",
        "dbg.drawModulePivots",
        "ServerHitVolumeRenderer::render",
    ):
        require(token in scene_cpp, f"prepared renderer lost Debug Control runtime wiring: {token}")

    require(
        "std::vector<DebugAssemblyItem> debugAssemblies;" in prepared_h,
        "prepared scene no longer carries presentation-frame debug geometry data",
    )

    require(
        "debug::get().render.renderSystemMapObjects" in system_renderer,
        "Render system map objects checkbox is no longer consumed by the map renderer",
    )
    require(
        system_scene_cpp.count("if (options.drawObjects)") >= 2,
        "system-map object overlays/labels are no longer gated by the debug render option",
    )

def check_debug_panels() -> None:
    panel_h = read("src/ui/html/HtmlUiPanelId.h")
    message_h = read("src/ui/html/HtmlUiMessage.h")
    space_cpp = read("src/game/SpaceState.cpp")

    diagnostic_panels = {
        "DebugControl": "debug_control",
        "AttachmentEditor": "attachment_editor",
        "StructureDebug": "structure_debug",
        "VolumeViewer": "volume_viewer",
        "ShipCore": "ship_core",
        "FrustumDebug": "frustum_debug",
        "SystemMap": "system_map",
    }

    for enum_name, wire_name in diagnostic_panels.items():
        require(
            f"HtmlUiPanelId::{enum_name}" in panel_h,
            f"diagnostic panel enum disappeared: {enum_name}",
        )
        require(
            f'return "{wire_name}"' in panel_h,
            f"diagnostic panel lost toString route: {wire_name}",
        )
        require(
            f'panel == "{wire_name}"' in message_h,
            f"diagnostic panel lost incoming message route: {wire_name}",
        )
        require(
            (ROOT / "src/assets/webui" / f"{wire_name}.html").exists(),
            f"diagnostic panel asset disappeared: {wire_name}.html",
        )

    for enum_name in (
        "DebugControl",
        "AttachmentEditor",
        "StructureDebug",
        "VolumeViewer",
        "ShipCore",
        "FrustumDebug",
        "SystemMap",
    ):
        require(
            f"HtmlUiPanelId::{enum_name}" in space_cpp,
            f"SpaceState no longer handles diagnostic panel {enum_name}",
        )


    debug_control = read("src/assets/webui/debug_control.html")
    attachment_html = read("src/assets/webui/attachment_editor.html")
    attachment_payload = read("src/game/debug/AttachmentEditorPayload.cpp")
    ship_core_html = read("src/assets/webui/ship_core.html")
    structure_html = read("src/assets/webui/structure_debug.html")
    frustum_html = read("src/assets/webui/frustum_debug.html")
    volume_html = read("src/assets/webui/volume_viewer.html")
    server_cpp = read("src/ui/html/HtmlUiServer.cpp")

    launcher_pages = (
        "attachment_editor.html",
        "volume_viewer.html",
        "frustum_debug.html",
        "ship_core.html",
        "structure_debug.html",
    )
    for page in launcher_pages:
        require(
            f'href="{page}"' in debug_control,
            f"Debug Control launcher lost link to {page}",
        )

    for kind in (
        "CameraCockpit",
        "CameraRear",
        "CameraDrone",
        "DroneDock",
        "DroneLaunch",
        "DroneRecovery",
        "RepairWorkPoint",
        "EquipmentMount",
        "WeaponMuzzle",
        "MissileRack",
        "ContainerMount",
    ):
        require(
            f'ShipAttachmentKind::{kind}' in attachment_payload,
            f"unified attachment editor lost attachment kind {kind}",
        )

    for page_name, page_text in (
        ("debug_control.html", debug_control),
        ("attachment_editor.html", attachment_html),
        ("structure_debug.html", structure_html),
        ("volume_viewer.html", volume_html),
        ("ship_core.html", ship_core_html),
        ("frustum_debug.html", frustum_html),
    ):
        require(
            "location.host" in page_text,
            f"{page_name} no longer connects WebSocket through the page host (LAN contract)",
        )

    require(
        'resource = "/debug_control.html"' in server_cpp,
        "HtmlUiServer root URL no longer opens the debug launcher",
    )
    require(
        'part == ".."' in server_cpp and "lexically_normal" in server_cpp,
        "LAN debug HTTP server lost path traversal guard",
    )

    require(
        "panel: 'ship_core'" in ship_core_html
        and "payload: params" in ship_core_html
        and "sendCommand('request_snapshot')" in ship_core_html,
        "Ship Core browser page lost current HtmlUiMessage envelope/live polling",
    )
    require(
        'msg.command == "damage_radiator"' in space_cpp,
        "Ship Core damage_radiator route is not handled by SpaceState",
    )
    require(
        "repairAllPanels()" in ship_core_html
        and "sendCommand('repair_all_panels')" in ship_core_html
        and 'msg.command == "repair_all_panels"' in space_cpp,
        "Ship Core repair-all debug control is not wired end-to-end",
    )
    require(
        "setInterval(requestSnapshot, 250)" in structure_html,
        "Structure Debug no longer polls independently of in-game activePanel",
    )
    require(
        "command: 'request_snapshot'" in debug_control
        and "setInterval(() =>" in debug_control,
        "Debug Control lost independent browser telemetry polling",
    )

    for panel in ("DebugControl", "StructureDebug", "ShipCore", "FrustumDebug"):
        require(
            f"setActivePanel(HtmlUiPanelId::{panel})" not in space_cpp,
            f"external browser panel {panel} again steals the in-game activePanel",
        )

def check_map_feature_surface() -> None:
    map_mode = read("src/game/system_map/MapMode.h")
    space_h = read("src/game/SpaceState.h")
    space_cpp = read("src/game/SpaceState.cpp")
    app_cpp = read("src/core/Application.cpp")
    function_key_router = read("src/ui/presentation/PresentationFunctionKeyRouter.cpp")
    map_tests = read("tests/system_map/SystemMapBehaviorTests.cpp")

    for mode in ("Galaxy", "System", "Detail", "Hub"):
        require(
            re.search(rf"\b{mode}\b", map_mode) is not None,
            f"MapMode::{mode} disappeared",
        )
        require(
            f"MapMode::{mode}" in map_tests,
            f"system-map behavior tests no longer exercise MapMode::{mode}",
        )

    required_space_routes = (
        "setSystemMapGalaxyMode()",
        "setSystemMapCurrentSystemMode()",
        "setSystemMapPlayerSystemMode()",
        "setSystemMapPlayerDetailMode()",
        "setSystemMapPlayerLocalMode()",
        "setSystemMapDetailMode()",
        "setSystemMapHubMode()",
        "setSystemMapLoadedDetailMode()",
    )
    for route in required_space_routes:
        require(route in space_h, f"SpaceState public map route disappeared: {route}")
        require(route in space_cpp, f"SpaceState map route has no implementation: {route}")

    require(
        "preparePlayerNavigationMapLevel" in space_cpp
        and "m_gameUi.armSceneTarget(requested)" in app_cpp,
        "Application no longer prepares direct navigation targets on the scene surface",
    )
    for forbidden in (
        "parseSystemMapUiCommand(webCommand)",
        "dispatchSystemMapUiCommand(",
        "m_mapPanelWebView",
        "system_map_panel_state_prepared|",
    ):
        require(
            forbidden not in app_cpp,
            f"obsolete WebView map path returned to Application: {forbidden}",
        )

    for function_key, view in (
        (9, "NavigationPresentationView::Galaxy"),
        (10, "NavigationPresentationView::System"),
        (11, "NavigationPresentationView::Detail"),
        (12, "NavigationPresentationView::Local"),
    ):
        require(
            f"case {function_key}: return GameUiTarget::forNavigation({view});"
            in function_key_router,
            f"F{function_key} direct navigation selector disappeared",
        )

    require(
        "pollFunctionKeyPress(press)" in app_cpp
        and "ui::presentation::directTargetForFunctionKey(functionKey)" in app_cpp,
        "Application no longer drains message-backed F-key presses through the presentation router",
    )

    require(
        "CoordinateDisplayService::instance()" in app_cpp and ".cycle();" in app_cpp,
        "Ctrl+F11 coordinate-format route disappeared",
    )
    require(
        "toggleConstellationOverlay()" in app_cpp,
        "Ctrl+F12 constellation-overlay route disappeared",
    )
    require(
        "testGalaxySystemDetailHubTransitionSequence" in map_tests,
        "Galaxy -> System -> Detail -> Hub vertical behavior test disappeared",
    )

    renderer_cpp = read("src/scene/SceneRenderer.cpp")
    renderer_h = read("src/scene/SceneRenderer.h")
    for overlay in ("renderStarSystemLabels", "renderConstellationHoverOverlay"):
        require(
            f"void SceneRenderer::{overlay}(" in renderer_cpp,
            f"live map/sky overlay lost implementation: SceneRenderer::{overlay}",
        )
        require(
            f"void {overlay}(" in renderer_h,
            f"live map/sky overlay lost declaration: SceneRenderer::{overlay}",
        )
        require(
            f"{overlay}(renderView, proj);" in renderer_cpp,
            f"live map/sky overlay is no longer reached by SceneRenderer: {overlay}",
        )



def check_world_authority_boundaries() -> None:
    initial_json = json.loads(read("src/assets/data/initial_world_state.json"))
    initial_h = read("src/game/world_state/InitialWorldState.h")
    scene_cpp = read("src/game/scene/GameSceneSetup.cpp")
    server_cpp = read("src/game/server/GameServer.cpp")
    server_h = read("src/game/server/GameServer.h")
    space_cpp = read("src/game/SpaceState.cpp")
    snapshot_h = read("src/game/simulation/SimulationSnapshot.h")
    ship_core_h = read("src/game/ship/core/ShipCore.h")
    renderer_cpp = read("src/scene/SceneRenderer.cpp")
    orbital_h = read("src/world/orbits/OrbitalMotion.h")
    sim_cpp = read("src/game/simulation/GameSimulation.cpp")
    architecture = read("src/game/ARCHITECTURE_STATUS.md")

    # Keep the authored schema honest: settings that are not consumed by the
    # authoritative bootstrap must not masquerade as working world features.
    for unsupported_key in ("epoch_universe_time_seconds", "unique_npcs"):
        require(
            unsupported_key not in initial_json,
            f"initial world exposes unsupported setting: {unsupported_key}",
        )

    for hub in initial_json.get("orbital_hubs", []):
        for unsupported_key in ("routes", "traffic_zones", "hub_kind", "map_label_suffix"):
            require(
                unsupported_key not in hub,
                f"orbital hub exposes unsupported setting: {unsupported_key}",
            )
        for module in hub.get("modules", []):
            for unsupported_key in ("parent_hub_id", "map_object", "infrastructure_role"):
                require(
                    unsupported_key not in module,
                    f"hub module exposes unsupported/redundant setting: {unsupported_key}",
                )
            state = module.get("state", {})
            for unsupported_key in ("hull", "power"):
                require(
                    unsupported_key not in state,
                    f"hub module state exposes unsupported setting: {unsupported_key}",
                )

    player_start = initial_json.get("player_start")
    require(isinstance(player_start, dict), "initial world lost authored player_start")
    require(
        isinstance(player_start.get("system_id"), int)
        and isinstance(player_start.get("hub_id"), str)
        and bool(player_start.get("hub_id")),
        "player_start must identify a physical system and hub",
    )

    system_states = initial_json.get("system_states")
    require(
        isinstance(system_states, list) and system_states,
        "initial world lost server-owned physical-system map state",
    )
    require(
        "InitialWorldStateSystemState" in initial_h
        and 'root.contains("system_states")' in initial_h,
        "InitialWorldState no longer parses server-owned system map facts",
    )
    require(
        "validateInitialWorldState" in initial_h
        and "InitialWorldState candidate" in initial_h
        and "out = std::move(candidate)" in initial_h,
        "initial-world loading is no longer transactional/validated",
    )

    game_scene = extract_braced_function(scene_cpp, "EntityId buildGameScene(")
    require(
        "spawnPromoStation" not in game_scene,
        "production scene silently falls back to a hard-coded promo station",
    )
    require(
        "initialState.playerStart" in game_scene,
        "production player bootstrap no longer comes from authoritative world state",
    )
    require(
        "module.mapVisible" in scene_cpp,
        "authored hub-module map_visible setting is parsed but not consumed",
    )

    require(
        '"earth_orbital_hub"' not in extract_braced_function(server_cpp, "GameServer::GameServer("),
        "GameServer startup hard-codes the Earth diagnostic hub",
    )
    require(
        "jurisdictionForSystemId" not in server_cpp
        and "Core Jurisdiction" not in server_cpp
        and "Colonial Administration" not in server_cpp,
        "server still infers political/map facts from numeric physical-system IDs",
    )
    require(
        "m_systemJurisdictions" in server_h
        and "for (const auto& [systemId, jurisdiction] : m_systemJurisdictions)" in server_cpp
        and "overlay.jurisdiction = jurisdiction" in server_cpp,
        "Galaxy map jurisdiction is no longer sourced from server world state",
    )

    for forbidden in (
        "Core Jurisdiction",
        "Colonial Administration",
        "Frontier / Independent",
        "COORD DEBUG",
        '"system_0.Sol.Земля"',
        '"Earth High Orbital"',
        "fallback: если currentSystemId не найден — считаем от Sol",
    ):
        require(
            forbidden not in space_cpp,
            f"client SpaceState still contains world-specific authority/debug hack: {forbidden}",
        )

    require(
        'src/render/HUD/WorldLabel.h' not in snapshot_h,
        "authoritative SimulationSnapshot includes client HUD presentation types",
    )
    require(
        "ShipSignalPresentation.h" not in ship_core_h,
        "authoritative ShipCore includes client signal-presentation state",
    )

    for dead_path in ("renderCelestialPass", "renderVisualShips"):
        require(
            dead_path not in renderer_cpp,
            f"legacy duplicate SceneRenderer path survived cleanup: {dead_path}",
        )
    for fake_world in ("MoonDistanceM", "EarthRadiusM", "SunRadiusM"):
        require(
            fake_world not in renderer_cpp,
            f"gameplay SceneRenderer still owns hard-coded celestial truth: {fake_world}",
        )

    require(
        "enum class OrbitalPeriodPolicy" in orbital_h
        and "OrbitalPeriodPolicy::Kepler" in sim_cpp,
        "orbital-period policy is no longer preserved into authoritative runtime motion",
    )
    require(
        "forceKeplerPeriod" not in sim_cpp,
        "runtime still globally forces Kepler periods and ignores authored orbit policy",
    )

    require(
        "Render-style boundary" in architecture
        and "server" in architecture.lower()
        and "client" in architecture.lower(),
        "future render-style ownership boundary is not documented",
    )


def check_headless_server_geometry_boundary() -> None:
    library_cpp = read("src/game/geometry/AssemblyMeshLibrary.cpp")
    library_h = read("src/game/geometry/AssemblyMeshLibrary.h")
    gpu_library_cpp = read("src/render/geometry/AssemblyGpuLibrary.cpp")
    gpu_library_h = read("src/render/geometry/AssemblyGpuLibrary.h")
    gpu_resources_h = read("src/render/geometry/ObjectAssemblyGpuResources.h")
    scene_renderer = read("src/scene/SceneRenderer.cpp")
    hub_map = read("src/game/system_map/HubMapGeometryPass.cpp")

    cpu_load = extract_braced_function(
        library_cpp,
        "ObjectAssembly AssemblyMeshLibrary::loadAssembly",
    )

    require(
        ".upload(" not in cpu_load,
        "CPU AssemblyMeshLibrary::loadAssembly performs GPU upload; "
        "headless authoritative server would require an OpenGL context",
    )
    require(
        "getGpuReady" not in library_h
        and "getGpuReady" not in library_cpp,
        "shared CPU AssemblyMeshLibrary regained render-side GPU ownership",
    )
    require(
        "class AssemblyGpuLibrary" in gpu_library_h
        and "ObjectAssemblyGpuResources" in gpu_library_h,
        "render-side assembly GPU library/sidecar is missing",
    )
    require(
        "AssemblyMeshLibrary::get(typeId)" in gpu_library_cpp
        and "gpuPart.lod0.upload(cpuPart.lod0Mesh)" in gpu_library_cpp
        and "gpuPart.lod1.upload(cpuPart.lod1Mesh)" in gpu_library_cpp,
        "render-side AssemblyGpuLibrary no longer derives GPU resources from the shared CPU assembly",
    )
    require(
        "render::MeshGPU lod0" in gpu_resources_h
        and "render::MeshGPU lod1" in gpu_resources_h
        and "render::MeshGPU wholeShipProxy" in gpu_resources_h,
        "assembly GPU resources are no longer isolated in the render-side sidecar",
    )

    authoritative_roots = (
        ROOT / "src/game/server",
        ROOT / "src/game/simulation",
        ROOT / "src/game/ship/core",
        ROOT / "src/world/modules",
    )
    forbidden_gpu_markers = (
        "getGpuReady(",
        "AssemblyGpuLibrary",
        "ObjectAssemblyGpuResources",
        "lod0Gpu",
        "lod1Gpu",
        "wholeShipProxyGpu",
    )

    for authoritative_root in authoritative_roots:
        for path in list(authoritative_root.rglob("*.cpp")) + list(authoritative_root.rglob("*.h")):
            text = path.read_text(encoding="utf-8", errors="replace")
            rel = path.relative_to(ROOT)
            for marker in forbidden_gpu_markers:
                require(
                    marker not in text,
                    f"authoritative/CPU path {rel} depends on GPU assembly state ({marker})",
                )
            require(
                re.search(r"\bgl[A-Z][A-Za-z0-9_]*\s*\(", text) is None,
                f"authoritative/CPU path {rel} calls OpenGL directly",
            )

    require(
        "render::geometry::AssemblyGpuLibrary::get(" in scene_renderer,
        "SceneRenderer no longer obtains assembly GPU resources through the render-side library",
    )
    require(
        "render::geometry::AssemblyGpuLibrary::get(typeId)" in hub_map,
        "HubMapGeometryPass no longer obtains assembly GPU resources through the render-side library",
    )

def check_ready_orchestration() -> None:
    run_all = read("tests/run_all_mingw64.sh")
    manifest = json.loads(read("tests/feature_contracts/critical_features.json"))

    required_suites = (
        "WORLD RUNTIME + GLOBAL TIME CONTRACT",
        "CROSS-TIMELINE + DIAGNOSTIC CONTRACTS",
        "CLIENT PRESENTATION PIPELINE",
        "SERVER INTERACTION ACTIVATION",
        "SYSTEM MAP BEHAVIOR + ARCHITECTURE",
        "FEATURE SURFACE CONTRACTS",
    )
    for suite in required_suites:
        require(suite in run_all, f"readiness runner no longer executes '{suite}'")

    require(
        re.search(r'EliteGame\.exe\s+--self-test-fast-universe(?:\s|\)|;|$)', run_all) is not None,
        "readiness runner no longer executes the real-scene fast-universe smoke",
    )
    build_layout = read("tests/helpers/build_layout.sh")
    require(
        "elite_build_canonical_client" in run_all,
        "readiness runner no longer invokes the canonical client build helper",
    )
    require(
        "--target EliteGame" in build_layout,
        "canonical client build helper no longer builds the main EliteGame target",
    )

    features = manifest.get("features", [])
    require(features, "critical feature manifest is empty")
    ids = [entry.get("id") for entry in features]
    require(len(ids) == len(set(ids)), "critical feature manifest contains duplicate ids")

    allowed_evidence = {
        "debug-schema",
        "compiled-roundtrip",
        "debug-route",
        "diagnostic-panel-route",
        "real-scene-smoke",
        "application-route",
        "system-map-behavior",
        "main-target-build",
        "headless-authority-boundary",
        "world-authority-boundary",
        "suite:CLIENT PRESENTATION PIPELINE",
        "suite:SERVER INTERACTION ACTIVATION",
    }

    for entry in features:
        feature_id = entry.get("id")
        evidence = entry.get("evidence", [])
        require(feature_id and evidence, "every critical feature needs id and evidence")
        unknown = set(evidence) - allowed_evidence
        require(not unknown, f"feature {feature_id} uses unknown evidence: {sorted(unknown)}")


def main() -> int:
    check_debug_control_schema()
    check_debug_control_runtime_wiring()
    check_debug_panels()
    check_map_feature_surface()
    check_world_authority_boundaries()
    check_headless_server_geometry_boundary()
    check_ready_orchestration()
    print("[PASS] critical feature surface contracts")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
