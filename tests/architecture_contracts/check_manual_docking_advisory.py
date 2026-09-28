#!/usr/bin/env python3
from pathlib import Path
import json
import sys

ROOT = Path(__file__).resolve().parents[2]
CHECK_REVISION = "20260928-vehicle-derived-terminal-radius-v9"


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
            "SimulationSnapshotWireSchemaVersion = 10u")
    require("src/game/SpaceState.cpp",
            "phase=stabilizing", "buildAuthoritativeHubSnapshot",
            "relativeSpeedMps", "angularRateRadPerSec", "SettleHoldSeconds",
            "request.gateSpacingMeters = 500.0",
            "request.terminalGateSpacingMeters = 250.0",
            "request.hasInitialForward = true",
            "request.initialForwardLeadMeters",
            "frame.worldToLocalVector(",
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
    require("src/game/SpaceState.h",
            "game::navigation::DockingAdvisoryPlan plan",
            "SpaceState owns presentation/tracking state only",
            "Planner output is")
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
            "hasInitialForward = false",
            "initialForwardLeadMeters = 0.0",
            "terminalGateSpacingMeters = 250.0",
            "terminalDenseDistanceMeters = 2000.0",
            "terminalApproachLengthMeters = 0.0",
            "terminalTurnSegmentFraction = 0.40",
            "deriveTerminalTurnRadiusFromVehicle = false",
            "maxAngularVelocityRadPerSecond = 0.0",
            "maxAngularAccelerationRadPerSecond2 = 0.0",
            "preferredTerminalTurnRadiusMeters = 0.0")
    require("src/game/navigation/DockingAdvisoryPlanner.cpp",
            "routeSearchStart",
            "initial forward corridor blocked",
            "prependInitialForwardLead",
            "initialForwardProtectedStraightMeters",
            "maximumLaunchCut",
            "initialForwardAcceptedLeadMeters",
            "initialTurnPresent",
            "initialTurnRadiusMeters",
            "mandatoryApproachLengthMeters",
            "preferredApproachLengthMeters",
            "dock mandatory ingress blocked",
            "axisRejoinMarginMeters",
            "clearLength-axisRejoinMarginMeters",
            "axisRetreatMeters",
            "terminalApproachShortened=true",
            "desiredRadius",
            "tangentDistance/tangentScale",
            "arcLength=radius*turnAngle",
            "center=entry+radius*inwardNormal",
            "preferred terminal turn radius unavailable on candidate",
            "terminalTurnSpeedMps",
            "lateralTerminalRadiusMeters",
            "angularTerminalRadiusMeters",
            "angularRampDistanceMeters",
            "terminalPrimitiveRadius",
            "terminalIngressSamples=36",
            "const glm::dvec3 preEntry",
            "const glm::dvec3 center",
            "const glm::dvec3 entry",
            "candidate.terminalTurnRadiusMeters=",
            "candidate.terminalArcRotationDegrees",
            "candidate.terminalApproachLengthMeters",
            "terminalArcCandidatesTested",
            "terminalArcAcceptedCandidates",
            "axisOffsetFactors",
            "candidateApproachLengthMeters",
            "transit-endpoint-mismatch",
            "firstBlockingObstacle",
            "roundGeometry(points,0.0,entry)",
            "no collision-free exact-radius terminal arc",
            "dockingAdvisoryPlanDiagnosticSummary",
            "\" final_axis_m=\"",
            "\" axis_passes=\"",
            "\" blocker=\"",
            "\" last_rejection=\"",
            "remainingFromPrevious",
            "r.terminalDenseDistanceMeters+r.gateSpacingMeters",
            "USER-CONTRACT: published docking frames use the authored")
    require("src/game/SpaceState.cpp",
            "phase=settled",
            "vrel_mps=",
            "SettleHoldSeconds",
            "guidanceAssisted",
            "guidanceControlLaw",
            "request.roundTurns = guidanceAssisted",
            "request.terminalApproachLengthMeters = 9000.0",
            "request.terminalTurnSegmentFraction = 0.85",
            "request.deriveTerminalTurnRadiusFromVehicle = true",
            "request.maxAngularVelocityRadPerSecond =",
            "request.maxAngularAccelerationRadPerSecond2 =",
            "turn_radius_policy=",
            "vehicle-derived",
            "m_dockAdvice.plan = std::move(job->plan)",
            "const auto& gates = active.plan.gates",
            "const auto& mapRouteGates =",
            "active.plan.executionGates",
            "auto route = makeRoute(mapRouteGates)",
            "auto frameRoute = makeRoute(gates)",
            "dockingAdvisoryPlanDiagnosticSummary(job->plan)",
            "hud_gates=",
            "map_points=",
            "longitudinalToleranceMeters + std::max(",
            "30.0,")
    require("tests/navigation_runtime/DockingAdvisoryPlannerTests.cpp",
            "manual docking launch cadence is not 500 m",
            "nose-first route did not author a continuous launch fillet",
            "never transitioned into a launch arc")
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
    for forbidden in (
        "m_dockAdvice.gates =",
        "m_dockAdvice.mapRouteGates",
        "active.mapRouteGates",
        "active.gates",
        "plan.executionGates.empty()",
    ):
        if forbidden in space_cpp:
            raise AssertionError(
                "SpaceState regained ownership of Planner route geometry: "
                + forbidden
            )

    if "request.gateSpacingMeters = 150.0" in space_cpp:
        raise AssertionError(
            "manual docking cadence changed from locked 500 m / 250 m contract"
        )
    if "longitudinalToleranceMeters * 0.25" in space_cpp:
        raise AssertionError(
            "manual docking longitudinal release reverted to the old 25% margin"
        )

    planner_cpp = read("src/game/navigation/DockingAdvisoryPlanner.cpp")

    if "distanceToActivation" in planner_cpp:
        raise AssertionError(
            "docking planner reintroduced fractional cadence-transition frames"
        )

    # The preferred long axis is allowed to be probed with clear(align, stop);
    # that probe now shortens the soft lead. What must never return is the old
    # control flow where the preferred-axis probe immediately rejects the task.
    retired_hard_axis_failures = (
        'if (!clear(align,stop))\\n    { out.failure = "dock alignment blocked"; return out; }',
        'if (!clear(align, stop))\\n    { out.failure = "dock alignment blocked"; return out; }',
        'out.failure = "dock alignment blocked"',
    )
    for retired in retired_hard_axis_failures:
        if retired in planner_cpp:
            raise AssertionError(
                "preferred full docking-axis lead became a hard failure again"
            )

    if "terminalApproachShortened=true" not in planner_cpp:
        raise AssertionError(
            "preferred-axis obstruction no longer shortens the soft lead"
        )
    if "dock mandatory ingress blocked" not in planner_cpp:
        raise AssertionError(
            "mandatory close-in docking ingress lost its dedicated hard failure"
        )

    if 'out.failure="manual terminal turn radius unavailable"' in planner_cpp:
        raise AssertionError(
            "preferred manual terminal radius became a task-failure threshold again"
        )

    if "preferredTerminalRadius*clearanceScale" in planner_cpp:
        raise AssertionError(
            "terminal docking arc regressed to clearance-scaling around a preselected route"
        )

    if "selected=std::move(relaxed)" in planner_cpp:
        raise AssertionError(
            "Assisted terminal arc regained silent radius-relaxation fallback"
        )

    if "request.preferredTerminalTurnRadiusMeters = 6000.0" in space_cpp:
        raise AssertionError(
            "manual Assisted docking regressed to a fixed 6 km terminal radius"
        )

    if "roundGeometry(\n                    points,\n                    terminalPrimitiveRadius" in planner_cpp:
        raise AssertionError(
            "exact terminal docking arc was handed back to generic corner rounding"
        )

    if "terminalIngressSamples=36" not in planner_cpp:
        raise AssertionError(
            "terminal docking arc no longer searches all sides around the docking axis"
        )

    if "j==arcSegments" not in planner_cpp or "point=align" not in planner_cpp:
        raise AssertionError(
            "exact terminal arc no longer preserves ALIGN as its authored endpoint"
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

    print(f"[PASS] manual docking contract {CHECK_REVISION}: prep -> authoritative stop -> guidance -> human hand-back")
except (AssertionError, KeyError, json.JSONDecodeError) as exc:
    print(f"[FAIL] {exc}", file=sys.stderr)
    raise SystemExit(1)
