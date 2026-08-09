#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]

checks = []

def require(path: str, *needles: str):
    text = (ROOT / path).read_text(encoding="utf-8")
    for needle in needles:
        checks.append((path, needle, needle in text))

require(
    "src/game/diagnostics/HubMotionLab.h",
    "HubMotionLabActorKind::SlowOrbit",
    "HubMotionLabActorKind::FastOrbit",
    "HubMotionLabActorKind::MatchPlayer",
    "evaluateHubMotionLabCube"
)

require(
    "src/game/scene/GameSceneSetup.cpp",
    "spawnHubMotionLabNpcs(sim, stationPos);",
    "registerHubMotionLabShip"
)

require(
    "src/game/simulation/GameSimulation.cpp",
    "if (isHubMotionLabShip(id))",
    "updateHubMotionLabActors();",
    "s.motionLabKind = hubMotionLabActorKind(id);",
    "const bool motionLabProbe = isHubMotionLabShip(id);",
    "!motionLabProbe &&"
)

require(
    "src/game/client/ClientWorldState.cpp",
    "m_presentationServerTimeSeconds = renderServerTimeSeconds;",
    "state.motionLabKind = s.motionLabKind;",
    "labTelemetry.clampedToNewest",
    "appendHubMotionLabPresentationCsv(labTelemetry);"
)

require(
    "src/game/diagnostics/HubMotionLabTelemetry.h",
    "requestedMinusNewestMilliseconds",
    "slowNpcLocalErrorMeters",
    "matchVsPredictedPlayerDistanceDeltaMeters",
    "matchVsDelayedPlayerErrorMeters",
    "playerFixedToFractionalTargetMeters",
    "hub_motion_lab_presentation.csv"
)

require(
    "src/scene/SceneRenderer.cpp",
    "renderHubMotionLabAnalyticCube",
    "world.presentationServerTimeSeconds()",
    "evaluateHubMotionLabCube"
)

require(
    "src/game/server/GameServer.cpp",
    "hubMotionLabLabel(motionLabKind)",
    "hubMotionLabActorKind(entityId)",
    "diagnostic:hub_motion_lab_cube"
)

require(
    "src/game/SpaceState.cpp",
    "[HubMotionLab][startup] scene-renderer-ready",
    "[HubMotionLab][startup] hud-ready",
    "[HubMotionLab][bad_alloc] phase=scene-renderer-initialize",
    "[HubMotionLab][bad_alloc] phase=system-map-renderer-init",
    "[HubMotionLab][bad_alloc] phase=galaxy-snapshot-request",
    "[HubMotionLab][bad_alloc] phase=hud-init",
    "[HubMotionLab][bad_alloc] phase=client-prepareGameplayFrame",
    "[HubMotionLab][bad_alloc] phase=server-advance",
    "[HubMotionLab][bad_alloc] phase=client-update",
    "[HubMotionLab][bad_alloc] phase=station-traffic"
)

require(
    "src/scene/SceneRenderer.cpp",
    "[HubMotionLab][bad_alloc] phase=SceneRenderer::prepareScene",
    "[HubMotionLab][bad_alloc] phase=SceneRenderer::renderPrepared"
)

failed = [(path, needle) for path, needle, ok in checks if not ok]

if failed:
    for path, needle in failed:
        print(f"[FAIL] {path}: missing {needle!r}")
    sys.exit(1)

print(f"[PASS] Hub Motion Lab architecture guard ({len(checks)} assertions)")
