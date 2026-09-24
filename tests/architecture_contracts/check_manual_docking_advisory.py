#!/usr/bin/env python3
from pathlib import Path
import json
import sys

ROOT = Path(__file__).resolve().parents[2]


def read(rel: str) -> str:
    path = ROOT / rel
    if not path.is_file():
        raise AssertionError(f"missing {rel}")
    return path.read_text(encoding="utf-8", errors="replace")


def require(rel: str, *tokens: str) -> None:
    body = read(rel)
    for token in tokens:
        if token not in body:
            raise AssertionError(f"{rel}: missing docking commissioning token {token!r}")


try:
    require("src/game/server/ControlRegistry.h",
            "ControllerKind::Autopilot", "takeAutopilotControl", "restoreHumanControl")
    require("src/game/server/GameServer.h",
            "controlledEntityAutopilotActiveForSession")
    require("src/game/server/GameServer.cpp",
            "BeginDockingGuidancePreparation",
            "CancelDockingGuidancePreparation",
            "CompleteDockingGuidancePreparation",
            "applyDockingGuidancePreparationControls",
            "VelocityAlignmentMode::BrakeToStop",
            "discardPendingAndAcknowledgeNewest",
            "controlledEntityAutopilotActive")
    server = read("src/game/server/GameServer.cpp")
    if server.count("controlledEntityAutopilotActiveForSession(sessionId)") != 3:
        raise AssertionError(
            "all three per-session snapshot copy paths must publish Autopilot authority"
        )

    require("src/game/client/GameClient.cpp",
            "m_externalControlPredictionSuppressed",
            "setExternalControlPredictionSuppressed",
            "never replayed locally")
    require("src/game/simulation/ClientSessionSnapshot.h",
            "controlledEntityAutopilotActive")
    require("src/game/network/WireDataCodec.h",
            "SimulationSnapshotWireSchemaVersion = 9u")
    require("src/game/SpaceState.cpp",
            "phase=stabilizing", "buildAuthoritativeHubSnapshot",
            "relativeSpeedMps", "angularRateRadPerSec", "SettleHoldSeconds",
            "request.gateSpacingMeters = 500.0",
            "phase=handoff_wait", "human_control=1",
            "controlledEntityAutopilotActive",
            "cockpit.docking.manual_mode")
    require("src/game/SpaceState.cpp",
            "resolveDockingAdvisoryLocalPortAt(",
            "sampleHubMapRuntimeAtServerTime(",
            "metadata.serverTick != active.lastValidatedTick",
            "observedShip->localPositionMeters",
            "active.timelineRevision",
            "request.startMeters = snapshot.controlledShip.localPositionMeters",
            "const double renderTime = playerRenderFrame.universeTimeSeconds",
            "playerRenderFrame.kinematicFrame()",
            "buildGuidanceCorridorHudPresentation(")
    advisory = read("src/game/SpaceState.cpp").split(
        "void SpaceState::updateDockingAdvisory()", 1
    )[1].split("void SpaceState::update(float dt)", 1)[0]
    if ("frame.worldToLocalPosition(ship" in advisory or
            "makeRoute(frame, port, time)" in advisory or
            "predictHubSemanticAnchorAt(active.port" in advisory):
        raise AssertionError("docking advisory reintroduced mixed-epoch geometry")
    require("src/game/navigation/GuidanceCorridor.h",
            "hubLocalFrameId", "hubLocalGatePositionsMeters",
            "deviationWarning", "deviationCritical")
    require("src/game/navigation/DockingAdvisoryCorridor.h",
            "dockingAdvisoryReleaseCrossSection",
            "dockingAdvisoryFrameExtentMeters",
            "DockingAdvisoryTrackingResult::Warning")
    require("src/game/system_map/SystemMapRenderer.cpp",
            "corridor->hubLocalFrameId == hub.hubId",
            "corridor->hubLocalGatePositionsMeters[index]",
            "m_hubPresentation.camera.project(")
    require("src/render/cockpit/GuidanceCorridorRenderer.cpp",
            "projectedUpperLeft",
            "frameDistanceMeters <= 500.0",
            "bottomCenter",
            "deviationBlinkOn")
    renderer = read("src/render/cockpit/GuidanceCorridorRenderer.cpp")
    label_begin = renderer.find("speedLabels.emplace_back(")
    label_end = renderer.find("label.str()", label_begin)
    if label_begin < 0 or label_end < 0:
        raise AssertionError("speed label placement block missing")
    label_anchor = renderer[label_begin:label_end]
    if "projected.corners[" in label_anchor:
        raise AssertionError("speed label is still tied to an arbitrary projected corner")
    if "projectedUpperLeft(projected.corners)" not in label_anchor:
        raise AssertionError("speed label lost stable projected-frame anchor")

    flight = json.loads(read("src/assets/localization/ui/cockpit/flight.json"))
    entry = flight["strings"].get("cockpit.docking.manual_mode", {})
    for locale in ("en", "ru", "zh-Hans", "es", "ja"):
        if not entry.get(locale):
            raise AssertionError(f"cockpit.docking.manual_mode missing locale {locale}")

    for retired in (
        "src/world/navigation/GuidanceTunnel.cpp",
        "src/world/navigation/GuidanceTunnel.h",
        "src/game/navigation/DockingPathPlanner.cpp",
        "src/game/navigation/DockingPathPlanner.h",
    ):
        if (ROOT / retired).exists():
            raise AssertionError(f"retired docking path returned: {retired}")

    print("[PASS] manual docking prep -> authoritative stop -> 500m guidance -> human hand-back")
except (AssertionError, KeyError, json.JSONDecodeError) as exc:
    print(f"[FAIL] {exc}", file=sys.stderr)
    raise SystemExit(1)
