#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "src/game/navigation/DockingCompatibility.h"
#include "src/game/navigation/DockingPortRuntimeStateCatalog.h"
#include "src/game/navigation/DockingRouteRequest.h"
#include "src/game/navigation/GalacticReferenceFrame.h"
#include "src/game/navigation/GuidanceCorridor.h"
#include "src/game/navigation/HubSemanticAnchor.h"
#include "src/game/navigation/LocalGuidancePlanner.h"
#include "src/game/navigation/NavigationModuleState.h"
#include "src/game/navigation/NavigationPlanningSnapshot.h"
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
    NavigationObstacle official;
    official.id = "debris-7";
    official.physicalRadiusMeters = 20.0;
    official.requiredClearanceMeters = 60.0;
    official.positionUncertaintyMeters = 100.0;
    official.source = NavigationKnowledgeSource::AuthoritativeWorld;
    base.obstacles.push_back(official);

    NavigationObstacle radar = official;
    radar.positionMeters = glm::dvec3(1000.0, 0.0, 0.0);
    radar.physicalRadiusMeters = 5.0;
    radar.requiredClearanceMeters = 10.0;
    radar.positionUncertaintyMeters = 2.0;
    radar.source = NavigationKnowledgeSource::Radar;

    NavigationPlanningSnapshotBuilder builder(base);
    builder.mergeObstacleObservation(radar);
    auto merged = std::move(builder).build();

    require(merged.obstacles.size() == 1, "fusion duplicated obstacle identity");
    const auto& result = merged.obstacles.front();
    require(near(result.positionUncertaintyMeters, 2.0), "radar did not refine position uncertainty");
    require(near(result.physicalRadiusMeters, 20.0), "radar shrank authoritative physical radius");
    require(near(result.requiredClearanceMeters, 60.0), "radar shrank authoritative clearance");
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
    NavigationObstacle obstacle;
    obstacle.id = "crossing-rock";
    obstacle.systemId = 0;
    obstacle.epochUniverseTimeSeconds = 100.0;
    obstacle.positionMeters = glm::dvec3(50.0, 50.0, 0.0);
    obstacle.velocityMps = glm::dvec3(0.0, -10.0, 0.0);
    obstacle.physicalRadiusMeters = 2.0;
    obstacle.requiredClearanceMeters = 2.0;
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

    NavigationObstacle blocking;
    blocking.id = "blocking-debris";
    blocking.systemId = 0;
    blocking.epochUniverseTimeSeconds = 100.0;
    blocking.positionMeters = glm::dvec3(100.0, 0.0, 0.0);
    blocking.physicalRadiusMeters = 25.0;
    blocking.requiredClearanceMeters = 10.0;
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

} // namespace

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
