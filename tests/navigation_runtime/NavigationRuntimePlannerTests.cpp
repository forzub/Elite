#include "src/game/navigation/NavigationRuntimePlanner.h"

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
    require(glm::length(
                rejected.intent.idealLinearAccelerationDemandMapMps2
            ) <= 1.0e-12,
            "stationary static hold must not invent translation demand");
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
        testStaticCorridorBecomesLivePortalWaypoint();
        testSamePortalRejectsOversizedHull();
        testNavigationMapCrossingConflictProducesBrakingHold();
        testPlannerIntentCrossesAcceptedPilotBridge();

        std::cout << "NAVIGATION RUNTIME PLANNER TESTS: PASS\n";
        std::cout << " - static corridor portal -> bounded live target\n";
        std::cout << " - portal clearance rejects oversized hull\n";
        std::cout << " - NavigationMap crossing conflict -> braking hold\n";
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
