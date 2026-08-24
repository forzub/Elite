#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

#include "src/game/navigation/DockingCompatibility.h"
#include "src/game/navigation/DockingPortRuntimeStateCatalog.h"
#include "src/game/navigation/DockingRouteRequest.h"
#include "src/game/navigation/GalacticReferenceFrame.h"
#include "src/game/navigation/GuidanceCorridor.h"
#include "src/game/navigation/HubCoMovingFrame.h"
#include "src/game/navigation/HubFrameBasis.h"
#include "src/game/navigation/HubSemanticAnchor.h"
#include "src/game/navigation/LocalGuidancePlanner.h"
#include "src/game/navigation/NavigationModuleState.h"
#include "src/game/navigation/NavigationPlanningSnapshot.h"
#include "src/game/navigation/NavigationPlanningEpoch.h"
#include "src/game/navigation/NavigationWorldPredictor.h"
#include "src/game/navigation/ReplicatedHubFrame.h"
#include "src/game/navigation/DockingPathPlanner.h"
#include "src/world/navigation/GeometricPathPlanner.h"
#include "src/world/navigation/NavigationObstacleGeometry.h"
#include "src/game/navigation/TrajectoryPredictor.h"
#include "src/game/navigation/TrajectorySafetyEvaluator.h"

namespace
{
using namespace game::navigation;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

bool near(double a, double b, double tolerance = 1.0e-6)
{
    return std::abs(a - b) <= tolerance;
}

TrajectoryPredictionResult coast(
    double startTime,
    double horizon,
    const glm::dvec3& position,
    const glm::dvec3& velocity
)
{
    TrajectoryPredictionRequest request;
    request.systemId = 0;
    request.startUniverseTimeSeconds = startTime;
    request.initialState.positionMeters = position;
    request.initialState.velocityMps = velocity;
    request.horizonSeconds = horizon;
    request.sampleIntervalSeconds = 0.25;
    request.maxIntegrationStepSeconds = 0.05;
    return TrajectoryPredictor::predict(request);
}

void testModuleSwitchesAreIndependent()
{
    NavigationModuleState state;
    require(state.enabled(NavigationModuleId::SafetyEvaluation), "safety must default on");
    require(!state.enabled(NavigationModuleId::HudGuidanceCorridor), "experimental guidance HUD must default off");

    state.setEnabled(NavigationModuleId::HudGuidanceCorridor, true);
    require(state.enabled(NavigationModuleId::HudGuidanceCorridor), "HUD switch did not change");
    require(state.enabled(NavigationModuleId::SafetyEvaluation), "hiding HUD disabled computation");

    state.setAllHudLayers(false);
    require(!state.enabled(NavigationModuleId::HudTargetMarkers), "target HUD group not disabled");
    require(!state.enabled(NavigationModuleId::HudRouteMarkers), "route HUD group not disabled");
    require(!state.enabled(NavigationModuleId::HudFlightVector), "flight vector HUD group not disabled");
    require(state.enabled(NavigationModuleId::TrajectoryPrediction), "HUD group touched predictor");
}

void testSensorFusionNeverShrinksPhysicalEnvelope()
{
    NavigationPlanningSnapshot base;
    NavigationObstacleState official;
    official.geometry.id = "debris-7";
    official.geometry.radiusMeters = 20.0;
    official.geometry.requiredClearanceMeters = 60.0;
    official.positionUncertaintyMeters = 100.0;
    official.source = NavigationKnowledgeSource::AuthoritativeWorld;
    base.obstacles.push_back(official);

    NavigationObstacleState radar = official;
    radar.geometry.centerMeters = glm::dvec3(1000.0, 0.0, 0.0);
    radar.geometry.radiusMeters = 5.0;
    radar.geometry.requiredClearanceMeters = 10.0;
    radar.positionUncertaintyMeters = 2.0;
    radar.source = NavigationKnowledgeSource::Radar;

    NavigationPlanningSnapshotBuilder builder(base);
    builder.mergeObstacleObservation(radar);
    auto merged = std::move(builder).build();

    require(merged.obstacles.size() == 1, "fusion duplicated obstacle identity");
    const auto& result = merged.obstacles.front();
    require(near(result.positionUncertaintyMeters, 2.0), "radar did not refine position uncertainty");
    require(near(result.geometry.conservativeRadiusMeters(), 20.0),
        "radar shrank authoritative physical radius");
    require(near(result.geometry.requiredClearanceMeters, 60.0),
        "radar shrank authoritative clearance");
}

void testMovingObstacleIsCheckedAtPassageTime()
{
    const auto trajectory = coast(
        100.0,
        10.0,
        glm::dvec3(0.0),
        glm::dvec3(10.0, 0.0, 0.0)
    );
    require(trajectory.ok(), "coast prediction failed");

    NavigationPlanningSnapshot environment;
    environment.systemId = 0;
    NavigationObstacleState obstacle;
    obstacle.geometry.id = "crossing-rock";
    obstacle.systemId = 0;
    obstacle.epochUniverseTimeSeconds = 100.0;
    obstacle.geometry.centerMeters = glm::dvec3(50.0, 50.0, 0.0);
    obstacle.velocityMps = glm::dvec3(0.0, -10.0, 0.0);
    obstacle.geometry.radiusMeters = 2.0;
    obstacle.geometry.requiredClearanceMeters = 2.0;
    environment.obstacles.push_back(obstacle);

    const auto safety = TrajectorySafetyEvaluator::evaluate(
        trajectory,
        environment,
        2.0
    );
    require(!safety.safe, "future crossing obstacle was treated as static/current-only");
    require(!safety.conflicts.empty(), "future obstacle conflict was not reported");
    require(std::abs(safety.conflicts.front().universeTimeSeconds - 105.0) < 0.5,
            "collision time is not the future closest approach");
}

KnownTrafficIntent crossingTraffic(double start)
{
    KnownTrafficIntent traffic;
    traffic.id = "liner-12";
    traffic.systemId = 0;
    traffic.physicalRadiusMeters = 5.0;
    traffic.requiredSeparationMeters = 10.0;
    traffic.timingUncertaintySeconds = 1.0;
    traffic.samples = {
        {start, glm::dvec3(50.0, 50.0, 0.0), glm::dvec3(0.0, -10.0, 0.0)},
        {start + 10.0, glm::dvec3(50.0, -50.0, 0.0), glm::dvec3(0.0, -10.0, 0.0)}
    };
    return traffic;
}

void testScheduledTrafficIsFourDimensionalAndExpires()
{
    NavigationPlanningSnapshot environment;
    environment.systemId = 0;
    environment.scheduledTraffic.push_back(crossingTraffic(100.0));

    const auto simultaneous = coast(
        100.0,
        10.0,
        glm::dvec3(0.0),
        glm::dvec3(10.0, 0.0, 0.0)
    );
    const auto conflict = TrajectorySafetyEvaluator::evaluate(
        simultaneous,
        environment,
        2.0
    );
    require(!conflict.safe, "same-place/same-time traffic conflict was missed");

    const auto muchLater = coast(
        200.0,
        10.0,
        glm::dvec3(0.0),
        glm::dvec3(10.0, 0.0, 0.0)
    );
    const auto clear = TrajectorySafetyEvaluator::evaluate(
        muchLater,
        environment,
        2.0
    );
    require(clear.safe, "scheduled ship was frozen forever at its last sample");
}


void testScheduledTrafficHonorsIntermediateSamples()
{
    // The ship coasts along +X at y=0. Traffic starts and ends clear at y=+80,
    // but its published middle sample crosses the ship path at t=105. A single
    // endpoint chord would miss this; piecewise 4D evaluation must catch it.
    NavigationPlanningSnapshot environment;
    environment.systemId = 0;

    KnownTrafficIntent traffic;
    traffic.id = "kinked-freighter";
    traffic.systemId = 0;
    traffic.physicalRadiusMeters = 4.0;
    traffic.requiredSeparationMeters = 6.0;
    traffic.samples = {
        {100.0, glm::dvec3(50.0, 80.0, 0.0), glm::dvec3(0.0)},
        {105.0, glm::dvec3(50.0, 0.0, 0.0), glm::dvec3(0.0)},
        {110.0, glm::dvec3(50.0, 80.0, 0.0), glm::dvec3(0.0)}
    };
    environment.scheduledTraffic.push_back(traffic);

    const auto trajectory = coast(
        100.0,
        10.0,
        glm::dvec3(0.0),
        glm::dvec3(10.0, 0.0, 0.0)
    );
    const auto safety = TrajectorySafetyEvaluator::evaluate(
        trajectory,
        environment,
        2.0
    );

    require(!safety.safe, "intermediate scheduled-traffic sample was ignored");
    require(!safety.conflicts.empty(), "intermediate traffic conflict missing");
    require(std::abs(safety.conflicts.front().universeTimeSeconds - 105.0) < 0.6,
            "intermediate traffic conflict time is wrong");
}

void testSemanticAnchorIsIndependentFromMesh()
{
    HubSemanticAnchorDefinition definition;
    definition.id = "gate-a";
    definition.hubModuleId = "dock-a";
    definition.kind = HubSemanticAnchorKind::DockingPort;
    definition.localPositionMeters = glm::dvec3(0.0, 0.0, -100.0);
    definition.localForward = glm::dvec3(0.0, 0.0, -1.0);
    definition.localUp = glm::dvec3(0.0, 1.0, 0.0);

    const glm::dmat4 rotation = glm::rotate(
        glm::dmat4(1.0),
        0.5 * 3.14159265358979323846,
        glm::dvec3(0.0, 1.0, 0.0)
    );
    const auto resolved = resolveHubSemanticAnchor(
        definition,
        0,
        10.0,
        glm::dvec3(1000.0, 0.0, 0.0),
        glm::dvec3(1.0, 2.0, 3.0),
        glm::mat4(rotation),
        glm::dvec3(0.0, 0.1, 0.0)
    );

    require(resolved.id == "gate-a", "semantic identity was lost");
    require(glm::length(resolved.positionMeters - glm::dvec3(900.0, 0.0, 0.0)) < 1.0e-4,
            "semantic local gate pose was not transformed by module pose");
    require(glm::length(resolved.velocityMps - glm::dvec3(1.0, 2.0, 13.0)) < 1.0e-4,
            "rotating semantic gate did not inherit omega cross r velocity");
}


void testDockingCompatibilityGatesRouteAction()
{
    HubSemanticAnchorDefinition dock;
    dock.id = "dock_gate_front";
    dock.hubModuleId = "guidance_dock_cube_a";
    dock.kind = HubSemanticAnchorKind::DockingPort;
    dock.orientationPolicy = DockOrientationPolicy::Upright;
    dock.extentMeters = glm::dvec3(110.0, 190.0, 900.0);
    dock.requiredClearanceMeters = 18.0;

    ShipDockingEnvelope cobra;
    cobra.lengthMeters = 22.2;
    cobra.widthMeters = 26.0;
    cobra.heightMeters = 5.0;
    cobra.valid = true;

    DockingPortRuntimeState state;
    state.hubModuleId = dock.hubModuleId;
    state.anchorId = dock.id;
    state.operational = DockingOperationalState::Online;
    state.occupancy = DockingOccupancyState::Free;
    state.access = DockingAccessState::Allowed;

    const auto available = evaluateDockingCompatibility(cobra, dock, state);
    require(available.geometryFits, "Cobra logical envelope should fit diagnostic dock");
    require(available.routeAvailable, "green docking state did not enable route availability");
    require(near(available.usableWidthMeters, 74.0), "dock width clearance was not applied");
    require(near(available.usableHeightMeters, 154.0), "dock height clearance was not applied");

    state.occupancy = DockingOccupancyState::Occupied;
    require(!evaluateDockingCompatibility(cobra, dock, state).routeAvailable,
            "occupied dock still allowed route calculation");

    state.occupancy = DockingOccupancyState::Free;
    state.access = DockingAccessState::Denied;
    require(!evaluateDockingCompatibility(cobra, dock, state).routeAvailable,
            "denied dock still allowed route calculation");

    state.access = DockingAccessState::Allowed;
    ShipDockingEnvelope tooWide = cobra;
    tooWide.widthMeters = 90.0;
    const auto rejected = evaluateDockingCompatibility(tooWide, dock, state);
    require(!rejected.geometryFits && !rejected.routeAvailable,
            "oversize ship still allowed docking route");
}

void testSemanticDockRouteRequestUsesStableIdentity()
{
    RouteTargetRef front;
    front.kind = NavigationRouteAnchorKind::SemanticAnchor;
    front.systemId = 0;
    front.stableObjectId = "guidance_dock_cube_a";
    front.semanticAnchorId = "dock_gate_front";

    RouteTargetRef rear = front;
    rear.semanticAnchorId = "dock_gate_rear";

    require(front.valid(), "semantic dock target is not valid");
    require(!sameRouteTarget(front, rear), "two docks on one module collapsed to one route target");

    DockingRouteRequestState requests;
    const auto serial = requests.request(front);
    const auto& request = requests.pending();
    require(serial != 0 && request.valid(), "typed docking route request was not created");
    require(request.target.stableObjectId == "guidance_dock_cube_a",
            "docking request lost stable module identity");
    require(request.target.semanticAnchorId == "dock_gate_front",
            "docking request lost stable anchor identity");
}

void testDiagnosticDockRuntimeStatesCoverDecisionCases()
{
    DockingPortRuntimeStateCatalog states;
    require(
        states.load("src/assets/data/navigation/hub_docking_runtime_test.json"),
        "diagnostic docking state catalog did not load"
    );

    const auto* green = states.find("guidance_dock_cube_a", "dock_gate_front");
    const auto* occupied = states.find("guidance_dock_cube_a", "dock_gate_rear");
    const auto* denied = states.find("guidance_dock_cylinder_b", "dock_gate_front");
    require(green && green->operationalNow() && green->freeNow() && green->accessAllowedNow(),
            "diagnostic catalog lacks free/allowed docking case");
    require(occupied && !occupied->freeNow(),
            "diagnostic catalog lacks occupied docking case");
    require(denied && !denied->accessAllowedNow(),
            "diagnostic catalog lacks denied docking case");
}

void testGalacticCompassUsesStandardLBasis()
{
    const auto frame = makeGalacticReferenceFrame(
        glm::dvec3(1.0, 0.0, 0.0),
        glm::dvec3(0.0, 0.0, 1.0)
    );
    require(frame.valid, "galactic frame construction failed");

    const auto center = galacticAnglesForDirection(frame, frame.centerDir);
    const auto l90 = galacticAnglesForDirection(frame, frame.longitude90Dir);
    const auto north = galacticAnglesForDirection(frame, frame.northDir);
    require(near(center.longitudeDeg, 0.0), "galactic center is not l=0");
    require(near(center.latitudeDeg, 0.0), "galactic center is not b=0");
    require(near(l90.longitudeDeg, 90.0), "second galactic axis is not l=90");
    require(near(north.latitudeDeg, 90.0), "north galactic pole is not b=+90");
}

void testGuidanceStateChoosesPriorityAndExpiry()
{
    NavigationGuidanceState state;
    GuidanceCorridor low;
    low.id = "mission";
    low.systemId = 0;
    low.priority = 10;
    low.generatedAtUniverseTimeSeconds = 100.0;
    low.validUntilUniverseTimeSeconds = 200.0;
    low.frames.push_back({});

    GuidanceCorridor high = low;
    high.id = "atc";
    high.priority = 50;
    state.publish(low);
    state.publish(high);

    require(state.active(0, 120.0) && state.active(0, 120.0)->id == "atc",
            "guidance source priority was ignored");
    state.pruneExpired(250.0);
    require(state.corridors().empty(), "expired corridors were not pruned");
}

void testLocalPlannerUsesPredictorAndSafetyEvaluator()
{
    LocalGuidanceRequest request;
    request.corridorId = "dock-test";
    request.systemId = 0;
    request.startUniverseTimeSeconds = 100.0;
    request.actorState.positionMeters = glm::dvec3(0.0, 0.0, 0.0);
    request.actorState.velocityMps = glm::dvec3(5.0, 0.0, 0.0);
    request.target.id = "gate";
    request.target.hubModuleId = "guidance_dock_cube_a";
    request.target.kind = HubSemanticAnchorKind::DockingPort;
    request.target.systemId = 0;
    request.target.epochUniverseTimeSeconds = 100.0;
    request.target.positionMeters = glm::dvec3(200.0, 0.0, 0.0);
    request.target.velocityMps = glm::dvec3(1.0, 0.0, 0.0);
    request.target.extentMeters = glm::dvec3(40.0, 40.0, 1.0);
    request.target.requiredClearanceMeters = 10.0;
    request.target.maxEntrySpeedMps = 10.0;
    request.profile.horizonSeconds = 10.0;
    request.profile.frameIntervalSeconds = 0.5;
    request.profile.predictorIntegrationStepSeconds = 0.05;
    request.profile.motionEnvelope.maxProperAccelerationMps2 = 4.0 * StandardGravityMps2;
    request.profile.motionEnvelope.maxProperJerkMps3 = 2.0 * StandardGravityMps2;

    const auto result = LocalGuidancePlanner::plan(request);
    require(result.status == LocalGuidanceStatus::Ready,
            "clear direct local guidance candidate was not ready");
    require(result.prediction.ok(), "local planner did not use predictor");
    require(result.safety.safe, "clear local candidate failed safety evaluation");
    require(!result.corridor.frames.empty(), "local planner did not publish corridor frames");
    require(result.corridor.source == GuidanceSource::LocalPlanner,
            "local corridor source was not preserved");

    NavigationObstacleState blocking;
    blocking.geometry.id = "blocking-debris";
    blocking.systemId = 0;
    blocking.epochUniverseTimeSeconds = 100.0;
    blocking.geometry.centerMeters = glm::dvec3(100.0, 0.0, 0.0);
    blocking.geometry.radiusMeters = 25.0;
    blocking.geometry.requiredClearanceMeters = 10.0;
    request.environment.obstacles.push_back(blocking);

    const auto detour = LocalGuidancePlanner::plan(request);
    require(detour.status == LocalGuidanceStatus::Ready,
            "local planner did not find a simple lateral bypass");
    require(detour.detourUsed, "local planner hid whether a detour was used");
    require(detour.safety.safe, "accepted local detour is not safe");

    request.environment.obstacles.clear();
    RestrictedNavigationVolume forbidden;
    forbidden.id = "closed-test-volume";
    forbidden.systemId = 0;
    forbidden.centerMeters = glm::dvec3(100.0, 0.0, 0.0);
    forbidden.radiusMeters = 1000.0;
    request.environment.restrictedVolumes.push_back(forbidden);

    const auto blocked = LocalGuidancePlanner::plan(request);
    require(blocked.status == LocalGuidanceStatus::Blocked,
            "planner accepted a target inside an unavoidable restricted volume");
    require(!blocked.safety.safe, "blocked local candidate lost safety conflict");
}


void testDockingGuidanceEndsNormalToGateWithRequiredHullPose()
{
    LocalGuidanceRequest request;
    request.corridorId = "dock-6dof";
    request.systemId = 0;
    request.startUniverseTimeSeconds = 100.0;
    request.actorState.positionMeters = glm::dvec3(0.0, 0.0, 0.0);
    request.actorState.velocityMps = glm::dvec3(0.0);
    request.actorOrientation = glm::dquat(1.0, 0.0, 0.0, 0.0);

    request.target.id = "gate";
    request.target.hubModuleId = "dock-module";
    request.target.kind = HubSemanticAnchorKind::DockingPort;
    request.target.orientationPolicy = DockOrientationPolicy::Upright;
    request.target.systemId = 0;
    request.target.epochUniverseTimeSeconds = 100.0;
    request.target.positionMeters = glm::dvec3(0.0, 0.0, 200.0);
    request.target.orientation = glm::dquat(1.0, 0.0, 0.0, 0.0);
    request.target.extentMeters = glm::dvec3(40.0, 24.0, 1.0);
    request.target.requiredClearanceMeters = 2.0;
    request.target.maxEntrySpeedMps = 8.0;

    request.profile.purpose = GuidancePurpose::Docking;
    request.profile.horizonSeconds = 10.0;
    request.profile.frameIntervalSeconds = 0.25;
    request.profile.predictorIntegrationStepSeconds = 0.02;
    request.profile.vehicleEnvelope = {
        20.0, 8.0, 4.0, 0.0, true
    };
    request.profile.recommendedSpeedMps = 8.0;

    const auto result = LocalGuidancePlanner::plan(request);
    require(result.status == LocalGuidanceStatus::Ready,
            "clear 6-DOF docking guidance was not ready");
    require(!result.corridor.frames.empty(),
            "6-DOF docking guidance did not create tunnel frames");

    const auto& first = result.corridor.frames.front();
    const auto& last = result.corridor.frames.back();
    require(glm::length(first.centerMeters - request.actorState.positionMeters) < 1.0e-6,
            "first docking frame is not the actual ship position");
    require(first.requiredVehiclePose && last.requiredVehiclePose,
            "docking tunnel does not carry required hull pose semantics");

    const glm::dvec3 finalForward = glm::normalize(
        last.orientation * glm::dvec3(0.0, 0.0, -1.0)
    );
    const glm::dvec3 finalUp = glm::normalize(
        last.orientation * glm::dvec3(0.0, 1.0, 0.0)
    );
    const glm::dvec3 inbound(0.0, 0.0, 1.0);
    require(glm::dot(finalForward, inbound) > 0.999,
            "terminal ship nose is not perpendicular/inbound to dock plane");
    require(glm::dot(finalUp, glm::dvec3(0.0, 1.0, 0.0)) > 0.999,
            "terminal ship top is not aligned with dock top");

    const double expectedDepth = 12.0; // half length 10 + dock clearance 2
    require(result.terminal.evaluated && result.terminal.matched,
            "docking planner accepted a trajectory that misses terminal state");
    require(std::abs(last.centerMeters.z - (200.0 + expectedDepth)) <=
                result.terminal.positionToleranceMeters,
            "physical terminal sample does not place the hull inside the gate");
    require(glm::length(
                last.centerMeters -
                result.prediction.samples.back().state.positionMeters) < 1.0e-9,
            "corridor snapped its last frame away from the physical prediction");
    require(result.corridor.hasTerminalTarget,
            "docking corridor lost its separate terminal target marker");

    const glm::dvec3 terminalRelativeVelocity =
        result.prediction.samples.back().state.velocityMps;
    require(glm::dot(glm::normalize(terminalRelativeVelocity), inbound) > 0.995,
            "terminal trajectory velocity is not normal to the entrance plane");
}

void testHubCoMovingFrameRemovesOrbitalTangentFromLocalMotion()
{
    constexpr double radiusMeters = 7000000.0;
    constexpr double speedMps = 7500.0;
    const double start = 100.0;

    const auto seed = makeHubCoMovingFrameSeed(
        0,
        "hub-a",
        start,
        glm::dvec3(0.0, radiusMeters, 0.0),
        glm::dvec3(speedMps, 0.0, 0.0),
        glm::dvec3(0.0),
        glm::dvec3(0.0)
    );
    require(seed.valid, "Hub co-moving frame seed is invalid");

    const auto now = predictHubCoMovingFrameAt(seed, start);
    const auto future = predictHubCoMovingFrameAt(seed, start + 30.0);
    require(now.valid && future.valid, "Hub co-moving prediction failed");

    const glm::dvec3 fixedLocal(-10000.0, 250.0, 40.0);
    const glm::dvec3 futureWorld = future.localToWorldPosition(fixedLocal);
    require(glm::length(future.worldToLocalPosition(futureWorld) - fixedLocal) < 1.0e-6,
            "future Hub frame did not preserve a fixed local point");

    const glm::dvec3 localVelocity = future.worldToLocalVelocity(
        futureWorld,
        future.localToWorldVelocity(fixedLocal, glm::dvec3(0.0))
    );
    require(glm::length(localVelocity) < 1.0e-6,
            "common orbital tangent leaked into Hub-local velocity");
}

void testDockingPlannerConvergesAgainstCoMovingOrbitalTarget()
{
    constexpr double mu = 3.986004418e14;
    constexpr double radiusMeters = 7000000.0;
    const double speedMps = std::sqrt(mu / radiusMeters);
    const double start = 100.0;

    const auto seed = makeHubCoMovingFrameSeed(
        0,
        "hub-orbit",
        start,
        glm::dvec3(0.0, radiusMeters, 0.0),
        glm::dvec3(speedMps, 0.0, 0.0),
        glm::dvec3(0.0),
        glm::dvec3(0.0)
    );
    const auto initialFrame = predictHubCoMovingFrameAt(seed, start);
    require(initialFrame.valid, "orbital Hub frame seed did not resolve");

    LocalGuidanceRequest request;
    request.corridorId = "orbital-dock";
    request.systemId = 0;
    request.startUniverseTimeSeconds = start;
    const glm::dvec3 initialLocalPosition(-10000.0, 0.0, 0.0);
    request.actorState.positionMeters =
        initialFrame.localToWorldPosition(initialLocalPosition);
    request.actorState.velocityMps = initialFrame.localToWorldVelocity(
        initialLocalPosition,
        glm::dvec3(0.0)
    );
    request.actorOrientation = glm::dquat(1.0, 0.0, 0.0, 0.0);

    request.target.id = "orbital-gate";
    request.target.hubModuleId = "orbital-module";
    request.target.kind = HubSemanticAnchorKind::DockingPort;
    request.target.orientationPolicy = DockOrientationPolicy::Upright;
    request.target.systemId = 0;
    request.target.epochUniverseTimeSeconds = start;
    request.target.orientation = glm::dquat(1.0, 0.0, 0.0, 0.0);
    request.target.extentMeters = glm::dvec3(110.0, 190.0, 900.0);
    request.target.requiredClearanceMeters = 18.0;
    request.target.maxEntrySpeedMps = 18.0;

    request.profile.purpose = GuidancePurpose::Docking;
    request.profile.horizonSeconds = 60.0;
    request.profile.frameIntervalSeconds = 0.5;
    request.profile.predictorIntegrationStepSeconds = 0.05;
    request.profile.vehicleEnvelope = {
        22.2, 26.0, 5.0, 0.0, true
    };
    request.profile.recommendedSpeedMps = 18.0;
    request.profile.motionEnvelope.maxProperAccelerationMps2 =
        6.0 * StandardGravityMps2;
    request.profile.motionEnvelope.maxProperJerkMps3 =
        1.5 * StandardGravityMps2;

    GravityBody parent;
    parent.id = "parent";
    parent.centerMeters = glm::dvec3(0.0);
    parent.radiusMeters = 6370000.0;
    parent.gravitationalParameterM3s2 = mu;
    request.environment.gravityBodies.push_back(parent);

    for (int i = 0; i <= 120; ++i)
    {
        const double time = start + static_cast<double>(i) * 0.5;
        const auto frame = predictHubCoMovingFrameAt(seed, time);
        ResolvedHubSemanticAnchor sample = request.target;
        sample.epochUniverseTimeSeconds = time;
        sample.positionMeters = frame.originMeters;
        sample.velocityMps = frame.linearVelocityMps;
        sample.hasRotationCenterKinematics = false;
        request.targetMotionSamples.push_back(sample);
    }
    request.target = request.targetMotionSamples.front();

    const auto result = LocalGuidancePlanner::plan(request);
    require(result.status == LocalGuidanceStatus::Ready,
            "orbital docking candidate did not converge to moving terminal state");
    require(result.terminal.evaluated && result.terminal.matched,
            "orbital docking candidate was accepted without terminal match");
    require(result.terminal.positionErrorMeters <=
                result.terminal.positionToleranceMeters,
            "orbital docking terminal position miss exceeded tolerance");
    require(result.terminal.velocityErrorMps <=
                result.terminal.velocityToleranceMps,
            "orbital docking terminal relative velocity exceeded tolerance");
    require(glm::length(
                result.corridor.frames.back().centerMeters -
                result.prediction.samples.back().state.positionMeters) < 1.0e-9,
            "orbital docking corridor snapped away from physical prediction");
}

void testRotatingSemanticAnchorPredictsCircularGateMotion()
{
    HubSemanticAnchorDefinition definition;
    definition.id = "rotating-gate";
    definition.hubModuleId = "rotor";
    definition.kind = HubSemanticAnchorKind::NavigationReference;
    definition.localPositionMeters = glm::dvec3(10.0, 0.0, 0.0);

    const double start = 100.0;
    const auto target = resolveHubSemanticAnchor(
        definition,
        0,
        start,
        glm::dvec3(100.0, 0.0, 0.0),
        glm::dvec3(2.0, 0.0, 0.0),
        glm::mat4(1.0f),
        glm::dvec3(0.0, 0.0, glm::half_pi<double>())
    );

    // Two seconds at pi/2 rad/s rotates the +X offset by pi. The module origin
    // also translates +4 m along X, so the gate ends at x = 100 + 4 - 10 = 94.
    // Test the semantic-anchor kinematics directly: the vehicle predictor is
    // allowed to have finite tracking error while obeying acceleration/jerk
    // envelopes and must not be used as a proxy for target motion.
    const auto predicted = predictHubSemanticAnchorAt(target, start + 2.0);
    const glm::dvec3 expectedPosition(94.0, 0.0, 0.0);
    const glm::dvec3 expectedVelocity(
        2.0,
        -5.0 * glm::pi<double>(),
        0.0
    );
    require(glm::length(predicted.positionMeters - expectedPosition) < 1.0e-9,
            "rotating gate position did not follow circular motion");
    require(glm::length(predicted.velocityMps - expectedVelocity) < 1.0e-9,
            "rotating gate velocity did not remain tangent to circular motion");

    const glm::dvec3 predictedUp = glm::normalize(predicted.up());
    const glm::dvec3 expectedUp(0.0, -1.0, 0.0);
    require(glm::dot(predictedUp, expectedUp) > 0.999999,
            "rotating gate orientation did not advance with module rotation");
}

void testUnsafeDockingPublishesEscapeThenRecoversPrimaryRoute()
{
    LocalGuidanceRequest request;
    request.corridorId = "dock-replan";
    request.systemId = 0;
    request.startUniverseTimeSeconds = 100.0;
    request.actorState.positionMeters = glm::dvec3(0.0, 0.0, 0.0);
    request.actorOrientation = glm::dquat(1.0, 0.0, 0.0, 0.0);
    request.target.id = "gate";
    request.target.hubModuleId = "dock-module";
    request.target.kind = HubSemanticAnchorKind::DockingPort;
    request.target.orientationPolicy = DockOrientationPolicy::Upright;
    request.target.systemId = 0;
    request.target.epochUniverseTimeSeconds = 100.0;
    request.target.positionMeters = glm::dvec3(0.0, 0.0, 200.0);
    request.target.orientation = glm::dquat(1.0, 0.0, 0.0, 0.0);
    request.target.extentMeters = glm::dvec3(40.0, 24.0, 1.0);
    request.target.requiredClearanceMeters = 2.0;
    request.target.maxEntrySpeedMps = 8.0;
    request.profile.purpose = GuidancePurpose::Docking;
    request.profile.horizonSeconds = 10.0;
    request.profile.frameIntervalSeconds = 0.25;
    request.profile.predictorIntegrationStepSeconds = 0.02;
    request.profile.vehicleEnvelope = {
        20.0, 8.0, 4.0, 0.0, true
    };

    RestrictedNavigationVolume blockedGate;
    blockedGate.id = "temporary-closed-volume";
    blockedGate.systemId = 0;
    blockedGate.centerMeters = glm::dvec3(0.0, 0.0, 200.0);
    blockedGate.radiusMeters = 70.0;
    request.environment.restrictedVolumes.push_back(blockedGate);

    const auto escape = LocalGuidancePlanner::plan(request);
    require(escape.status == LocalGuidanceStatus::EmergencyEscapeReady,
            "blocked docking did not produce emergency escape guidance");
    require(escape.emergencyEscapeUsed,
            "emergency escape status lost its explicit diagnostic flag");
    require(escape.corridor.purpose == GuidancePurpose::EmergencyEscape,
            "unsafe docking did not switch tunnel purpose to emergency escape");
    require(escape.corridor.noSafePrimarySolution,
            "emergency tunnel did not request NO SAFE GUIDANCE warning");
    require(escape.safety.safe,
            "published emergency escape corridor is not safety-evaluator clear");

    // The caller keeps the same typed docking request. Once the temporary
    // hazard disappears, the next rolling replan must resume the original dock.
    request.startUniverseTimeSeconds += 0.2;
    request.target.epochUniverseTimeSeconds += 0.2;
    request.environment.restrictedVolumes.clear();
    const auto recovered = LocalGuidancePlanner::plan(request);
    require(recovered.status == LocalGuidanceStatus::Ready,
            "docking route did not recover after the hazard cleared");
    require(recovered.corridor.purpose == GuidancePurpose::Docking,
            "recovered guidance did not return to the original docking task");
    require(!recovered.corridor.noSafePrimarySolution,
            "NO SAFE GUIDANCE warning survived a recovered docking solution");
}

} // namespace




void testHubLocalRotationKeepsPrecisionAtLargeUniverseEpoch()
{
    constexpr double universeTimeSeconds = 8.73395e8;
    constexpr double angularSpeedDegPerSecond = 2.0;

    const double phase0 = angularSpeedDegPerSecond * universeTimeSeconds;
    const double phase1 =
        angularSpeedDegPerSecond * (universeTimeSeconds + 0.25);

    const glm::mat4 orientation0 = hubLocalEulerDegToMatrix(
        glm::dvec3(0.0, 0.0, phase0)
    );
    const glm::mat4 orientation1 = hubLocalEulerDegToMatrix(
        glm::dvec3(0.0, 0.0, phase1)
    );

    const glm::vec3 x0(orientation0[0]);
    const glm::vec3 x1(orientation1[0]);
    const double observedStep = glm::length(x1 - x0);

    // 2 deg/s over 0.25 s is a 0.5 degree step.  At the project's large
    // universe epoch this must remain a small continuous rotation, not freeze
    // until float precision jumps by tens/hundreds of degrees.
    require(observedStep > 0.005 && observedStep < 0.02,
        "Hub-local rotation lost sub-degree precision at large universe epoch");

    const double wrapped0 = std::remainder(phase0, 360.0);
    const glm::mat4 expected0 = hubLocalEulerDegToMatrix(
        glm::dvec3(0.0, 0.0, wrapped0)
    );
    require(glm::length(glm::vec3(expected0[0]) - x0) < 1.0e-6f,
        "Hub-local rotation is not periodic after phase wrapping");
}


void testPlanningFrameRoundTripsCompleteKinematicState()
{
    const glm::dvec3 prograde = glm::normalize(glm::dvec3(0.71, 0.42, -0.55));
    const glm::dvec3 radialSeed = glm::normalize(glm::dvec3(-0.18, 0.90, 0.40));
    const glm::dvec3 normal = glm::normalize(glm::cross(prograde, radialSeed));
    const glm::dvec3 radial = glm::normalize(glm::cross(normal, prograde));

    KinematicFrame frame;
    frame.systemId = 7;
    frame.frameId = "foundation-lock-frame";
    frame.originMeters = glm::dvec3(8.2e10, -3.7e10, 1.9e10);
    frame.linearVelocityMps = glm::dvec3(3120.0, -880.0, 147.0);
    frame.linearAccelerationMps2 = glm::dvec3(-0.03, 0.017, 0.002);
    frame.localToWorldBasis = glm::dmat3(prograde, radial, normal);
    frame.angularVelocityWorldRadPerSecond = glm::dvec3(0.0007, -0.0011, 0.0004);
    frame.angularAccelerationWorldRadPerSecond2 = glm::dvec3(1.0e-6, -2.0e-6, 0.5e-6);
    frame.valid = true;

    const LocalKinematicState samples[] = {
        {glm::dvec3(0.0), glm::dvec3(0.0), glm::dvec3(0.0)},
        {glm::dvec3(1200.0, -75.0, 430.0), glm::dvec3(9.0, -2.0, 0.5), glm::dvec3(0.2, -0.04, 0.01)},
        {glm::dvec3(-5400.0, 2300.0, 17.0), glm::dvec3(-35.0, 11.0, 4.0), glm::dvec3(-0.7, 0.3, -0.02)}
    };

    constexpr double positionToleranceMeters = 1.0e-5;

    // At ~1e11 m world coordinates, worldPosition-origin necessarily loses a
    // few micrometres even in double precision.  In a rotating frame that
    // position quantisation propagates into velocity as omega x delta-r.
    // Derive the velocity/acceleration bounds from the accepted position
    // precision instead of demanding an impossible absolute 1e-9 m/s.
    const double velocityToleranceMps =
        2.0 * glm::length(frame.angularVelocityWorldRadPerSecond) *
            positionToleranceMeters +
        1.0e-10;
    const double accelerationToleranceMps2 =
        2.0 * (
            glm::length(frame.angularAccelerationWorldRadPerSecond2) +
            glm::dot(
                frame.angularVelocityWorldRadPerSecond,
                frame.angularVelocityWorldRadPerSecond
            )
        ) * positionToleranceMeters +
        2.0 * glm::length(frame.angularVelocityWorldRadPerSecond) *
            velocityToleranceMps +
        1.0e-10;

    for (const auto& local : samples)
    {
        const WorldKinematicState world = localToWorldKinematics(frame, local);
        const LocalKinematicState restored = worldToLocalKinematics(frame, world);

        require(glm::length(restored.positionMeters - local.positionMeters) <
                positionToleranceMeters,
            "planning frame position round trip drifted");
        require(glm::length(restored.velocityMps - local.velocityMps) <
                velocityToleranceMps,
            "planning frame velocity round trip exceeded position-precision bound");
        require(glm::length(restored.accelerationMps2 - local.accelerationMps2) <
                accelerationToleranceMps2,
            "planning frame acceleration round trip exceeded kinematic precision bound");
    }
}

void testHubAttachmentPredictionIsContinuousAtLargeUniverseEpoch()
{
    KinematicFrame frame;
    frame.systemId = 3;
    frame.frameId = "earth_orbital_hub";
    frame.originMeters = glm::dvec3(1000.0, 2000.0, -3000.0);
    frame.linearVelocityMps = glm::dvec3(12.0, -7.0, 2.0);
    frame.localToWorldBasis = glm::dmat3(1.0);
    frame.angularVelocityWorldRadPerSecond = glm::dvec3(0.0, 0.0, 0.001);
    frame.valid = true;

    constexpr double t0 = 8.73398e8;
    constexpr double dt = 0.25;

    const auto cube0 = NavigationWorldPredictor::resolveHubAttachmentAt(
        frame, t0, glm::dvec3(1700.0, 0.0, 0.0), glm::dvec3(0.0), glm::dvec3(0.0, 0.0, 2.0));
    const auto cube1 = NavigationWorldPredictor::resolveHubAttachmentAt(
        frame, t0 + dt, glm::dvec3(1700.0, 0.0, 0.0), glm::dvec3(0.0), glm::dvec3(0.0, 0.0, 2.0));

    require(cube0.valid && cube1.valid,
        "slow rotating Hub attachment prediction is invalid");
    require(glm::length(cube1.positionMeters - cube0.positionMeters) < 1.0e-9,
        "local module spin changed the attachment center");

    const glm::dvec3 x0 = glm::normalize(glm::dvec3(cube0.orientation[0]));
    const glm::dvec3 x1 = glm::normalize(glm::dvec3(cube1.orientation[0]));
    const double dotValue = std::clamp(glm::dot(x0, x1), -1.0, 1.0);
    const double observedStepDeg = glm::degrees(std::acos(dotValue));
    require(std::abs(observedStepDeg - 0.5) < 0.01,
        "slow Hub box rotation snapped or lost phase at large universe epoch");

    const auto cylinder0 = NavigationWorldPredictor::resolveHubAttachmentAt(
        frame, t0, glm::dvec3(-3000.0, -250.0, 0.0), glm::dvec3(12.0, 7.0, -3.0), glm::dvec3(0.0));
    const auto cylinder1 = NavigationWorldPredictor::resolveHubAttachmentAt(
        frame, t0 + 1000.0, glm::dvec3(-3000.0, -250.0, 0.0), glm::dvec3(12.0, 7.0, -3.0), glm::dvec3(0.0));

    require(cylinder0.valid && cylinder1.valid,
        "static Hub cylinder prediction is invalid");
    for (int column = 0; column < 3; ++column)
    {
        require(glm::length(
            glm::dvec3(cylinder1.orientation[column]) -
            glm::dvec3(cylinder0.orientation[column])
        ) < 1.0e-7, "static Hub cylinder orientation drifted");
    }
}

void testGeometricPlannerIsDeterministicAndInputPure()
{
    world::navigation::NavigationObstacle box;
    box.id = "box";
    box.shape = world::navigation::NavigationObstacleShape::Box;
    box.centerMeters = glm::dvec3(-200.0, 0.0, 0.0);
    box.halfExtentsMeters = glm::dvec3(180.0, 260.0, 220.0);
    const double angle = glm::radians(28.0);
    box.localToWorldBasis = glm::dmat3(
        glm::rotate(glm::dmat4(1.0), angle, glm::dvec3(0.0, 0.0, 1.0))
    );

    world::navigation::NavigationObstacle capsule;
    capsule.id = "capsule";
    capsule.shape = world::navigation::NavigationObstacleShape::Capsule;
    capsule.centerMeters = glm::dvec3(500.0, -120.0, 0.0);
    capsule.radiusMeters = 130.0;
    capsule.capsuleHalfLengthMeters = 260.0;

    world::navigation::GeometricPathRequest request;
    request.startMeters = glm::dvec3(-1400.0, 0.0, 0.0);
    request.goalMeters = glm::dvec3(1400.0, 0.0, 0.0);
    request.params.agentRadiusMeters = 24.0;
    request.obstacles = {box, capsule};

    const auto before = request;
    const auto baseline = world::navigation::GeometricPathPlanner::plan(request);
    require(baseline.valid, "determinism fixture did not produce a geometric path");

    for (int run = 0; run < 5; ++run)
    {
        const auto repeated = world::navigation::GeometricPathPlanner::plan(request);
        require(repeated.valid == baseline.valid,
            "repeated geometric plan changed validity");
        require(repeated.obstacleDetourUsed == baseline.obstacleDetourUsed,
            "repeated geometric plan changed topology flag");
        require(repeated.pointsMeters.size() == baseline.pointsMeters.size(),
            "repeated geometric plan changed waypoint count");
        for (std::size_t i = 0; i < baseline.pointsMeters.size(); ++i)
        {
            require(glm::length(repeated.pointsMeters[i] - baseline.pointsMeters[i]) < 1.0e-12,
                "repeated geometric plan changed waypoint geometry");
        }
    }

    require(glm::length(request.startMeters - before.startMeters) < 1.0e-12 &&
            glm::length(request.goalMeters - before.goalMeters) < 1.0e-12 &&
            request.obstacles.size() == before.obstacles.size() &&
            glm::length(request.obstacles[0].centerMeters - before.obstacles[0].centerMeters) < 1.0e-12,
        "geometric route calculation mutated its input snapshot");
}


void testNavigationPlanningEpochPreservesAuthoritativeIdentity()
{
    NavigationPlanningEpoch epoch;
    epoch.sourceTick = 77;
    epoch.serverTimeSeconds = 12.5;
    epoch.universeTimeSeconds = 9123.25;
    epoch.universeTimelineRevision = 4;

    require(epoch.valid(), "authoritative planning epoch is invalid");
    require(epoch.sourceTick == 77,
        "planning epoch changed source tick");
    require(near(epoch.serverTimeSeconds, 12.5),
        "planning epoch changed server time");
    require(near(epoch.universeTimeSeconds, 9123.25),
        "planning epoch changed universe time");
    require(epoch.universeTimelineRevision == 4,
        "planning epoch changed timeline revision");
}

void testNavigationWorldPredictorAdvancesOrbitalHubToPlanningEpoch()
{
    world::orbits::OrbitalMotion motion;
    motion.enabled = true;
    motion.centerMeters = glm::dvec3(0.0);
    motion.parentRadiusMeters = 1000.0;
    motion.altitudeMeters = 9000.0;
    motion.orbitalPeriodSeconds = 120.0;
    motion.initialPhaseDeg = 15.0;
    motion.epochSeconds = 0.0;

    const double sourceTime = 10.0;
    const double targetTime = 10.35;
    const glm::dvec3 sourcePosition =
        world::orbits::computeOrbitPositionMeters(motion, sourceTime);
    const glm::dvec3 sourceVelocity =
        world::orbits::computeOrbitVelocityMetersPerSecond(motion, sourceTime);

    const glm::dvec3 radial = glm::normalize(sourcePosition);
    const glm::dvec3 prograde = glm::normalize(sourceVelocity);
    const glm::dvec3 normal = glm::normalize(glm::cross(prograde, radial));

    HubPredictionSource source;
    source.systemId = 3;
    source.hubId = "hub-predict";
    source.sourceUniverseTimeSeconds = sourceTime;
    source.positionMeters = sourcePosition;
    source.velocityMps = sourceVelocity;
    source.orientation = hubVisualOrientation(prograde, radial, normal);
    source.orbitalMotion = motion;

    const auto frame = NavigationWorldPredictor::predictHubFrameAt(
        source,
        targetTime
    );
    require(frame.valid, "predicted orbital Hub frame is invalid");

    const glm::dvec3 expectedPosition =
        world::orbits::computeOrbitPositionMeters(motion, targetTime);
    const glm::dvec3 expectedVelocity =
        world::orbits::computeOrbitVelocityMetersPerSecond(motion, targetTime);

    require(glm::length(frame.originMeters - expectedPosition) < 1.0e-3,
        "Hub predictor did not advance orbital position to planning epoch");
    require(glm::length(frame.linearVelocityMps - expectedVelocity) < 1.0e-2,
        "Hub predictor did not advance orbital velocity to planning epoch");
}

void testNavigationWorldPredictorKeepsHubAttachmentsInOneFrame()
{
    KinematicFrame frame;
    frame.systemId = 5;
    frame.frameId = "hub-attached";
    frame.originMeters = glm::dvec3(1000.0, 2000.0, -3000.0);
    frame.linearVelocityMps = glm::dvec3(7.0, -2.0, 3.0);
    frame.localToWorldBasis = glm::dmat3(1.0);
    frame.angularVelocityWorldRadPerSecond = glm::dvec3(0.0, 0.0, 0.02);
    frame.valid = true;

    const glm::dvec3 localOffset(20.0, 30.0, 40.0);
    const glm::dvec3 localSpinDegPerSecond(0.0, 10.0, 0.0);
    const auto attached = NavigationWorldPredictor::resolveHubAttachmentAt(
        frame,
        12.0,
        localOffset,
        glm::dvec3(0.0),
        localSpinDegPerSecond
    );
    require(attached.valid, "Hub attachment prediction is invalid");

    // Hub visual X/Y/Z map to tactical normal/radial/-prograde.
    const glm::dvec3 expectedTactical(-localOffset.z, localOffset.y, localOffset.x);
    require(glm::length(
        frame.worldToLocalPosition(attached.positionMeters) - expectedTactical
    ) < 1.0e-9, "Hub attachment drifted out of the predicted Hub frame");

    require(glm::length(
        attached.angularVelocityWorldRadPerSecond -
        (frame.angularVelocityWorldRadPerSecond +
         glm::dvec3(0.0, glm::radians(10.0), 0.0))
    ) < 1.0e-9, "module local spin was lost from attachment angular velocity");
}

void testReplicatedHubFrameSharesMapNavigationBasis()
{
    const glm::dvec3 prograde(0.0, 1.0, 0.0);
    const glm::dvec3 radial(1.0, 0.0, 0.0);
    const glm::dvec3 normal(0.0, 0.0, -1.0);
    const glm::mat4 visualOrientation = hubVisualOrientation(
        prograde, radial, normal
    );

    const glm::dvec3 origin(1000.0, -2000.0, 3000.0);
    const auto frame = makeReplicatedHubKinematicFrame(
        2,
        "hub-test",
        origin,
        glm::dvec3(4.0, 5.0, 6.0),
        glm::dvec3(0.0, 0.0, 0.01),
        visualOrientation
    );

    require(frame.valid, "replicated hub frame is invalid");
    require(glm::length(glm::dvec3(frame.localToWorldBasis[0]) - prograde) < 1.0e-9,
        "replicated hub frame changed prograde axis");
    require(glm::length(glm::dvec3(frame.localToWorldBasis[1]) - radial) < 1.0e-9,
        "replicated hub frame changed radial axis");
    require(glm::length(glm::dvec3(frame.localToWorldBasis[2]) - normal) < 1.0e-9,
        "replicated hub frame changed normal axis");

    const glm::dvec3 world = origin +
        prograde * 130.0 + radial * -40.0 + normal * 17.0;
    const glm::dvec3 local = frame.worldToLocalPosition(world);
    const glm::dvec3 restored = frame.localToWorldPosition(local);
    require(glm::length(restored - world) < 1.0e-9,
        "replicated hub frame world/local round trip drifted");
}

void testDockingPathPreservesPhysicalEndpointsAndIngress()
{
    DockingPathRequest request;
    // Start behind the target module so transit must route around the
    // solid OBB before the final authored ingress is allowed through it.
    request.startPositionMeters = glm::dvec3(1400.0, 300.0, 0.0);
    request.dockCenterMeters = glm::dvec3(1000.0, 300.0, 0.0);
    request.dockOutward = glm::dvec3(-1.0, 0.0, 0.0);
    request.approachStandoffMeters = 250.0;
    request.terminalDepthMeters = 25.0;
    request.vehicleSafetyRadiusMeters = 20.0;

    world::navigation::NavigationObstacle target;
    target.id = "target-module";
    target.shape = world::navigation::NavigationObstacleShape::Box;
    target.centerMeters = request.dockCenterMeters;
    target.halfExtentsMeters = glm::dvec3(200.0, 200.0, 200.0);
    request.targetObstacleId = target.id;
    request.obstacles.push_back(target);

    const auto plan = DockingPathPlanner::plan(request);
    require(plan.valid, "docking geometric path was not built");
    require(plan.obstacleDetourUsed,
        "target module was incorrectly ignored during docking transit");
    require(plan.pointsMeters.size() >= 4, "docking geometric path is incomplete");
    require(glm::length(plan.pointsMeters.front() - request.startPositionMeters) < 1.0e-9,
        "docking path does not start at the ship");

    const glm::dvec3 inbound = -glm::normalize(request.dockOutward);
    const glm::dvec3 last = glm::normalize(
        plan.pointsMeters.back() - plan.pointsMeters[plan.pointsMeters.size() - 2]);
    require(glm::dot(last, inbound) > 0.999999,
        "docking ingress is not perpendicular to entrance plane");
    require(glm::length(plan.pointsMeters.back() - plan.terminalPointMeters) < 1.0e-9,
        "docking path does not end at terminal point");
}


void testGeometricPlannerKeepsClearDirectPath()
{
    world::navigation::GeometricPathRequest request;
    request.startMeters = glm::dvec3(-500.0, 20.0, 10.0);
    request.goalMeters = glm::dvec3(700.0, -30.0, 80.0);
    request.params.agentRadiusMeters = 15.0;

    const auto plan = world::navigation::GeometricPathPlanner::plan(request);
    require(plan.valid, "clear geometric path was rejected");
    require(!plan.obstacleDetourUsed, "clear geometric path invented a detour");
    require(plan.pointsMeters.size() == 2, "clear geometric path gained extra waypoints");
    require(glm::length(plan.pointsMeters.front() - request.startMeters) < 1.0e-12,
        "clear geometric path changed its start");
    require(glm::length(plan.pointsMeters.back() - request.goalMeters) < 1.0e-12,
        "clear geometric path changed its goal");
}


void testGeometricPlannerDetoursRotatedObb()
{
    world::navigation::NavigationObstacle obstacle;
    obstacle.id = "rotated-box";
    obstacle.shape = world::navigation::NavigationObstacleShape::Box;
    obstacle.centerMeters = glm::dvec3(0.0, 0.0, 0.0);
    obstacle.halfExtentsMeters = glm::dvec3(180.0, 420.0, 240.0);
    obstacle.localToWorldBasis = glm::dmat3(
        glm::rotate(
            glm::dmat4(1.0),
            glm::radians(35.0),
            glm::dvec3(0.0, 0.0, 1.0)
        )
    );

    world::navigation::GeometricPathRequest request;
    request.startMeters = glm::dvec3(-1200.0, 0.0, 0.0);
    request.goalMeters = glm::dvec3(1200.0, 0.0, 0.0);
    request.params.agentRadiusMeters = 20.0;
    request.obstacles.push_back(obstacle);

    const auto plan = world::navigation::GeometricPathPlanner::plan(request);
    require(plan.valid, "rotated OBB detour was not built");
    require(plan.obstacleDetourUsed, "geometric planner ignored rotated OBB");
    require(plan.pointsMeters.size() >= 3, "rotated OBB detour has no bypass waypoint");
    for (std::size_t i = 1; i < plan.pointsMeters.size(); ++i)
    {
        require(world::navigation::segmentClearOfNavigationObstacles(
            plan.pointsMeters[i - 1],
            plan.pointsMeters[i],
            request.obstacles,
            request.params.agentRadiusMeters),
            "geometric planner emitted a segment through rotated OBB");
    }
}

void testGeometricPlannerUsesSphereBoxAndCapsuleKernel()
{
    world::navigation::NavigationObstacle sphere;
    sphere.id = "sphere";
    sphere.shape = world::navigation::NavigationObstacleShape::Sphere;
    sphere.centerMeters = glm::dvec3(0.0);
    sphere.radiusMeters = 100.0;
    require(world::navigation::segmentIntersectsNavigationObstacle(
        glm::dvec3(-300.0, 0.0, 0.0), glm::dvec3(300.0, 0.0, 0.0), sphere),
        "canonical sphere collision was lost");

    world::navigation::NavigationObstacle box;
    box.id = "box";
    box.shape = world::navigation::NavigationObstacleShape::Box;
    box.centerMeters = glm::dvec3(0.0);
    box.halfExtentsMeters = glm::dvec3(50.0, 120.0, 80.0);
    box.localToWorldBasis = glm::dmat3(
        glm::rotate(glm::dmat4(1.0), glm::radians(45.0), glm::dvec3(0.0, 0.0, 1.0))
    );
    require(world::navigation::segmentIntersectsNavigationObstacle(
        glm::dvec3(-300.0, 0.0, 0.0), glm::dvec3(300.0, 0.0, 0.0), box),
        "canonical OBB collision was lost");

    world::navigation::NavigationObstacle capsule;
    capsule.id = "capsule";
    capsule.shape = world::navigation::NavigationObstacleShape::Capsule;
    capsule.centerMeters = glm::dvec3(0.0);
    capsule.radiusMeters = 60.0;
    capsule.capsuleHalfLengthMeters = 250.0;
    require(world::navigation::segmentIntersectsNavigationObstacle(
        glm::dvec3(-200.0, 0.0, 0.0), glm::dvec3(200.0, 0.0, 0.0), capsule),
        "canonical capsule collision was lost");
    require(!world::navigation::segmentIntersectsNavigationObstacle(
        glm::dvec3(-200.0, 200.0, 0.0), glm::dvec3(200.0, 200.0, 0.0), capsule),
        "canonical capsule collision is over-inflated without clearance");
}


int main()
{
    const struct { const char* name; void (*fn)(); } tests[] = {
        {"module switches are independent", testModuleSwitchesAreIndependent},
        {"sensor fusion preserves safety envelope", testSensorFusionNeverShrinksPhysicalEnvelope},
        {"moving obstacle is checked at passage time", testMovingObstacleIsCheckedAtPassageTime},
        {"scheduled traffic is 4D and expires", testScheduledTrafficIsFourDimensionalAndExpires},
        {"scheduled traffic honors intermediate samples", testScheduledTrafficHonorsIntermediateSamples},
        {"semantic anchor is independent from mesh", testSemanticAnchorIsIndependentFromMesh},
        {"docking compatibility gates route action", testDockingCompatibilityGatesRouteAction},
        {"semantic dock request uses stable identity", testSemanticDockRouteRequestUsesStableIdentity},
        {"diagnostic dock runtime decision cases", testDiagnosticDockRuntimeStatesCoverDecisionCases},
        {"galactic compass uses standard l/b basis", testGalacticCompassUsesStandardLBasis},
        {"guidance priority and expiry", testGuidanceStateChoosesPriorityAndExpiry},
        {"local planner uses predictor and safety", testLocalPlannerUsesPredictorAndSafetyEvaluator},
        {"docking guidance terminal 6-DOF pose", testDockingGuidanceEndsNormalToGateWithRequiredHullPose},
        {"Hub co-moving frame removes orbital tangent", testHubCoMovingFrameRemovesOrbitalTangentFromLocalMotion},
        {"orbital docking converges to co-moving target", testDockingPlannerConvergesAgainstCoMovingOrbitalTarget},
        {"rotating semantic anchor follows circular motion", testRotatingSemanticAnchorPredictsCircularGateMotion},
        {"unsafe docking escapes and rolling replan recovers", testUnsafeDockingPublishesEscapeThenRecoversPrimaryRoute},
        {"Hub-local rotation keeps precision at large universe epoch", testHubLocalRotationKeepsPrecisionAtLargeUniverseEpoch},
        {"planning frame round trips complete kinematic state", testPlanningFrameRoundTripsCompleteKinematicState},
        {"Hub attachment prediction is continuous at large universe epoch", testHubAttachmentPredictionIsContinuousAtLargeUniverseEpoch},
        {"geometric planner is deterministic and input-pure", testGeometricPlannerIsDeterministicAndInputPure},
        {"planning epoch preserves authoritative identity", testNavigationPlanningEpochPreservesAuthoritativeIdentity},
        {"navigation world predictor advances orbital Hub to planning epoch", testNavigationWorldPredictorAdvancesOrbitalHubToPlanningEpoch},
        {"navigation world predictor keeps Hub attachments in one frame", testNavigationWorldPredictorKeepsHubAttachmentsInOneFrame},
        {"replicated Hub frame shares map/navigation basis", testReplicatedHubFrameSharesMapNavigationBasis},
        {"docking path preserves endpoints and ingress", testDockingPathPreservesPhysicalEndpointsAndIngress},
        {"geometric planner keeps clear direct path", testGeometricPlannerKeepsClearDirectPath},
        {"geometric planner detours rotated OBB", testGeometricPlannerDetoursRotatedObb},
        {"geometric collision kernel covers sphere box capsule", testGeometricPlannerUsesSphereBoxAndCapsuleKernel},
    };

    std::size_t passed = 0;
    for (const auto& test : tests)
    {
        try
        {
            test.fn();
            ++passed;
            std::cout << "[PASS] " << test.name << '\n';
        }
        catch (const std::exception& error)
        {
            std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n';
        }
    }

    std::cout << passed << "/" << (sizeof(tests) / sizeof(tests[0]))
              << " navigation guidance tests passed\n";
    return passed == (sizeof(tests) / sizeof(tests[0]))
        ? EXIT_SUCCESS
        : EXIT_FAILURE;
}
