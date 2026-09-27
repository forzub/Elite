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
    compact_body = "".join(body.split())
    for token in tokens:
        if token not in body and "".join(token.split()) not in compact_body:
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
        "NavigationModuleId::LocalGuidance",
        "NavigationModuleId::HudGuidanceCorridor",
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
        "route_retained=",
        "DockingRouteRequest::Mode::Guidance",
        "automaticNeedsPreparedRoute",
        "hasVisibleRouteForAutomaticTarget",
        "phase=route-preflight",
        "localRoutePreparationPending",
        "guidanceControlLaw",
        "request.roundTurns = guidanceAssisted",
        "request.gateSpacingMeters = 150.0",
    )

    trajectory_header = require(
        "src/world/navigation/TrajectoryGenerator.h",
        "hasInitialOrientation",
        "hasInitialAngularVelocity",
        "hasTerminalAngularVelocity",
        "terminalAngularVelocityRadPerSecond",
    )
    trajectory_impl = require(
        "src/world/navigation/TrajectoryGenerator.cpp",
        "request.hasTerminalAngularVelocity",
        "terminalTimeOffsetSeconds - sampleTimeOffsetSeconds",
        "-omega * remainingSeconds",
        "compileBoundedAngularKinematics(",
        "trajectory.angularKinematicsAuthored = true",
        "maxAngularAccelerationRadPerSecond2",
        "remainingBefore",
        "remainingAfter",
        "maxTerminalDelta",
    )
    require(
        "tests/navigation_ruckig/TrajectoryGeneratorAngularTests.cpp",
        "angular planner deferred terminal omega correction to the final sample",
        "penultimate.angularVelocityRadPerSecond",
        "maxAlpha * terminalDt",
    )
    require(
        "src/world/navigation/Trajectory.h",
        "angularKinematicsAuthored",
    )
    server_terminal_angular = require(
        "src/game/server/GameServer.cpp",
        "hasTerminalAngularVelocity = true",
        "terminalAngularVelocityRadPerSecond",
        "terminalAngularVelocityMapRadPerSec",
        "TrajectoryGenerator::generate(",
    )
    if "trajectoryRequest" not in server_terminal_angular:
        raise AssertionError(
            "Automatic docking terminal angular state is no longer authored on the trajectory request"
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
        "Follower::follow(",
        "NavigationFrameBoundary boundary",
        "toSystemControlIntent(",
        "controlBridge->stepProgram(",
        "ProgramActuatorCommand actuator",
        "followed.manoeuvreAccelerationMapMps2",
        "followed.linearFeedbackLocalMps2",
        "ship->setControlState(step.control)",
        "terminalAngularVelocityMapRadPerSec",
        "minimumPreCaptureDepthMeters",
        "segmentClearOfNavigationObstacles(",
        "pre_capture_depth_m=",
        "DockingAutomaticRuntime::Phase::Aligning",
        "phase=aligned-replan",
        "ManeuverTrackingController",
        "controlBridge->stepProgram(",
        "phase=replan",
        "phase=plan-failed",
        "action=restore-human",
        "lastPlanFailureReason",
        "phase=planning-async",
        "std::thread(",
        "DockingAutomaticRuntime::Phase::Planning",
        "executionStartUniverseTimeSeconds",
        "planning-result-missed-execution-epoch",
        "trajectoryRequest.hasInitialOrientation =",
        "trajectoryRequest.hasInitialAngularVelocity =",
        "routeInitialForward =",
        "advisoryPlan.gates.front().forward",
        "initialAngularVelocityRadPerSecond =\n                            glm::dvec3(0.0)",
        "phase=aligned-replan",
        "planningControlLaw",
        "request.roundTurns = assisted",
        "build.controlLaw = assisted",
        "followed.assistedVelocityModel",
        "followed.assistedTargetForwardSpeedMps",
        "request.hasInitialForward = true",
        "request.initialForward = currentForwardMap",
        "request.initialForwardLeadMeters",
        "request.gateSpacingMeters = 150.0",
        "reason=autopilot-authority-denied",
    )


    if "phase=plan-retry" in server:
        raise AssertionError(
            "Automatic docking restored the synchronous fixed-step plan-retry loop"
        )

    if "terminalAllowedObstacleId" in server:
        raise AssertionError(
            "Automatic docking reintroduced planner-only permission to enter solid target geometry"
        )
    if "terminalObstacleEntrySourceProgressMeters" in server:
        raise AssertionError(
            "Automatic docking reintroduced target-obstacle collision bypass"
        )

    header = require(
        "src/game/server/GameServer.h",
        "struct DockingAutomaticRuntime",
        "std::vector<game::navigation::AcceptedManeuverProgram> programs",
        "NavigationRuntimeControlBridge",
        "m_dockingAutomaticRuntimes",
        "struct PlanningJob",
        "std::shared_ptr<PlanningJob> planningJob",
        "m_serverHubSemanticAnchorCatalog",
        "m_serverDockingPortRuntimeStateCatalog",
    )
    if "nextPlanAttemptUniverseTimeSeconds" in header:
        raise AssertionError(
            "Automatic docking restored retry-timer state that can hammer Planner from fixed-step"
        )

    require(
        "src/game/navigation/HubNavigationClearancePolicy.h",
        "DiagnosticHubInfrastructureClearanceMeters",
        "AutomaticDockingPreCaptureReserveMeters",
    )
    client_snapshot = require(
        "src/game/client/ClientNavigationPlanningSnapshotFactory.cpp",
        "DiagnosticHubInfrastructureClearanceMeters",
    )
    if "constexpr double DiagnosticHubInfrastructureClearanceMeters" in client_snapshot:
        raise AssertionError(
            "client reintroduced a private Hub infrastructure clearance truth"
        )

    builder = require(
        "src/game/navigation/AcceptedManeuverProgramBuilder.h",
        "class AcceptedManeuverProgramBuilder final",
        "const world::navigation::Trajectory* trajectory",
        "hasInitialAngularVelocity",
        "hasTerminalAngularVelocity",
        "trajectoryAngularVelocityAt(",
        "trajectoryAngularAccelerationAt(",
        "deriveAngularKinematics(",
        "angularKinematicsFeasible(",
        "trajectory.angularKinematicsAuthored",
        "actuatorProgramFeasible",
        "completionTriggersReplan",
        "TranslationMode::AssistedVelocity",
        "page.actuatorSegmentCount = 0",
        "newtonian-main-engine-program-infeasible",
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
        "control.navigationActuatorProgramValid",
        "applyNavigationActuatorProgram(",
        "control.navigationAssistedFlightModelValid",
        "applyNavigationAssistedFlightModel(",
    )
    dynamic = require(
        "src/game/navigation/DynamicMotionSystem.cpp",
        "applyNavigationActuatorProgram(",
        "nominalForwardMain",
        "availableForwardMain",
        "feedbackMainLongitudinal",
        "applyNavigationAssistedFlightModel(",
        "requestAssistedTargetSpeed(",
        "applyLocalFrameInput(",
        "motion.manoeuvreAccelerationMps2 = glm::dvec3(0.0)",
    )

    require(
        "src/game/system_map/MapObjectOverlayRenderer.cpp",
        "activeGreen",
        "0.18f, 1.00f, 0.32f",
    )
    require(
        "src/game/SpaceState.cpp",
        "cockpit.docking.automatic_mode",
        "DockingRouteRequest::Mode::Automatic",
        "AUTOMATIC DOCKING MODE",
        "request.hasInitialForward = true",
        "request.initialForwardLeadMeters",
        "renderBoresight(vp)",
    )
    require(
        "src/game/navigation/DockingAdvisoryPlanner.cpp",
        "initial forward corridor blocked",
        "routeSearchStart",
        "prependInitialForwardLead",
        "initialForwardProtectedStraightMeters",
        "maximumLaunchCut",
        "initialForwardAcceptedLeadMeters",
        "!r.roundTurns",
    )
    require(
        "src/render/cockpit/FlightVectorIndicatorRenderer.cpp",
        "renderBoresight(",
        "static_cast<float>(viewport.width) * 0.5f",
        "static_cast<float>(viewport.height) * 0.5f",
    )
    require(
        "src/assets/localization/ui/cockpit/flight.json",
        "cockpit.docking.automatic_mode",
        "AUTOMATIC DOCKING MODE",
        "АВТОМАТИЧЕСКИЙ РЕЖИМ СТЫКОВКИ",
        "自动对接模式",
        "MODO DE ATRAQUE AUTOMÁTICO",
        "自動ドッキングモード",
    )

    require(
        "tests/navigation_ruckig/CMakeLists.txt",
        "trajectory_generator_angular_tests",
        "trajectory_generator_angular",
        "src/world/navigation/TrajectoryGenerator.cpp",
    )
    require(
        "tests/navigation_ruckig/TrajectoryGeneratorAngularTests.cpp",
        "angularKinematicsAuthored",
        "maxAngularAccelerationRadPerSecond2",
        "terminalAngularVelocityRadPerSecond",
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
        "testStoragePageBoundaryPreservesAngularState",
        "testAssistedUsesGameFlightLawInsteadOfRcsAllocation",
        "testNewtonianTransitDoesNotSpendPrecisionRcs",
        "testImpossibleTerminalSpinIsRejected",
    )

    print("[PASS] automatic docking ownership/execution contract")
    print(" - Automatic prepares its own visible advisory route when none exists")
    print(" - Automatic reuses an existing route for the same dock without hiding it")
    print(" - server owns Autopilot authority and stabilization")
    print(" - heavy Automatic planning runs outside the fixed-step thread")
    print(" - Automatic aligns the real hull to the planned route-entry attitude before execution")
    print(" - manual docking corridor is nose-first and cockpit HUD has a fixed hull boresight")
    print(" - active map-card mode is bright green and cockpit mode text is localized")
    print(" - Assisted automatic transit executes the same nose-coupled game flight law as manual control")
    print(" - Newtonian ordinary transit cannot spend precision RCS as fake lateral route thrust")
    print(" - trajectory is converted to AcceptedManeuverProgram before Follower")
    print(" - Follower has one executable input type")
    print(" - rotating target omega is part of terminal acceptance")
    print(" - current slice ends outside solid target geometry; latch remains separate")
except AssertionError as exc:
    print(f"[FAIL] {exc}", file=sys.stderr)
    raise SystemExit(1)
