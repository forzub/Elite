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
            "marginFraction = 1.00",
            "minimumMarginMeters = 30.0",
            "releaseGraceSeconds = 1.00",
            "dockingAdvisoryFrameExtentMeters",
            "DockingAdvisoryTrackingResult::Warning",
            "nearBoundary")
    require("src/game/navigation/DockingAdvisoryPlanner.h",
            "gateSpacingMeters = 500.0",
            "terminalGateSpacingMeters = 250.0",
            "terminalDenseDistanceMeters = 2000.0",
            "terminalApproachLengthMeters = 0.0",
            "terminalTurnSegmentFraction = 0.40",
            "preferredTerminalTurnRadiusMeters = 0.0")
    require("src/game/navigation/DockingAdvisoryPlanner.cpp",
            "mandatoryApproachLengthMeters",
            "preferredApproachLengthMeters",
            "dock mandatory ingress blocked",
            "terminalApproachShortened=true",
            "desiredRadius",
            "tangentDistance/tangentScale",
            "arcLength=radius*turnAngle",
            "center=entry+radius*inwardNormal",
            "preferred terminal turn radius unavailable on candidate",
            "preferredTerminalRadius*clearanceScale",
            "no collision-free docking route after reroute/tighten fallback",
            "remainingFromPrevious",
            "r.terminalDenseDistanceMeters+terminalSpacing")
    require("src/game/SpaceState.cpp",
            "phase=settled",
            "vrel_mps=",
            "SettleHoldSeconds",
            "manual-assisted",
            "request.terminalApproachLengthMeters = 9000.0",
            "request.terminalTurnSegmentFraction = 0.85",
            "request.preferredTerminalTurnRadiusMeters = 6000.0",
            "longitudinalToleranceMeters + std::max(",
            "30.0,",
            "final_axis_m=",
            "final_axis_shortened=",
            "terminal_radius_m=",
            "radius_relaxed=")
    require("src/game/server/GameServer.cpp",
            "[DockPrep] begin entity=",
            "vrel_mps=",
            "localFlightControlLawName",
            "forward_main_mps2=",
            "reverse_main_mps2=",
            "VelocityAlignmentMode::BrakeToStop")
    require("src/game/system_map/SystemMapRenderer.cpp",
            "corridor->hubLocalFrameId == hub.hubId",
            "corridor->hubLocalGatePositionsMeters[index]",
            "m_hubPresentation.camera.project(")
    require("src/render/cockpit/GuidanceCorridorRenderer.cpp",
            "projectedUpperLeft",
            "frameDistanceMeters <= 500.0",
            "bottomCenter",
            "deviationBlinkOn")
    space_cpp = read("src/game/SpaceState.cpp")
    if "longitudinalToleranceMeters * 0.25" in space_cpp:
        raise AssertionError(
            "manual docking longitudinal release reverted to the old 25% margin"
        )

    planner_cpp = read("src/game/navigation/DockingAdvisoryPlanner.cpp")
    if "if (!clear(align,stop))" in planner_cpp:
        raise AssertionError(
            "preferred full docking-axis lead became a hard failure again"
        )
    if 'out.failure="manual terminal turn radius unavailable"' in planner_cpp:
        raise AssertionError(
            "preferred manual terminal radius became a task-failure threshold again"
        )

    corridor_header = read("src/game/navigation/DockingAdvisoryCorridor.h")
    if "const auto near =" in corridor_header:
        raise AssertionError("Windows-unsafe near identifier returned in docking corridor")

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
