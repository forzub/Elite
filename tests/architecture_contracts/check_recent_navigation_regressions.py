#!/usr/bin/env python3
from pathlib import Path
import sys
import sys as _sys, pathlib as _pl; _sys.path.insert(0, str(_pl.Path(__file__).resolve().parents[1])); import source_units  # noqa: E402,F401  (split classes read as one unit)

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
def body(signature: str) -> str:
    start = space.find(signature)
    if start < 0:
        fail(f"HUD function missing: {signature}")
    depth = 0
    for i in range(space.index("{", start), len(space)):
        depth += {"{": 1, "}": -1}.get(space[i], 0)
        if depth == 0:
            return space[start:i + 1]
    fail(f"unterminated HUD function: {signature}")


# renderHUD was split into section functions (behaviour-preserving,
# 2026-11-06): the draw order is the order of the section calls in renderHUD.
hud = body("void SpaceState::renderHUD()")
guidance_section = body("void SpaceState::renderHudGuidanceAndCompass(")
instruments_section = body("void SpaceState::renderHudCockpitInstruments(")
guidance_draw = guidance_section.find("m_guidanceCorridorRenderer.render(")
compass_draw = guidance_section.find("m_galacticCompassRenderer.render(")
flight_draw = instruments_section.find("m_flightVectorIndicatorRenderer.render(")
guidance_call = hud.find("renderHudGuidanceAndCompass(")
cockpit_draw = hud.find("m_playerView->renderCockpit();")
instruments_call = hud.find("renderHudCockpitInstruments(")
if min(guidance_draw, compass_draw, cockpit_draw, flight_draw, guidance_call, instruments_call) < 0:
    fail("navigation/cockpit HUD draw seam is incomplete")
if not (guidance_draw < compass_draw and guidance_call < cockpit_draw < instruments_call):
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

# The rear monitor and its second full world render were removed (0c647339:
# one main camera, F1 front / F2 rear / F3 drone). A second per-frame full
# scene render near a dense station caused alternating frame cost/stalls, so it
# must not come back.
for token in ("kRearCameraFrameStride", "rearView->drawCallback(", "m_perfRearCameraMs"):
    if token in space:
        fail(f"a second full rear-camera scene render returned: {token}")

print("[PASS] sparse/Hub/HUD/single-camera navigation regressions remain locked")
