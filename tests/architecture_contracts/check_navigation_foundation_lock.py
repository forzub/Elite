#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]


def source(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(path: str, *needles: str) -> None:
    text = source(path)
    for needle in needles:
        if needle not in text:
            raise AssertionError(f"{path}: missing foundation invariant {needle!r}")


def forbid(path: str, *needles: str) -> None:
    text = source(path)
    for needle in needles:
        if needle in text:
            raise AssertionError(f"{path}: forbidden foundation dependency {needle!r}")


def function_body(path: str, signature: str) -> str:
    text = source(path)
    start = text.find(signature)
    if start < 0:
        raise AssertionError(f"{path}: function {signature!r} not found")
    brace = text.find("{", start)
    if brace < 0:
        raise AssertionError(f"{path}: function body for {signature!r} not found")
    depth = 0
    for index in range(brace, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[brace:index + 1]
    raise AssertionError(f"{path}: unterminated function {signature!r}")


try:
    # Planning is seeded only from canonical replication, then every participant
    # is resolved to one epoch. Presentation time/poses must never leak back in.
    require(
        "src/game/client/ClientNavigationPlanningSnapshotFactory.cpp",
        "sampleHubMapRuntimeAtServerTime",
        "out.sourceEpoch",
        "out.epoch",
        "planningServerTimeSeconds",
        "NavigationWorldPredictor::predictHubFrameAt",
        "resolveHubAttachmentAt",
        "CoordinateRoundTripToleranceMeters",
    )
    forbid(
        "src/game/client/ClientNavigationPlanningSnapshotFactory.cpp",
        "renderTransform",
        "renderWorldPosition",
        "renderOrientation",
        "renderServerTimeSeconds()",
        "ProceduralCloud",
        "HubMapPlanetPass",
        "SystemMapRenderer",
    )

    # Prediction is a reusable read-only navigation calculation. It may model
    # world motion, but it must not own or rewrite production simulation.
    for path in (
        "src/game/navigation/NavigationWorldPredictor.cpp",
        "src/game/navigation/NavigationWorldPredictor.h",
    ):
        forbid(
            path,
            "GameSimulation",
            "GameServer",
            "GameClient",
            "SystemMapRenderer",
            "ProceduralCloud",
            "MapCelestialRenderResources",
        )

    # The dependency direction is world -> navigation snapshot/prediction. A
    # navigation refactor must never replace authoritative GameSimulation code.
    forbid(
        "src/game/simulation/GameSimulation.cpp",
        "NavigationWorldPredictor",
        "ClientNavigationPlanningSnapshotFactory",
        "DockingPathPlanner",
        "StrategicTrajectoryPlanner",
        "GeometricPathPlanner",
    )

    # Route calculation is allowed to mutate only navigation workspace/output.
    # It must not write simulation, cloud, map-resource or replicated transforms.
    guidance = function_body(
        "src/game/SpaceState.cpp",
        "void SpaceState::updateDockingGuidance(float dt)",
    )
    for forbidden in (
        "renderTransform",
        "renderWorldPosition",
        "celestialSnapshot()",
        "ProceduralCloud",
        "proceduralCloudLayer",
        "clearCache(",
        "rebuildHubNavigationFrames(",
        "computeOrbitPositionMeters(",
        "computeOrbitVelocityMetersPerSecond(",
        "hubVisualLocalToWorldPosition(",
        "hubAttachedVisualOrientation(",
    ):
        if forbidden in guidance:
            raise AssertionError(
                "SpaceState::updateDockingGuidance crossed frozen foundation boundary: "
                + repr(forbidden)
            )

    # Map interpolation remains generic. Hub attachment phase belongs at the
    # Hub bridge/presentation boundary rather than in the global interpolator.
    forbid(
        "src/game/system_map/AuthoritativeMapInterpolator.cpp",
        "HubFrameBasis",
        "hubAttachedVisualOrientation",
        "localAngularVelocityDegPerSecond",
        "NavigationWorldPredictor",
    )
    require(
        "src/game/client/ClientHubMapBridge.h",
        "currentLocalRotationDeg",
        "source.hubAttachment.localRotationDeg",
        "source.hubAttachment.localAngularVelocityDegPerSecond *",
        "universeTimeSeconds",
        "hubAttachedVisualOrientation",
    )

    # Large universe epochs must be reduced modulo the periodic angle before a
    # float conversion; otherwise slow attachments freeze and then snap.
    require(
        "src/game/navigation/HubFrameBasis.h",
        "wrapHubLocalDegrees",
        "std::remainder(degrees, 360.0)",
        "const glm::dvec3 wrapped",
        "static_cast<float>(wrapped.x)",
        "static_cast<float>(wrapped.y)",
        "static_cast<float>(wrapped.z)",
    )

    # Keep the diagnostic Hub lab deterministic: one slow rotating box, one
    # genuinely static cylinder. This is our visual + collision-frame fixture.
    setup = source("src/game/scene/GameSceneSetup.cpp")
    box_pattern = re.compile(
        r'ObjectType::GuidanceDockCube.*?glm::dvec3\(0\.0, 0\.0, 2\.0\)',
        re.S,
    )
    cylinder_pattern = re.compile(
        r'ObjectType::GuidanceDockCylinder.*?glm::dvec3\(0\.0, 0\.0, 0\.0\)',
        re.S,
    )
    if not box_pattern.search(setup):
        raise AssertionError("Hub guidance box is no longer the slow 2 deg/s rotation probe")
    if not cylinder_pattern.search(setup):
        raise AssertionError("Hub guidance cylinder is no longer static")

    # Cloud animation must remain presentation-owned and completely unaware of
    # route planning/prediction. Protect the regression that reappeared when
    # navigation refactoring touched production/presentation timing.
    for path in (
        "src/game/system_map/HubMapPlanetPass.cpp",
        "src/game/system_map/MapCelestialRenderResources.cpp",
    ):
        forbid(
            path,
            "DockingPathPlanner",
            "GeometricPathPlanner",
            "NavigationWorldPredictor",
            "ClientNavigationPlanningSnapshotFactory",
            "NavigationPlanningEpoch",
        )

    # Future client/server execution placement changes who performs the same
    # deterministic calculation, not the world-state authority or algorithm.
    require(
        "src/game/shared/SpatialComputationPlacement.h",
        "humanParticipantCount > 1",
        "SpatialComputationPlacement::ClientLocal",
        "SpatialComputationPlacement::ServerShared",
        "server-resolved consistency domain",
    )

    # Runtime regression coverage must remain present. These tests defend
    # complete frame round-trips, large-epoch attachment continuity and route
    # determinism/input purity rather than only grepping implementation text.
    require(
        "tests/navigation_guidance/NavigationGuidanceTests.cpp",
        "testPlanningFrameRoundTripsCompleteKinematicState",
        "testHubAttachmentPredictionIsContinuousAtLargeUniverseEpoch",
        "testGeometricPlannerIsDeterministicAndInputPure",
        "8.73398e8",
        "static Hub cylinder orientation drifted",
    )
    require(
        "tests/system_map/SystemMapBehaviorTests.cpp",
        "testHubMapOrbitKeepsCapturedPivotOnScreen",
        "testHubMapOrbitPivotPrefersDirectThenNearestObject",
    )

    print("[PASS] navigation time/frame foundation lock")
except (AssertionError, FileNotFoundError, ValueError) as exc:
    print(f"[FAIL] {exc}", file=sys.stderr)
    sys.exit(1)
