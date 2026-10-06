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
        "RouteFollower::follow(",
        "RouteFollower::sampleReference(",
        "PredictivePilot::make(",
        "ReferenceMode::SpatialCorridor",
        "ShipControlState",
    )

    follower = require(
        "src/game/navigation/autopilot/RouteFollower.cpp",
        "the accepted centerline is the ONLY path",
        "2608765f48",
        "upperSpeed",
        "centerlinePoint",
        "inward * correctionSpeed",
        "targetVelocity / targetSpeed",
        "reference.forwardMap = desiredForward",
    )
    for token in ("TrajectoryFollower", "lookAhead", "steeringRay"):
        if token in follower:
            raise AssertionError(
                "RouteFollower V2 regained a private route mechanism: " + token
            )

    pilot = require(
        "src/game/navigation/autopilot/PredictivePilot.h",
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
    print(" - Planner/Follower/PredictivePilot live on the client")
    print(" - Manual route and Automatic use one client Planner product")
    print(" - server receives only ordinary ShipControlState execution input")
    print(" - docking/navigation task intent is absent from the network protocol")
    print(" - RouteFollower V2 executes the exact authored centerline")
except AssertionError as exc:
    print(f"[FAIL] {exc}")
    raise SystemExit(1)
