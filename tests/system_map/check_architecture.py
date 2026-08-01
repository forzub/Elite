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
detail_backend = MAP_DIR / "SystemMapRendererDetail.inl"
hub_backend = MAP_DIR / "SystemMapRendererHub.inl"

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
    if f"const {view_type}& view" not in scene_text:
        fail(scene_path, f"scene renderer must consume const {view_type}")
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
    start_marker = "void SystemMapRenderer::renderDetailMapPasses("
    end_marker = "void SystemMapRenderer::drawPlanetSphereGrid("
    start_index = detail_text.find(start_marker)
    end_index = detail_text.find(end_marker, start_index + 1)
    if start_index < 0 or end_index < 0:
        fail(detail_backend, "could not locate renderDetailMapPasses")
    else:
        render_text = detail_text[start_index:end_index]
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
    start_marker = "void SystemMapRenderer::renderHubMapPasses("
    end_marker = "std::vector<"
    start_index = hub_text.find(start_marker)
    end_index = hub_text.find(end_marker, start_index + 1)
    if start_index < 0 or end_index < 0:
        fail(hub_backend, "could not locate renderHubMapPasses")
    else:
        render_text = hub_text[start_index:end_index]
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

if errors:
    print("System map architecture check failed:", file=sys.stderr)
    for error in errors:
        print(f"  - {error}", file=sys.stderr)
    raise SystemExit(1)

print("Map architecture check passed.")
