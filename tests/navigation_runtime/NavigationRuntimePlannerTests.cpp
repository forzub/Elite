#include "src/game/navigation/NavigationRuntimePlanner.h"
#include "src/game/navigation/NavigationRuntimeControlBridge.h"
#include "src/game/navigation/NavigationFrameBoundary.h"
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
using StaticQueries = Planner::StaticQueries;
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

Space orientedPortalCaptureSpace()
{
    Space space;
    Space::StaticSpaceUpdate update;
    update.sourceRevision = 150;
    update.regions = {
        region(1, 5.0, 0.0, 5.0, 15.0, 10.0),
        region(2, 15.0, 0.0, 5.0, 15.0, 10.0)
    };

    Space::PortalInput gate =
        portal(151, 1, 2, 10.0, 0.0, 4.0);
    gate.traversal.enabled = true;
    gate.traversal.normalAToBMap = {1.0, 0.0, 0.0};
    gate.traversal.approachDistanceMeters = 3.0;
    gate.traversal.maximumVelocityAngleRad = 0.08726646259971647;
    gate.traversal.maximumForwardAngleRad = 0.08726646259971647;
    gate.traversal.maximumLateralSpeedMps = 0.5;
    gate.traversal.transitSpeedMps = 3.0;
    gate.traversal.requireVehicleForwardAlignment = true;
    update.portals = {gate};

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

    policy.avoidance.lateralGridHalfExtentSamples = 5;
    policy.avoidance.minimumLateralStepMeters = 2.0;
    policy.avoidance.lateralStepEnvelopeMultiplier = 1.0;
    policy.avoidance.maximumLateralOffsetMeters = 80.0;
    policy.avoidance.longitudinalSamples = 3;
    policy.avoidance.projectionPaddingMeters = 1.0;
    policy.avoidance.trajectorySamples = 32;
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
        StaticQueries(space),
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
    require(result.intent.idealLinearAccelerationLocalMps2.y > 0.0,
            "forced detour must create a real lateral acceleration demand");
}

void testPortalCaptureAlignsVelocityAndHullBeforeTransit()
{
    Space space = orientedPortalCaptureSpace();
    Planner::Policy policy = basePolicy();
    Planner::Goal goal = goalAt(18.0);

    Planner::AgentState approaching = baseAgent();
    approaching.positionMapMeters = {2.0, 0.0, 0.0};
    approaching.forwardMap = {0.0, 0.0, -1.0};

    const Planner::Result approach = Planner::plan(
        approaching,
        goal,
        emptyDynamic(),
        0.0,
        StaticQueries(space),
        policy
    );

    require(approach.portalTraversalActive &&
            approach.activePortalId == 151,
            "oriented portal must activate the generic traversal contract");
    require(approach.status == Planner::Status::PortalCapture,
            "misaligned hull must remain in portal capture phase");
    require(!approach.portalForwardAligned &&
            !approach.portalCaptureReady,
            "misaligned hull must not be released through the portal");
    require(!approach.usedPortalWaypoint &&
            near(approach.coarseWaypointMapMeters.x, 7.0),
            "capture phase must first stage at the authored approach point");
    require(
        approach.selectedManeuverRequiresForwardAlignment &&
        near(approach.selectedManeuverForwardMap.x, 1.0),
        "portal capture must publish its CURRENT maneuver forward-alignment requirement"
    );

    const auto angular =
        approach.intent.idealAngularAccelerationLocalRadPerSec2;
    require(
        std::sqrt(
            angular.x * angular.x +
            angular.y * angular.y +
            angular.z * angular.z
        ) > 1.0e-6,
        "portal capture must command hull-axis alignment, not angular damping only"
    );

    Planner::AgentState aligned = approaching;
    aligned.positionMapMeters = {7.0, 0.0, 0.0};
    aligned.forwardMap = {1.0, 0.0, 0.0};
    aligned.rightMap = {0.0, 0.0, 1.0};
    aligned.upMap = {0.0, 1.0, 0.0};
    aligned.velocityMapMetersPerSecond = {0.0, 0.0, 0.0};

    const Planner::Result transit = Planner::plan(
        aligned,
        goal,
        emptyDynamic(),
        0.0,
        StaticQueries(space),
        policy
    );

    require(transit.portalVelocityAligned &&
            transit.portalForwardAligned &&
            transit.portalCaptureReady,
            "aligned stationary staging state must satisfy entry capture");
    require(transit.usedPortalWaypoint &&
            transit.status == Planner::Status::PortalTransit,
            "capture-ready vehicle must be released toward the portal boundary");
    require(transit.desiredVelocityMapMetersPerSecond.x > 2.9 &&
            std::abs(transit.desiredVelocityMapMetersPerSecond.y) < 1.0e-9,
            "portal transit velocity must follow the oriented portal normal");

    Planner::AgentState sliding = aligned;
    sliding.velocityMapMetersPerSecond = {0.0, 1.0, 0.0};

    const Planner::Result rejected = Planner::plan(
        sliding,
        goal,
        emptyDynamic(),
        0.0,
        StaticQueries(space),
        policy
    );

    require(rejected.status == Planner::Status::PortalCapture &&
            !rejected.portalVelocityAligned &&
            !rejected.portalCaptureReady,
            "excess cross-track velocity must keep the vehicle out of transit");
    require(
        rejected.intent.idealLinearAccelerationLocalMps2.y < 0.0,
        "capture controller must brake cross-track velocity at the staging point"
    );
}

void testAdjustedVisibilityDoesNotInheritFuturePortalAlignment()
{
    Space space = orientedPortalCaptureSpace();

    Map::Config config;
    config.halfExtentMeters = 100.0;
    config.cellSizeMeters = 10.0;
    config.predictionHorizonSeconds = 3.0;
    config.interactionMarginMeters = 0.0;

    Map map(config);
    Map::DynamicWorldUpdate update;
    update.sourceRevision = 151;

    Map::DynamicActorInput blocker;
    blocker.entityId = 9151;
    blocker.positionMapMeters = {4.0, 0.0, 0.0};
    blocker.velocityMapMetersPerSecond = {0.0, 0.0, 0.0};
    blocker.accelerationMapMetersPerSecond2 = {0.0, 0.0, 0.0};
    blocker.radiusMeters = 0.75;
    blocker.motionRevision = 1;
    update.actors.push_back(blocker);
    map.replaceDynamicWorld(std::move(update));

    Map::CorridorQuery query;
    query.startMapMeters = {1.0, 0.0, 0.0};
    query.endMapMeters = {7.0, 0.0, 0.0};
    query.radiusMeters = 5.0;
    const Map::QueryResult dynamic = map.queryCorridor(query);
    require(!dynamic.candidates.empty(),
            "future-portal bypass fixture must publish its dynamic blocker");

    Planner::AgentState agent = baseAgent();
    agent.positionMapMeters = {1.0, 0.0, 0.0};
    agent.forwardMap = {0.0, 0.0, -1.0};

    Planner::Goal goal = goalAt(18.0);
    Planner::Policy policy = basePolicy();
    policy.horizon.minimumHorizonMeters = 10.0;
    policy.horizon.turnDistanceMeters = 0.0;
    policy.horizon.safetyMarginMeters = 0.0;

    const Planner::Result result = Planner::plan(
        agent,
        goal,
        dynamic,
        0.0,
        StaticQueries(space),
        policy
    );

    require(result.portalTraversalActive,
            "fixture must retain the future oriented portal as route context");
    require(result.status == Planner::Status::AdjustedClear &&
            result.adjustedTarget,
            "dynamic blocker must produce a local visibility bypass");
    require(!result.selectedManeuverRequiresForwardAlignment,
            "AdjustedClear bypass must not inherit future portal forward alignment");

    const auto angular =
        result.intent.idealAngularAccelerationLocalRadPerSec2;
    require(
        std::sqrt(
            angular.x * angular.x +
            angular.y * angular.y +
            angular.z * angular.z
        ) <= 1.0e-9,
        "stationary AdjustedClear bypass must retain angular damping semantics instead of portal alignment"
    );
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
        StaticQueries(space),
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
        StaticQueries(space),
        policy
    );
    require(rejected.status == Planner::Status::StaticHold,
            "oversized hull must fail closed at the same static corridor");
    require(!rejected.safeProgressTargetDemonstrated,
            "oversized hull must not claim safe progress");
    const auto& held =
        rejected.intent.idealLinearAccelerationLocalMps2;
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
        StaticQueries(space),
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
        StaticQueries(space),
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
    obstacle.positionMapMeters = {500.0, 0.0, 0.0};
    obstacle.velocityMapMetersPerSecond = {0.0, 0.0, 0.0};
    obstacle.accelerationMapMetersPerSecond2 = {0.0, 0.0, 0.0};
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
    policy.avoidance.lateralGridHalfExtentSamples = 4;

    const Planner::Result result = Planner::plan(
        agent,
        goal,
        dynamic,
        0.0,
        StaticQueries(space),
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


void testProjectedVisibleHorizonBypassPublishesMergeTarget()
{
    Map::Config config;
    config.halfExtentMeters = 2000.0;
    config.cellSizeMeters = 100.0;
    config.predictionHorizonSeconds = 3.0;
    config.interactionMarginMeters = 0.0;

    Map map(config);
    Map::DynamicWorldUpdate update;
    update.sourceRevision = 93;

    Map::DynamicActorInput obstacle;
    obstacle.entityId = 202;
    obstacle.positionMapMeters = {25.0, 10.0, 0.0};
    obstacle.velocityMapMetersPerSecond = {0.0, -5.0, 0.0};
    obstacle.accelerationMapMetersPerSecond2 = {0.0, 0.0, 0.0};
    obstacle.radiusMeters = 2.0;
    obstacle.motionRevision = 5;
    update.actors.push_back(obstacle);
    map.replaceDynamicWorld(std::move(update));

    Map::CorridorQuery query;
    query.startMapMeters = {0.0, 0.0, 0.0};
    query.endMapMeters = {100.0, 0.0, 0.0};
    query.radiusMeters = 30.0;
    query.lookAheadSeconds = 3.0;
    const Map::QueryResult dynamic = map.queryCorridor(query);

    Space space;
    Space::StaticSpaceUpdate staticWorld;
    staticWorld.sourceRevision = 202;
    Space::RegionInput wideRegion;
    wideRegion.regionId = 1;
    wideRegion.boundsMapMeters.minMapMeters = {-100.0, -200.0, -200.0};
    wideRegion.boundsMapMeters.maxMapMeters = {1200.0, 200.0, 200.0};
    wideRegion.clearanceRadiusMeters = 1000.0;
    wideRegion.geometryRevision = 1;
    staticWorld.regions = {wideRegion};
    space.replaceStaticWorld(std::move(staticWorld));

    Planner::AgentState agent = baseAgent();
    agent.radiusMeters = 2.0;
    agent.velocityMapMetersPerSecond = {10.0, 0.0, 0.0};

    Planner::Goal goal = goalAt(1000.0);
    goal.maximumTargetSpeedMps = 20.0;

    Planner::Policy policy = basePolicy();
    policy.horizon.turnDistanceMeters = 20.0;
    policy.horizon.minimumHorizonMeters = 40.0;
    policy.horizon.safetyMarginMeters = 1.0;

    const Planner::Result result = Planner::plan(
        agent, goal, dynamic, 0.0, StaticQueries(space), policy
    );

    require(
        result.status == Planner::Status::AdjustedClear &&
        result.adjustedTarget,
        "visible-horizon crossing obstacle must produce a temporary bypass"
    );
    require(
        result.nominalDynamicConflictsFound > 0,
        "visible-horizon fixture must retain the nominal dynamic conflict"
    );
    require(
        result.avoidanceProjectedDynamicObstacles > 0 &&
        result.avoidanceOffsetCandidatesExamined > 0 &&
        result.avoidanceRouteCandidatesExamined > 0,
        "runtime planner must consume projected moving occupancy and search normal-plane offsets plus longitudinal stations"
    );
    require(
        result.localBypassLateralOffsetMeters > 0.0,
        "runtime planner must publish a non-zero lateral bypass offset"
    );
    require(
        result.localBypassForwardDistanceMeters > 0.0,
        "runtime planner must publish a longitudinal bypass station before merge"
    );
    require(
        std::abs(result.localBypassMergeTargetMapMeters.y) < 1.0e-6 &&
        std::abs(result.localBypassMergeTargetMapMeters.z) < 1.0e-6,
        "local merge target must remain on the original planned trajectory"
    );
    require(
        std::abs(result.selectedTargetMapMeters.y) > 1.0e-6 ||
        std::abs(result.selectedTargetMapMeters.z) > 1.0e-6,
        "temporary bypass target must leave the blocked centerline"
    );
}

void testNavigationMapConflictFailsClosedOnlyWhenNoOffsetFits()
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
    crossing.positionMapMeters = {20.0, 0.0, 0.0};
    crossing.velocityMapMetersPerSecond = {0.0, 0.0, 0.0};
    crossing.accelerationMapMetersPerSecond2 = {0.0, 0.0, 0.0};
    crossing.radiusMeters = 2.0;
    crossing.motionRevision = 3;
    update.actors.push_back(crossing);
    map.replaceDynamicWorld(std::move(update));

    Map::CorridorQuery query;
    query.startMapMeters = {0.0, 0.0, 0.0};
    query.endMapMeters = {50.0, 0.0, 0.0};
    query.radiusMeters = 20.0;
    query.lookAheadSeconds = 3.0;
    const Map::QueryResult dynamic = map.queryCorridor(query);

    Space space;
    Space::StaticSpaceUpdate staticWorld;
    staticWorld.sourceRevision = 91;
    Space::RegionInput narrow;
    narrow.regionId = 1;
    narrow.boundsMapMeters.minMapMeters = {-10.0, -2.5, -2.5};
    narrow.boundsMapMeters.maxMapMeters = {100.0, 2.5, 2.5};
    narrow.clearanceRadiusMeters = 100.0;
    narrow.geometryRevision = 1;
    staticWorld.regions = {narrow};
    space.replaceStaticWorld(std::move(staticWorld));

    Planner::AgentState agent = baseAgent();
    agent.radiusMeters = 1.0;
    agent.velocityMapMetersPerSecond = {10.0, 0.0, 0.0};

    Planner::Policy policy = basePolicy();
    policy.horizon.safetyMarginMeters = 0.5;
    policy.avoidance.minimumLateralStepMeters = 3.0;

    const Planner::Result result = Planner::plan(
        agent, goalAt(50.0), dynamic, 0.0, StaticQueries(space), policy
    );

    require(
        result.status == Planner::Status::ConflictHold,
        "dynamic conflict may hold only when projected local free space is exhausted"
    );
    require(
        result.localBypassExhausted,
        "runtime planner must explicitly surface visible-horizon bypass exhaustion"
    );
    require(
        result.avoidanceStaticRejected > 0,
        "narrow static corridor must reject projected offsets"
    );
    require(
        result.intent.emergency &&
        result.intent.idealLinearAccelerationLocalMps2.x < 0.0,
        "exhausted local free space must retain fail-closed braking intent"
    );
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
    upper.positionMapMeters = {20.0, 2.5, 0.0};
    upper.velocityMapMetersPerSecond = {1.0, 0.0, 0.0};
    upper.angularVelocityMapRadPerSecond = {0.0, 0.0, 1.0};
    upper.radiusMeters = 2.0;
    upper.motionRevision = 11;
    update.actors.push_back(upper);

    Map::DynamicActorInput lower;
    lower.entityId = 302;
    lower.positionMapMeters = {20.0, -2.5, 0.0};
    lower.velocityMapMetersPerSecond = {1.0, 0.0, 0.0};
    lower.angularVelocityMapRadPerSecond = {0.0, 0.0, -1.0};
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
        StaticQueries(space),
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
    upper.positionMapMeters = {20.0, 2.5, 0.0};
    upper.velocityMapMetersPerSecond = {0.0, -1.0, 0.0};
    upper.radiusMeters = 2.0;
    upper.motionRevision = 21;
    update.actors.push_back(upper);

    Map::DynamicActorInput lower;
    lower.entityId = 312;
    lower.positionMapMeters = {20.0, -2.5, 0.0};
    lower.velocityMapMetersPerSecond = {0.0, 1.0, 0.0};
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
        StaticQueries(space),
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
    upper.positionMapMeters = {20.0, 2.5, 0.0};
    upper.velocityMapMetersPerSecond = {1.0, 0.0, 0.0};
    upper.angularVelocityMapRadPerSecond = {0.0, 0.0, 1.0};
    upper.radiusMeters = 2.0;
    upper.motionRevision = 31;
    update.actors.push_back(upper);

    Map::DynamicActorInput lower;
    lower.entityId = 322;
    lower.positionMapMeters = {20.0, -2.5, 0.0};
    lower.velocityMapMetersPerSecond = {1.0, 0.0, 0.0};
    lower.angularVelocityMapRadPerSecond = {0.0, 0.0, -1.0};
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
        StaticQueries(space),
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
    upper.positionMapMeters = {20.0, 2.5, 0.0};
    upper.velocityMapMetersPerSecond = {1.0, 0.0, 0.0};
    upper.angularVelocityMapRadPerSecond = {0.0, 0.0, 1.0};
    upper.radiusMeters = 2.0;
    upper.motionRevision = 41;
    update.actors.push_back(upper);

    Map::DynamicActorInput lower;
    lower.entityId = 332;
    lower.positionMapMeters = {20.0, -2.5, 0.0};
    lower.velocityMapMetersPerSecond = {1.0, 0.0, 0.0};
    lower.angularVelocityMapRadPerSecond = {0.0, 0.0, -1.0};
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
        StaticQueries(space),
        policy
    );

    require(planned.movingPassageFeasible &&
            planned.movingPassageStaticSafe,
            "authority fixture requires both moving and exact-static proofs");
    require(planned.status == Planner::Status::MovingPassageClear &&
            planned.movingPassageAuthorityUsed,
            "doubly-proven moving passage must take explicit planner authority");

    const auto& exactSample =
        planned.movingPassageInitialAccelerationMapMps2;
    const auto& localDemand =
        planned.intent.idealLinearAccelerationLocalMps2;
    require(near(localDemand.x, exactSample.x) &&
            near(localDemand.y, exactSample.y) &&
            near(localDemand.z, exactSample.z),
            "moving-passage authority must execute the exact proved local sample");

    game::navigation::KinematicFrame frame;
    frame.systemId = 1;
    frame.valid = true;
    frame.localToWorldBasis = glm::dmat3(
        glm::dvec3(0.0, 1.0, 0.0),
        glm::dvec3(0.0, 0.0, 1.0),
        glm::dvec3(1.0, 0.0, 0.0)
    );
    const game::navigation::NavigationFrameBoundary boundary(frame);
    require(boundary.valid(),
            "non-identity navigation boundary fixture must be valid");

    const Bridge::Intent systemIntent =
        boundary.toSystemControlIntent(planned.intent);
    require(near(systemIntent.idealLinearAccelerationSystemMps2.x,
                 exactSample.z) &&
            near(systemIntent.idealLinearAccelerationSystemMps2.y,
                 exactSample.x) &&
            near(systemIntent.idealLinearAccelerationSystemMps2.z,
                 exactSample.y),
            "proved sample must cross the typed local->system boundary exactly");

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
            systemIntent
        );
    }

    require(executed.snapshot.valid &&
            executed.control.navigationAccelerationDemandValid,
            "proved moving-passage authority must cross PilotSkillExecutor into ShipControlState");
    require(executed.snapshot.intentRevision == goal.revision,
            "moving-passage authority must preserve planner revision through pilot execution");
    require(glm::length(
                executed.snapshot.executedLinearAccelerationDemandSystemMps2
            ) > 0.0,
            "PilotSkillExecutor must execute a non-zero system-space demand");
}

void testTypedNavigationBoundaryTransformsLocalControlIntoSystemControl()
{
    game::navigation::NavigationLocalControlIntent local;
    local.revision = 99;
    local.emergency = true;
    local.hazardUrgency01 = 0.25;
    local.idealLinearAccelerationLocalMps2 = {2.0, 3.0, 4.0};
    local.idealAngularAccelerationLocalRadPerSec2 = {-1.0, 5.0, 7.0};

    game::navigation::KinematicFrame frame;
    frame.systemId = 5;
    frame.valid = true;
    frame.originMeters = {100.0, 200.0, 300.0};
    frame.angularVelocityWorldRadPerSecond = {0.01, -0.02, 0.03};
    frame.localToWorldBasis = glm::dmat3(
        glm::dvec3(0.0, 1.0, 0.0),
        glm::dvec3(0.0, 0.0, 1.0),
        glm::dvec3(1.0, 0.0, 0.0)
    );

    const game::navigation::NavigationFrameBoundary boundary(frame);
    require(boundary.valid(),
            "typed navigation boundary must accept an orthonormal right-handed frame");

    const auto system = boundary.toSystemControlIntent(local);

    require(system.revision == local.revision &&
            system.emergency == local.emergency &&
            near(system.hazardUrgency01, local.hazardUrgency01),
            "local->system boundary must preserve intent metadata");

    require(near(system.idealLinearAccelerationSystemMps2.x, 4.0) &&
            near(system.idealLinearAccelerationSystemMps2.y, 2.0) &&
            near(system.idealLinearAccelerationSystemMps2.z, 3.0),
            "linear demand did not rotate from NavLocal to system axes");
    require(near(system.idealAngularAccelerationSystemRadPerSec2.x, 7.0) &&
            near(system.idealAngularAccelerationSystemRadPerSec2.y, -1.0) &&
            near(system.idealAngularAccelerationSystemRadPerSec2.z, 5.0),
            "angular demand did not rotate from NavLocal to system axes");

    game::navigation::NavigationFrameBoundary::NavAngularVelocity
        relativeAngularVelocity;
    relativeAngularVelocity.radiansPerSecond = {0.4, -0.2, 0.1};
    const auto absoluteAngularVelocity =
        boundary.toSystem(relativeAngularVelocity);
    const auto roundTripAngularVelocity =
        boundary.toNavigation(absoluteAngularVelocity);
    require(
        near(
            roundTripAngularVelocity.radiansPerSecond.x,
            relativeAngularVelocity.radiansPerSecond.x
        ) &&
        near(
            roundTripAngularVelocity.radiansPerSecond.y,
            relativeAngularVelocity.radiansPerSecond.y
        ) &&
        near(
            roundTripAngularVelocity.radiansPerSecond.z,
            relativeAngularVelocity.radiansPerSecond.z
        ),
        "angular-velocity state did not round-trip through the rotating frame"
    );

    game::navigation::KinematicFrame invalid = frame;
    invalid.localToWorldBasis[1] = invalid.localToWorldBasis[0];
    const game::navigation::NavigationFrameBoundary rejected(invalid);
    require(!rejected.valid(),
            "navigation boundary must reject a non-orthonormal frame");
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
        StaticQueries(space),
        basePolicy()
    );
    require(planned.status == Planner::Status::NominalClear,
            "clear fixture must produce navigation intent");
    require(planned.intent.idealLinearAccelerationLocalMps2.x > 0.0,
            "clear fixture must request forward NavLocal acceleration");

    game::navigation::KinematicFrame frame;
    frame.systemId = 1;
    frame.valid = true;
    frame.localToWorldBasis = glm::dmat3(1.0);
    const game::navigation::NavigationFrameBoundary boundary(frame);

    const Bridge::Intent systemIntent =
        boundary.toSystemControlIntent(planned.intent);

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
            systemIntent
        );
    }

    require(executed.snapshot.valid,
            "typed planner intent must become an accepted pilot execution snapshot");
    require(executed.control.navigationAccelerationDemandValid,
            "system intent must reach the direct navigation control channel");
    require(executed.snapshot.intentRevision == goal.revision,
            "planner goal revision must survive pilot execution");
    require(executed.snapshot.executedLinearAccelerationDemandSystemMps2.x > 0.0,
            "pilot execution must preserve forward progress demand");
}

} // namespace

int main()
{
    try
    {
        testHitVolumeAdapterUsesAuthoritativeLocalObb();
        testStaticCorridorBecomesLivePortalWaypoint();
        testPortalCaptureAlignsVelocityAndHullBeforeTransit();
        testAdjustedVisibilityDoesNotInheritFuturePortalAlignment();
        testSamePortalRejectsOversizedHull();
        testExactStaticObstacleParticipatesInRuntimeComposition();
        testLiveScaleStaticObstacleInsideFirstBoundedHorizon();
        testAdjustedTargetPreservesNominalConflictIdentity();
        testProjectedVisibleHorizonBypassPublishesMergeTarget();
        testNavigationMapConflictFailsClosedOnlyWhenNoOffsetFits();
        testMovingGapPrecisionProbeUsesRuntimeCandidates();
        testClosingMovingGapFailsClosedBeforePassageEvaluation();
        testStaticObstacleRejectsSameAcceptedMovingHermiteTrajectory();
        testDoublyProvenMovingPassageTakesAuthorityThroughPilotBridge();
        testTypedNavigationBoundaryTransformsLocalControlIntoSystemControl();
        testPlannerIntentCrossesAcceptedPilotBridge();

        std::cout << "NAVIGATION RUNTIME PLANNER TESTS: PASS\n";
        std::cout << " - authoritative HitVolume -> navigation OBB adapter\n";
        std::cout << " - static corridor portal -> bounded live target\n";
        std::cout << " - oriented portal capture aligns flight path + hull axis before transit\n";
        std::cout << " - adjusted visibility does not inherit future portal attitude\n";
        std::cout << " - portal clearance rejects oversized hull\n";
        std::cout << " - exact static OBB participates in runtime composition\n";
        std::cout << " - live-scale 1300 m OBB triggers first-horizon adjustment\n";
        std::cout << " - adjusted target retains nominal conflict identity\n";
        std::cout << " - projected visible-horizon bypass publishes an on-route reacquisition reference\n";
        std::cout << " - local conflict holds only when projected free space is exhausted\n";
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
