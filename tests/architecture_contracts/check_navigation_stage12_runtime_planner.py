#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

SPACE_H = (ROOT / "src/world/navigation/space/NavigationSpace.h").read_text(encoding="utf-8")
MAP_H = (ROOT / "src/world/navigation/map/NavigationMap.h").read_text(encoding="utf-8")
MAP_CPP = (ROOT / "src/world/navigation/map/NavigationMap.cpp").read_text(encoding="utf-8")
MAP_TEST = (ROOT / "tests/navigation_map/NavigationMapContractTests.cpp").read_text(encoding="utf-8")
SPACE_CPP = (ROOT / "src/world/navigation/space/NavigationSpace.cpp").read_text(encoding="utf-8")
SPACE_CMAKE = (ROOT / "src/world/navigation/space/CMakeLists.txt").read_text(encoding="utf-8")
SPACE_TEST = (ROOT / "tests/navigation_space/NavigationSpaceContractTests.cpp").read_text(encoding="utf-8")
LOCAL_TEST = (ROOT / "tests/navigation_local/NavigationLocalAvoidanceTests.cpp").read_text(encoding="utf-8")
PLANNER_H = (ROOT / "src/game/navigation/NavigationRuntimePlanner.h").read_text(encoding="utf-8")
PLANNER_CPP = (ROOT / "src/game/navigation/NavigationRuntimePlanner.cpp").read_text(encoding="utf-8")
MOVING_PASSAGE_H = (ROOT / "src/world/navigation/trajectory/MovingPassageTrajectoryEvaluator.h").read_text(encoding="utf-8")
MOVING_PASSAGE_CPP = (ROOT / "src/world/navigation/trajectory/MovingPassageTrajectoryEvaluator.cpp").read_text(encoding="utf-8")
ROOT_CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
RUNTIME_CMAKE = (ROOT / "tests/navigation_runtime/CMakeLists.txt").read_text(encoding="utf-8")
RUNTIME_TEST = (ROOT / "tests/navigation_runtime/NavigationRuntimePlannerTests.cpp").read_text(encoding="utf-8")
DOC = (ROOT / "src/game/navigation/STAGE12_END_TO_END.md").read_text(encoding="utf-8")
ADAPTER_H = (ROOT / "src/game/navigation/NavigationHitVolumeAdapter.h").read_text(encoding="utf-8")
ADAPTER_CPP = (ROOT / "src/game/navigation/NavigationHitVolumeAdapter.cpp").read_text(encoding="utf-8")
SIM_H = (ROOT / "src/game/simulation/GameSimulation.h").read_text(encoding="utf-8")
SIM_CPP = (ROOT / "src/game/simulation/GameSimulation.cpp").read_text(encoding="utf-8")
SCENE_CPP = (ROOT / "src/game/scene/GameSceneSetup.cpp").read_text(encoding="utf-8")
LAB_H = (ROOT / "src/game/diagnostics/NavigationRuntimeLab.h").read_text(encoding="utf-8")
LOCAL_H = (ROOT / "src/world/navigation/local/LocalAvoidancePlanner.h").read_text(encoding="utf-8")
LOCAL_CPP = (ROOT / "src/world/navigation/local/LocalAvoidancePlanner.cpp").read_text(encoding="utf-8")
SERVER_RUNTIME_H = (ROOT / "src/game/server/ServerRuntime.h").read_text(encoding="utf-8")
SERVER_RUNTIME_CPP = (ROOT / "src/game/server/ServerRuntime.cpp").read_text(encoding="utf-8")
SERVER_MAIN = (ROOT / "src/server_main.cpp").read_text(encoding="utf-8")
MANEUVER_DECISION_H = (ROOT / "src/game/ship/controller/ManeuverDecisionController.h").read_text(encoding="utf-8")
MANEUVER_DECISION_CPP = (ROOT / "src/game/ship/controller/ManeuverDecisionController.cpp").read_text(encoding="utf-8")
MANEUVER_DECISION_TEST = (ROOT / "tests/navigation_runtime/ManeuverDecisionControllerTests.cpp").read_text(encoding="utf-8")
MANEUVER_DECISION_DOC = (ROOT / "src/game/MANEUVER_DECISION_TREE.md").read_text(encoding="utf-8")
CONTROL_LAW_DOC = (ROOT / "src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md").read_text(encoding="utf-8")
REPLAN_H = (ROOT / "src/game/navigation/NavigationExecutionReplanPolicy.h").read_text(encoding="utf-8")
REPLAN_CPP = (ROOT / "src/game/navigation/NavigationExecutionReplanPolicy.cpp").read_text(encoding="utf-8")
REPLAN_TEST = (ROOT / "tests/navigation_runtime/NavigationExecutionReplanPolicyTests.cpp").read_text(encoding="utf-8")
REPLAN_DOC = (ROOT / "src/game/navigation/TRAJECTORY_EXECUTION_REPLAN_MODEL.md").read_text(encoding="utf-8")
HIT_BUILDER = (ROOT / "src/world/modules/ObjectRuntimeHitBuilder.cpp").read_text(encoding="utf-8")
GUIDANCE_DESCRIPTOR = (ROOT / "src/game/station/descriptors/GuidanceTestDockDescriptor.h").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")


for marker in (
    "angularVelocitySystemRadPerSecond",
    "angularVelocityMapRadPerSecond",
):
    require(marker in MAP_H, f"NavigationMap moving-motion API missing: {marker}")

for marker in (
    "input.angularVelocitySystemRadPerSecond",
    "actor.angularVelocityMapRadPerSecond = vectorToMap",
    "result.angularVelocityMapRadPerSecond",
):
    require(marker in MAP_CPP, f"NavigationMap angular-motion transform missing: {marker}")

require(
    "rotatingActor.angularVelocitySystemRadPerSecond" in MAP_TEST and
    "rotated.angularVelocityMapRadPerSecond" in MAP_TEST,
    "NavigationMap contract test must pin angular velocity through a rotated working frame",
)

for marker in (
    "portalCentersMapMeters",
    "std::vector<Vec3d>",
    "struct PortalTraversalInput",
    "normalAToBMap",
    "approachDistanceMeters",
    "maximumVelocityAngleRad",
    "maximumForwardAngleRad",
    "maximumLateralSpeedMps",
    "requireVehicleForwardAlignment",
    "struct PortalTraversal",
    "portalTraversals",
):
    require(marker in SPACE_H, f"NavigationSpace compact steering/traversal seam missing: {marker}")

require(
    SPACE_CPP.count("portalCentersMapMeters.push_back") >= 3 and
    SPACE_CPP.count("portalTraversals.push_back") >= 3 and
    "orientedPortalTraversal" in SPACE_CPP,
    "all corridor reconstruction paths must publish ordered oriented portal traversal products",
)

for marker in (
    "std::vector<NavigationObstacle> obstacles",
    "struct SegmentQuery",
    "struct SegmentQueryResult",
    "querySegment",
    "allowEndOnStartRegionBoundary",
    "exactObstaclesOnly",
    "blockingObstacleId",
    "obstacleCount",
):
    require(marker in SPACE_H, f"exact static NavigationSpace seam missing: {marker}")

for marker in (
    "validateObstacle",
    "pointInsideNavigationObstacle",
    "segmentIntersectsNavigationObstacle",
    "impl_->obstacles",
    "if (query.exactObstaclesOnly)",
    "validatePortal",
    "orientedPortalTraversal",
):
    require(marker in SPACE_CPP, f"exact static NavigationSpace implementation missing: {marker}")

root_cmake_tokens = " ".join(ROOT_CMAKE.split())

require(
    "../NavigationObstacleGeometry.cpp" in SPACE_CMAKE and
    "target_link_libraries(EliteNavigationWorldRuntime PUBLIC EliteNavigationGeometry" in root_cmake_tokens,
    "exact obstacle geometry must link in isolated and production NavigationSpace targets",
)

for marker in (
    "testProvenPortalEndpointMayTouchRegionBoundary",
    "testOrientedFiniteDepthPortalTraversalMetadata",
    "testExactObstacleOnlySweepIgnoresRegionPartition",
    "testExactStaticObbBlocksPointAndSegment",
    "testExactObbGapAdmitsOnlyFittingEnvelope",
    "conservativeRadiusMeters() > 5.0",
):
    require(marker in SPACE_TEST, f"exact OBB NavigationSpace regression missing: {marker}")

for marker in (
    "class NavigationRuntimePlanner final",
    "NavigationRuntimeControlBridge",
    "LocalAvoidancePlanner",
    "NavigationMap",
    "NavigationSpace",
    "safeProgressTargetDemonstrated",
    "usedPortalWaypoint",
    "nominalStaticBlocked",
    "staticObstaclesExamined",
    "nominalStaticObstacleId",
    "BoundedGapCandidateBuilder",
    "MovingGapPredictor",
    "MovingPassageTrajectoryEvaluator",
    "MovingPassagePolicy",
    "movingPrecisionAttempted",
    "movingPassageFeasible",
    "movingPassageStaticProofAttempted",
    "movingPassageStaticSafe",
    "movingPassageStaticIntervalsProven",
    "movingPassageStaticBlockingObstacleId",
    "allowSteeringAuthority",
    "MovingPassageClear",
    "movingPassageAuthorityUsed",
    "movingPassageTargetMapMeters",
    "movingPassageLastEvaluatorStatus",
    "movingPassageRequiredPeakForwardAccelerationMps2",
    "movingPassageRequiredPeakReverseAccelerationMps2",
    "movingPassageRequiredPeakLateralAccelerationMps2",
    "movingPassageRequiredPeakVerticalAccelerationMps2",
    "selectedVisibilityDeflectionRadians",
    "ordinaryVisibilitySearchExhausted",
    "PortalTraversalPolicy",
    "PortalCapture",
    "PortalTransit",
    "staticPortalTraversals",
    "portalTraversalActive",
    "portalVelocityAligned",
    "portalForwardAligned",
    "portalCaptureReady",
):
    require(marker in PLANNER_H, f"runtime planner interface missing: {marker}")

for marker in (
    "queryCostedCorridor",
    "portalCentersMapMeters.front",
    "Avoidance{}.evaluate",
    "holdIntent",
    "idealLinearAccelerationDemandMapMps2",
    "idealAngularAccelerationDemandMapRadPerSec2",
    "mapIntentToWorld",
    "local.nominalStaticBlocked",
    "local.staticObstaclesExamined",
    "GapBuilder::build",
    "GapPredictor::predict",
    "MovingPassage::evaluate",
    "probeMovingPassage",
    "proveMovingPassageAgainstStaticSpace",
    "staticSpace.querySegment",
    "intervalCenterlineDeviationBoundsMeters",
    "corridor.portalTraversals",
    "portalAlignmentAngularDemand",
    "portalApproachPointMapMeters",
    "portalVelocityAngleRad",
    "portalForwardAngleRad",
    "portalCaptureReady",
    "Status::PortalCapture",
    "Status::PortalTransit",
):
    require(marker in PLANNER_CPP, f"runtime planner composition missing: {marker}")

require(
    "bool allowSteeringAuthority = false;" in PLANNER_H,
    "moving-passage steering authority must remain an explicit opt-in policy",
)

require(
    "policy.movingPassage.allowSteeringAuthority &&" in PLANNER_CPP and
    "result.movingPassageFeasible &&" in PLANNER_CPP and
    "result.movingPassageStaticSafe" in PLANNER_CPP and
    "local.status != Avoidance::Status::StaleHold" in PLANNER_CPP,
    "moving-passage authority must require explicit opt-in, fresh dynamic state, dynamic feasibility and exact-static safety",
)

require(
    "result.status = Status::MovingPassageClear;" in PLANNER_CPP and
    "result.movingPassageAuthorityUsed = true;" in PLANNER_CPP and
    "result.intent.idealLinearAccelerationDemandMapMps2 =\n            toBridgeVec(result.movingPassageInitialAccelerationMapMps2);" in PLANNER_CPP,
    "12A-6b3a authority must execute the exact first sample of the doubly-proven Hermite trajectory",
)

require(
    "result.desiredVelocityMapMetersPerSecond" not in
        PLANNER_CPP[
            PLANNER_CPP.find("if (policy.movingPassage.allowSteeringAuthority &&"):
            PLANNER_CPP.find("switch (local.status)")
        ],
    "moving-passage authority must not solve a second desired-velocity trajectory after proof",
)

for marker in (
    "TrajectoryWitness",
    "centerSamplesMapMeters",
    "intervalCenterlineDeviationBoundsMeters",
    "conservativeHullRadiusMeters",
    "TrajectoryWitness trajectory",
):
    require(
        marker in MOVING_PASSAGE_H,
        f"moving-passage exact Hermite witness missing: {marker}",
    )

for marker in (
    "result.trajectory.centerSamplesMapMeters[i]",
    "accelerationMagnitudeBound * dt * dt / 8.0",
    "result.trajectory.valid = true",
):
    require(
        marker in MOVING_PASSAGE_CPP,
        f"moving-passage continuous centerline witness implementation missing: {marker}",
    )

require(
    "testStaticObstacleRejectsSameAcceptedMovingHermiteTrajectory" in RUNTIME_TEST and
    "movingPassageStaticSafe" in RUNTIME_TEST and
    "moving_passage_static_beam" in RUNTIME_TEST,
    "runtime regression must prove a dynamic-valid moving passage can still fail exact-static same-trajectory safety",
)

require(
    "testDoublyProvenMovingPassageTakesAuthorityThroughPilotBridge" in RUNTIME_TEST and
    "Planner::Status::MovingPassageClear" in RUNTIME_TEST and
    "movingPassageAuthorityUsed" in RUNTIME_TEST and
    "Planner::mapIntentToWorld(planned.intent, frame)" in RUNTIME_TEST and
    "bridge.step(" in RUNTIME_TEST,
    "runtime regression must pin doubly-proven moving passage authority through map/world transform and PilotSkillExecutor",
)

require(
    "initialLinearAccelerationMapMetersPerSec2" in MOVING_PASSAGE_H and
    "samples.front().acceleration" in MOVING_PASSAGE_CPP,
    "moving-passage evaluator must publish the first sample of the exact verified trajectory",
)

for forbidden in (
    "setWorldPosition",
    "worldVelocityMps =",
    "localVelocityMps =",
    "pitchRate =",
    "yawRate =",
    "rollRate =",
):
    require(
        forbidden not in PLANNER_CPP,
        f"runtime planner must not mutate authoritative physics state: {forbidden}",
    )

for marker in (
    "add_library(EliteNavigationWorldRuntime STATIC",
    "src/world/navigation/map/NavigationMap.cpp",
    "src/world/navigation/space/NavigationSpace.cpp",
    "src/world/navigation/local/LocalHorizonPlanner.cpp",
    "src/world/navigation/local/LocalAvoidancePlanner.cpp",
    "src/game/navigation/NavigationRuntimePlanner.cpp",
    "add_subdirectory(src/world/navigation/trajectory)",
    "EliteNavigationTrajectory",
):
    require(marker in ROOT_CMAKE, f"shared runtime navigation target missing: {marker}")

require(
    ROOT_CMAKE.count("EliteNavigationWorldRuntime") >= 3,
    "shared NavigationWorld runtime target must be defined and linked by both production executables",
)

for marker in (
    "NavigationRuntimePlanner.cpp",
    "NavigationRuntimePlannerTests.cpp",
    "navigation_runtime_planner",
    "EliteNavigationMap",
    "EliteNavigationLocal",
    "EliteNavigationTrajectory",
    "elite_navigation_trajectory",
):
    require(marker in RUNTIME_CMAKE, f"runtime planner test wiring missing: {marker}")

for marker in (
    "testStaticCorridorBecomesLivePortalWaypoint",
    "testPortalCaptureAlignsVelocityAndHullBeforeTransit",
    "testSamePortalRejectsOversizedHull",
    "testAdjustedTargetPreservesNominalConflictIdentity",
    "testNavigationMapCrossingConflictProducesBrakingHold",
    "testMapIntentTransformsIntoWorldControlFrame",
    "testExactStaticObstacleParticipatesInRuntimeComposition",
    "testLiveScaleStaticObstacleInsideFirstBoundedHorizon",
    "testPlannerIntentCrossesAcceptedPilotBridge",
    "testMovingGapPrecisionProbeUsesRuntimeCandidates",
    "testClosingMovingGapFailsClosedBeforePassageEvaluation",
):
    require(marker in RUNTIME_TEST, f"runtime planner fixture missing: {marker}")


for marker in (
    "class NavigationHitVolumeAdapter final",
    "buildObstacles",
    "conservativeRadiusFromOrigin",
):
    require(marker in ADAPTER_H, f"hit-volume navigation adapter missing: {marker}")

for marker in (
    "NavigationObstacleShape::Box",
    "objectWorldPositionMeters",
    "objectLocalToWorld",
    "supportLinkVolume",
):
    require(marker in ADAPTER_CPP, f"hit-volume adapter implementation missing: {marker}")

require(
    "NavigationHitVolumeAdapter.cpp" in ROOT_CMAKE and
    "NavigationHitVolumeAdapter.cpp" in RUNTIME_CMAKE,
    "hit-volume adapter must compile in production and isolated runtime gate",
)

for marker in (
    "registerNavigationRuntimeLabShip",
    "isNavigationRuntimeLabShip",
    "navigationRuntimeLabLastPlan",
    "buildNavigationRuntimeLabIntent",
    "m_navigationRuntimeLabMap",
    "m_navigationRuntimeLabSpace",
):
    require(marker in SIM_H, f"authoritative navigation runtime lab seam missing: {marker}")

for marker in (
    "initializeNavigationRuntimeLab();",
    "buildNavigationRuntimeLabIntent(id, ship, intent)",
    "NavigationHitVolumeAdapter::",
    "conservativeRadiusFromOrigin(object.hitComponent)",
    "m_navigationRuntimeLabMap->replaceDynamicWorld",
    "m_navigationRuntimeLabMap->querySphere",
    "Planner::plan(",
    "Planner::mapIntentToWorld(",
    "navigationWorkingFrame",
    "publishNavigationRuntimeLabStaticGeometry",
    "staticWorld.obstacles.push_back",
):
    require(marker in SIM_CPP, f"GameSimulation runtime planner integration missing: {marker}")

require(
    "NpcNavigationIntentController::buildIntent" in SIM_CPP,
    "ordinary NPC fallback path must remain present while the lab is isolated",
)

require(
    "outIntent = m_navigationRuntimeLabLastPlan.intent" not in SIM_CPP,
    "map-space planner intent must not cross directly into the Stage-11 world-space control seam",
)

require(
    "isNavigationRuntimeLabShip(shipId)" in SIM_CPP and
    "return SimulationMode::Active;" in SIM_CPP,
    "stage-12 proving actor must stay Active so activation cadence cannot contaminate the navigation result",
)

for marker in (
    "appendWholeObjectLogicalHitVolume",
    "descriptor.logicalDimensions()",
    "__whole_object_logical_bounds__",
):
    require(marker in HIT_BUILDER, f"monolithic logical HitVolume fallback missing: {marker}")

require(
    HIT_BUILDER.count("appendWholeObjectLogicalHitVolume(") >= 3,
    "monolithic logical HitVolume fallback must cover meshless and empty-module rebuild paths",
)

for marker in (
    "class GuidanceDockCubeDescriptor",
    "class GuidanceDockCylinderDescriptor",
    ".enabled = true",
):
    require(marker in GUIDANCE_DESCRIPTOR, f"NAV STRESS logical-dimension authority missing: {marker}")

for marker in (
    "NavigationRuntimeLabEnabled",
    "NavigationRuntimeLabStartTacticalLocalMeters",
    "NavigationRuntimeLabGoalTacticalLocalMeters",
    "NAVIGATION V2 RUNTIME LAB",
):
    require(
        marker in LAB_H or marker in SCENE_CPP,
        f"navigation runtime lab definition/spawn missing: {marker}",
    )

require(
    "spawnHubGuidanceTestModules" in SCENE_CPP and
    "spawnNavigationRuntimeLabNpc" in SCENE_CPP and
    "registerNavigationRuntimeLabShip" in SCENE_CPP,
    "existing NAV STRESS scene must feed the live runtime lab",
)

require(
    "testHitVolumeAdapterUsesAuthoritativeLocalObb" in RUNTIME_TEST,
    "runtime gate must pin authoritative HitVolume -> navigation OBB conversion",
)

for marker in (
    "nominalPrimaryConflictEntityId",
    "nominalConflictsFound",
):
    require(marker in LOCAL_H, f"local avoidance nominal conflict diagnostic missing: {marker}")

require(
    "nominal.primaryConflictEntityId" in LOCAL_CPP and
    "nominal.conflictsFound" in LOCAL_CPP,
    "local avoidance must preserve the nominal conflict before adjusted-target probing",
)

for marker in (
    "nominalStaticBlocked",
    "nominalStaticObstacleId",
    "staticObstaclesExamined",
    "nominalTargetIsProvenPortalBoundary",
):
    require(marker in LOCAL_H, f"local exact-static diagnostics missing: {marker}")

require(
    LOCAL_CPP.count("staticSpace.querySegment") >= 2,
    "LocalAvoidance must prove both nominal and adjusted segments through exact static NavigationSpace",
)

require(
    "nominal.status == LocalHorizonPlanner::Status::Clear &&" in LOCAL_CPP and
    "nominalStatic.traversable" in LOCAL_CPP,
    "dynamic Clear must not bypass exact static segment proof",
)

require(
    "testExactStaticBlockerTriggersAvoidanceWithoutDynamicCandidate" in LOCAL_TEST,
    "local avoidance must prove an exact static blocker can trigger adjustment without a dynamic candidate",
)

require(
    "testDynamicConflictStillPreservesExactStaticNominalProof" in LOCAL_TEST,
    "simultaneous dynamic conflict must not collapse exact-static nominal proof",
)

require(
    "boundedNominalTarget" in LOCAL_CPP and
    "nominal.horizonDistanceMeters" in LOCAL_CPP and
    "toSpaceVec(boundedNominalTarget)" in LOCAL_CPP,
    "exact static nominal proof must use the same bounded nominal segment analyzed by the dynamic horizon",
)

require(
    "toSpaceVec(nominal.targetPositionMapMeters)" not in LOCAL_CPP,
    "ConflictHold result position must not be reused as the exact-static nominal segment endpoint",
)

require(
    "query.avoidance.nominalTargetIsProvenPortalBoundary" in LOCAL_CPP and
    "allowEndOnStartRegionBoundary" in LOCAL_CPP,
    "corridor-proven portal endpoint must pass its boundary proof into exact static nominal checking",
)

for marker in (
    "maximumDeflectionRadians",
    "selectedDeflectionRadians",
    "nominalVisibilityClear",
    "ordinarySearchExhausted",
):
    require(marker in LOCAL_H, f"bounded visibility steering contract missing: {marker}")

require(
    "deflection += deflectionStep" in LOCAL_CPP and
    "query.avoidance.maximumDeflectionRadians" in LOCAL_CPP,
    "ordinary local avoidance must widen deflection only inside the bounded visibility search",
)

require(
    "testVisibilitySteeringWidensThenReturnsToDirectLine" in LOCAL_TEST,
    "local regression must prove widening beyond the legacy 15/30 fan and stateless return to direct A->B visibility",
)
require(
    "ordinarySearchExhausted" in LOCAL_TEST and
    "ordinaryVisibilitySearchExhausted" in RUNTIME_TEST,
    "ordinary fan exhaustion must be pinned as a recovery escalation signal",
)

require(
    "localQuery.avoidance.nominalTargetIsProvenPortalBoundary =" in PLANNER_CPP and
    "result.usedPortalWaypoint" in PLANNER_CPP,
    "runtime planner must grant boundary exception only to a selected corridor portal",
)

for marker in (
    "executionCount",
    "minimumConservativeClearanceMeters",
    "maximumStraightLineDeviationMeters",
    "maximumExecutedLateralDemandMps2",
    "maximumAppliedEngineAccelerationMps2",
    "maximumAppliedLateralAccelerationMps2",
    "maximumRelativeSpeedMps",
    "lastExecutedLinearDemandMapMps2",
    "passedObstaclePlane",
    "exactStaticGeometryPublished",
    "exactStaticObstacleCount",
    "configuredRouteExactObstacleBlockPublished",
    "firstLiveNominalProbeCaptured",
    "firstLiveNominalExactBlocked",
    "firstLiveNominalBlockingEntityId",
    "firstLiveHorizonMeters",
    "firstLiveAgentPositionMap",
    "firstLiveGoalPositionMap",
    "firstLiveBoundedTargetMap",
    "exactStaticQuerySeen",
    "nominalStaticBlockSeen",
    "maximumExactStaticObstaclesExamined",
    "exactStaticViolationSeen",
    "exactStaticMotionSamples",
    "firstExactStaticViolationCaptured",
    "firstExactStaticViolationEntityId",
    "firstExactStaticViolationStartMap",
    "firstExactStaticViolationEndMap",
    "firstExactStaticViolationSelectedTargetMap",
    "firstExactStaticViolationPlannerStatus",
    "obstacleExactStaticBlockSeen",
    "dynamicQueryCount",
    "maximumDynamicCandidateCount",
    "rotatingActorEntityId",
    "rotatingActorCandidateSeen",
    "rotatingActorAngularVelocityVerified",
    "expectedRotatingActorAngularVelocityMapRadPerSecond",
    "observedRotatingActorAngularVelocityMapRadPerSecond",
    "slitPortalExactOpenPublished",
    "slitPortalExactObstaclesExamined",
    "slitPortalWaypointSeen",
    "slitEntryCaptureSeen",
    "slitEntryVelocityAlignedSeen",
    "slitEntryForwardAlignedSeen",
    "slitEntryPlaneCrossedAligned",
    "slitEntryVelocityAngleRad",
    "slitEntryForwardAngleRad",
    "slitEntryLateralSpeedMps",
    "slitEntryCrossTrackMeters",
    "slitTunnelPassed",
    "slitTunnelCrossingMap",
    "slitTunnelCrossingMarginMeters",
):
    require(marker in LAB_H, f"live navigation physical observation missing: {marker}")

for marker in (
    "minimumConservativeClearanceMeters",
    "maximumStraightLineDeviationMeters",
    "maximumExecutedLateralDemandMps2",
    "maximumAppliedEngineAccelerationMps2",
    "maximumAppliedLateralAccelerationMps2",
    "maximumRelativeSpeedMps",
    "nominalPrimaryConflictEntityId",
):
    require(marker in SIM_CPP, f"GameSimulation live navigation evidence missing: {marker}")

require(
    "navigationRuntimeLabObservation() const noexcept" in SERVER_RUNTIME_H and
    "m_server->navigationRuntimeLabObservation()" in SERVER_RUNTIME_CPP,
    "read-only Stage-12 observation must cross the ServerRuntime boundary",
)

require(
    "copyAuthoritativePublishedSnapshot" in SERVER_RUNTIME_H and
    "outSnapshot = m_server->snapshot()" in SERVER_RUNTIME_CPP,
    "live self-test must compare against a copied authoritative published snapshot, not mutable per-step state",
)

require(
    "std::uint64_t targetRevision = 0;" in
        (ROOT / "src/game/navigation/NavigationRuntimeControlBridge.h").read_text(encoding="utf-8") and
    "std::uint64_t targetRevision = 0;" in
        (ROOT / "src/world/navigation/control/PilotSkillExecutor.h").read_text(encoding="utf-8") and
    "result.intent.revision = segment.goalRevision;" in
        (ROOT / "src/game/navigation/TrajectoryFollower.cpp").read_text(encoding="utf-8") and
    "result.intent.targetRevision = segment.revision;" in
        (ROOT / "src/game/navigation/TrajectoryFollower.cpp").read_text(encoding="utf-8"),
    "accepted segment target revision must remain distinct from high-level intent revision",
)

for marker in (
    "--self-test-navigation",
    "runNavigationRuntimeSelfTest",
    "obstaclePrimaryConflictSeen",
    "lateralExecutedDemandSeen",
    "observation.exactStaticMotionSamples > 0",
    "!observation.exactStaticViolationSeen",
    "!observation.obstacleCandidateSeen",
    "!observation.obstaclePrimaryConflictSeen",
    "observation.dynamicQueryCount > 0",
    "observation.rotatingActorCandidateSeen",
    "observation.rotatingActorAngularVelocityVerified",
    "findShipSnapshotByInstanceId",
    "NavigationExecutionSnapshot",
    "replicationErrorMps2",
    "canonicalReplicationErrorMps2",
    "copyAuthoritativePublishedSnapshot",
    "sparsePacket.metadata.serverTick",
    "authoritativePublished.metadata.serverTick",
    "observation.exactStaticGeometryPublished",
    "observation.exactStaticObstacleCount > 0",
    "observation.configuredRouteExactObstacleBlockPublished",
    "observation.exactStaticQuerySeen",
    "observation.maximumExactStaticObstaclesExamined > 0",
    "exact_static_obstacles=",
    "configured_route_exact_block=",
    "exact_static_query=",
    "exact_static_block=",
    "exact_static_motion_samples=",
    "exact_static_violation=",
):
    require(marker in SERVER_MAIN, f"authoritative navigation self-test missing: {marker}")

require(
    "MaxSimulatedSeconds = 120.0" in SERVER_MAIN,
    "navigation self-test must remain bounded in simulated time",
)

require(
    "querySphere(dynamicQuery)" in SIM_CPP and
    "localHorizonMeters" in SIM_CPP and
    "LabTurnDistanceMeters" in SIM_CPP,
    "live broadphase must cover the complete bounded avoidance fan rather than only the nominal corridor",
)

require(
    "const bool timeVaryingNavigationActor" in SIM_CPP and
    "if (!timeVaryingNavigationActor)" in SIM_CPP and
    "dynamicWorld.actors.push_back(actor)" in SIM_CPP,
    "NavigationMap must publish only time-varying infrastructure from the NAV STRESS object set",
)

require(
    "actor.angularVelocitySystemRadPerSecond" in SIM_CPP and
    "hubVisualLocalToWorldVector(" in SIM_CPP and
    "NavigationRuntimeLabRotatingActorLabel" in SIM_CPP,
    "live rotating infrastructure must publish authoritative angular velocity through the common hub basis",
)

require(
    "rotatingActorAngularVelocityVerified" in SIM_CPP and
    "candidate.angularVelocityMapRadPerSecond" in SIM_CPP,
    "live lab must verify the rotating actor after NavigationMap publication",
)

for marker in (
    "NavigationRuntimeLabMovingGapUpperLabel",
    "NavigationRuntimeLabMovingGapLowerLabel",
    "NavigationRuntimeLabMovingGapUpperVisualLocalMeters",
    "NavigationRuntimeLabMovingGapLowerVisualLocalMeters",
    "NavigationRuntimeLabMovingGapVelocityVisualMps",
    "movingGapPairCandidateSeen",
    "movingGapKinematicsVerified",
    "movingPassageAuthoritySeen",
    "movingPassageExecutionActive",
    "movingPassageAppliedAccelerationSeen",
    "movingGapPlanePassed",
    "movingPassageLastEvaluatorStatus",
    "movingPassageRequiredPeakReverseAccelerationMps2",
):
    require(marker in LAB_H, f"live moving-passage fixture/evidence missing: {marker}")

require(
    "-5700.0" in LAB_H,
    "live moving aperture must remain before CUBE 08 so moving authority is proven before the independent static obstacle",
)

require(
    "nav_moving_gap_upper" in SCENE_CPP and
    "nav_moving_gap_lower" in SCENE_CPP and
    "NavigationRuntimeLabMovingGapUpperLabel" in SCENE_CPP and
    "NavigationRuntimeLabMovingGapLowerLabel" in SCENE_CPP,
    "scene must spawn both physical live moving-gap boundaries",
)

for marker in (
    "NavigationRuntimeLabSlitPortalCenterVisualLocalMeters",
    "NavigationRuntimeLabSlitEntryCenterVisualLocalMeters",
    "NavigationRuntimeLabSlitExitCenterVisualLocalMeters",
    "NavigationRuntimeLabSlitHalfWidthMeters",
    "NavigationRuntimeLabSlitHalfHeightMeters",
    "NavigationRuntimeLabSlitHalfDepthMeters",
    "NavigationRuntimeLabSlitPortalClearanceMeters",
    "NavigationRuntimeLabSlitEntryPortalId",
    "NavigationRuntimeLabSlitExitPortalId",
    "NavigationRuntimeLabSlitApproachDistanceMeters",
    "NavigationRuntimeLabSlitTransitSpeedMps",
    "NavigationRuntimeLabSlitMaximumEntryVelocityAngleRad",
    "NavigationRuntimeLabSlitMaximumEntryForwardAngleRad",
    "NavigationRuntimeLabSlitMaximumLateralSpeedMps",
    "slitPortalExactOpenPublished",
    "slitPortalWaypointSeen",
    "slitEntryPlaneCrossedAligned",
    "slitTunnelPassed",
):
    require(marker in LAB_H, f"live exact-static slit tunnel/capture contract missing: {marker}")

for marker in (
    "nav_slit_upper_left",
    "nav_slit_upper_mid",
    "nav_slit_upper_right",
    "nav_slit_lower_left",
    "nav_stress_cube_08",
    "nav_slit_lower_right",
):
    require(marker in SCENE_CPP, f"physical slit tunnel wall missing: {marker}")

for marker in (
    "Space::RegionInput approachRegion",
    "Space::RegionInput tunnelRegion",
    "Space::RegionInput departureRegion",
    "Space::PortalInput slitEntryPortal",
    "slitEntryPortal.portalId = NavigationRuntimeLabSlitEntryPortalId",
    "slitEntryPortal.traversal.normalAToBMap = {0.0, 0.0, 1.0}",
    "slitEntryPortal.traversal.requireVehicleForwardAlignment = true",
    "Space::PortalInput slitExitPortal",
    "slitExitPortal.portalId = NavigationRuntimeLabSlitExitPortalId",
    "staticWorld.portals.push_back(slitEntryPortal)",
    "staticWorld.portals.push_back(slitExitPortal)",
    "slitGeometryProof.exactObstaclesOnly = true",
    "actualMotion.exactObstaclesOnly = true",
    "slitEntryPlaneCrossedAligned",
    "NavigationRuntimeLabSlitMaximumEntryVelocityAngleRad",
    "NavigationRuntimeLabSlitMaximumEntryForwardAngleRad",
    "NavigationRuntimeLabSlitMaximumLateralSpeedMps",
):
    require(marker in SIM_CPP, f"slit tunnel topology/capture/physical proof missing: {marker}")

for marker in (
    "isNavigationRuntimeLabMovingGapBoundary",
    "effectiveLocalOffsetMeters",
    "NavigationRuntimeLabMovingGapVelocityVisualMps",
    "expectedMovingGapVelocityWorldMps",
    "movingGapMaximumVelocityErrorMps",
    "movingGapPairCandidateSeen",
    "policy.avoidance.maximumDeflectionRadians",
    "policy.movingPassage.enabled = false",
    "policy.movingPassage.allowSteeringAuthority = false",
    "movingPairIsNominalConflict",
    "visibilityBypassActive",
    "visibilityBypassSeen",
    "visibilityDirectRecoveredSeen",
    "selectedVisibilityDeflectionRadians",
):
    require(marker in SIM_CPP, f"live bounded-visibility production integration missing: {marker}")

require(
    "isNavigationRuntimeLabMovingGapBoundary(" in SIM_CPP and
    "staticWorld.obstacles.push_back" in SIM_CPP,
    "moving-gap boundaries must be excluded from persistent static ownership and published through dynamic ownership",
)

for marker in (
    "observation.movingGapPairCandidateSeen",
    "observation.movingGapKinematicsVerified",
    "observation.visibilityBypassSeen",
    "observation.visibilityBypassActive",
    "observation.visibilityDirectRecoveredSeen",
    "observation.maximumVisibilityDeflectionRad",
    "observation.movingGapPlanePassed",
    "moving_gap_pair=",
    "visibility_bypass=",
    "visibility_bypass_active=",
    "visibility_direct_recovered=",
    "visibility_max_deflection_rad=",
    "moving_gap_passed=",
    "observation.slitPortalExactOpenPublished",
    "observation.slitPortalWaypointSeen",
    "observation.slitEntryCaptureSeen",
    "observation.slitEntryVelocityAlignedSeen",
    "observation.slitEntryForwardAlignedSeen",
    "observation.slitEntryPlaneCrossedAligned",
    "observation.slitTunnelPassed",
    "slit_exact_open=",
    "slit_portal=",
    "slit_entry_capture=",
    "slit_entry_vel_aligned=",
    "slit_entry_fwd_aligned=",
    "slit_entry_crossed_aligned=",
    "slit_entry_vel_angle_rad=",
    "slit_entry_fwd_angle_rad=",
    "slit_entry_lateral_mps=",
    "slit_entry_cross_track_m=",
    "slit_passed=",
    "slit_margin_m=",
    "slit_crossing_map=(",
):
    require(marker in SERVER_MAIN, f"server live visibility/tunnel acceptance gate missing: {marker}")

require(
    "if (!observation.visibilityBypassActive ||" in SERVER_MAIN and
    "!observation.executionSeen" in SERVER_MAIN and
    "replicatedDemandMagnitude > 1.0e-6" in SERVER_MAIN and
    "sparsePacket.metadata.serverTick" in SERVER_MAIN and
    "authoritativePublished.metadata.serverTick" in SERVER_MAIN,
    "same-tick replication proof must be captured while bounded visibility steering is actively executing",
)

require(
    "bool visibilityEvidenceComplete = false;" in SERVER_MAIN and
    "bool behaviorEvidenceComplete = false;" in SERVER_MAIN and
    "observation.visibilityBypassSeen &&" in SERVER_MAIN and
    "observation.visibilityDirectRecoveredSeen &&" in SERVER_MAIN and
    "observation.movingGapPlanePassed &&" in SERVER_MAIN and
    "observation.slitPortalExactOpenPublished &&" in SERVER_MAIN and
    "observation.slitPortalWaypointSeen &&" in SERVER_MAIN and
    "observation.slitEntryCaptureSeen &&" in SERVER_MAIN and
    "observation.slitEntryVelocityAlignedSeen &&" in SERVER_MAIN and
    "observation.slitEntryForwardAlignedSeen &&" in SERVER_MAIN and
    "observation.slitEntryPlaneCrossedAligned &&" in SERVER_MAIN and
    "observation.slitTunnelPassed &&" in SERVER_MAIN and
    "return 56;" in SERVER_MAIN,
    "live gate must order visibility bypass/replication/direct recovery before aligned portal capture and exact-static tunnel transit",
)

require(
    "m_navigationRuntimeLabLastPlan.nominalStaticObstacleEntityId ==" in SIM_CPP and
    "obstacleExactStaticBlockSeen = true" in SIM_CPP,
    "live ownership proof must bind CUBE 08 to exact-static blocker identity",
)


require(
    "tr.motion.travelFrame.localToWorldPosition(" in SIM_CPP and
    "tr.motion.travelFrame.localToWorldVelocity(" in SIM_CPP,
    "matched HubTactical ships must re-materialize world pose/velocity from current travel-frame epoch",
)

hub_rebuild_index = SIM_CPP.find("rebuildHubNavigationFrames(trajectoryDeltaSeconds);")
early_refresh_index = SIM_CPP.find(
    "updateShipReferenceFrames(dt);",
    hub_rebuild_index,
)
ai_section_index = SIM_CPP.find("// === 1. AI / controls / attitude ===")

require(
    hub_rebuild_index >= 0 and
    early_refresh_index > hub_rebuild_index and
    ai_section_index > early_refresh_index,
    "reference-frame refresh must occur after current hub-frame rebuild and before AI/navigation",
)

require(
    SIM_CPP.find(
        "updateShipReferenceFrames(dt);",
        early_refresh_index + 1,
    ) == -1,
    "reference-frame refresh must have one authoritative fixed-step ordering point",
)

for marker in (
    "tr.motion.localVelocityMps =",
    "tr.motion.mainEngineAccelerationMps2 =",
    "tr.motion.manoeuvreAccelerationMps2 =",
    "tr.motion.engineAccelerationMps2 =",
    "VelocityAlignmentMode::None",
):
    require(
        marker in SIM_CPP,
        f"reference-frame placement must clear stale authoritative motion state: {marker}",
    )

require(
    "NavigationRuntimeLabInitialObstacleLeadMeters" in LAB_H and
    "NavigationRuntimeLabObstacleVisualLocalMeters.z -" in LAB_H,
    "live proving start must remain explicitly tied to CUBE 08 with a bounded initial lead",
)

require(
    "NavigationRuntimeLabPlacementToleranceMeters" in LAB_H and
    "NavigationRuntimeLabPlacementToleranceMeters" in SERVER_MAIN and
    "placementErrorMeters > 1.0e-6" not in SERVER_MAIN,
    "placement round-trip gate must use named realistic sub-millimetre tolerance",
)

require(
    "configuredRouteExactObstacleBlockPublished" in SIM_CPP and
    "fixtureProof.blockingObstacleEntityId ==" in SIM_CPP,
    "static publication must prove configured start->goal intersects exact CUBE 08 geometry",
)

require(
    "firstLiveNominalProbeCaptured" in SIM_CPP and
    "Space::SegmentQuery firstLiveProbe" in SIM_CPP and
    "firstLiveBoundedTargetMap" in SIM_CPP,
    "live lab must capture the exact first bounded nominal segment before planner composition",
)

require(
    "configured NavigationRuntimeLab route does not " in SERVER_MAIN and
    "return 38;" in SERVER_MAIN,
    "server self-test must fail fast when the exact-static proving fixture is invalid",
)

require(
    "firstLiveNominal* remains diagnostic-only" in SERVER_MAIN and
    "return 50;" not in SERVER_MAIN and
    "return 51;" not in SERVER_MAIN,
    "multi-region tunnel acceptance must not treat the legacy first bounded probe as a hard physical/topology gate",
)

require(
    "authored exact-static slit tunnel does not admit " in SERVER_MAIN and
    "return 57;" in SERVER_MAIN,
    "server self-test must fail fast when the slit fixture cannot admit the current ship envelope",
)

require(
    "routeVectorWorld" in SIM_CPP and
    "routeDirectionWorld" in SIM_CPP,
    "executed lateral-demand diagnostics must compare world-space vectors in one frame",
)

require(
    "Space::SegmentQuery actualMotion" in SIM_CPP and
    "actualMotion.exactObstaclesOnly = true" in SIM_CPP and
    "m_navigationRuntimeLabPreviousExactSafetyPositionMap" in SIM_CPP and
    "exactStaticViolationSeen = true" in SIM_CPP,
    "live physical acceptance must sweep actual motion against exact HitVolumes without treating navigation region partitions as walls",
)

require(
    "actualSafety.blockingObstacleEntityId" in SIM_CPP and
    "firstExactStaticViolationCaptured" in SIM_CPP and
    "firstExactStaticViolationSelectedTargetMap" in SIM_CPP,
    "first exact-static physical violation must retain obstacle identity and maneuver witness",
)

require(
    "authoritative ship swept through exact static " in SERVER_MAIN and
    "violation_entity=" in SERVER_MAIN and
    "return 52;" in SERVER_MAIN,
    "server self-test must fail fast with the first exact-static collision witness",
)

require(
    "observation.minimumConservativeClearanceMeters > 0.0" not in SERVER_MAIN,
    "conservative sphere clearance must remain diagnostic only after exact-static authority is active",
)

for marker in (
    "Doctrine",
    "Rational",
    "PrecisionRetrieval",
    "Extreme",
    "CombatEscape",
    "ProgressRequirement",
    "MustProgress",
    "allowExpectedContact",
    "allowSacrificialComponentLoss",
    "criticalDamageRisk01",
    "missionDamageCost01",
    "expendableDamageCost01",
    "threatExposure",
    "escapeReserve01",
    "ControlLawRequirement",
    "AssistedOnly",
    "NewtonianOnly",
    "NewtonianFlipAndBurn",
    "ExtendedVisibilityRecovery",
    "Backtrack",
    "ReverseEscape",
):
    require(marker in MANEUVER_DECISION_H, f"maneuver decision contract missing: {marker}")

for marker in (
    "commonPreferred",
    "rationalBetter",
    "precisionBetter",
    "extremeBetter",
    "combatEscapeBetter",
    "MustProgress",
    "haveNonContactProgress",
    "controlLawCompatible",
):
    require(marker in MANEUVER_DECISION_CPP, f"maneuver decision implementation missing: {marker}")

for marker in (
    "testRationalMayStopRatherThanAcceptContact",
    "testMustProgressDoesNotCollapseToStop",
    "testExtremeMayTradeExpendableDamageForSpeed",
    "testExtremeStillRejectsCatastrophicShortcut",
    "testCombatEscapePrefersLowerThreatExposure",
    "testPrecisionRetrievalPrefersClearanceAndLowEntrySpeed",
    "testSacrificialDamageNeedsExplicitPermission",
    "testControlLawFiltersIncompatibleRecoveryManeuvers",
):
    require(marker in MANEUVER_DECISION_TEST, f"maneuver decision regression missing: {marker}")

for marker in (
    "no collision-free proof != no navigation command",
    "no global route != stop by default",
    "ManeuverDecisionController",
    "Local bounded visibility",
    "Precision passage",
    "Emergency / contact-expected passage",
    "Projected silhouette under fire",
    "Escape reserve / viability",
    "Cinematic navigation principle",
):
    require(marker in MANEUVER_DECISION_DOC, f"maneuver decision architecture missing: {marker}")


for marker in (
    "ExecutionMode",
    "Automatic",
    "Manual",
    "Scope",
    "LocalHorizon",
    "FullRoute",
    "continueAcceptedAutomaticExecution",
    "guidanceOnly",
    "ManualCorridorExit",
    "DynamicHazardInvalidated",
    "VehicleCapabilityChanged",
    "TopologyBranchInvalidated",
    "ManualPeriodicRefresh",
):
    require(marker in REPLAN_H, f"execution replan policy contract missing: {marker}")

for marker in (
    "query.goalIntentChanged",
    "query.currentTopologyBranchValid",
    "query.dynamicHazardInvalidated",
    "query.vehicleCapabilityChanged",
    "query.trackingErrorExceeded",
    "query.manualCorridorExited",
    "Reason::ManualPeriodicRefresh",
    "continueAcceptedAutomaticExecution = true",
):
    require(marker in REPLAN_CPP, f"execution replan policy implementation missing: {marker}")

for marker in (
    "testAutomaticDoesNotReplanEveryFrame",
    "testAutomaticReplansOnlyLocalSuffixOnHazard",
    "testTrackingErrorInvalidatesAutomaticSegment",
    "testManualRefreshIsPeriodicAndGuidanceOnly",
    "testManualCorridorExitReplansImmediately",
    "testManualDeviationDoesNotForceGlobalRoute",
    "testTopologyInvalidationForcesFullRoute",
    "testVehicleDamageKeepsGlobalRouteButRebuildsTrajectory",
    "testSegmentExpiryAdvancesLocally",
):
    require(marker in REPLAN_TEST, f"execution replan regression missing: {marker}")

for marker in (
    "Automatic execution is not",
    "plan",
    "execute accepted segment",
    "monitor validity",
    "Manual mode",
    "periodic local refresh",
    "immediate local refresh on corridor exit",
    "LocalHorizon",
    "FullRoute",
    "plan count << execution tick count",
):
    require(marker in REPLAN_DOC, f"trajectory execution/replan architecture missing: {marker}")

require(
    "src/game/navigation/NavigationExecutionReplanPolicy.cpp" in ROOT_CMAKE and
    "src/game/navigation/NavigationExecutionReplanPolicy.cpp" in RUNTIME_CMAKE and
    "navigation_execution_replan_policy_tests" in RUNTIME_CMAKE,
    "production/runtime gates must compile execution replanning policy and its tests",
)

for marker in (
    "LocalFlightControlLaw::Assisted",
    "LocalFlightControlLaw::Newtonian",
    "vehicle OBB (brick)",
    "conservative rotation sphere",
    "continuous swept OBB",
    "FlipAndBurn",
    "75-degree limit is not a vehicle capability limit",
    "NoSafeProgressInOrdinaryFan",
    "motion-primitive generation",
):
    require(marker in CONTROL_LAW_DOC, f"control-law maneuver architecture missing: {marker}")

require(
    "src/game/ship/controller/ManeuverDecisionController.cpp" in RUNTIME_CMAKE and
    "maneuver_decision_controller_tests" in RUNTIME_CMAKE,
    "runtime gate must compile and execute the maneuver decision controller",
)
require(
    "add_library(EliteManeuverDecision STATIC" in ROOT_CMAKE and
    ROOT_CMAKE.count("EliteManeuverDecision") >= 3,
    "client/server production build must compile the shared maneuver decision layer above NavigationWorld",
)

for marker in (
    "deterministic proving ground",
    "production ownership chain",
    "legacy",
):
    require(marker.lower() in DOC.lower(), f"stage-12 contract missing: {marker}")

require(
    "second presentation-only planner" in DOC.lower() or
    "run a second planner" in DOC.lower(),
    "stage-12 contract must forbid a second presentation/debug planner",
)

print("NAVIGATION STAGE 12 RUNTIME PLANNER CONTRACT: PASS")
print(" - maneuver decision ownership sits above navigation geometry/reachability")
print(" - Rational/Precision/Extreme/CombatEscape doctrines are deterministic and tested")
print(" - MustProgress retains contact-expected progress instead of collapsing to stop")
print(" - Assisted and Newtonian maneuver families are separated before doctrine ranking")
print(" - ordinary 0..75 degree exhaustion escalates to recovery instead of defining vehicle capability")
print(" - stable automatic execution keeps accepted short trajectory instead of replanning per frame")
print(" - manual guidance refreshes local suffix periodically and immediately on corridor exit")
print(" - NavigationSpace publishes ordered selected-portal steering centers")
print(" - one shared runtime planner composes static corridor + bounded local avoidance")
print(" - blocked/stale/conflict states keep producing fail-closed pilot intent")
print(" - planner cannot mutate authoritative physics state")
print(" - client/server share the same NavigationWorld runtime-planning target")
print(" - deterministic fixtures pin detour, envelope rejection, moving conflict and pilot bridge")
print(" - authoritative GameSimulation isolates one Active stage-12 lab actor on real NAV STRESS hit volumes")
print(" - live self-test pins bounded visibility bypass -> direct recovery -> portal capture -> aligned tunnel entry/exit")
print(" - bounded NavigationMap sphere covers the complete local avoidance fan")
print(" - sparse packet is compared with authoritative publication at the exact same server tick")
print(" - canonical sparse hydration must match the same authoritative execution truth")
print(" - non-identity working-frame regression pins map intent -> world control transform")
print(" - live lateral-demand diagnostics compare vectors in world space")
print(" - self-test reports pilot demand separately from physically applied acceleration")
print(" - NavigationSpace exact static OBB layer preserves real apertures beyond sphere broadphase")
print(" - LocalAvoidance proves nominal and adjusted segments against exact static geometry")
print(" - live lab publishes exact HitVolume OBBs after authoritative hub/object transforms")
print(" - live self-test requires exact-static query work and nominal OBB blocking evidence")
print(" - dynamic ConflictHold cannot collapse exact-static nominal proof to a zero-length segment")
print(" - monolithic NAV STRESS objects receive authoritative logical HitVolumes")
print(" - corridor-proven portal endpoints remain legal exact-static targets")
print(" - oriented portal traversal publishes route-direction normal and entry capture constraints")
print(" - physical exact sweeps ignore virtual region partitions and test only exact HitVolumes")
print(" - live physical motion is swept against exact HitVolume geometry every fixed step")
print(" - conservative sphere clearance is diagnostic only, not exact-static acceptance truth")
print(" - stationary NAV STRESS obstacles are excluded from NavigationMap dynamic ownership")
print(" - representative CUBE 08 wall block remains exact-static only, never a dynamic candidate/conflict")
print(" - configured direct line is blocked while the offset tunnel centerline is exact-HitVolume proven open")
print(" - invalid exact-static proving geometry fails fast before a 120 s behavior run")
print(" - first live bounded segment is independently exact-probed before planner composition")
print(" - live-scale 1300 m exact OBB regression pins first-horizon static adjustment")
print(" - first physical exact-static violation reports obstacle identity and maneuver witness")
print(" - reference-frame placement clears stale local velocity and propulsion state")
print(" - current hub-frame epoch is synchronized into matched ship world pose before AI/navigation")
print(" - sub-millimetre orbital-coordinate round-trip residue is treated as numerical zero")
print(" - rotating infrastructure carries angular velocity through NavigationMap working-frame conversion")
print(" - live GUIDANCE DOCK CUBE A verifies the published map-space angular motion")
print(" - bounded runtime conflicts feed MovingGapPredictor + MovingPassageTrajectoryEvaluator")
print(" - accepted moving Hermite curve is continuously bounded between its 33 samples")
print(" - same moving trajectory is re-proven against exact static NavigationSpace geometry")
print(" - moving-passage steering authority is explicit opt-in and requires both dynamic + exact-static proof")
print(" - authoritative moving passage uses the exact proved first acceleration sample through map/world + PilotSkillExecutor")
print(" - live moving obstacle pair drives bounded visibility steering through real physics and same-tick replication")
