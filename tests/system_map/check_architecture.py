#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
MAP_DIR = ROOT / "src" / "game" / "system_map"

errors: list[str] = []


def fail(path: Path, message: str) -> None:
    errors.append(f"{path.relative_to(ROOT)}: {message}")


scene_header = MAP_DIR / "SystemMapSceneRenderer.h"
scene_cpp = MAP_DIR / "SystemMapSceneRenderer.cpp"
builder_header = MAP_DIR / "SystemMapPresentationBuilder.h"
builder_cpp = MAP_DIR / "SystemMapPresentationBuilder.cpp"
backend = MAP_DIR / "SystemMapRendererSystem.inl"

for required in (
    scene_header,
    scene_cpp,
    builder_header,
    builder_cpp,
    MAP_DIR / "SystemMapPresentation.h",
):
    if not required.is_file():
        fail(required, "required System presentation component is missing")

if scene_header.is_file():
    header_text = scene_header.read_text(encoding="utf-8", errors="replace")
    if "const SystemMapView& view" not in header_text:
        fail(scene_header, "scene renderer must consume const SystemMapView")
    if "const SystemMapPresentation& presentation" not in header_text:
        fail(scene_header, "scene renderer must consume immutable presentation")

if scene_cpp.is_file():
    scene_text = scene_cpp.read_text(encoding="utf-8", errors="replace")

    for forbidden in (
        "resolvePresentationTimeSeconds",
        "glfwGetTime",
        "lastCameraFitSystemId =",
        "selectedBodyId.clear()",
        "selectedHubId.clear()",
        "navigationGrid.activateSystem",
        "cancelCameraFlight",
        "hoverVisualAlpha =",
        "hoverOutgoingAlpha =",
    ):
        if forbidden in scene_text:
            fail(
                scene_cpp,
                f"persistent state mutation leaked into render: {forbidden}",
            )

if backend.is_file():
    backend_text = backend.read_text(encoding="utf-8", errors="replace")
    match = re.search(
        r"void SystemMapRenderer::drawSystemNavigationGrid\([\s\S]*?\n}\r?\n",
        backend_text,
    )

    if not match:
        fail(backend, "could not locate drawSystemNavigationGrid")
    else:
        navigation_render = match.group(0)
        for forbidden in (
            "m_systemView.state().hoverVisualLastTimeSeconds =",
            "m_systemView.state().hoverVisualAlpha =",
            "m_systemView.state().hoverOutgoingAlpha =",
            "m_systemView.state().hoverVisualCell.reset()",
            "m_systemView.state().hoverOutgoingCell.reset()",
        ):
            if forbidden in navigation_render:
                fail(
                    backend,
                    f"navigation render mutates hover presentation: {forbidden}",
                )

# Local Detail/Hub presentation boundaries.
local_builder_header = MAP_DIR / "LocalMapPresentationBuilder.h"
local_builder_cpp = MAP_DIR / "LocalMapPresentationBuilder.cpp"
local_presentation = MAP_DIR / "LocalMapPresentation.h"
detail_scene_header = MAP_DIR / "DetailMapSceneRenderer.h"
detail_scene_cpp = MAP_DIR / "DetailMapSceneRenderer.cpp"
hub_scene_header = MAP_DIR / "HubMapSceneRenderer.h"
hub_scene_cpp = MAP_DIR / "HubMapSceneRenderer.cpp"
detail_backend = MAP_DIR / "DetailMapBackend.cpp"
hub_backend = MAP_DIR / "HubMapBackend.cpp"
authoritative_interpolator = MAP_DIR / "AuthoritativeMapInterpolator.cpp"

if authoritative_interpolator.is_file():
    interpolator_text = authoritative_interpolator.read_text(encoding="utf-8", errors="replace")
    for forbidden in (
        "HubFrameBasis.h",
        "hubAttachedVisualOrientation",
        "analyticHubAttachmentAxesAt",
        "hasAnalyticLocalEulerRotation",
    ):
        if forbidden in interpolator_text:
            fail(
                authoritative_interpolator,
                f"Hub-specific motion leaked into generic map interpolation: {forbidden}",
            )

for required in (
    local_builder_header,
    local_builder_cpp,
    local_presentation,
    detail_scene_header,
    detail_scene_cpp,
    hub_scene_header,
    hub_scene_cpp,
):
    if not required.is_file():
        fail(required, "required local-map presentation component is missing")

for scene_path, view_type, presentation_type in (
    (detail_scene_header, "DetailMapView", "DetailMapPresentation"),
    (hub_scene_header, "HubMapView", "HubMapPresentation"),
):
    if not scene_path.is_file():
        continue

    scene_text = scene_path.read_text(encoding="utf-8", errors="replace")
    if view_type in scene_text:
        fail(scene_path, f"scene renderer must not depend on mutable {view_type}")
    if f"const {presentation_type}& presentation" not in scene_text:
        fail(scene_path, f"scene renderer must consume {presentation_type}")

for scene_path in (detail_scene_cpp, hub_scene_cpp):
    if not scene_path.is_file():
        continue

    scene_text = scene_path.read_text(encoding="utf-8", errors="replace")
    for forbidden in (
        ".beginScene(",
        ".reset(",
        "selectedHubId.clear()",
        "selectedHubParentBodyId.clear()",
        "camera.zoom =",
        "camera.pan =",
    ):
        if forbidden in scene_text:
            fail(
                scene_path,
                f"persistent local-map mutation leaked into render: {forbidden}",
            )

if detail_backend.is_file():
    detail_text = detail_backend.read_text(encoding="utf-8", errors="replace")
    start_marker = "void DetailMapBackend::renderDetailMapPasses("
    start_index = detail_text.find(start_marker)
    if start_index < 0:
        fail(detail_backend, "could not locate renderDetailMapPasses")
    else:
        render_text = detail_text[start_index:]
        for forbidden in (
            "m_detailView.frame()",
            "m_systemView.state().selectedHubId.clear()",
            "m_systemView.state().selectedHubParentBodyId.clear()",
            "view.beginScene(",
        ):
            if forbidden in render_text:
                fail(
                    detail_backend,
                    f"Detail render mutates persistent/pick state: {forbidden}",
                )

if hub_backend.is_file():
    hub_text = hub_backend.read_text(encoding="utf-8", errors="replace")
    start_marker = "void HubMapBackend::renderHubMapPasses("
    start_index = hub_text.find(start_marker)
    if start_index < 0:
        fail(hub_backend, "could not locate renderHubMapPasses")
    else:
        render_text = hub_text[start_index:]
        for forbidden in (
            "m_hubView.frame()",
            "pickables.push_back",
            "view.beginScene(",
        ):
            if forbidden in render_text:
                fail(
                    hub_backend,
                    f"Hub render mutates persistent/pick state: {forbidden}",
                )

# Hub orbit must use a captured scene pivot, not the Hub/world origin.
hub_view_header = MAP_DIR / "HubMapView.h"
local_interaction_cpp = MAP_DIR / "LocalMapInteraction.cpp"

if hub_view_header.is_file():
    hub_view_text = hub_view_header.read_text(encoding="utf-8", errors="replace")
    for required in (
        "captureOrbitPivot",
        "stabilizeCapturedOrbitPivot",
        "pickOrbitPivot",
        "orbitPivotActive",
    ):
        if required not in hub_view_text:
            fail(hub_view_header, f"Hub orbit-pivot contract missing: {required}")

if local_interaction_cpp.is_file():
    interaction_text = local_interaction_cpp.read_text(encoding="utf-8", errors="replace")
    for required in (
        "hubView.captureOrbitPivot",
        "hubView.stabilizeCapturedOrbitPivot",
        "hubFrame.centerPx",
    ):
        if required not in interaction_text:
            fail(local_interaction_cpp, f"Hub interaction lost cursor/object pivot: {required}")

if local_builder_cpp.is_file():
    builder_text = local_builder_cpp.read_text(encoding="utf-8", errors="replace")
    if "addOrbitPivotPickable" not in builder_text or "DetailObjectClass::Hub" not in builder_text:
        fail(local_builder_cpp, "Hub infrastructure is not exposed as an orbit-pivot candidate")

# Stage 3: one prepared CPU frame for picking and System rendering.
scene_frame_header = MAP_DIR / "SystemMapSceneFrame.h"
scene_frame_builder_header = MAP_DIR / "SystemMapSceneFrameBuilder.h"
scene_frame_builder_cpp = MAP_DIR / "SystemMapSceneFrameBuilder.cpp"
frame_interaction_header = MAP_DIR / "SystemMapFrameInteractionContext.h"
frame_interaction_cpp = MAP_DIR / "SystemMapFrameInteractionContext.cpp"
renderer_header = MAP_DIR / "SystemMapRenderer.h"
renderer_cpp = MAP_DIR / "SystemMapRenderer.cpp"
space_state_cpp = ROOT / "src" / "game" / "SpaceState.cpp"

for required in (
    scene_frame_header,
    scene_frame_builder_header,
    scene_frame_builder_cpp,
    frame_interaction_header,
    frame_interaction_cpp,
):
    if not required.is_file():
        fail(required, "required Stage-3 presentation-frame component is missing")

if scene_header.is_file():
    header_text = scene_header.read_text(encoding="utf-8", errors="replace")
    if "const SystemMapSceneFrame& frame" not in header_text:
        fail(scene_header, "System render must consume the prepared CPU frame")

if scene_cpp.is_file():
    scene_text = scene_cpp.read_text(encoding="utf-8", errors="replace")
    for forbidden in (
        "systemFrameData()",
        "frame.clearPresentation()",
        "bodyScreenPoints.push_back",
        "orbitPivotScreenPoints.push_back",
        "hubScreenPoints.push_back",
    ):
        if forbidden in scene_text:
            fail(scene_cpp, f"render rebuilds interaction frame: {forbidden}")

if renderer_header.is_file():
    renderer_header_text = renderer_header.read_text(
        encoding="utf-8", errors="replace"
    )
    if "private game::system_map::SystemMapInteractionContext" in renderer_header_text:
        fail(renderer_header, "renderer facade still implements input context")

if renderer_cpp.is_file():
    renderer_text = renderer_cpp.read_text(encoding="utf-8", errors="replace")
    build_pos = renderer_text.find("m_systemSceneFrameBuilder.build(")
    input_pos = renderer_text.find("m_systemInteraction.handleInput(")
    if build_pos < 0 or input_pos < 0 or build_pos > input_pos:
        fail(renderer_cpp, "System CPU frame must be built before input/picking")

# Local map input and rendering must consume one frame-stable snapshot.
# The application lifecycle now has an explicit prepareFrame() phase before
# input; SpaceState resolves map responses/presentation there and update() must
# not mutate those snapshots later in the same frame.
game_state_header = ROOT / "src" / "core" / "GameState.h"
application_cpp = ROOT / "src" / "core" / "Application.cpp"
space_state_header = ROOT / "src" / "game" / "SpaceState.h"

for required in (game_state_header, application_cpp, space_state_header):
    if not required.is_file():
        fail(required, "required frame-preparation lifecycle file is missing")

if game_state_header.is_file():
    text = game_state_header.read_text(encoding="utf-8", errors="replace")
    if "virtual void prepareFrame(float dt)" not in text:
        fail(game_state_header, "GameState lost the pre-input frame preparation hook")

if application_cpp.is_file():
    text = application_cpp.read_text(encoding="utf-8", errors="replace")
    # The Local ESC pause policy may pass 0 dt into frame preparation while
    # multiplayer and all F1-F12 targets keep normal time. The architecture
    # contract is the lifecycle order, not the exact argument spelling.
    prepare_pos = text.find("state->prepareFrame(")
    input_pos = text.find("state->handleInput();")
    update_pos = text.find("state->update(dt);")
    if (
        prepare_pos < 0 or
        input_pos < 0 or
        update_pos < 0 or
        not (prepare_pos < input_pos < update_pos)
    ):
        fail(
            application_cpp,
            "frame preparation must run before input and simulation update",
        )

if space_state_header.is_file():
    text = space_state_header.read_text(encoding="utf-8", errors="replace")
    if "void prepareFrame(float dt) override;" not in text:
        fail(space_state_header, "SpaceState does not own map-frame preparation")

if space_state_cpp.is_file():
    space_text = space_state_cpp.read_text(encoding="utf-8", errors="replace")
    prepare_match = re.search(
        r"void SpaceState::prepareFrame\(float dt\)[\s\S]*?\n}\r?\n",
        space_text,
    )
    if not prepare_match:
        fail(space_state_cpp, "could not locate SpaceState::prepareFrame")
    else:
        prepare_text = prepare_match.group(0)
        for required_call in (
            "updateSystemMapLiveFlags();",
            "updateLiveMapSnapshots(",
            "updateLocalMapPresentationSnapshots(",
        ):
            if required_call not in prepare_text:
                fail(
                    space_state_cpp,
                    f"map frame preparation is incomplete: {required_call}",
                )

    update_match = re.search(
        r"void SpaceState::update\(float dt\)[\s\S]*?// Render",
        space_text,
    )
    if update_match:
        update_text = update_match.group(0)
        for forbidden_call in (
            "updateLiveMapSnapshots(",
            "updateLocalMapPresentationSnapshots(",
        ):
            if forbidden_call in update_text:
                fail(
                    space_state_cpp,
                    f"update mutates a map frame after input: {forbidden_call}",
                )

# Stage 4: Views own camera math and renderers consume immutable snapshots.
camera_snapshot_header = MAP_DIR / "MapCameraSnapshot.h"
system_view_header = MAP_DIR / "SystemMapView.h"
system_view_cpp = MAP_DIR / "SystemMapView.cpp"
galaxy_view_header = MAP_DIR / "GalaxyMapView.h"
galaxy_renderer_cpp = MAP_DIR / "GalaxyMapRenderer.cpp"
local_presentation_header = MAP_DIR / "LocalMapPresentation.h"

if not camera_snapshot_header.is_file():
    fail(camera_snapshot_header, "shared camera snapshot contract is missing")
else:
    camera_text = camera_snapshot_header.read_text(
        encoding="utf-8", errors="replace"
    )
    for required in (
        "struct SystemMapCameraSnapshot",
        "struct GalaxyMapCameraSnapshot",
        "struct LocalMapCameraSnapshot",
        "orbitCameraBasis(",
        "glm::dvec2 project(",
    ):
        if required not in camera_text:
            fail(camera_snapshot_header, f"missing camera contract: {required}")

if system_view_header.is_file():
    text = system_view_header.read_text(encoding="utf-8", errors="replace")
    if "SystemMapCameraSnapshot cameraSnapshot(" not in text:
        fail(system_view_header, "System View must publish a camera snapshot")

if galaxy_view_header.is_file():
    text = galaxy_view_header.read_text(encoding="utf-8", errors="replace")
    if "GalaxyMapCameraSnapshot cameraSnapshot(" not in text:
        fail(galaxy_view_header, "Galaxy View must publish a camera snapshot")

if scene_frame_header.is_file():
    text = scene_frame_header.read_text(encoding="utf-8", errors="replace")
    if "SystemMapCameraSnapshot camera" not in text:
        fail(scene_frame_header, "System frame must carry its camera snapshot")

if local_presentation_header.is_file():
    text = local_presentation_header.read_text(
        encoding="utf-8", errors="replace"
    )
    if text.count("MapCameraSnapshot camera") < 2:
        fail(
            local_presentation_header,
            "Detail and Hub presentations must carry camera snapshots",
        )

for context_path, forbidden_view in (
    (MAP_DIR / "DetailMapRenderContext.h", "DetailMapView"),
    (MAP_DIR / "HubMapRenderContext.h", "HubMapView"),
):
    if context_path.is_file():
        text = context_path.read_text(encoding="utf-8", errors="replace")
        if forbidden_view in text:
            fail(context_path, f"render context depends on {forbidden_view}")

if galaxy_renderer_cpp.is_file():
    text = galaxy_renderer_cpp.read_text(encoding="utf-8", errors="replace")
    for forbidden in (
        "orbitCameraDirectionFromYawPitch(",
        "viewState.viewMatrix()",
        "viewState.projectionMatrix(",
    ):
        if forbidden in text:
            fail(galaxy_renderer_cpp, f"Galaxy renderer owns camera math: {forbidden}")
    if "const GalaxyMapCameraSnapshot camera" not in text:
        fail(galaxy_renderer_cpp, "Galaxy renderer must consume a camera snapshot")

for renderer_path in (
    renderer_cpp,
    MAP_DIR / "SystemMapRendererSystem.inl",
    detail_backend,
    hub_backend,
):
    if not renderer_path.is_file():
        continue
    text = renderer_path.read_text(encoding="utf-8", errors="replace")
    for forbidden in (
        "activeDetailCamera(",
        "planetMapProject(",
        "hubMapProject(",
        "planetMapCameraSpaceRelative(",
        "orbitCameraDirectionFromYawPitch(",
        "orbitCameraUpFromYawPitch(",
        "systemMapPerspectiveEyeDistance(",
        "systemMapWorldUnitsPerPixel(",
    ):
        # Historical names inside explanatory comments are harmless.
        code_text = re.sub(r"/\*[\s\S]*?\*/|//[^\n]*", "", text)
        if forbidden in code_text:
            fail(renderer_path, f"renderer-side camera helper remains: {forbidden}")

if renderer_cpp.is_file():
    text = renderer_cpp.read_text(encoding="utf-8", errors="replace")
    direct_camera_assignment = re.search(
        r"m_(?:galaxy|system|detail|hub)View"
        r"\.(?:state\(\)\.camera|camera\(\))[^;\n]*=",
        text,
    )
    if direct_camera_assignment:
        fail(renderer_cpp, "renderer facade directly mutates View camera state")

# Stage 5: Galaxy and System share one cubic-navigation state core.
cubic_grid_header = ROOT / "src" / "game" / "navigation" / "CubicNavigationGrid.h"
galaxy_grid_header = ROOT / "src" / "game" / "navigation" / "GalaxyNavigationGrid.h"
galaxy_grid_cpp = ROOT / "src" / "game" / "navigation" / "GalaxyNavigationGrid.cpp"
system_grid_header = ROOT / "src" / "game" / "navigation" / "SystemNavigationGrid.h"

if cubic_grid_header.is_file():
    text = cubic_grid_header.read_text(encoding="utf-8", errors="replace")
    for required in (
        "class CubicNavigationPolicy",
        "struct CubicNavigationAnchorState",
        "struct CubicNavigationHoverState",
        "struct CubicNavigationSelectionState",
        "std::vector<CubicNavigationCell> neighborhood(",
    ):
        if required not in text:
            fail(cubic_grid_header, f"missing shared navigation core contract: {required}")

if galaxy_grid_header.is_file():
    text = galaxy_grid_header.read_text(encoding="utf-8", errors="replace")
    for forbidden in (
        "struct GalaxyNavigationFrame",
        "struct GalaxyGridIndex",
        "struct GalaxyNavigationCell",
    ):
        if forbidden in text:
            fail(galaxy_grid_header, f"Galaxy duplicates shared navigation type: {forbidden}")

    for required in (
        "using GalaxyNavigationFrame = CubicNavigationFrame",
        "using GalaxyGridIndex = CubicGridIndex",
        "using GalaxyNavigationCell = CubicNavigationCell",
        "class GalaxyNavigationGrid final : public CubicNavigationGrid",
    ):
        if required not in text:
            fail(galaxy_grid_header, f"Galaxy does not use shared core: {required}")

if system_grid_header.is_file():
    text = system_grid_header.read_text(encoding="utf-8", errors="replace")
    if "class SystemNavigationGrid final : public CubicNavigationGrid" not in text:
        fail(system_grid_header, "System navigation must use shared cubic core")

if galaxy_grid_cpp.is_file():
    text = galaxy_grid_cpp.read_text(encoding="utf-8", errors="replace")
    for forbidden in (
        "m_anchorIndex",
        "m_hoveredCell",
        "m_selectedCell",
        "GalaxyNavigationGrid::refineAroundAnchor",
        "GalaxyNavigationGrid::coarsenAroundAnchor",
        "GalaxyNavigationGrid::cell(",
    ):
        if forbidden in text:
            fail(galaxy_grid_cpp, f"Galaxy reimplements shared grid state/math: {forbidden}")


# Stage 6A: local-map render contexts are owned by dedicated backends.
detail_backend_header = MAP_DIR / "DetailMapBackend.h"
detail_backend_cpp = MAP_DIR / "DetailMapBackend.cpp"
hub_backend_header = MAP_DIR / "HubMapBackend.h"
hub_backend_cpp = MAP_DIR / "HubMapBackend.cpp"
renderer_header = MAP_DIR / "SystemMapRenderer.h"
renderer_cpp = MAP_DIR / "SystemMapRenderer.cpp"
detail_inl = MAP_DIR / "SystemMapRendererDetail.inl"
hub_inl = MAP_DIR / "SystemMapRendererHub.inl"

for required in (
    detail_backend_header,
    detail_backend_cpp,
    hub_backend_header,
    hub_backend_cpp,
):
    if not required.is_file():
        fail(required, "required Stage-6 backend owner is missing")

if renderer_header.is_file():
    text = renderer_header.read_text(encoding="utf-8", errors="replace")
    for forbidden in (
        "private game::system_map::DetailMapRenderContext",
        "private game::system_map::HubMapRenderContext",
        "void renderDetailMapPasses(",
        "void renderHubMapPasses(",
        "m_hubGpuQueries",
        "m_hubGpuFrameActive",
        "m_hubMapPerformanceStats",
    ):
        if forbidden in text:
            fail(renderer_header, f"facade still owns local backend contract/state: {forbidden}")

    for required in (
        "game::system_map::DetailMapBackend m_detailBackend",
        "game::system_map::HubMapBackend m_hubBackend",
        "return m_hubBackend.performanceStats()",
    ):
        if required not in text:
            fail(renderer_header, f"facade is not routing through backend owner: {required}")

if renderer_cpp.is_file():
    text = renderer_cpp.read_text(encoding="utf-8", errors="replace")
    for required in (
        "m_detailBackend(m_mapResources)",
        "m_hubBackend(m_mapResources)",
        "m_detailBackend,",
        "m_hubBackend,",
    ):
        if required not in text:
            fail(renderer_cpp, f"local scene is not wired to backend owner: {required}")

for old_backend, forbidden in (
    (detail_inl, "SystemMapRenderer::renderDetailMapPasses("),
    (hub_inl, "SystemMapRenderer::renderHubMapPasses("),
    (hub_inl, "SystemMapRenderer::ensureHubGpuQueries("),
):
    if old_backend.is_file():
        text = old_backend.read_text(encoding="utf-8", errors="replace")
        if forbidden in text:
            fail(old_backend, f"orchestration/profiling remains in facade backend: {forbidden}")

if detail_backend_header.is_file():
    text = detail_backend_header.read_text(encoding="utf-8", errors="replace")
    if "class DetailMapBackend final : public DetailMapRenderContext" not in text:
        fail(detail_backend_header, "Detail backend must own DetailMapRenderContext")

if hub_backend_header.is_file():
    text = hub_backend_header.read_text(encoding="utf-8", errors="replace")
    for required in (
        "class HubMapBackend final : public HubMapRenderContext",
        "HubMapPerformanceStats m_performanceStats",
        "m_gpuQueries",
    ):
        if required not in text:
            fail(hub_backend_header, f"Hub backend does not own profiling/render contract: {required}")


# Stage 6B: Hub passes/resources are physically extracted from the facade.
hub_geometry_header = MAP_DIR / "HubMapGeometryPass.h"
hub_geometry_cpp = MAP_DIR / "HubMapGeometryPass.cpp"
hub_planet_header = MAP_DIR / "HubMapPlanetPass.h"
hub_planet_cpp = MAP_DIR / "HubMapPlanetPass.cpp"
local_environment_style = MAP_DIR / "LocalMapEnvironmentStyle.h"
local_atmosphere_header = MAP_DIR / "LocalMapAtmosphereRenderer.h"
local_atmosphere_cpp = MAP_DIR / "LocalMapAtmosphereRenderer.cpp"
root_cmake = ROOT / "CMakeLists.txt"

for required in (
    hub_geometry_header,
    hub_geometry_cpp,
    hub_planet_header,
    hub_planet_cpp,
    local_environment_style,
    local_atmosphere_header,
    local_atmosphere_cpp,
):
    if not required.is_file():
        fail(required, "required Stage-6B Hub pass component is missing")

if hub_inl.is_file():
    fail(hub_inl, "legacy Hub facade implementation must be removed")


if renderer_header.is_file():
    text = renderer_header.read_text(encoding="utf-8", errors="replace")
    for forbidden in (
        "m_hubMapGpuGeometryRenderer",
        "m_hubPlanetOverlayRenderer",
        "m_hubSphericalGridRenderer",
        "m_lastHubPlanetVisualRadiusPx",
        "void drawHubMapBox(",
        "void drawHubMapPlanetSurfaceHint(",
        "glm::dvec3 visualSizeForHubShip(",
    ):
        if forbidden in text:
            fail(renderer_header, f"facade still owns Hub pass/resource: {forbidden}")

if hub_backend_header.is_file():
    text = hub_backend_header.read_text(encoding="utf-8", errors="replace")
    for required in (
        "HubMapGeometryPass m_geometryPass",
        "HubMapPlanetPass m_planetPass",
    ):
        if required not in text:
            fail(hub_backend_header, f"Hub backend does not own extracted pass: {required}")

if hub_geometry_header.is_file():
    text = hub_geometry_header.read_text(encoding="utf-8", errors="replace")
    if "HubMapGpuGeometryRenderer" not in text:
        fail(hub_geometry_header, "Hub geometry pass must own GPU geometry renderer")

if local_atmosphere_cpp.is_file():
    text = local_atmosphere_cpp.read_text(encoding="utf-8", errors="replace")
    for required in (
        "void drawLocalMapAtmosphereSoftBand(",
        "void drawLocalMapAtmosphereStack(",
    ):
        if required not in text:
            fail(local_atmosphere_cpp, f"shared local atmosphere pass is incomplete: {required}")

if hub_planet_header.is_file():
    text = hub_planet_header.read_text(encoding="utf-8", errors="replace")
    for required in (
        "HubPlanetOverlayRenderer",
        "HubSphericalGridRenderer",
        "m_lastHubPlanetVisualRadiusPx",
    ):
        if required not in text:
            fail(hub_planet_header, f"Hub planet pass does not own planet resource/state: {required}")

if hub_planet_cpp.is_file():
    text = hub_planet_cpp.read_text(encoding="utf-8", errors="replace")
    for forbidden in (
        "HubMapPlanetPass::drawHubMapPlanetSoftBand(",
        "HubMapPlanetPass::drawHubMapPlanetAtmosphereStack(",
    ):
        if forbidden in text:
            fail(hub_planet_cpp, f"Hub pass duplicates shared atmosphere implementation: {forbidden}")
    if "drawLocalMapAtmosphereStack(" not in text:
        fail(hub_planet_cpp, "Hub planet pass is not routed through shared atmosphere renderer")

if hub_backend_cpp.is_file():
    text = hub_backend_cpp.read_text(encoding="utf-8", errors="replace")
    for required in (
        "m_planetPass.drawHubMapPlanetSurfaceHint(",
        "m_geometryPass.beginFrame(",
        "m_geometryPass.flush()",
    ):
        if required not in text:
            fail(hub_backend_cpp, f"Hub backend is not routing through extracted pass: {required}")

if root_cmake.is_file():
    text = root_cmake.read_text(encoding="utf-8", errors="replace")
    for required in (
        "src/game/system_map/HubMapGeometryPass.cpp",
        "src/game/system_map/HubMapPlanetPass.cpp",
        "src/game/system_map/LocalMapAtmosphereRenderer.cpp",
    ):
        if required not in text:
            fail(root_cmake, f"main target is missing extracted Hub pass: {required}")


# Stage 6C: Detail passes/resources are physically extracted from the facade.
detail_geometry_header = MAP_DIR / "DetailMapGeometryPass.h"
detail_geometry_cpp = MAP_DIR / "DetailMapGeometryPass.cpp"
detail_planet_header = MAP_DIR / "DetailMapPlanetPass.h"
detail_planet_cpp = MAP_DIR / "DetailMapPlanetPass.cpp"
local_primitives_header = MAP_DIR / "LocalMapPrimitiveRenderer.h"
local_primitives_cpp = MAP_DIR / "LocalMapPrimitiveRenderer.cpp"

for required in (
    detail_geometry_header,
    detail_geometry_cpp,
    detail_planet_header,
    detail_planet_cpp,
    local_primitives_header,
    local_primitives_cpp,
):
    if not required.is_file():
        fail(required, "required Stage-6C Detail pass component is missing")

if detail_inl.is_file():
    fail(detail_inl, "legacy Detail facade implementation must be removed")

if renderer_header.is_file():
    text = renderer_header.read_text(encoding="utf-8", errors="replace")
    for forbidden in (
        "m_celestialShapeMeshes",
        "m_planetDetailSculptShader",
        "m_planetDetailSculptVao",
        "void drawPlanetSphereGrid(",
        "void drawPlanetFilledDisk(",
        "void drawPlanetTexturedGlobe(",
        "bool drawPlanetShapeModelDetail(",
        "void drawDetailMapOrbit3D(",
        "void drawPlanetMapLine(",
        "void drawPlanetMapCross(",
        "void drawPlanetMapCircle(",
    ):
        if forbidden in text:
            fail(renderer_header, f"facade still owns Detail pass/resource: {forbidden}")

if renderer_cpp.is_file():
    text = renderer_cpp.read_text(encoding="utf-8", errors="replace")
    if "SystemMapRendererDetail.inl" in text:
        fail(renderer_cpp, "facade still includes legacy Detail implementation")

if detail_backend_header.is_file():
    text = detail_backend_header.read_text(encoding="utf-8", errors="replace")
    for required in (
        "DetailMapPlanetPass m_planetPass",
        "DetailMapGeometryPass m_geometryPass",
    ):
        if required not in text:
            fail(detail_backend_header, f"Detail backend does not own extracted pass: {required}")

if detail_backend_cpp.is_file():
    text = detail_backend_cpp.read_text(encoding="utf-8", errors="replace")
    for required in (
        "m_planetPass.renderCentralBody(",
        "m_geometryPass.renderScene(",
        "presentation.camera.starfieldViewMatrix()",
    ):
        if required not in text:
            fail(detail_backend_cpp, f"Detail backend is not routing through extracted pass: {required}")
    if "m_activeLocalCameraSnapshot" in text:
        fail(detail_backend_cpp, "Detail backend still mutates facade camera bridge")

if detail_planet_header.is_file():
    text = detail_planet_header.read_text(encoding="utf-8", errors="replace")
    for required in (
        "CelestialShapeMeshLibrary m_shapeMeshes",
        "m_detailSculptShader",
        "m_detailSculptVao",
    ):
        if required not in text:
            fail(detail_planet_header, f"Detail planet pass does not own Detail resource: {required}")

if local_primitives_cpp.is_file():
    text = local_primitives_cpp.read_text(encoding="utf-8", errors="replace")
    for required in (
        "void drawLocalMapLine(",
        "void drawLocalMapCross(",
        "void drawLocalMapCircle(",
    ):
        if required not in text:
            fail(local_primitives_cpp, f"shared local primitive pass is incomplete: {required}")

for hub_file in (hub_backend_cpp, hub_geometry_cpp, hub_planet_cpp):
    if hub_file.is_file():
        text = hub_file.read_text(encoding="utf-8", errors="replace")
        for forbidden in (
            "m_host.drawPlanetMapLine(",
            "m_host.drawPlanetMapCross(",
            "m_host.drawPlanetMapCircle(",
        ):
            if forbidden in text:
                fail(hub_file, f"Hub still depends on Detail-era facade primitive: {forbidden}")

if root_cmake.is_file():
    text = root_cmake.read_text(encoding="utf-8", errors="replace")
    for required in (
        "src/game/system_map/DetailMapGeometryPass.cpp",
        "src/game/system_map/DetailMapPlanetPass.cpp",
        "src/game/system_map/LocalMapPrimitiveRenderer.cpp",
    ):
        if required not in text:
            fail(root_cmake, f"main target is missing extracted Detail pass: {required}")


# Stage 6D: shared celestial resources are explicit and local backends no longer
# depend on the SystemMapRenderer facade.
map_resources_header = MAP_DIR / "MapCelestialRenderResources.h"
map_resources_cpp = MAP_DIR / "MapCelestialRenderResources.cpp"

for required in (map_resources_header, map_resources_cpp):
    if not required.is_file():
        fail(required, "required Stage-6D shared-resource owner is missing")

if map_resources_header.is_file():
    text = map_resources_header.read_text(encoding="utf-8", errors="replace")
    for required in (
        "class MapCelestialRenderResources",
        "DetailMapVisualSettings m_detailVisuals",
        "HubMapVisualSettings m_hubVisuals",
        "m_generatedCelestialAssets",
        "m_environmentProfiles",
        "m_planetGlobeMeshRenderer",
        "m_planetRingRenderer",
        "m_proceduralCloudLayer",
        "m_mapStarfieldRenderer",
    ):
        if required not in text:
            fail(map_resources_header, f"shared-resource owner is incomplete: {required}")

if renderer_header.is_file():
    text = renderer_header.read_text(encoding="utf-8", errors="replace")
    required_owner = "MapCelestialRenderResources m_mapResources"
    if required_owner not in text:
        fail(renderer_header, "facade does not own the shared map resource service")

    for forbidden in (
        "m_activeLocalCameraSnapshot",
        "m_detailVisuals",
        "m_hubVisuals",
        "m_generatedCelestialAssets",
        "m_environmentProfiles",
        "m_mapPreviewTextureByAssetKey",
        "m_globalAlbedoTextureByAssetKey",
        "m_globalNormalTextureByAssetKey",
        "m_hubPlanetSurfaceRenderer",
        "m_planetGlobeMeshRenderer",
        "m_planetRingRenderer",
        "m_proceduralCloudLayer",
        "m_mapStarfieldRenderer",
        "m_galaxyBackdropStarfieldRenderer",
        "friend class game::system_map::DetailMap",
        "friend class game::system_map::HubMap",
    ):
        if forbidden in text:
            fail(renderer_header, f"facade still owns local/shared render state: {forbidden}")

if renderer_cpp.is_file():
    text = renderer_cpp.read_text(encoding="utf-8", errors="replace")
    for required in (
        "m_detailBackend(m_mapResources)",
        "m_hubBackend(m_mapResources)",
        "m_mapResources.init(",
        "m_mapResources.beginFrame()",
        "m_mapResources.resetPresentationTime()",
        "m_mapResources.drawStarfield(",
    ):
        if required not in text:
            fail(renderer_cpp, f"facade is not coordinating the shared owner: {required}")

    for forbidden in (
        "SystemMapRenderer::ensureGeneratedCelestialAssets(",
        "SystemMapRenderer::ensureEnvironmentProfiles(",
        "SystemMapRenderer::cloudStylesForBody(",
        "SystemMapRenderer::atmosphereStyleForBody(",
        "SystemMapRenderer::environmentVisualTimeSeconds(",
    ):
        if forbidden in text:
            fail(renderer_cpp, f"shared resource implementation leaked back into facade: {forbidden}")

local_backend_files = (
    detail_backend_header,
    detail_backend_cpp,
    detail_geometry_header,
    detail_geometry_cpp,
    detail_planet_header,
    detail_planet_cpp,
    hub_backend_header,
    hub_backend_cpp,
    hub_geometry_header,
    hub_geometry_cpp,
    hub_planet_header,
    hub_planet_cpp,
)

for local_file in local_backend_files:
    if local_file.is_file():
        text = local_file.read_text(encoding="utf-8", errors="replace")
        for forbidden in (
            "SystemMapRenderer",
            "m_host",
        ):
            if forbidden in text:
                fail(local_file, f"local backend still depends on facade: {forbidden}")

if detail_backend_header.is_file():
    text = detail_backend_header.read_text(encoding="utf-8", errors="replace")
    if "DetailMapBackend(MapCelestialRenderResources& resources)" not in text:
        fail(detail_backend_header, "Detail backend does not receive explicit resources")

if hub_backend_header.is_file():
    text = hub_backend_header.read_text(encoding="utf-8", errors="replace")
    for required in (
        "HubMapBackend(MapCelestialRenderResources& resources)",
        "const LocalMapCameraSnapshot* m_activeCamera",
    ):
        if required not in text:
            fail(hub_backend_header, f"Hub backend ownership is incomplete: {required}")

if root_cmake.is_file():
    text = root_cmake.read_text(encoding="utf-8", errors="replace")
    if "src/game/system_map/MapCelestialRenderResources.cpp" not in text:
        fail(root_cmake, "main target is missing shared map resource owner")


if errors:
    print("System map architecture check failed:", file=sys.stderr)
    for error in errors:
        print(f"  - {error}", file=sys.stderr)
    raise SystemExit(1)

print("Map architecture check passed.")
