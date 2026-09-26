#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def read(rel: str) -> str:
    path = ROOT / rel
    if not path.is_file():
        raise AssertionError(f"missing {rel}")
    return path.read_text(encoding="utf-8", errors="replace")


def require(rel: str, *tokens: str) -> str:
    body = read(rel)
    for token in tokens:
        if token not in body:
            raise AssertionError(f"{rel}: missing automatic-docking token {token!r}")
    return body


try:
    command = require(
        "src/game/network/ClientShipCommand.h",
        "BeginAutomaticDocking",
        "CancelAutomaticDocking",
        "dockingTargetSystemId",
        "dockingTargetModuleId",
        "dockingTargetAnchorId",
    )

    wire = require(
        "src/game/network/WireProtocol.h",
        "WireProtocolVersion = 11u",
        "value.dockingTargetSystemId",
        "value.dockingTargetModuleId",
        "value.dockingTargetAnchorId",
        "ClientShipCommand::CancelAutomaticDocking",
    )

    renderer = require(
        "src/game/system_map/SystemMapRenderer.cpp",
        'automatic.key = "start_docking"',
        "automatic.enabled = compatibility.routeAvailable",
        "DockingRouteRequest::Mode::Automatic",
        "dockingRouteRequests().request(",
    )
    if "automatic.enabled = false" in renderer:
        raise AssertionError("Start Docking returned to a permanently disabled presentation action")
    if 'if (actionKey == "start_docking")\n        return' in renderer:
        raise AssertionError("Start Docking dispatch is still a fail-closed no-op")

    space = require(
        "src/game/SpaceState.cpp",
        "BeginAutomaticDocking",
        "CancelAutomaticDocking",
        "m_automaticDockingAuthoritySeen",
        "serverAutopilotActive()",
        "setExternalControlPredictionSuppressed(true)",
        "setExternalControlPredictionSuppressed(false)",
        "phase=requested",
        "phase=server-handoff",
    )

    server = require(
        "src/game/server/GameServer.cpp",
        "beginAutomaticDocking(",
        "applyAutomaticDockingControls(",
        "planAutomaticDocking(",
        "finishAutomaticDocking(",
        "takeAutopilotControl",
        "VelocityAlignmentMode::BrakeToStop",
        "TrajectoryGenerator::generate(",
        "AcceptedManeuverProgramBuilder::build(",
        "TrajectoryFollower::follow(",
        "NavigationFrameBoundary boundary",
        "toSystemControlIntent(",
        "controlBridge->step(",
        "ship->setControlState(step.control)",
        "terminalAngularVelocityMapRadPerSec",
        "phase=replan",
    )

    header = require(
        "src/game/server/GameServer.h",
        "struct DockingAutomaticRuntime",
        "std::vector<game::navigation::AcceptedManeuverProgram> programs",
        "NavigationRuntimeControlBridge",
        "m_dockingAutomaticRuntimes",
        "m_serverHubSemanticAnchorCatalog",
        "m_serverDockingPortRuntimeStateCatalog",
    )

    builder = require(
        "src/game/navigation/AcceptedManeuverProgramBuilder.h",
        "class AcceptedManeuverProgramBuilder final",
        "const world::navigation::Trajectory* trajectory",
        "hasTerminalAngularVelocity",
        "deriveAngularKinematics(",
        "angularKinematicsFeasible(",
        "actuatorProgramFeasible",
        "completionTriggersReplan",
    )

    follower_h = read("src/game/navigation/TrajectoryFollower.h")
    follower_cpp = read("src/game/navigation/TrajectoryFollower.cpp")
    if "AcceptedShortSegment" in follower_h + follower_cpp:
        raise AssertionError(
            "TrajectoryFollower regained the retired AcceptedShortSegment execution API"
        )

    lab_adapter = require(
        "src/game/diagnostics/NavigationRuntimeLabAcceptedProgramAdapter.h",
        "NavigationRuntimeLabAcceptedProgramAdapter",
        "AcceptedShortSegment",
        "AcceptedManeuverProgram",
    )
    simulation = require(
        "src/game/simulation/GameSimulation.cpp",
        "NavigationRuntimeLabAcceptedProgramAdapter::adapt",
        "followAcceptedSegment",
    )

    cmake = require(
        "CMakeLists.txt",
        "src/game/navigation/DockingPortRuntimeStateCatalog.cpp",
    )
    runtime_cmake = require(
        "tests/navigation_runtime/CMakeLists.txt",
        "accepted_maneuver_program_builder_tests",
        "accepted_maneuver_program_builder",
    )
    builder_test = require(
        "tests/navigation_runtime/AcceptedManeuverProgramBuilderTests.cpp",
        "testTerminalAngularVelocityIsAcceptedAndPreserved",
        "testImpossibleTerminalSpinIsRejected",
    )

    print("[PASS] automatic docking ownership/execution contract")
    print(" - UI request is distinct from manual guidance")
    print(" - server owns Autopilot authority and stabilization")
    print(" - trajectory is converted to AcceptedManeuverProgram before Follower")
    print(" - Follower has one executable input type")
    print(" - rotating target omega is part of terminal acceptance")
except AssertionError as exc:
    print(f"[FAIL] {exc}", file=sys.stderr)
    raise SystemExit(1)
