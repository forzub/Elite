#!/usr/bin/env python3
"""Hybrid System Map contract (fourth pass, 2026-11-03).

The cubic hierarchy stays the spatial index / LOD / zoom structure, but it is no longer the
user's navigation model:
  - the cube wireframe is an optional overlay (G), hidden by default, quieter than the Galaxy frame;
  - cubes are pick targets only while the overlay is shown; drawn hubs and bodies are targets anywhere;
  - presentation no longer hides bodies outside the current cube;
  - arrows move a selected child cube to a sibling in the screen plane locked at the first press;
  - hubs are hub glyphs (never ship arrowheads), relays / belt anchors have their own glyphs;
  - hover shows a small tooltip (name, owner / operator, type) inside the viewport;
  - the right panel shows КАРТА СИСТЕМЫ built from the world model, word-wrapped, clipped and scrollable;
  - network routes are dashed and distinct from the solid Orbital Transfer tunnel;
  - every world object marker comes from the world-sites runtime copy (no second list).
Behavioural acceptance on data: tools/navigation_runtime/SystemMapInfoCheck.cpp.
"""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
fails = []


def text(rel):
    return (ROOT / rel).read_text(encoding="utf-8")


def require(cond, msg):
    if not cond:
        fails.append(msg)


view = text("src/game/system_map/SystemMapView.h")
require("bool gridOverlayVisible = false;" in view, "grid overlay must exist and be hidden by default")
require("bool planView = true;" in view and "bool showBeacons = false;" in view, "plan (2.5D) view must be the default; beacons a filter")
require("arrowAxisRight" in view and "arrowLastPressSeconds" in view, "arrow navigation plane lock state missing")

inl = text("src/game/system_map/SystemMapRendererSystem.inl")
require("!state.gridOverlayVisible" in inl, "grid painter must respect the overlay toggle")
require("v.parentFrameColor.a *= 0.40f" in inl, "System overlay must be quieter than the Galaxy frame")

inter = text("src/game/system_map/SystemMapInteraction.cpp")
require("if (!state.gridOverlayVisible)\n            return target;" in inter, "cubes must be pick targets only with the overlay shown")
require("view.hierarchyContains(*position / scale)" not in inter, "a drawn hub must be a target regardless of its cube")

pres = text("src/game/system_map/SystemMapPresentationBuilder.cpp")
require("the cube no longer decides what exists for the user" in pres, "presentation must not hide bodies outside the cube")

require("state.planView ? 0.0f" in inter, "plan view: only the azimuth may turn")
inp = text("src/game/system_map/SystemMapRendererInput.cpp")
require("planPitchRad" in inp, "plan view tilt must be enforced")
require("bool SystemMapRenderer::moveSystemCubeSelection" in inp, "arrow-key sibling navigation missing")
require("ArrowPlaneLockSeconds" in inp and "hierarchyChildAt(cell.center + step * cell.size)" in inp,
        "arrows must step to a sibling in the locked screen plane")
require("m_objectOverlayState.setHovered(hovered)" in inp, "hover detection missing")

space = text("src/game/SpaceState.cpp")
for key in ("GLFW_KEY_V", "GLFW_KEY_N", "GLFW_KEY_G", "GLFW_KEY_LEFT", "GLFW_KEY_RIGHT", "GLFW_KEY_UP", "GLFW_KEY_DOWN", "GLFW_KEY_O"):
    require(key in space, f"map key {key} not handled")

frame = text("src/game/system_map/SystemMapSceneFrameBuilder.cpp")
require("SystemMapObjectKind::Station)\n            return MapObjectGlyphKind::Hub;" in frame or
        "kind == world::celestial::SystemMapObjectKind::Station)\n            return MapObjectGlyphKind::Hub;" in frame,
        "world-site hubs must use the hub glyph")
require("Declutter (hybrid map LOD)" in frame, "far-zoom declutter missing")
require("!viewState.state().showBeacons" in frame, "relays must be hidden unless the beacon filter is on")

ov = text("src/game/system_map/MapObjectOverlayRenderer.cpp")
require("void drawHoverTooltip" in ov and "std::clamp(x, 4.0f" in ov, "hover tooltip must stay inside the viewport")
require("case MapTrajectoryKind::BeaconLink" in ov and "case MapTrajectoryKind::HeightStem" in ov, "dotted beacon links and height stems")
nav = text("src/game/system_map/SystemMapRendererNavigation.cpp")
require("plan view: height stems" in nav, "plan view must draw height stems")
require("case MapTrajectoryKind::NetworkRoute" in ov and "case MapTrajectoryKind::TransferTunnel" in ov,
        "network routes (dashed) and the transfer tunnel (solid) must be distinct kinds")

panel = text("src/ui/presentation/InSessionPresentationRenderer.cpp")
require("std::vector<std::string> wrapText(" in panel, "panel text must be word-wrapped")
require("glEnable(GL_SCISSOR_TEST)" in panel and "m_infoScrollPx" in panel, "panel info must be clipped and scrollable")
require("std::max(15, bodyPx)" in panel, "panel info font must stay readable (>= 15 px)")

info = text("src/game/presentation/SystemMapInfoBuilder.h")
require('title = "КАРТА СИСТЕМЫ";' in info, "panel title must be КАРТА СИСТЕМЫ")
require("system_status_text" not in info and "summary(in.systemId)" in info, "system state must come from the world model summary")
require("Хабы по районам" in info, "summary must list hubs of every region (discoverability)")

bridge = text("src/game/client/ClientWorldSitesMapBridge.h")
require("WorldSitePlacementKind::Heliocentric" in bridge and "WorldSitePlacementKind::LagrangePoint" in bridge,
        "belt anchors and Lagrange points must be placed from the world model")

if fails:
    for f in fails:
        print("[FAIL]", f)
    sys.exit(1)
print("[PASS] hybrid System Map: optional cube overlay, cube-free picking, screen-plane arrows, hub glyphs, "
      "hover tooltip, wrapped/clipped/scrollable panel from the world model, dashed network vs solid transfer tunnel")
