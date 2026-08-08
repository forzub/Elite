#!/usr/bin/env python3

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "src"
errors = []

def fail(path: Path, message: str) -> None:
    errors.append(f"{path.relative_to(ROOT)}: {message}")

spatial_header = SRC / "game/client/ClientSpatialDomain.h"
client_world_h = SRC / "game/client/ClientWorldState.h"
client_world_cpp = SRC / "game/client/ClientWorldState.cpp"
game_client_cpp = SRC / "game/client/GameClient.cpp"
prepared_scene = SRC / "scene/PreparedScene.h"
scene_renderer_cpp = SRC / "scene/SceneRenderer.cpp"
traffic_h = SRC / "game/traffic/StationTrafficSystem.h"
traffic_cpp = SRC / "game/traffic/StationTrafficSystem.cpp"
promo_h = SRC / "game/promo/PromoSceneScenario.h"
promo_cpp = SRC / "game/promo/PromoSceneScenario.cpp"

for path in (
    spatial_header, client_world_h, client_world_cpp, game_client_cpp,
    prepared_scene, scene_renderer_cpp, traffic_h, traffic_cpp, promo_h, promo_cpp,
):
    if not path.is_file():
        fail(path, "required client spatial-domain file is missing")

if spatial_header.is_file():
    text = spatial_header.read_text(encoding="utf-8", errors="replace")
    for required in (
        "canInterpolateSystemLocalState(", "belongsToRenderSystem(",
        "olderSystemId >= 0", "olderSystemId == newerSystemId",
        "entitySystemId == renderSystemId",
    ):
        if required not in text:
            fail(spatial_header, f"spatial-domain policy is incomplete: {required}")

if client_world_h.is_file():
    text = client_world_h.read_text(encoding="utf-8", errors="replace")
    for required in ("int playerSystemId() const", "m_snapshotActiveSystemId = -1"):
        if required not in text:
            fail(client_world_h, f"client world system boundary is incomplete: {required}")

if client_world_cpp.is_file():
    text = client_world_cpp.read_text(encoding="utf-8", errors="replace")
    for required in (
        "current.motion.systemId", "target.motion.systemId",
        "incomingActiveSystemId", "activeSystemChanged",
        "itOld->transform.motion.systemId", "itNew->transform.motion.systemId",
        "itOld->systemId", "itNew->systemId", "drone.transform.motion.systemId",
    ):
        if required not in text:
            fail(client_world_cpp, f"client interpolation can lose system domain: {required}")
    if "if (timelineRevisionChanged || activeSystemChanged)" not in text:
        fail(client_world_cpp, "snapshot history is not fenced on active-system changes")

if game_client_cpp.is_file():
    text = game_client_cpp.read_text(encoding="utf-8", errors="replace")
    for required in (
        "const bool playerSystemChanged",
        "if (timelineRevisionChanged || playerSystemChanged)",
        "m_pendingInputs.clear();", "m_accumulator = 0.0f;",
    ):
        if required not in text:
            fail(game_client_cpp, f"prediction is not reset on system transfer: {required}")

if prepared_scene.is_file():
    if "int activeSystemId = -1;" not in prepared_scene.read_text(encoding="utf-8", errors="replace"):
        fail(prepared_scene, "prepared scene does not name its spatial domain")

if scene_renderer_cpp.is_file():
    text = scene_renderer_cpp.read_text(encoding="utf-8", errors="replace")
    for required in (
        "prepared.activeSystemId", "ship.renderTransform.motion.systemId",
        "obj.systemId", "drone.renderTransform.motion.systemId", "belongsToRenderSystem(",
    ):
        if required not in text:
            fail(scene_renderer_cpp, f"scene renderer spatial filter is incomplete: {required}")
    far_start = text.find("void SceneRenderer::renderFarStationProxyPass(")
    if far_start < 0 or "belongsToRenderSystem(" not in text[far_start:far_start + 4000]:
        fail(scene_renderer_cpp, "far-station proxy bypasses active-system filtering")
    drone_start = text.find("void SceneRenderer::renderVisualDrones(")
    if drone_start < 0 or "belongsToRenderSystem(" not in text[drone_start:drone_start + 4000]:
        fail(scene_renderer_cpp, "visual drones bypass active-system filtering")

for header, cpp, label in (
    (traffic_h, traffic_cpp, "station traffic"),
    (promo_h, promo_cpp, "promo visual ships"),
):
    if header.is_file() and "m_systemId = -1" not in header.read_text(encoding="utf-8", errors="replace"):
        fail(header, f"{label} does not remember its system domain")
    if cpp.is_file():
        text = cpp.read_text(encoding="utf-8", errors="replace")
        for required in ("world.playerSystemId()", "transform.motion.systemId = m_systemId"):
            if required not in text:
                fail(cpp, f"{label} lacks explicit system membership: {required}")

if errors:
    print("Client spatial-domain architecture check failed:", file=sys.stderr)
    for error in errors:
        print(f"  - {error}", file=sys.stderr)
    sys.exit(1)

print("Client spatial-domain architecture check passed.")
