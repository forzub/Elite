#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

def read(rel: str) -> str:
    path = ROOT / rel
    if not path.is_file():
        raise AssertionError(f"missing {rel}")
    return path.read_text(encoding="utf-8", errors="replace")

def require(rel: str, *tokens: str) -> str:
    body = read(rel)
    compact = "".join(body.split())
    for token in tokens:
        if token not in body and "".join(token.split()) not in compact:
            raise AssertionError(f"{rel}: missing token {token!r}")
    return body

def forbid(rel: str, *tokens: str) -> str:
    body = read(rel)
    for token in tokens:
        if token in body:
            raise AssertionError(f"{rel}: forbidden server-navigation token {token!r}")
    return body

try:
    for deleted in (
        "src/game/navigation/autopilot/ShipControlAdapter.h",
        "src/game/navigation/autopilot/RouteFollowerApi.h",
        "src/game/navigation/autopilot/RouteFollower.cpp",
        "src/game/navigation/autopilot/CorridorCaptureGuidance.h",
        "src/game/navigation/autopilot/VelocityCourseGuidance.h",
        "tests/navigation_runtime/NavigationCompositeProvingGroundTests.cpp",
        "tests/navigation_runtime/RouteFollowerApiTests.cpp",
        "tests/navigation_runtime/CorridorCaptureGuidanceTests.cpp",
        "tests/navigation_runtime/VelocityCourseGuidanceTests.cpp",
    ):
        if (ROOT / deleted).is_file():
            raise AssertionError(
                "retired navigation generation returned: " + deleted
            )

    renderer = require(
        "src/game/system_map/SystemMapRenderer.cpp",
        'automatic.key = "start_docking"',
        "[DockUi] pointer-consumed",
        "DockingRouteRequest::Mode::Automatic",
        "dockingRouteRequests().request(",
        "NavigationModuleId::LocalGuidance",
        "NavigationModuleId::HudGuidanceCorridor",
    )
    if "automatic.enabled = false" in renderer:
        raise AssertionError("Start Docking is permanently disabled")

    # Navigation task intent never crosses the network boundary.
    forbid(
        "src/game/network/ClientShipCommand.h",
        "BeginDockingGuidancePreparation",
        "CompleteDockingGuidancePreparation",
        "CancelDockingGuidancePreparation",
        "BeginAutomaticDocking",
        "CancelAutomaticDocking",
        "dockingTargetSystemId",
        "dockingTargetModuleId",
        "dockingTargetAnchorId",
        "requestSerial",
    )
    forbid(
        "src/game/network/WireProtocol.h",
        "dockingTargetSystemId",
        "dockingTargetModuleId",
        "dockingTargetAnchorId",
        "CancelAutomaticDocking",
    )
    forbid(
        "src/game/simulation/ClientSessionSnapshot.h",
        "AutomaticDockingRoutePoint",
        "automaticDockingRoute",
        "dockingResultSerial",
        "controlledEntityAutopilotActive",
    )

    server = require(
        "src/game/server/GameServer.cpp",
        "submitCommand(controlledEntityId, payload)",
        "FixedStepControlQueue",
        "ship.setControlState(cmd)",
        "Server sees only ordinary ship controls",
    )
    for token in (
        "DockingAutomaticRuntime",
        "beginAutomaticDocking",
        "planAutomaticDocking",
        "applyAutomaticDockingControls",
        "beginDockingGuidancePreparation",
        "applyDockingGuidancePreparationControls",
        "RoutePlanner",
        "TrajectoryGenerator",
        "AcceptedManeuverProgram",
        "RouteFollower",
        "PredictivePilot",
        "DockingAdvisoryPlanner",
        "automaticDockingRoute",
        "DockResult",
        "DockAuto",
        "DockPrep",
    ):
        if token in server:
            raise AssertionError(
                "GameServer regained client-navigation ownership: " + token
            )

    forbid(
        "src/game/server/GameServer.h",
        "DockingAutomaticRuntime",
        "RouteFollower",
        "PredictivePilot",
        "AcceptedManeuverProgram",
        "HubSemanticAnchorCatalog",
        "DockingPortRuntimeStateCatalog",
    )

    # Manual route and Automatic share one client planner. Only the input owner
    # differs after RouteReady.
    space = require(
        "src/game/SpaceState.cpp",
        "ClientDockingPhase::Stabilizing",
        "ClientDockingPhase::Planning",
        "ClientDockingPhase::RouteReady",
        "ClientDockingPhase::Executing",
        "coastForPlanningControl",
        "navigationAccelerationDemandValid = true",
        "PlanningLeadSeconds = 1.0",
        "startVelocityMps * PlanningLeadSeconds",
        "executionStartUniverseTimeSeconds",
        "ClientNavigationPlanningSnapshotFactory",
        "RoutePlanner::plan(request)",
        "request.gateSpacingMeters = 500.0",
        "request.terminalGateSpacingMeters = 250.0",
        "request.deriveTerminalTurnRadiusFromVehicle = true",
        "ClientAutopilot::start(",
        "ClientAutopilot::update(",
        "m_client->submitInput(m_clientAutopilotControl)",
        "execution=client-input",
        "route_source=client",
        "control_path=ShipControlState",
        "[DockClientFlow] observed-request",
        "phase=manual",
        "phase=executing",
    )
    for forbidden_token in (
        "BeginAutomaticDocking",
        "BeginDockingGuidancePreparation",
        "serverAutopilotActive",
        "automatic_route_source=server",
        "source=accepted-program",
        "controlledEntityAutopilotActive",
    ):
        if forbidden_token in space:
            raise AssertionError(
                "SpaceState still depends on server docking orchestration: "
                + forbidden_token
            )

    client_auto = require(
        "src/game/navigation/autopilot/ClientRouteAutopilot.h",
        "class ClientRouteAutopilot final",
        "TrajectoryGenerator::generate(",
        "AcceptedManeuverProgramBuilder::build(",
        "pathGeometryAlreadyAuthored = true",
        "CourseCaptureGuidance::evaluate(",
        "RouteSpeedGuidance::evaluateTurnSlowdown(",
        "sampleRouteCurveAtProgress(",
        "makeManeuverExecutionAuthority(params, law)",
        "PredictivePilot::make(",
        "ReferenceMode::SpatialCorridor",
        "ShipControlState",
        "actualCourseForCapture",
        "courseCaptureActive",
    )
    for token in (
        "TrajectoryFollower::follow(",
        "RouteFollower::follow(",
        "RouteFollower::sampleReference(",
        "steeringRay",
    ):
        if token in client_auto:
            raise AssertionError(
                "ClientRouteAutopilot regained a retired execution seam: " + token
            )

    capture = require(
        "src/game/navigation/autopilot/CourseCaptureGuidance.h",
        "actualVelocityMapMps",
        "routeCurves",
        "desiredCourseMap = current.tangent",
        "captureP1",
        "captureP2",
        "meetingRouteProgressMeters",
        "courseResponseSeconds",
        "start tangent is the ACTUAL velocity direction",
        "there is no fixed 50-250 m lookahead",
    )
    for token in ("ShipControlState", "HullPoseGuidance", "HullAttitudeControl"):
        if token in capture:
            raise AssertionError(
                "CourseCaptureGuidance is no longer pure route geometry: " + token
            )

    pilot = require(
        "src/game/navigation/autopilot/PredictivePilot.h",
        "Navigation course is owned by desired velocity",
        "request.desiredVelocityMapMps / desiredSpeed",
        "HullPoseGuidance::evaluate(",
        "HullAttitudeControl::evaluate(",
        "pitchInput",
        "yawInput",
        "rollInput",
        "targetSpeedRate",
        "forwardInput",
        "strafeInput",
        "liftInput",
        "VelocityAlignmentMode::BrakeToStop",
        "navigationAccelerationDemandValid = false",
        "navigationVelocityTargetValid = false",
        "navigationPrecisionTranslationOnly = false",
    )

    require(
        "tests/navigation_runtime/ClientRouteAutopilotTests.cpp",
        "testClientAutopilotEmitsOrdinaryControls",
        "testClientStabilizerUsesOrdinaryControls",
        "ReferenceMode::SpatialCorridor",
        "navigationAccelerationDemandValid",
        "navigationVelocityTargetValid",
    )
    require(
        "tests/navigation_runtime/CMakeLists.txt",
        "client_route_autopilot_tests",
        "NAME client_route_autopilot",
        "course_capture_guidance",
        "NAME navigation_scenario_runner",
        "NAME navigation_v2_tunnel_proving_ground",
        'PROPERTIES LABELS "legacy_navigation_lab"',
    )
    runtime_runner = require(
        "tests/navigation_runtime/run_mingw64.sh",
        "-LE legacy_navigation_lab",
        "navigation_v2_tunnel_proving_ground",
        "ELITE_RUN_LEGACY_NAVIGATION_LABS",
    )
    replan_tests = require(
        "tests/navigation_runtime/NavigationExecutionReplanPolicyTests.cpp",
        "testAutomaticDoesNotReplanEveryFrame",
        "testTrackingErrorInvalidatesAutomaticSegment",
    )
    if "TrajectoryFollower" in replan_tests:
        raise AssertionError(
            "production replan-policy gate regained retired TrajectoryFollower execution"
        )

    # Planner remains responsible for understandable failure taxonomy.
    require(
        "src/game/navigation/planner/RoutePlannerApi.h",
        "RoutePlanDisposition",
        "NeedsRefinement",
        "WaitForWindow",
        "PhysicallyImpossible",
        "InvalidWorldData",
        "HullDoesNotFit",
        "GoalGeometricallyIsolated",
        "UnavoidableCollision",
        "PropulsionInsufficient",
        "userMessage",
    )

    # Stop-and-settle must not collapse the authored terminal arc.
    require(
        "src/game/navigation/DockingAdvisoryPlanner.cpp",
        "designTurnSpeedMps",
        "0.80 * r.maxSpeedMps",
        "30.0 * r.hullRadiusMeters",
    )
    require(
        "tests/navigation_runtime/DockingAdvisoryPlannerTests.cpp",
        "stop-and-settle incorrectly collapsed the authored terminal turn",
    )

    print("[PASS] client-owned automatic docking architecture")
    print(" - Planner/ClientRouteAutopilot/PredictivePilot live on the client")
    print(" - Manual route and Automatic use one client Planner product")
    print(" - server receives only ordinary ShipControlState execution input")
    print(" - docking/navigation task intent is absent from the network protocol")
    print(" - velocity course, smooth recapture and PredictivePilot are separate frozen layers")
except AssertionError as exc:
    print(f"[FAIL] {exc}")
    raise SystemExit(1)
