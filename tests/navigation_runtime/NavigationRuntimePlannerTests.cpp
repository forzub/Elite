#include "src/game/navigation/NavigationRuntimePlanner.h"
#include "src/game/navigation/NavigationHitVolumeAdapter.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{

using Planner = game::navigation::NavigationRuntimePlanner;
using Map = world::navigation::NavigationMap;
using Space = world::navigation::NavigationSpace;
using Bridge = game::navigation::NavigationRuntimeControlBridge;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

bool near(double a, double b, double tolerance = 1.0e-6)
{
    return std::abs(a - b) <= tolerance;
}

Space::RegionInput region(
    Space::RegionId id,
    double cx,
    double cy,
    double hx,
    double hy,
    double clearance = 20.0
)
{
    Space::RegionInput out;
    out.regionId = id;
    out.boundsMapMeters.minMapMeters = {cx - hx, cy - hy, -10.0};
    out.boundsMapMeters.maxMapMeters = {cx + hx, cy + hy, 10.0};
    out.clearanceRadiusMeters = clearance;
    out.geometryRevision = 1;
    return out;
}

Space::PortalInput portal(
    Space::PortalId id,
    Space::RegionId a,
    Space::RegionId b,
    double x,
    double y,
    double clearance
)
{
    Space::PortalInput out;
    out.portalId = id;
    out.regionA = a;
    out.regionB = b;
    out.centerMapMeters = {x, y, 0.0};
    out.clearanceRadiusMeters = clearance;
    out.bidirectional = true;
    out.geometryRevision = 1;
    return out;
}

Space forcedDetourSpace()
{
    Space space;
    Space::StaticSpaceUpdate update;
    update.sourceRevision = 100;
    update.regions = {
        region(1, 0.0, 0.0, 5.0, 15.0),
        region(2, 10.0, 10.0, 5.0, 5.0),
        region(3, 20.0, 0.0, 5.0, 15.0)
    };
    update.portals = {
        portal(11, 1, 2, 5.0, 10.0, 4.0),
        portal(12, 2, 3, 15.0, 10.0, 4.0)
    };
    space.replaceStaticWorld(std::move(update));
    return space;
}

Space singleRegionSpace()
{
    Space space;
    Space::StaticSpaceUpdate update;
    update.sourceRevision = 200;
    update.regions = {
        region(1, 25.0, 0.0, 35.0, 50.0, 30.0)
    };
    space.replaceStaticWorld(std::move(update));
    return space;
}

Planner::AgentState baseAgent()
{
    Planner::AgentState agent;
    agent.entityId = 100;
    agent.positionMapMeters = {0.0, 0.0, 0.0};
    agent.velocityMapMetersPerSecond = {0.0, 0.0, 0.0};
    agent.accelerationMapMetersPerSecond2 = {0.0, 0.0, 0.0};
    agent.radiusMeters = 1.0;
    return agent;
}

Planner::Goal goalAt(double x, double y = 0.0)
{
    Planner::Goal goal;
    goal.revision = 42;
    goal.targetPositionMapMeters = {x, y, 0.0};
    goal.maximumTargetSpeedMps = 12.0;
    goal.velocityResponsePerSecond = 1.0;
    goal.angularDampingPerSecond = 2.0;
    goal.arrivalRadiusMeters = 0.5;
    return goal;
}

Planner::Policy basePolicy()
{
    Planner::Policy policy;
    policy.horizon.lookAheadSeconds = 3.0;
    policy.horizon.maxResultAgeSeconds = 0.25;
    policy.horizon.maxBrakingAccelerationMetersPerSecond2 = 8.0;
    policy.horizon.turnDistanceMeters = 0.0;
    policy.horizon.safetyMarginMeters = 1.0;
    policy.horizon.minimumHorizonMeters = 10.0;

    policy.avoidance.primaryDeflectionRadians = 0.2617993877991494;
    policy.avoidance.secondaryDeflectionRadians = 0.5235987755982988;
    policy.avoidance.azimuthSamples = 8;
    policy.avoidance.staticAdditionalClearanceMeters = 0.0;
    return policy;
}

Map::QueryResult emptyDynamic()
{
    Map::QueryResult result;
    result.mapRevision = 7;
    result.sourceRevision = 8;
    return result;
}

Bridge::PilotSkillProfile expertProfile()
{
    Bridge::PilotSkillProfile profile;
    profile.execution.reactionDelaySeconds = 0.0;
    profile.execution.perceptionDecisionRateHz = 100.0;
    profile.execution.commandLatencySeconds = 0.0;
    profile.execution.responseFrequencyHz = 4.0;
    profile.execution.dampingRatio = 1.0;
    profile.execution.commandGain = 1.0;
    profile.execution.maxLinearCommandSlewMetersPerSec3 = 1000.0;
    profile.execution.maxAngularCommandSlewRadPerSec3 = 1000.0;
    return profile;
}

void testHitVolumeAdapterUsesAuthoritativeLocalObb()
{
    game::damage::HitComponent hitComponent;

    game::damage::HitVolume primary;
    primary.center = glm::vec3(2.0f, 0.0f, 0.0f);
    primary.halfSize = glm::vec3(1.0f, 2.0f, 3.0f);
    primary.orientation = glm::mat3(1.0f);
    hitComponent.volumes.push_back(primary);

    game::damage::HitVolume support = primary;
    support.center = glm::vec3(100.0f, 0.0f, 0.0f);
    support.supportLinkVolume = true;
    hitComponent.volumes.push_back(support);

    game::damage::HitVolume destroyed = primary;
    destroyed.center = glm::vec3(200.0f, 0.0f, 0.0f);
    destroyed.destroyed = true;
    hitComponent.volumes.push_back(destroyed);

    glm::dmat3 ownerBasis(1.0);
    ownerBasis[0] = glm::dvec3(0.0, 1.0, 0.0);
    ownerBasis[1] = glm::dvec3(-1.0, 0.0, 0.0);
    ownerBasis[2] = glm::dvec3(0.0, 0.0, 1.0);

    const auto obstacles =
        game::navigation::NavigationHitVolumeAdapter::buildObstacles(
            hitComponent,
            77,
            glm::dvec3(10.0, 20.0, 30.0),
            ownerBasis,
            "fixture"
        );

    require(obstacles.size() == 1,
            "destroyed/support hit volumes must not become navigation solids");

    const auto& obstacle = obstacles.front();
    require(obstacle.entityId == 77,
            "hit-volume navigation obstacle lost entity identity");
    require(obstacle.shape ==
                world::navigation::NavigationObstacleShape::Box,
            "HitVolume OBB must remain a box navigation obstacle");
    require(near(obstacle.centerMeters.x, 10.0) &&
            near(obstacle.centerMeters.y, 22.0) &&
            near(obstacle.centerMeters.z, 30.0),
            "local HitVolume center was not transformed by object pose");
    require(near(obstacle.halfExtentsMeters.x, 1.0) &&
            near(obstacle.halfExtentsMeters.y, 2.0) &&
            near(obstacle.halfExtentsMeters.z, 3.0),
            "HitVolume half extents changed at navigation boundary");
    require(near(obstacle.localToWorldBasis[0].x, 0.0) &&
            near(obstacle.localToWorldBasis[0].y, 1.0),
            "HitVolume OBB orientation was not composed with object basis");

    const double expectedRadius =
        2.0 + std::sqrt(14.0);
    const double radius =
        game::navigation::NavigationHitVolumeAdapter::
            conservativeRadiusFromOrigin(hitComponent);

    require(near(radius, expectedRadius),
            "broadphase radius must contain the authoritative local HitVolume");
}

void testStaticCorridorBecomesLivePortalWaypoint()
{
    Space space = forcedDetourSpace();
    Planner::AgentState agent = baseAgent();
    Planner::Goal goal = goalAt(20.0);
    Planner::Policy policy = basePolicy();

    const Planner::Result result = Planner::plan(
        agent,
        goal,
        emptyDynamic(),
        0.0,
        space,
        policy
    );

    require(
        result.status == Planner::Status::NominalClear,
        "detour fixture must produce a live bounded target"
    );
    require(result.usedPortalWaypoint,
            "multi-region route must steer toward the first selected portal");
    require(result.staticRegionPath ==
                std::vector<Space::RegionId>({1, 2, 3}),
            "runtime planner changed NavigationSpace region route");
    require(result.staticPortalPath ==
                std::vector<Space::PortalId>({11, 12}),
            "runtime planner changed NavigationSpace portal route");
    require(result.staticPortalCentersMapMeters.size() == 2,
            "runtime planner lost compact portal centers");
    require(near(result.coarseWaypointMapMeters.x, 5.0) &&
            near(result.coarseWaypointMapMeters.y, 10.0),
            "first selected portal must be the coarse live waypoint");
    require(result.safeProgressTargetDemonstrated,
            "clear bounded local target must be explicitly demonstrated");
    require(result.intent.idealLinearAccelerationDemandMapMps2.y > 0.0,
            "forced detour must create a real lateral acceleration demand");
}

void testSamePortalRejectsOversizedHull()
{
    Space space = forcedDetourSpace();
    Planner::AgentState small = baseAgent();
    Planner::Goal goal = goalAt(20.0);
    Planner::Policy policy = basePolicy();

    const Planner::Result accepted = Planner::plan(
        small,
        goal,
        emptyDynamic(),
        0.0,
        space,
        policy
    );
    require(accepted.status == Planner::Status::NominalClear,
            "small hull must fit the authored portal route");

    Planner::AgentState oversized = small;
    oversized.radiusMeters = 4.01;

    const Planner::Result rejected = Planner::plan(
        oversized,
        goal,
        emptyDynamic(),
        0.0,
        space,
        policy
    );
    require(rejected.status == Planner::Status::StaticHold,
            "oversized hull must fail closed at the same static corridor");
    require(!rejected.safeProgressTargetDemonstrated,
            "oversized hull must not claim safe progress");
    const auto& held =
        rejected.intent.idealLinearAccelerationDemandMapMps2;
    require(std::abs(held.x) <= 1.0e-12 &&
            std::abs(held.y) <= 1.0e-12 &&
            std::abs(held.z) <= 1.0e-12,
            "stationary static hold must not invent translation demand");
}

void testExactStaticObstacleParticipatesInRuntimeComposition()
{
    Space space;
    Space::StaticSpaceUpdate update;
    update.sourceRevision = 205;
    update.regions = {
        region(1, 10.0, 0.0, 20.0, 20.0, 20.0)
    };

    world::navigation::NavigationObstacle wall;
    wall.id = "runtime_static_wall";
    wall.entityId = 333;
    wall.shape =
        world::navigation::NavigationObstacleShape::Box;
    wall.centerMeters = {8.0, 0.0, 0.0};
    wall.localToWorldBasis = glm::dmat3(1.0);
    wall.halfExtentsMeters = {1.0, 0.25, 2.0};
    update.obstacles.push_back(wall);

    space.replaceStaticWorld(std::move(update));

    const Planner::Result result = Planner::plan(
        baseAgent(),
        goalAt(20.0),
        emptyDynamic(),
        0.0,
        space,
        basePolicy()
    );

    require(result.status == Planner::Status::AdjustedClear,
            "static-only OBB blocker must participate in runtime composition");
    require(result.adjustedTarget,
            "runtime composition must own the exact-static adjusted target");
    require(result.nominalStaticBlocked,
            "runtime planner must surface nominal exact-static rejection");
    require(result.nominalStaticObstacleId == "runtime_static_wall" &&
            result.nominalStaticObstacleEntityId == 333,
            "runtime planner lost exact static blocker identity");
    require(result.staticObstaclesExamined > 0,
            "runtime planner must expose exact static query work");
    require(result.nominalDynamicConflictsFound == 0,
            "static-only fixture must not fabricate a dynamic conflict");
}

void testLiveScaleStaticObstacleInsideFirstBoundedHorizon()
{
    Space space;
    Space::StaticSpaceUpdate staticWorld;
    staticWorld.sourceRevision = 206;

    Space::RegionInput regionInput;
    regionInput.regionId = 1;
    regionInput.boundsMapMeters.minMapMeters = {
        -1000.0, -4000.0, -4000.0
    };
    regionInput.boundsMapMeters.maxMapMeters = {
        8000.0, 4000.0, 4000.0
    };
    regionInput.clearanceRadiusMeters = 12000.0;
    regionInput.geometryRevision = 1;
    staticWorld.regions.push_back(regionInput);

    world::navigation::NavigationObstacle cube;
    cube.id = "live_scale_cube_08";
    cube.entityId = 808;
    cube.shape =
        world::navigation::NavigationObstacleShape::Box;
    cube.centerMeters = {1300.0, 0.0, 0.0};
    cube.localToWorldBasis = glm::dmat3(1.0);
    cube.halfExtentsMeters = {180.0, 180.0, 450.0};
    staticWorld.obstacles.push_back(cube);

    space.replaceStaticWorld(std::move(staticWorld));

    Planner::AgentState agent = baseAgent();
    agent.radiusMeters = 20.0;

    Planner::Goal goal = goalAt(7200.0);
    goal.maximumTargetSpeedMps = 60.0;

    Planner::Policy policy = basePolicy();
    policy.horizon.maxBrakingAccelerationMetersPerSecond2 = 8.0;
    policy.horizon.turnDistanceMeters = 1400.0;
    policy.horizon.safetyMarginMeters = 20.0;
    policy.horizon.minimumHorizonMeters = 100.0;
    policy.avoidance.staticAdditionalClearanceMeters = 10.0;

    Map::QueryResult dynamic = emptyDynamic();

    Map::Candidate unrelated;
    unrelated.entityId = 909;
    unrelated.positionMapMeters = {0.0, 3000.0, 0.0};
    unrelated.velocityMapMetersPerSecond = {0.0, 0.0, 0.0};
    unrelated.accelerationMapMetersPerSecond2 = {0.0, 0.0, 0.0};
    unrelated.predictedEndPositionMapMeters = unrelated.positionMapMeters;
    unrelated.conservativeSweptCenterMapMeters = unrelated.positionMapMeters;
    unrelated.actorRadiusMeters = 50.0;
    unrelated.conservativeSweptRadiusMeters = 50.0;
    unrelated.motionRevision = 1;
    dynamic.candidates.push_back(unrelated);

    const Planner::Result result = Planner::plan(
        agent,
        goal,
        dynamic,
        0.0,
        space,
        policy
    );

    require(result.nominalStaticBlocked,
            "live-scale exact OBB inside first bounded horizon must reject nominal segment");
    require(result.nominalStaticObstacleEntityId == 808,
            "live-scale exact OBB blocker identity must survive composition");
    require(result.status == Planner::Status::AdjustedClear,
            "live-scale exact OBB must produce a safe adjusted target");
    require(result.adjustedTarget,
            "live-scale exact OBB must exercise adjusted local avoidance");
    require(result.nominalDynamicConflictsFound == 0,
            "unrelated dynamic actor must not become the cause of avoidance");
}

void testAdjustedTargetPreservesNominalConflictIdentity()
{
    Map::Config config;
    config.halfExtentMeters = 2000.0;
    config.cellSizeMeters = 100.0;
    config.predictionHorizonSeconds = 3.0;
    config.interactionMarginMeters = 0.0;

    Map map(config);
    Map::DynamicWorldUpdate update;
    update.sourceRevision = 92;

    Map::DynamicActorInput obstacle;
    obstacle.entityId = 201;
    obstacle.positionSystemMeters = {500.0, 0.0, 0.0};
    obstacle.velocitySystemMetersPerSecond = {0.0, 0.0, 0.0};
    obstacle.accelerationSystemMetersPerSecond2 = {0.0, 0.0, 0.0};
    obstacle.radiusMeters = 50.0;
    obstacle.motionRevision = 4;
    update.actors.push_back(obstacle);
    map.replaceDynamicWorld(std::move(update));

    Map::CorridorQuery query;
    query.startMapMeters = {0.0, 0.0, 0.0};
    query.endMapMeters = {1000.0, 0.0, 0.0};
    query.radiusMeters = 10.0;
    const Map::QueryResult dynamic = map.queryCorridor(query);

    Space space;
    Space::StaticSpaceUpdate staticWorld;
    staticWorld.sourceRevision = 201;
    staticWorld.regions = {
        region(1, 500.0, 0.0, 1000.0, 1000.0, 1000.0)
    };
    space.replaceStaticWorld(std::move(staticWorld));

    Planner::AgentState agent = baseAgent();
    agent.radiusMeters = 5.0;

    Planner::Goal goal = goalAt(1000.0);
    goal.maximumTargetSpeedMps = 20.0;

    Planner::Policy policy = basePolicy();
    policy.horizon.turnDistanceMeters = 600.0;
    policy.horizon.minimumHorizonMeters = 600.0;
    policy.horizon.safetyMarginMeters = 5.0;

    const Planner::Result result = Planner::plan(
        agent,
        goal,
        dynamic,
        0.0,
        space,
        policy
    );

    require(result.status == Planner::Status::AdjustedClear,
            "fixture must produce a safe adjusted target");
    require(result.adjustedTarget,
            "adjusted target flag must survive runtime composition");
    require(result.nominalPrimaryConflictEntityId == 201,
            "adjusted target must retain the obstacle that rejected the nominal route");
    require(result.nominalDynamicConflictsFound > 0,
            "adjusted target must retain nominal conflict count");
    require(result.primaryConflictEntityId == 0,
            "final safe adjusted probe must not pretend it is still in conflict");
}

void testNavigationMapCrossingConflictProducesBrakingHold()
{
    Map::Config config;
    config.halfExtentMeters = 1000.0;
    config.cellSizeMeters = 50.0;
    config.predictionHorizonSeconds = 3.0;
    config.interactionMarginMeters = 0.0;

    Map map(config);
    Map::DynamicWorldUpdate update;
    update.sourceRevision = 91;

    Map::DynamicActorInput crossing;
    crossing.entityId = 200;
    crossing.positionSystemMeters = {20.0, 20.0, 0.0};
    crossing.velocitySystemMetersPerSecond = {0.0, -10.0, 0.0};
    crossing.accelerationSystemMetersPerSecond2 = {0.0, 0.0, 0.0};
    crossing.radiusMeters = 2.0;
    crossing.motionRevision = 3;
    update.actors.push_back(crossing);
    map.replaceDynamicWorld(std::move(update));

    Map::CorridorQuery query;
    query.startMapMeters = {0.0, 0.0, 0.0};
    query.endMapMeters = {50.0, 0.0, 0.0};
    query.radiusMeters = 40.0;
    const Map::QueryResult dynamic = map.queryCorridor(query);
    require(!dynamic.candidates.empty(),
            "NavigationMap must publish the crossing actor to the live planner");

    Space space = singleRegionSpace();
    Planner::AgentState agent = baseAgent();
    agent.velocityMapMetersPerSecond = {10.0, 0.0, 0.0};
    Planner::Goal goal = goalAt(50.0);

    const Planner::Result result = Planner::plan(
        agent,
        goal,
        dynamic,
        0.0,
        space,
        basePolicy()
    );

    require(result.status == Planner::Status::ConflictHold,
            "predicted crossing conflict must fail closed when no safe probe exists");
    require(result.primaryConflictEntityId == 200,
            "live planner must preserve NavigationMap conflict identity");
    require(result.intent.emergency,
            "conflict hold must be marked as urgent pilot execution");
    require(result.intent.hazardUrgency01 >= 0.99,
            "conflict hold must carry maximum local hazard urgency");
    require(result.intent.idealLinearAccelerationDemandMapMps2.x < 0.0,
            "moving conflict must create a real braking demand");
}

void testMovingGapPrecisionProbeUsesRuntimeCandidates()
{
    Map::Config config;
    config.halfExtentMeters = 1000.0;
    config.cellSizeMeters = 50.0;
    config.predictionHorizonSeconds = 3.0;
    config.interactionMarginMeters = 0.0;

    Map map(config);
    Map::DynamicWorldUpdate update;
    update.sourceRevision = 120;

    Map::DynamicActorInput upper;
    upper.entityId = 301;
    upper.positionSystemMeters = {20.0, 2.5, 0.0};
    upper.velocitySystemMetersPerSecond = {1.0, 0.0, 0.0};
    upper.angularVelocitySystemRadPerSecond = {0.0, 0.0, 1.0};
    upper.radiusMeters = 2.0;
    upper.motionRevision = 11;
    update.actors.push_back(upper);

    Map::DynamicActorInput lower;
    lower.entityId = 302;
    lower.positionSystemMeters = {20.0, -2.5, 0.0};
    lower.velocitySystemMetersPerSecond = {1.0, 0.0, 0.0};
    lower.angularVelocitySystemRadPerSecond = {0.0, 0.0, -1.0};
    lower.radiusMeters = 2.0;
    lower.motionRevision = 12;
    update.actors.push_back(lower);

    map.replaceDynamicWorld(std::move(update));

    Map::CorridorQuery query;
    query.startMapMeters = {0.0, 0.0, 0.0};
    query.endMapMeters = {50.0, 0.0, 0.0};
    query.radiusMeters = 20.0;
    const Map::QueryResult dynamic = map.queryCorridor(query);
    require(dynamic.candidates.size() == 2,
            "moving-gap fixture must publish both dynamic boundaries");

    Space space = singleRegionSpace();

    Planner::AgentState agent = baseAgent();
    agent.radiusMeters = 1.0;
    agent.forwardMap = {1.0, 0.0, 0.0};
    agent.upMap = {0.0, 0.0, 1.0};
    agent.rightMap = {0.0, 1.0, 0.0};
    agent.hullHalfExtentsBodyMeters = {0.4, 0.4, 0.4};
    agent.linearCapability.maxForwardAccelerationMetersPerSec2 = 30.0;
    agent.linearCapability.maxReverseAccelerationMetersPerSec2 = 30.0;
    agent.linearCapability.maxLateralAccelerationMetersPerSec2 = 30.0;
    agent.linearCapability.maxVerticalAccelerationMetersPerSec2 = 30.0;
    agent.angularCapability.maxAngularAccelerationRadPerSec2 = 10.0;
    agent.angularCapability.maxAngularSpeedRadPerSec = 10.0;

    Planner::Goal goal = goalAt(50.0);
    goal.maximumTargetSpeedMps = 10.0;

    Planner::Policy policy = basePolicy();
    policy.horizon.minimumHorizonMeters = 30.0;
    policy.horizon.turnDistanceMeters = 0.0;
    policy.horizon.safetyMarginMeters = 0.0;

    policy.movingPassage.enabled = true;
    policy.movingPassage.durationSeconds = 3.0;
    policy.movingPassage.candidates.minimumForwardDistanceMeters = 0.0;
    policy.movingPassage.candidates.maximumForwardDistanceMeters = 100.0;
    policy.movingPassage.candidates.maximumCenterlineOffsetMeters = 10.0;
    policy.movingPassage.candidates.secondaryClearanceMeters = 4.0;
    policy.movingPassage.prediction.secondaryClearanceMeters = 4.0;

    const Planner::Result result = Planner::plan(
        agent,
        goal,
        dynamic,
        0.0,
        space,
        policy
    );

    require(result.nominalDynamicConflictsFound > 0,
            "moving-gap precision probe requires a real nominal dynamic conflict");
    require(result.movingPrecisionAttempted,
            "runtime planner must enter bounded moving precision after conflict");
    require(result.movingGapCandidatesBuilt == 1,
            "two-boundary fixture must produce exactly one bounded gap candidate");
    require(result.movingGapPredictionsEvaluated == 1,
            "runtime planner must consume the moving-gap predictor");
    require(result.movingPassagesEvaluated == 1,
            "open moving gap must reach continuous moving-passage evaluation");
    require(result.movingPassageFeasible,
            "open translating gap must be continuously feasible for the test hull");
    require(
        (result.movingPrimaryObstacleEntityId == 301 &&
         result.movingSecondaryObstacleEntityId == 302) ||
        (result.movingPrimaryObstacleEntityId == 302 &&
         result.movingSecondaryObstacleEntityId == 301),
        "moving precision lost the real NavigationMap boundary identities"
    );
    require(glm::length(result.movingPassageInitialAccelerationMapMps2) > 0.0,
            "feasible moving passage must publish the verified first control sample");
    require(result.movingPassageStaticProofAttempted,
            "feasible moving passage must enter exact-static same-trajectory proof");
    require(result.movingPassageStaticSafe,
            "empty static fixture must accept the exact moving Hermite trajectory");
    require(result.movingPassageStaticIntervalsProven ==
                Planner::MovingPassage::kIntervals,
            "exact-static moving proof must cover all 32 Hermite intervals");
    require(result.movingPassageStaticBlockingObstacleId.empty() &&
            result.movingPassageStaticBlockingObstacleEntityId == 0,
            "clear exact-static moving proof must not fabricate a blocker");
    require(!result.movingPassageAuthorityUsed &&
            result.status != Planner::Status::MovingPassageClear,
            "precision evaluation alone must remain observe-only without explicit authority opt-in");
}

void testClosingMovingGapFailsClosedBeforePassageEvaluation()
{
    Map::Config config;
    config.halfExtentMeters = 1000.0;
    config.cellSizeMeters = 50.0;
    config.predictionHorizonSeconds = 3.0;
    config.interactionMarginMeters = 0.0;

    Map map(config);
    Map::DynamicWorldUpdate update;
    update.sourceRevision = 121;

    Map::DynamicActorInput upper;
    upper.entityId = 311;
    upper.positionSystemMeters = {20.0, 2.5, 0.0};
    upper.velocitySystemMetersPerSecond = {0.0, -1.0, 0.0};
    upper.radiusMeters = 2.0;
    upper.motionRevision = 21;
    update.actors.push_back(upper);

    Map::DynamicActorInput lower;
    lower.entityId = 312;
    lower.positionSystemMeters = {20.0, -2.5, 0.0};
    lower.velocitySystemMetersPerSecond = {0.0, 1.0, 0.0};
    lower.radiusMeters = 2.0;
    lower.motionRevision = 22;
    update.actors.push_back(lower);

    map.replaceDynamicWorld(std::move(update));

    Map::CorridorQuery query;
    query.startMapMeters = {0.0, 0.0, 0.0};
    query.endMapMeters = {50.0, 0.0, 0.0};
    query.radiusMeters = 20.0;
    const Map::QueryResult dynamic = map.queryCorridor(query);

    Space space = singleRegionSpace();

    Planner::AgentState agent = baseAgent();
    agent.radiusMeters = 1.0;
    agent.forwardMap = {1.0, 0.0, 0.0};
    agent.upMap = {0.0, 0.0, 1.0};
    agent.rightMap = {0.0, 1.0, 0.0};
    agent.hullHalfExtentsBodyMeters = {0.4, 0.4, 0.4};
    agent.linearCapability.maxForwardAccelerationMetersPerSec2 = 30.0;
    agent.linearCapability.maxReverseAccelerationMetersPerSec2 = 30.0;
    agent.linearCapability.maxLateralAccelerationMetersPerSec2 = 30.0;
    agent.linearCapability.maxVerticalAccelerationMetersPerSec2 = 30.0;
    agent.angularCapability.maxAngularAccelerationRadPerSec2 = 10.0;
    agent.angularCapability.maxAngularSpeedRadPerSec = 10.0;

    Planner::Policy policy = basePolicy();
    policy.horizon.minimumHorizonMeters = 30.0;
    policy.horizon.safetyMarginMeters = 0.0;
    policy.movingPassage.enabled = true;
    policy.movingPassage.allowSteeringAuthority = true;
    policy.movingPassage.durationSeconds = 3.0;
    policy.movingPassage.candidates.maximumForwardDistanceMeters = 100.0;
    policy.movingPassage.candidates.maximumCenterlineOffsetMeters = 10.0;
    policy.movingPassage.candidates.secondaryClearanceMeters = 4.0;
    policy.movingPassage.prediction.secondaryClearanceMeters = 4.0;

    const Planner::Result result = Planner::plan(
        agent,
        goalAt(50.0),
        dynamic,
        0.0,
        space,
        policy
    );

    require(result.nominalDynamicConflictsFound > 0,
            "closing-gap fixture must be a real dynamic conflict");
    require(result.movingPrecisionAttempted,
            "closing gap must enter the same bounded precision path");
    require(result.movingGapCandidatesBuilt == 1,
            "closing-gap fixture must still discover the current aperture");
    require(result.movingGapPredictionsEvaluated == 1,
            "closing gap must be rejected by time prediction, not pair discovery");
    require(result.movingPassagesEvaluated == 0,
            "closed-in-horizon gap must never reach ship passage acceptance");
    require(!result.movingPassageFeasible,
            "closing gap must fail closed");
    require(!result.movingPassageAuthorityUsed &&
            result.status != Planner::Status::MovingPassageClear,
            "closing gap must never gain moving-passage steering authority");
}


void testStaticObstacleRejectsSameAcceptedMovingHermiteTrajectory()
{
    Map::Config config;
    config.halfExtentMeters = 1000.0;
    config.cellSizeMeters = 50.0;
    config.predictionHorizonSeconds = 3.0;
    config.interactionMarginMeters = 0.0;

    Map map(config);
    Map::DynamicWorldUpdate update;
    update.sourceRevision = 122;

    Map::DynamicActorInput upper;
    upper.entityId = 321;
    upper.positionSystemMeters = {20.0, 2.5, 0.0};
    upper.velocitySystemMetersPerSecond = {1.0, 0.0, 0.0};
    upper.angularVelocitySystemRadPerSecond = {0.0, 0.0, 1.0};
    upper.radiusMeters = 2.0;
    upper.motionRevision = 31;
    update.actors.push_back(upper);

    Map::DynamicActorInput lower;
    lower.entityId = 322;
    lower.positionSystemMeters = {20.0, -2.5, 0.0};
    lower.velocitySystemMetersPerSecond = {1.0, 0.0, 0.0};
    lower.angularVelocitySystemRadPerSecond = {0.0, 0.0, -1.0};
    lower.radiusMeters = 2.0;
    lower.motionRevision = 32;
    update.actors.push_back(lower);

    map.replaceDynamicWorld(std::move(update));

    Map::CorridorQuery query;
    query.startMapMeters = {0.0, 0.0, 0.0};
    query.endMapMeters = {50.0, 0.0, 0.0};
    query.radiusMeters = 20.0;
    const Map::QueryResult dynamic = map.queryCorridor(query);

    Space space;
    Space::StaticSpaceUpdate staticWorld;
    staticWorld.sourceRevision = 202;
    staticWorld.regions = {
        region(1, 25.0, 0.0, 35.0, 50.0, 30.0)
    };

    world::navigation::NavigationObstacle beam;
    beam.id = "moving_passage_static_beam";
    beam.entityId = 909;
    beam.shape = world::navigation::NavigationObstacleShape::Box;
    beam.centerMeters = {10.0, 0.0, 0.0};
    beam.localToWorldBasis = glm::dmat3(1.0);
    beam.halfExtentsMeters = {0.10, 0.10, 1.0};
    staticWorld.obstacles.push_back(beam);
    space.replaceStaticWorld(std::move(staticWorld));

    Planner::AgentState agent = baseAgent();
    agent.radiusMeters = 1.0;
    agent.forwardMap = {1.0, 0.0, 0.0};
    agent.upMap = {0.0, 0.0, 1.0};
    agent.rightMap = {0.0, 1.0, 0.0};
    agent.hullHalfExtentsBodyMeters = {0.4, 0.4, 0.4};
    agent.linearCapability.maxForwardAccelerationMetersPerSec2 = 30.0;
    agent.linearCapability.maxReverseAccelerationMetersPerSec2 = 30.0;
    agent.linearCapability.maxLateralAccelerationMetersPerSec2 = 30.0;
    agent.linearCapability.maxVerticalAccelerationMetersPerSec2 = 30.0;
    agent.angularCapability.maxAngularAccelerationRadPerSec2 = 10.0;
    agent.angularCapability.maxAngularSpeedRadPerSec = 10.0;

    Planner::Goal goal = goalAt(50.0);
    goal.maximumTargetSpeedMps = 10.0;

    Planner::Policy policy = basePolicy();
    policy.horizon.minimumHorizonMeters = 30.0;
    policy.horizon.turnDistanceMeters = 0.0;
    policy.horizon.safetyMarginMeters = 0.0;
    policy.movingPassage.enabled = true;
    policy.movingPassage.allowSteeringAuthority = true;
    policy.movingPassage.durationSeconds = 3.0;
    policy.movingPassage.candidates.minimumForwardDistanceMeters = 0.0;
    policy.movingPassage.candidates.maximumForwardDistanceMeters = 100.0;
    policy.movingPassage.candidates.maximumCenterlineOffsetMeters = 10.0;
    policy.movingPassage.candidates.secondaryClearanceMeters = 4.0;
    policy.movingPassage.prediction.secondaryClearanceMeters = 4.0;

    const Planner::Result result = Planner::plan(
        agent,
        goal,
        dynamic,
        0.0,
        space,
        policy
    );

    require(result.nominalDynamicConflictsFound > 0,
            "static-blocker fixture still requires the real moving conflict");
    require(result.movingPassageFeasible,
            "dynamic moving aperture must remain feasible before static composition");
    require(result.movingPassageStaticProofAttempted,
            "accepted moving Hermite curve must be submitted to exact-static proof");
    require(!result.movingPassageStaticSafe,
            "static beam crossing the accepted moving curve must fail closed");
    require(result.movingPassageStaticBlockingObstacleId ==
                "moving_passage_static_beam" &&
            result.movingPassageStaticBlockingObstacleEntityId == 909,
            "exact-static moving proof lost blocking HitVolume identity");
    require(result.movingPassageStaticIntervalsProven <
                Planner::MovingPassage::kIntervals,
            "static blocker must stop proof before all intervals are accepted");
    require(result.movingPassageStaticObstaclesExamined > 0,
            "static moving-trajectory proof must expose exact obstacle query work");
    require(!result.movingPassageAuthorityUsed &&
            result.status != Planner::Status::MovingPassageClear,
            "exact-static rejection must veto moving-passage steering authority");
}


void testDoublyProvenMovingPassageTakesAuthorityThroughPilotBridge()
{
    Map::Config config;
    config.halfExtentMeters = 1000.0;
    config.cellSizeMeters = 50.0;
    config.predictionHorizonSeconds = 3.0;
    config.interactionMarginMeters = 0.0;

    Map map(config);
    Map::DynamicWorldUpdate update;
    update.sourceRevision = 123;

    Map::DynamicActorInput upper;
    upper.entityId = 331;
    upper.positionSystemMeters = {20.0, 2.5, 0.0};
    upper.velocitySystemMetersPerSecond = {1.0, 0.0, 0.0};
    upper.angularVelocitySystemRadPerSecond = {0.0, 0.0, 1.0};
    upper.radiusMeters = 2.0;
    upper.motionRevision = 41;
    update.actors.push_back(upper);

    Map::DynamicActorInput lower;
    lower.entityId = 332;
    lower.positionSystemMeters = {20.0, -2.5, 0.0};
    lower.velocitySystemMetersPerSecond = {1.0, 0.0, 0.0};
    lower.angularVelocitySystemRadPerSecond = {0.0, 0.0, -1.0};
    lower.radiusMeters = 2.0;
    lower.motionRevision = 42;
    update.actors.push_back(lower);

    map.replaceDynamicWorld(std::move(update));

    Map::CorridorQuery query;
    query.startMapMeters = {0.0, 0.0, 0.0};
    query.endMapMeters = {50.0, 0.0, 0.0};
    query.radiusMeters = 20.0;
    const Map::QueryResult dynamic = map.queryCorridor(query);

    Space space = singleRegionSpace();

    Planner::AgentState agent = baseAgent();
    agent.radiusMeters = 1.0;
    agent.forwardMap = {1.0, 0.0, 0.0};
    agent.upMap = {0.0, 0.0, 1.0};
    agent.rightMap = {0.0, 1.0, 0.0};
    agent.hullHalfExtentsBodyMeters = {0.4, 0.4, 0.4};
    agent.linearCapability.maxForwardAccelerationMetersPerSec2 = 30.0;
    agent.linearCapability.maxReverseAccelerationMetersPerSec2 = 30.0;
    agent.linearCapability.maxLateralAccelerationMetersPerSec2 = 30.0;
    agent.linearCapability.maxVerticalAccelerationMetersPerSec2 = 30.0;
    agent.angularCapability.maxAngularAccelerationRadPerSec2 = 10.0;
    agent.angularCapability.maxAngularSpeedRadPerSec = 10.0;

    Planner::Goal goal = goalAt(50.0);
    goal.maximumTargetSpeedMps = 10.0;

    Planner::Policy policy = basePolicy();
    policy.horizon.minimumHorizonMeters = 30.0;
    policy.horizon.turnDistanceMeters = 0.0;
    policy.horizon.safetyMarginMeters = 0.0;
    policy.movingPassage.enabled = true;
    policy.movingPassage.allowSteeringAuthority = true;
    policy.movingPassage.durationSeconds = 3.0;
    policy.movingPassage.candidates.minimumForwardDistanceMeters = 0.0;
    policy.movingPassage.candidates.maximumForwardDistanceMeters = 100.0;
    policy.movingPassage.candidates.maximumCenterlineOffsetMeters = 10.0;
    policy.movingPassage.candidates.secondaryClearanceMeters = 4.0;
    policy.movingPassage.prediction.secondaryClearanceMeters = 4.0;

    const Planner::Result planned = Planner::plan(
        agent,
        goal,
        dynamic,
        0.0,
        space,
        policy
    );

    require(planned.movingPassageFeasible &&
            planned.movingPassageStaticSafe,
            "authority fixture requires both moving and exact-static proofs");
    require(planned.status == Planner::Status::MovingPassageClear &&
            planned.movingPassageAuthorityUsed,
            "doubly-proven moving passage must take explicit planner authority");
    require(planned.safeProgressTargetDemonstrated,
            "authoritative moving passage must publish proven safe progress");
    require(near(planned.selectedTargetMapMeters.x,
                 planned.movingPassageTargetMapMeters.x) &&
            near(planned.selectedTargetMapMeters.y,
                 planned.movingPassageTargetMapMeters.y) &&
            near(planned.selectedTargetMapMeters.z,
                 planned.movingPassageTargetMapMeters.z),
            "moving-passage authority must retain its proved trajectory endpoint");

    const auto& exactSample =
        planned.movingPassageInitialAccelerationMapMps2;
    const auto& mapDemand =
        planned.intent.idealLinearAccelerationDemandMapMps2;
    require(near(mapDemand.x, exactSample.x) &&
            near(mapDemand.y, exactSample.y) &&
            near(mapDemand.z, exactSample.z),
            "moving-passage authority must execute the exact first proved Hermite acceleration sample");
    require(glm::length(planned.desiredVelocityMapMetersPerSecond) <= 1.0e-12,
            "moving-passage authority must not solve a second desired-velocity trajectory");

    Map::WorkingFrame frame;
    frame.xAxisSystem = {0.0, 1.0, 0.0};
    frame.yAxisSystem = {0.0, 0.0, 1.0};
    frame.zAxisSystem = {1.0, 0.0, 0.0};

    const Bridge::Intent worldIntent =
        Planner::mapIntentToWorld(planned.intent, frame);
    require(near(worldIntent.idealLinearAccelerationDemandMapMps2.x,
                 exactSample.z) &&
            near(worldIntent.idealLinearAccelerationDemandMapMps2.y,
                 exactSample.x) &&
            near(worldIntent.idealLinearAccelerationDemandMapMps2.z,
                 exactSample.y),
            "proved moving-passage sample must cross the non-identity map->world boundary exactly");

    Bridge bridge(expertProfile());
    Bridge::Intent neutral;
    require(bridge.reset(0.0, neutral),
            "moving-passage authority bridge reset must succeed");

    Bridge::StepResult executed;
    for (int i = 1; i <= 20; ++i)
    {
        executed = bridge.step(
            0.01 * static_cast<double>(i),
            0.01,
            worldIntent
        );
    }

    require(executed.snapshot.valid &&
            executed.control.navigationAccelerationDemandValid,
            "proved moving-passage authority must cross PilotSkillExecutor into ShipControlState");
    require(executed.snapshot.intentRevision == goal.revision,
            "moving-passage authority must preserve planner revision through pilot execution");
    require(near(executed.snapshot.idealLinearAccelerationDemandMapMps2.x,
                 worldIntent.idealLinearAccelerationDemandMapMps2.x) &&
            near(executed.snapshot.idealLinearAccelerationDemandMapMps2.y,
                 worldIntent.idealLinearAccelerationDemandMapMps2.y) &&
            near(executed.snapshot.idealLinearAccelerationDemandMapMps2.z,
                 worldIntent.idealLinearAccelerationDemandMapMps2.z),
            "PilotSkillExecutor must receive the transformed proved acceleration without re-planning");
}

void testMapIntentTransformsIntoWorldControlFrame()
{
    Bridge::Intent mapIntent;
    mapIntent.revision = 99;
    mapIntent.emergency = true;
    mapIntent.hazardUrgency01 = 0.25;
    mapIntent.idealLinearAccelerationDemandMapMps2 =
        {2.0, 3.0, 4.0};
    mapIntent.idealAngularAccelerationDemandMapRadPerSec2 =
        {-1.0, 5.0, 7.0};

    Map::WorkingFrame frame;
    frame.originSystemMeters = {100.0, 200.0, 300.0};
    frame.xAxisSystem = {0.0, 1.0, 0.0};
    frame.yAxisSystem = {0.0, 0.0, 1.0};
    frame.zAxisSystem = {1.0, 0.0, 0.0};

    const Bridge::Intent worldIntent =
        Planner::mapIntentToWorld(mapIntent, frame);

    require(worldIntent.revision == mapIntent.revision &&
            worldIntent.emergency == mapIntent.emergency &&
            near(worldIntent.hazardUrgency01, mapIntent.hazardUrgency01),
            "map-to-world transform must preserve intent metadata");

    const auto& linear =
        worldIntent.idealLinearAccelerationDemandMapMps2;
    require(near(linear.x, 4.0) &&
            near(linear.y, 2.0) &&
            near(linear.z, 3.0),
            "linear acceleration demand did not rotate from map to world frame");

    const auto& angular =
        worldIntent.idealAngularAccelerationDemandMapRadPerSec2;
    require(near(angular.x, 7.0) &&
            near(angular.y, -1.0) &&
            near(angular.z, 5.0),
            "angular acceleration demand did not rotate from map to world frame");

    bool rejected = false;
    try
    {
        Map::WorkingFrame invalid;
        invalid.xAxisSystem = {1.0, 0.0, 0.0};
        invalid.yAxisSystem = {1.0, 0.0, 0.0};
        invalid.zAxisSystem = {0.0, 0.0, 1.0};
        (void)Planner::mapIntentToWorld(mapIntent, invalid);
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }

    require(rejected,
            "map-to-world control boundary must reject a non-orthogonal frame");
}

void testPlannerIntentCrossesAcceptedPilotBridge()
{
    Space space = singleRegionSpace();
    Planner::AgentState agent = baseAgent();
    Planner::Goal goal = goalAt(20.0);

    const Planner::Result planned = Planner::plan(
        agent,
        goal,
        emptyDynamic(),
        0.0,
        space,
        basePolicy()
    );
    require(planned.status == Planner::Status::NominalClear,
            "clear fixture must produce navigation intent");
    require(planned.intent.idealLinearAccelerationDemandMapMps2.x > 0.0,
            "clear fixture must request forward map acceleration");

    Bridge bridge(expertProfile());
    Bridge::Intent neutral;
    require(bridge.reset(0.0, neutral),
            "pilot bridge reset must succeed");

    Bridge::StepResult executed;
    for (int i = 1; i <= 20; ++i)
    {
        executed = bridge.step(
            0.01 * static_cast<double>(i),
            0.01,
            planned.intent
        );
    }

    require(executed.snapshot.valid,
            "planned intent must become an accepted pilot execution snapshot");
    require(executed.control.navigationAccelerationDemandValid,
            "planned intent must reach the direct navigation control channel");
    require(executed.snapshot.intentRevision == goal.revision,
            "planner goal revision must survive pilot execution");
    require(executed.snapshot.executedLinearAccelerationDemandMapMps2.x > 0.0,
            "pilot execution must preserve forward progress demand");
}

} // namespace

int main()
{
    try
    {
        testHitVolumeAdapterUsesAuthoritativeLocalObb();
        testStaticCorridorBecomesLivePortalWaypoint();
        testSamePortalRejectsOversizedHull();
        testExactStaticObstacleParticipatesInRuntimeComposition();
        testLiveScaleStaticObstacleInsideFirstBoundedHorizon();
        testAdjustedTargetPreservesNominalConflictIdentity();
        testNavigationMapCrossingConflictProducesBrakingHold();
        testMovingGapPrecisionProbeUsesRuntimeCandidates();
        testClosingMovingGapFailsClosedBeforePassageEvaluation();
        testStaticObstacleRejectsSameAcceptedMovingHermiteTrajectory();
        testDoublyProvenMovingPassageTakesAuthorityThroughPilotBridge();
        testMapIntentTransformsIntoWorldControlFrame();
        testPlannerIntentCrossesAcceptedPilotBridge();

        std::cout << "NAVIGATION RUNTIME PLANNER TESTS: PASS\n";
        std::cout << " - authoritative HitVolume -> navigation OBB adapter\n";
        std::cout << " - static corridor portal -> bounded live target\n";
        std::cout << " - portal clearance rejects oversized hull\n";
        std::cout << " - exact static OBB participates in runtime composition\n";
        std::cout << " - live-scale 1300 m OBB triggers first-horizon adjustment\n";
        std::cout << " - adjusted target retains nominal conflict identity\n";
        std::cout << " - NavigationMap crossing conflict -> braking hold\n";
        std::cout << " - bounded runtime candidates -> moving-gap/passage precision probe\n";
        std::cout << " - closing moving gap fails closed before passage evaluation\n";
        std::cout << " - exact-static blocker rejects the same accepted moving Hermite trajectory\n";
        std::cout << " - doubly-proven moving passage -> exact sample -> map/world -> PilotSkillExecutor authority\n";
        std::cout << " - non-identity map intent -> world control frame\n";
        std::cout << " - planner intent -> PilotSkillExecutor runtime bridge\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "NAVIGATION RUNTIME PLANNER TESTS: FAIL: "
                  << error.what() << '\n';
        return 1;
    }
}
