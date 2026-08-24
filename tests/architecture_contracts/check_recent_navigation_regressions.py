#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")


def fail(message: str) -> None:
    print(f"[FAIL] recent navigation regressions: {message}", file=sys.stderr)
    raise SystemExit(1)


space = read("src/game/SpaceState.cpp")
compass_renderer = read("src/render/cockpit/GalacticCompassRenderer.cpp")
scene_setup = read("src/game/scene/GameSceneSetup.cpp")

# Corridor and galactic compass are windshield/world-guidance layers. The
# cockpit/canopy frame must occlude them. The flight-vector instrument remains
# part of the later cockpit UI layer.
guidance_draw = space.find("m_guidanceCorridorRenderer.render(")
compass_draw = space.find("m_galacticCompassRenderer.render(")
cockpit_draw = space.find("m_playerView->renderCockpit();")
flight_draw = space.find("m_flightVectorIndicatorRenderer.render(")
if min(guidance_draw, compass_draw, cockpit_draw, flight_draw) < 0:
    fail("navigation/cockpit HUD draw seam is incomplete")
if not (guidance_draw < compass_draw < cockpit_draw < flight_draw):
    fail("corridor/compass must render before cockpit; flight vector must render after cockpit")

# The compass is deliberately decorative/coarse: decade ticks only and no
# precise rapidly-changing numeric readout.
if compass_renderer.count("tick += 10") < 2:
    fail("galactic compass lost decade-only ticks")
for forbidden in (
    "std::to_string(presentation.longitudeDeg)",
    "std::to_string(presentation.latitudeDeg)",
):
    if forbidden in compass_renderer:
        fail("galactic compass regained a noisy precise coordinate readout")

# The Hub guidance lab has one deliberate rotation probe and one static
# reference object: box = +2 deg/s around local Z, cylinder = 0 deg/s.
if "glm::dvec3(0.0, 0.0, 2.0)" not in scene_setup:
    fail("guidance box no longer rotates slowly around gate/local-Z axis")
if "glm::dvec3(0.0, 0.0, 0.0)" not in scene_setup:
    fail("guidance cylinder is no longer a static reference object")

# Rear camera must consume the scene already prepared for the main camera and
# retain the deliberately cheaper LOD policy. Reintroducing a second full scene
# preparation near a dense station caused alternating frame cost/stalls.
rear_counter = space.find("if ((rearCameraFrameCounter % kRearCameraFrameStride) == 0)")
rear_end = space.find("m_perfRearCameraMs = nowMs() - rearStartMs;", rear_counter)
if rear_counter < 0 or rear_end < 0:
    fail("rear-camera render block not found")
rear = space[rear_counter:rear_end]
for token in (
    "m_sceneRenderer.renderPrepared(",
    "m_preparedScene",
    "rearPolicy.forceAssemblyLod1 = true",
    "rearPolicy.maxVisualShipsToDraw = 24",
):
    if token not in rear:
        fail(f"rear-camera prepared/cheap-scene contract missing: {token}")
if "m_sceneRenderer.render(" in rear or "rearView->drawCallback(" in rear:
    fail("rear camera again prepares/renders an independent full scene")

print("[PASS] sparse/Hub/HUD/rear-view navigation regressions remain locked")
