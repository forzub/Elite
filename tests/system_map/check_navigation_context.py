#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")


def fail(message: str) -> None:
    print(f"[FAIL] map navigation context contract: {message}", file=sys.stderr)
    raise SystemExit(1)


def function_body(text: str, signature: str) -> str:
    start = text.find(signature)
    if start < 0:
        fail(f"function disappeared: {signature}")

    brace = text.find("{", start)
    if brace < 0:
        fail(f"function has no body: {signature}")

    depth = 0
    for index in range(brace, len(text)):
        char = text[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return text[start:index + 1]

    fail(f"function body is unterminated: {signature}")


def require(body: str, token: str, owner: str) -> None:
    if token not in body:
        fail(f"{owner} lost required contract token: {token}")


def forbid(body: str, token: str, owner: str) -> None:
    if token in body:
        fail(f"{owner} reintroduced forbidden fallback: {token}")


space = read("src/game/SpaceState.cpp")
client_map = read("src/game/client/ClientMapService.cpp") + read("src/game/client/ClientDetailMapBridge.h")
view = read("src/game/system_map/SystemMapView.cpp")
panel = read("src/game/presentation/SystemMapPanelPresentation.cpp")
renderer = read("src/ui/presentation/InSessionPresentationRenderer.cpp")
localization = read("src/assets/localization/ui/maps/map.json")

# System/Space -> Details must refine the map that is actually on screen. The
# player's physical current system is unrelated and must never be used as a
# fallback for a selected map address.
build_target = function_body(
    space,
    "bool SpaceState::buildSelectedMapDetailTarget("
)
for token in (
    "target.systemId = m_loadedSystemMapId;",
    "target.systemPositionLy = m_systemMapSnapshot.systemPositionLy;",
    "m_systemMapRenderer.selectedBodyId()",
    "m_systemMapRenderer.selectedHubId()",
    "m_systemMapRenderer.selectedTerminalDetailCell()",
    "target.sceneKind = DetailSceneKind::SpatialVolume;",
):
    require(build_target, token, "buildSelectedMapDetailTarget")
for token in (
    "playerNavigation().currentSystemId",
    "playerNavigation()",
):
    forbid(build_target, token, "buildSelectedMapDetailTarget")

# Details may only be opened as a child of the current System/Space map.
open_detail = function_body(space, "void SpaceState::setSystemMapDetailMode()")
for token in (
    "m_systemMapRenderer.mode() != SystemMapRenderer::Mode::System",
    "buildSelectedMapDetailTarget(target)",
    "composeDetailMapSnapshot(target)",
    "beginSystemMapDetailTransition(target)",
):
    require(open_detail, token, "setSystemMapDetailMode")

# Returning from Details/Hub to System must restore the loaded context, including
# an empty synthetic sector. It must not re-resolve the player's current system.
loaded_system = function_body(
    space,
    "void SpaceState::setSystemMapLoadedSystemMode()"
)
for token in (
    "const int loadedSystemMapId = m_loadedSystemMapId;",
    "const bool loadedMapIsEmptySector = m_systemMapShowsEmptySector;",
    "m_loadedSystemMapId != loadedSystemMapId",
    "m_systemMapShowsEmptySector = loadedMapIsEmptySector;",
    "SystemMapRenderer::Mode::System",
):
    require(loaded_system, token, "setSystemMapLoadedSystemMode")
forbid(
    loaded_system,
    "playerNavigation().currentSystemId",
    "setSystemMapLoadedSystemMode"
)

# Hub is a child of the currently loaded map context. System -> Hub is an
# allowed shortcut, but it must first materialize the exact parent Details
# target so Hub -> Details is deterministic.
open_hub = function_body(space, "void SpaceState::setSystemMapHubMode()")
for token in (
    "mode != SystemMapRenderer::Mode::System",
    "mode != SystemMapRenderer::Mode::Detail",
    "const int selectedId = m_loadedSystemMapId;",
    "if (mode == SystemMapRenderer::Mode::System)",
    "buildSelectedMapDetailTarget(detailTarget)",
    "composeDetailMapSnapshot(detailTarget)",
    "composeHubMapSnapshot(selectedId, hubId)",
    "beginSystemMapHubTransition(selectedId, hubId)",
):
    require(open_hub, token, "setSystemMapHubMode")
forbid(
    open_hub,
    "playerNavigation().currentSystemId",
    "setSystemMapHubMode"
)

loaded_detail = function_body(
    space,
    "void SpaceState::setSystemMapLoadedDetailMode()"
)
for token in (
    "const world::celestial::DetailTarget target = m_loadedDetailTarget;",
    "target.valid()",
    "composeDetailMapSnapshot(target)",
    "beginSystemMapDetailTransition(target)",
):
    require(loaded_detail, token, "setSystemMapLoadedDetailMode")

# Empty interstellar sectors are first-class map contexts with synthetic
# negative ids. Details must be composable locally from their terminal cube;
# silently substituting a real CelestialSystemSnapshot would reintroduce the
# original "selected empty space opens the Sun" bug.
unbound_detail = function_body(
    client_map,
    "inline bool rebuildUnboundSpatialDetailMap("
)
for token in (
    "target.sceneKind != DetailSceneKind::SpatialVolume",
    "target.systemId >= 0",
    "out.systemId = target.systemId;",
    "out.systemPositionLy = target.systemPositionLy;",
    "target.spatialCell.edgeAu",
    "target.spatialCell.centerAu",
    "out.scene.halfExtentMeters = out.detailHalfExtentMeters;",
):
    require(unbound_detail, token, "rebuildUnboundSpatialDetailMap")

# In a real system, a terminal spatial address remains a spatial volume unless
# its centre lies inside a body. In that one case the semantic Details target is
# intentionally promoted to the body occupying that address.
client_detail = function_body(
    client_map,
    "inline bool rebuildDetailMapFromClientState("
)
for token in (
    "case DetailSceneKind::SpatialVolume:",
    "requestedTarget.spatialCell.centerAu",
    "requestedTarget.spatialCell.edgeAu",
    "glm::length(out.planetCenterMeters - body.worldMeters)",
    "effectiveTarget.sceneKind = DetailSceneKind::CelestialBody;",
    "out.detailTarget = effectiveTarget;",
):
    require(client_detail, token, "rebuildDetailMapFromClientState")

# Selecting any non-terminal System cube resolves to the deterministic central
# descendant at maximum depth. This is the address sent to Details.
terminal = function_body(
    view,
    "SystemMapView::resolvedTerminalSelection() const"
)
for token in (
    "m_state.navigationCellExplicitlySelected",
    "while (terminalLevel < maximumLevel)",
    "terminalIndex.x *= subdivision;",
    "terminalIndex.y *= subdivision;",
    "terminalIndex.z *= subdivision;",
):
    require(terminal, token, "resolvedTerminalSelection")

# The native panel is semantic: no Close/toggle slot, SPACE is a presentation
# name for the generic System cubic layer, and Hub has parent actions only.
for forbidden in (
    "SystemMapPanelActionType::Close",
    "ToggleMode",
    "closeButton",
):
    if forbidden in panel + renderer:
        fail(f"obsolete fixed-slot navigation returned: {forbidden}")

for token in (
    "case MapMode::Hub:",
    "SystemMapPanelActionType::OpenDetail",
    "SystemMapPanelActionType::OpenSystem",
    "SystemMapPanelActionType::OpenGalaxy",
):
    if token not in panel:
        fail(f"Hub parent navigation contract missing: {token}")

for token in (
    'loc.text("map.space", "SPACE")',
    '"map.space"',
):
    if token not in renderer + localization:
        fail(f"System/Space naming contract missing: {token}")

print("[PASS] selected map context, empty-space Details and parent-layer navigation are locked")
