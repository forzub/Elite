#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def fail(path: Path, message: str) -> None:
    print(f"[FAIL] coordinate-frame architecture: {path}: {message}", file=sys.stderr)
    raise SystemExit(1)


def read(rel: str) -> tuple[Path, str]:
    path = ROOT / rel
    if not path.is_file():
        fail(path, "required file is missing")
    return path, path.read_text(encoding="utf-8", errors="replace")


world_pos_path, world_pos = read("src/world/coordinates/WorldPosition.h")
world_frame_path, world_frame = read("src/world/coordinates/WorldFrame.h")
resolver_path, resolver = read("src/game/navigation/PlayerSpatialDomainResolver.cpp")
galaxy_pres_path, galaxy_pres = read("src/game/presentation/GalaxyNavigationPresentation.cpp")
prepared_path, prepared = read("src/scene/PreparedScene.h")
renderer_path, renderer = read("src/scene/SceneRenderer.cpp")
space_path, space = read("src/game/SpaceState.cpp")

for required in (
    "GalacticCellSizeLy = 1.0",
    "struct GalacticCell",
    "struct WorldPosition",
    "glm::dvec3 localMeters",
    "relativeMeters(",
    "relativeMetersFloat(",
):
    if required not in world_pos:
        fail(world_pos_path, f"large-world coordinate primitive disappeared: {required}")

for required in (
    "struct WorldFrame",
    "WorldPosition origin",
    "makeRenderFrameFromCamera",
    "relativeMetersFloat(worldPosition, frame.origin)",
):
    if required not in world_frame:
        fail(world_frame_path, f"player-relative render frame contract disappeared: {required}")

for required in (
    "sourceSystem->positionLy",
    "world::coordinates::fullMeters(sourceWorldPosition)",
    "result.galacticPositionLy = galacticPositionLy",
):
    if required not in resolver:
        fail(resolver_path, f"system-local -> galactic resolution disappeared: {required}")

for required in (
    "system->positionLy",
    "navigation.systemLocalAu",
    "world::coordinates::toGalacticLy",
):
    if required not in galaxy_pres:
        fail(galaxy_pres_path, f"galaxy presentation position contract disappeared: {required}")

for required in (
    "observerGalacticPositionLy",
    "observerGalacticPositionValid",
):
    if required not in prepared:
        fail(prepared_path, f"prepared scene lost galaxy-scale observer state: {required}")

if "world::coordinates::toGalacticLy(\n                frame.origin" in renderer:
    fail(
        renderer_path,
        "starfield treats system-local render origin as galactic-absolute",
    )

if "const glm::vec3 cameraLocalPosition(0.0f)" in renderer:
    fail(
        renderer_path,
        "CPU visibility/LOD still assumes the optical camera is the player origin",
    )

for required in (
    "glm::inverse(renderView)[3]",
    "cameraLocalPosition",
):
    if required not in renderer:
        fail(renderer_path, f"renderer lost player-local camera offset contract: {required}")

for required in (
    "prepared.observerGalacticPositionValid",
    "prepared.observerGalacticPositionLy",
    "m_starfieldRenderer.setObserverPositionLy",
):
    if required not in renderer:
        fail(renderer_path, f"starfield no longer consumes resolved galactic observer: {required}")

for required in (
    "resolvePlayerGalacticPositionLy",
    "resolveGalaxyPlayerMarkerPosition",
    "observerGalacticPositionPtr",
    "m_sceneRenderer.prepareScene",
):
    if required not in space:
        fail(space_path, f"SpaceState no longer bridges navigation -> render observer: {required}")

print("[PASS] galactic/system-local/player-relative coordinate-frame architecture")
