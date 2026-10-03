#!/usr/bin/env python3
from pathlib import Path
import json
import sys

ROOT = Path(__file__).resolve().parents[2]
CHECK_REVISION = "20261003-client-owned-guidance-v1"

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
            raise AssertionError(f"{rel}: missing manual-docking token {token!r}")
    return body

def forbid(rel: str, *tokens: str) -> str:
    body = read(rel)
    for token in tokens:
        if token in body:
            raise AssertionError(f"{rel}: forbidden token {token!r}")
    return body

try:
    # "Show route" is client-only navigation intent. It must never become a
    # server task/authority transition.
    forbid(
        "src/game/network/ClientShipCommand.h",
        "DockingGuidancePreparation",
        "AutomaticDocking",
        "requestSerial",
        "dockingTarget",
    )
    forbid(
        "src/game/server/GameServer.cpp",
        "DockPrep",
        "beginDockingGuidancePreparation",
        "takeAutopilotControl",
        "restoreHumanControl",
        "RoutePlanner",
        "PredictivePilot",
        "RouteFollower",
    )

    space = require(
        "src/game/SpaceState.cpp",
        "DockingRouteRequest::Mode::Automatic",
        "ClientDockingPhase::Stabilizing",
        "ClientDockingPhase::Planning",
        "ClientDockingPhase::RouteReady",
        "ClientAutopilot::stabilize(",
        "m_client->submitInput(m_clientAutopilotControl)",
        "buildAuthoritativeHubSnapshot",
        "relativeSpeedMps",
        "angularRateRadPerSec",
        "SettleHoldSeconds",
        "request.startMeters =",
        "request.hasInitialForward = true",
        "request.initialForwardLeadMeters",
        "request.gateSpacingMeters = 500.0",
        "request.terminalGateSpacingMeters = 250.0",
        "RoutePlanner::plan(request)",
        "m_dockAdvice.plan = std::move(job->plan)",
        "phase=route-ready",
        "phase=manual",
        "human_control=1",
    )

    # Manual completion returns input ownership locally simply by stopping the
    # virtual-pilot mux. There is no server hand-off protocol.
    require(
        "src/game/SpaceState.cpp",
        "m_clientAutopilotControlActive = false",
        "m_clientAutopilotControl = {}",
    )

    require(
        "src/game/navigation/autopilot/ClientRouteAutopilot.h",
        "static ShipControlState stabilize(",
        "VelocityAlignmentMode::BrakeToStop",
        "PredictivePilot::make(",
    )

    # Authoritative snapshots are still the source of actual ship/world state.
    require(
        "src/game/client/ClientNavigationPlanningSnapshotFactory.cpp",
        "NavigationHitVolumeAdapter::buildObstacles",
        "debugHitVolumes",
        "ObstacleGeometryUnavailable",
    )
    require(
        "src/game/navigation/NavigationHitVolumeAdapter.h",
        "DebugHitVolumeSnapshot",
        "authoritative replicated local hit-volume",
    )

    # Both presentation and Automatic consume the same Planner geometry.
    require(
        "src/game/navigation/planner/RoutePlannerApi.h",
        "std::vector<RouteGate> gates",
        "std::vector<RouteGate> executionGates",
    )
    require(
        "src/game/SpaceState.cpp",
        "active.plan.executionGates",
        "active.plan.gates",
        "guidance.publish(",
    )

    # Locked tunnel cadence and stop-speed radius doctrine.
    require(
        "src/game/navigation/DockingAdvisoryPlanner.cpp",
        "designTurnSpeedMps",
        "0.50 * r.maxSpeedMps",
        "20.0 * r.hullRadiusMeters",
        "terminalTurnSpeedMps",
        "terminalArcCandidatesTested",
        "terminalArcAcceptedCandidates",
    )
    require(
        "tests/navigation_runtime/DockingAdvisoryPlannerTests.cpp",
        "stop-and-settle incorrectly collapsed the authored terminal turn",
        "corridor warning/leave/reentry semantics failed",
    )

    require(
        "src/game/system_map/SystemMapRenderer.cpp",
        'calculate.key = "show_docking_route"',
        'automatic.key = "start_docking"',
        "DockingRouteRequest::Mode::Guidance",
        "DockingRouteRequest::Mode::Automatic",
    )

    require(
        "src/render/cockpit/GuidanceCorridorRenderer.cpp",
        "frameDistanceMeters <= 500.0",
        "deviationBlinkOn",
    )

    # No retired duplicate path planner may return.
    for retired in (
        "src/world/navigation/GuidanceTunnel.cpp",
        "src/world/navigation/GuidanceTunnel.h",
        "src/game/navigation/DockingPathPlanner.cpp",
        "src/game/navigation/DockingPathPlanner.h",
    ):
        if (ROOT / retired).exists():
            raise AssertionError(f"retired docking path returned: {retired}")

    flight = json.loads(read("src/assets/localization/ui/cockpit/flight.json"))
    entry = flight["strings"].get("cockpit.docking.manual_mode", {})
    for locale in ("en", "ru", "zh-Hans", "es", "ja"):
        if not entry.get(locale):
            raise AssertionError(
                f"cockpit.docking.manual_mode missing locale {locale}"
            )

    print(
        f"[PASS] manual docking contract {CHECK_REVISION}: "
        "client stabilize -> authoritative state -> client plan -> human input"
    )
except (AssertionError, KeyError, json.JSONDecodeError) as exc:
    print(f"[FAIL] {exc}", file=sys.stderr)
    raise SystemExit(1)
