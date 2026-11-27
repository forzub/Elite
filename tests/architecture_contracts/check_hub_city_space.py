#!/usr/bin/env python3
"""Spatial integrity stage A (2026-11-27): the hub city space has ONE owner.

HubCitySpace (src/game/navigation/HubCitySpace.h) is the only plan <-> module <-> world conversion: the pad service's
CityStrip is that type, the HUD resolves an infrastructure target through presentationCitySpace(object).planToWorld, the
map builds its block / building points through the same type. A plan point is never stored or read as a module offset
(audit A1: 1.4-4 km off on earth_ring_hub). The numeric contract lives in tests/hub_space/HubSpaceCheck.cpp.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
failures = []


def text(rel):
    return (ROOT / rel).read_text(encoding="utf-8")


space = text("src/game/navigation/HubCitySpace.h")
for token in ("planToModule", "moduleToPlan", "planToWorld", "worldToPlan", "velocityToWorld", "alongDelta", "HubRingBend bend"):
    if token not in space:
        failures.append(f"HubCitySpace lost {token}")

pad = text("src/game/simulation/GameSimulationPadLanding.cpp")
if "using CityStrip = game::navigation::HubCitySpace;" not in pad or re.search(r"struct\s+CityStrip\b", pad):
    failures.append("GameSimulationPadLanding: CityStrip must be the shared HubCitySpace, not a private copy")

overlay = text("src/game/system_map/MapObjectOverlay.h")
if "semanticLocalOffsetMeters" in overlay or "semanticPlanPointMeters" not in overlay:
    failures.append("MapObjectOverlayItem must carry the infrastructure target as a PLAN point (semanticPlanPointMeters)")
tracking = text("src/game/navigation/TargetTrackingState.h")
if "planPointMeters" not in tracking or re.search(r"glm::dvec3\s+localOffsetMeters", tracking):
    failures.append("NavigationTrackedInfrastructure must store planPointMeters, not a module offset")

hud = text("src/game/presentation/NavigationHudPresentation.h")
body = hud.split("inline ResolvedTacticalKinematics resolveInfrastructureKinematics(", 1)
if len(body) < 2:
    failures.append("resolveInfrastructureKinematics not found")
else:
    fn = body[1].split("inline ResolvedTacticalKinematics", 1)[0]
    if "presentationCitySpace(" not in fn or "planToWorld(" not in fn:
        failures.append("the HUD must resolve a plan point through presentationCitySpace(object).planToWorld")
    if re.search(r"renderOrientation\)\)\s*\*\s*\w*[Oo]ffset", fn):
        failures.append("the HUD reads a plan point as a module offset again (R * offset)")

nav = text("src/game/system_map/SystemMapRendererNavigation.cpp")
deco = nav.split("void SystemMapRenderer::decorateHubPlanBlocks(", 1)[1].split("\nvoid SystemMapRenderer::", 1)[0]
if "HubCitySpace" not in deco or "planToWorld(" not in deco:
    failures.append("decorateHubPlanBlocks must place block / building points through HubCitySpace")

if failures:
    for f in failures:
        print("FAIL:", f)
    sys.exit(1)
print("hub city space contract OK")
