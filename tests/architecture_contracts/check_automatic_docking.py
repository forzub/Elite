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
        "WireProtocolVersion = 12u",
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
        "hasVisibleRouteForAutomaticTarget",
        "localRoutePreparationPending",
        "automatic_route_source=server",
        "source=accepted-program",
        "guidanceControlLaw",
        "request.roundTurns = guidanceAssisted",
        "request.gateSpacingMeters = 500.0",
        "request.terminalGateSpacingMeters = 250.0",
    )

    if "automaticNeedsPreparedRoute" in space or "phase=route-preflight" in space:
        raise AssertionError(
            "Automatic docking regressed to client-side route preflight before server planning"
        )

    trajectory_header = require(
        "src/world/navigation/TrajectoryGenerator.h",
        "hasInitialOrientation",
        "hasInitialAngularVelocity",
        "hasTerminalAngularVelocity",
        "terminalAngularVelocityRadPerSecond",
        "pathGeometryAlreadyAuthored",
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
        "AngularTimeScales",
        "angular-speed-relaxed",
        "after translation-speed relaxation",
        "buildAuthoredExecutionGuide(",
        "guide.points = request.pathPointsMeters",
        "path-progress acceleration exceeds vehicle envelope",
    )
    require(
        "tests/navigation_ruckig/TrajectoryGeneratorAngularTests.cpp",
        "angular planner deferred terminal omega correction to the final sample",
        "penultimate.angularVelocityRadPerSecond",
        "maxAlpha * terminalDt",
        "testTranslationSlowsWhenAngularTerminalNeedsMoreTime",
        "planner did not slow translation for the angular boundary",
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
        "build.referenceMode =",
        "ReferenceMode::SpatialCorridor",
        "ReferenceMode::TimeScheduled",
        "Timeline::selectSpatialPage(",
        "runtime.currentSpatialSegment",
        "Follower::follow(",
        "reference_mode=",
        "ref_segment=",
        "ref_distance_m=",
        "spatial_speed_scale=",
        "speed_target_mps=",
        "NavigationFrameBoundary boundary",
        "toSystemControlIntent(",
        "followed.targetVelocityMapMps",
        "automaticControl.navigationVelocityTargetValid = true",
        "ship->setControlState(automaticControl)",
        "terminalAngularVelocityMapRadPerSec",
        "minimumPreCaptureDepthMeters",
        "segmentClearOfNavigationObstacles(",
        "pre_capture_depth_m=",
        "DockingAutomaticRuntime::Phase::Aligning",
        "phase=aligned-replan",
        "ManeuverTrackingController",
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
        "executionGates.front().forward",
        "trajectoryRequest.pathGeometryAlreadyAuthored =",
        "const auto& executionGates =",
        "advisoryPlan.executionGates",
        "dockingAdvisoryTrace(advisoryPlan)",
        "requested_terminal_radius_m=",
        "arc_rotation_deg=",
        "arc_candidates=",
        "arc_accepted=",
        "initialAngularVelocityRadPerSecond =\n                            glm::dvec3(0.0)",
        "phase=aligned-replan",
        "planningControlLaw",
        "request.roundTurns =\n                            assisted && !nearHoldRecovery",
        "request.deriveTerminalTurnRadiusFromVehicle =",
        "request.maxAngularVelocityRadPerSecond =",
        "request.maxAngularAccelerationRadPerSecond2 =",
        "build.controlLaw = assisted",
        "followed.targetVelocityMapMps",
        "request.hasInitialForward = !nearHoldRecovery",
        "request.initialForward = currentForwardMap",
        "request.initialForwardLeadMeters",
        "request.gateSpacingMeters = 150.0",
        "NavigationHitVolumeAdapter::buildObstacles",
        "source.hitComponent = object.hitComponent",
        "reason=autopilot-authority-denied",
        "DockingAutomaticRuntime::Stage::ApproachHold",
        "DockingAutomaticRuntime::Stage::FinalIngress",
        "finalIngressStage",
        "This is the agreed stop before the short docking leg.",
        "build.hasTerminalAngularVelocity =",
        "stage=approach-hold",
        "phase=hold-complete",
        "next=final-ingress",
        "DockingAutomaticRecoveryPolicy::holdCaptureDistanceMeters",
        "DockingAutomaticRecoveryPolicy::holdCaptureSpeedMps",
        "enterFinalIngress(\"standoff-tracking-envelope\")",
        "recoverableDynamicExcursion(",
        "phase=correcting-envelope",
        "request.roundTurns =\n                            assisted && !nearHoldRecovery",
    )


    if "pathPointsMeters.\n                            push_back(\n                                preCaptureCenterMeters" in server:
        raise AssertionError(
            "Automatic docking regressed to appending pre-capture onto the long approach stage"
        )

    if "phase=plan-retry" in server:
        raise AssertionError(
            "Automatic docking restored the synchronous fixed-step plan-retry loop"
        )

    if server.index('enterFinalIngress("standoff-tracking-envelope")') > server.index(
        'phase=recovery reason=follower-rejected'
    ):
        raise AssertionError(
            "Automatic docking must capture a safe approach hold before "
            "rejecting the final program on residual dynamic error"
        )

    if "terminalAllowedObstacleId" in server:
        raise AssertionError(
            "Automatic docking reintroduced planner-only permission to enter solid target geometry"
        )
    if "terminalObstacleEntrySourceProgressMeters" in server:
        raise AssertionError(
            "Automatic docking reintroduced target-obstacle collision bypass"
        )

    if "advisoryPlan.executionGates.empty()" in server:
        raise AssertionError(
            "Automatic docking regained sparse-gate fallback instead of requiring Planner-owned dense geometry"
        )

    if "preferredTerminalTurnRadiusMeters =\n                                6000.0" in server:
        raise AssertionError(
            "Automatic Assisted docking regressed to a fixed 6 km terminal radius"
        )

    if "makeNavigationObstacleForObject" in server:
        raise AssertionError(
            "Automatic docking regressed to descriptor-wide Station obstacle geometry"
        )

    if "const double linearReserve = std::min(\n        0.5" in server:
        raise AssertionError(
            "Automatic docking tracking reserve regressed to the old 0.5 m/s^2 cap"
        )

    compact_server = "".join(server.split())
    if (
        "build.referenceMode=finalIngressStage?"
        "AcceptedManeuverProgram::ReferenceMode::TimeScheduled:"
        "AcceptedManeuverProgram::ReferenceMode::SpatialCorridor;"
        not in compact_server
    ):
        raise AssertionError(
            "Automatic docking lost Stage-1 spatial / FinalIngress timed reference split"
        )

    require(
        "src/game/simulation/ClientSessionSnapshot.h",
        "AutomaticDockingRoutePoint",
        "automaticDockingRouteValid",
        "automaticDockingRouteRevision",
        "automaticDockingRoute",
    )
    require(
        "src/game/SpaceState.cpp",
        "source=accepted-program",
        "authoritativeAutomaticRoute",
        "automaticDockingRouteRevision",
        "route.advisoryOnly =",
    )
    if "runtime.controlBridge" in server or "PilotSkillExecutor" in server:
        raise AssertionError(
            "Automatic docking still routes Follower output through pilot-skill filtering"
        )

    require(
        "src/game/navigation/DockingAutomaticRecoveryPolicy.h",
        "linearFeedbackReserveMps2(",
        "holdCaptureDistanceMeters(",
        "holdCaptureSpeedMps(",
        "recoverableDynamicExcursion(",
        "0.20",
        "1.25 * velocityLimitMps",
        "2.0 * angularVelocityLimitRadPerSec",
    )
    require(
        "tests/navigation_runtime/DockingAdvisoryPlannerTests.cpp",
        "automatic docking recovery policy lost safe in-place correction semantics",
    )

    header = require(
        "src/game/server/GameServer.h",
        "struct DockingAutomaticRuntime",
        "enum class Stage",
        "ApproachHold",
        "FinalIngress",
        "std::vector<game::navigation::AcceptedManeuverProgram> programs",
        "currentSpatialSegment",
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
        "NavigationHitVolumeAdapter::buildObstacles",
        "debugHitVolumes",
        "ObstacleGeometryUnavailable",
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
        "completionTriggersReplan",
        "ReferenceMode referenceMode",
        "page.referenceMode = request.referenceMode",
        "page.tracking.spatialSlowdownStartFraction",
        "TranslationMode::AssistedVelocity",
        "newtonian-motion-envelope-infeasible",
    )

    require(
        "src/game/navigation/AcceptedManeuverProgram.h",
        "enum class ReferenceMode",
        "TimeScheduled",
        "SpatialCorridor",
        "ReferenceMode referenceMode = ReferenceMode::TimeScheduled",
        "spatialSlowdownStartFraction = 0.50",
    )
    if "spatialSlowdownStartFraction" in builder:
        raise AssertionError(
            "spatial corridor regained the retired cross-track speed governor"
        )
    if "spatialSlowdownStartFraction" in read(
        "src/game/navigation/AcceptedManeuverProgram.h"
    ):
        raise AssertionError(
            "accepted program still exposes the retired cross-track slowdown policy"
        )

    require(
        "src/game/navigation/ManeuverProgramSampler.h",
        "sampleSpatial(",
        "minimumSegmentIndex",
        "spatialDistanceMeters",
    )
    require(
        "src/game/navigation/ManeuverProgramTimeline.h",
        "selectSpatialPage(",
        "positionMapMeters",
        "Storage is not a maneuver phase",
    )
    require(
        "tests/navigation_runtime/ManeuverProgramSamplerTests.cpp",
        "testSpatialSamplerFollowsVehicleInsteadOfNominalClock",
        "testSpatialSamplerDoesNotJumpAcrossHairpin",
        "testSpatialPageSelectionUsesPhysicalProgressNotTime",
        "testSpatialProgressCannotAdvanceOutsideCorridor",
        "testSpatialPageCannotSkipMultiplePathChunksPerStep",
        "testSpatialSamplerNeverJumpsBehindMonotonicCursor",
    )
    require(
        "tests/navigation_runtime/ManeuverTrackingControllerTests.cpp",
        "testFollowerSpatialCorridorTracksPathInsteadOfClock",
        "testSpatialCorridorSteersBackWithoutReducingRouteSpeed",
        "testFollowerSpatialCorridorCanCompleteBeforeNominalTime",
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
        "control.navigationVelocityTargetValid",
        "applyNavigationAssistedFlightModel(",
    )
    dynamic = require(
        "src/game/navigation/DynamicMotionSystem.cpp",
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
        "terminalTurnSpeedMps",
        "lateralTerminalRadiusMeters",
        "angularTerminalRadiusMeters",
        "angularRampDistanceMeters",
        "terminalPrimitiveRadius",
        "terminalIngressSamples=36",
        "axisOffsetFactors",
        "candidateApproachLengthMeters",
        "terminalArcRotationDegrees",
        "terminalArcAcceptedCandidates",
        "no collision-free exact-radius terminal arc",
        "Subdivide EACH authored segment independently",
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
    print(" - Automatic never runs the client advisory planner as a preflight")
    print(" - Automatic HUD is published only from the server AcceptedManeuverProgram")
    print(" - Automatic transit stops at a hold point before a separate final-ingress stage")
    print(" - server owns Autopilot authority and stabilization")
    print(" - heavy Automatic planning runs outside the fixed-step thread")
    print(" - Automatic aligns the real hull to the planned route-entry attitude before execution")
    print(" - manual docking corridor is nose-first and cockpit HUD has a fixed hull boresight")
    print(" - active map-card mode is bright green and cockpit mode text is localized")
    print(" - Automatic HUD is sourced from the same AcceptedManeuverProgram executed by Follower")
    print(" - Automatic Follower commands bypass human/NPC PilotSkill filtering")
    print(" - Assisted automatic transit executes the same nose-coupled game flight law as manual control")
    print(" - Newtonian ordinary transit cannot spend precision RCS as fake lateral route thrust")
    print(" - trajectory is converted to AcceptedManeuverProgram before Follower")
    print(" - Follower has one executable input type")
    print(" - rotating target omega is part of terminal acceptance")
    print(" - current slice ends outside solid target geometry; latch remains separate")
except AssertionError as exc:
    print(f"[FAIL] {exc}", file=sys.stderr)
    raise SystemExit(1)
