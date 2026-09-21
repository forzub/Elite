#include "src/game/navigation/NominalRoutePlanner.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include "src/world/navigation/NavigationObstacleGeometry.h"

namespace
{

using Planner = game::navigation::NominalRoutePlanner;
using Obstacle = world::navigation::NavigationObstacle;
using Shape = world::navigation::NavigationObstacleShape;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

Obstacle wall()
{
    Obstacle o;
    o.id = "wall";
    o.entityId = 1001;
    o.shape = Shape::Box;
    o.centerMeters = {150.0, 0.0, 0.0};
    o.halfExtentsMeters = {25.0, 12.0, 30.0};
    o.localToWorldBasis = glm::dmat3(1.0);
    return o;
}

Planner::Request baseRequest()
{
    Planner::Request q;
    q.goalRevision = 10;
    q.staticWorldRevision = 20;
    q.startMapMeters = {0.0, 0.0, 0.0};
    q.goalMapMeters = {300.0, 0.0, 0.0};
    q.navigationEnvelopeRadiusMeters = 5.0;
    return q;
}

void testDirectRouteIsRetainedAsOneNominalProduct()
{
    const auto result = Planner::plan(baseRequest());
    require(result.valid, "direct nominal route failed");
    require(!result.staticDetourUsed, "direct route invented a detour");
    require(result.pointsMapMeters.size() == 2,
            "direct route should contain start and finish only");
    require(result.goalRevision == 10 &&
            result.staticWorldRevision == 20,
            "nominal route lost source revisions");
}

void testStaticWallProducesDetour()
{
    auto q = baseRequest();
    q.staticObstacles.push_back(wall());

    const auto result = Planner::plan(q);
    require(result.valid, "wall detour route failed");
    require(result.staticDetourUsed,
            "wall route did not report static detour");
    require(result.pointsMapMeters.size() >= 3,
            "wall detour did not contain an intermediate point");

    for (std::size_t i = 1; i < result.pointsMapMeters.size(); ++i)
    {
        require(
            world::navigation::segmentClearOfNavigationObstacles(
                result.pointsMapMeters[i - 1],
                result.pointsMapMeters[i],
                q.staticObstacles,
                q.navigationEnvelopeRadiusMeters,
                q.additionalRouteClearanceMeters
            ),
            "nominal route segment intersects the static wall"
        );
    }
}

void testBoxDetourUsesNearestFacePlaneInsteadOfCornerEdge()
{
    auto q = baseRequest();
    q.staticObstacles.push_back(wall());

    const auto result = Planner::plan(q);
    require(result.valid, "box face-plane detour route failed");
    require(
        result.pointsMapMeters.size() == 4,
        "symmetric wall should produce one two-point face-plane detour"
    );

    // With the wall used by this fixture, Y is the cheapest transverse axis.
    // The old corner-only support graph unnecessarily also displaced Z and
    // produced a ~323 m-class route in the 13 m-envelope viewer scenario.
    // Edge support nodes must keep this equivalent test route in one
    // transverse plane.
    require(
        std::abs(result.pointsMapMeters[1].z) <= 1.0e-9 &&
        std::abs(result.pointsMapMeters[2].z) <= 1.0e-9,
        "box detour unnecessarily paid clearance on a second transverse axis"
    );
    require(
        std::abs(result.pointsMapMeters[1].y) > 1.0 &&
        std::abs(result.pointsMapMeters[2].y) > 1.0,
        "box detour did not move onto the nearest clear face plane"
    );
    require(
        result.lengthMeters < 310.0,
        "box detour remained materially longer than the nearest-face route"
    );
}

void testRequiredWaypointIsPreserved()
{
    auto q = baseRequest();
    q.requiredWaypointsMapMeters.push_back({80.0, 50.0, 0.0});

    const auto result = Planner::plan(q);
    require(result.valid, "required-waypoint route failed");

    bool found = false;
    for (const auto& p : result.pointsMapMeters)
    {
        if (glm::length(p - q.requiredWaypointsMapMeters.front()) < 1.0e-9)
        {
            found = true;
            break;
        }
    }
    require(found, "nominal route lost required authored waypoint");
}

void testDynamicRevisionDoesNotInvalidateNominalRoute()
{
    const auto route = Planner::plan(baseRequest());
    require(route.valid, "validity fixture route failed");

    Planner::ValidityQuery same;
    same.goalRevision = route.goalRevision;
    same.staticWorldRevision = route.staticWorldRevision;
    same.dynamicWorldRevision = 1;

    require(
        Planner::invalidationReason(route, same) ==
            Planner::InvalidationReason::None,
        "fresh nominal route was invalidated"
    );

    same.dynamicWorldRevision = 999999;
    require(
        Planner::invalidationReason(route, same) ==
            Planner::InvalidationReason::None,
        "dynamic-only revision incorrectly rebuilt global route"
    );

    same.staticWorldRevision++;
    require(
        Planner::invalidationReason(route, same) ==
            Planner::InvalidationReason::StaticWorldChanged,
        "static revision did not invalidate global route"
    );

    same.staticWorldRevision = route.staticWorldRevision;
    same.goalRevision++;
    require(
        Planner::invalidationReason(route, same) ==
            Planner::InvalidationReason::GoalChanged,
        "goal revision did not invalidate global route"
    );
}

} // namespace

int main()
{
    try
    {
        testDirectRouteIsRetainedAsOneNominalProduct();
        testStaticWallProducesDetour();
        testBoxDetourUsesNearestFacePlaneInsteadOfCornerEdge();
        testRequiredWaypointIsPreserved();
        testDynamicRevisionDoesNotInvalidateNominalRoute();

        std::cout << "NOMINAL ROUTE PLANNER TESTS: PASS\n";
        std::cout << " - static route builds once from start to finish\n";
        std::cout << " - raw static box geometry produces a detour\n";
        std::cout << " - authored checkpoints remain ordered route constraints\n";
        std::cout << " - dynamic revision does not invalidate nominal route\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& e)
    {
        std::cerr << "NOMINAL ROUTE PLANNER TESTS: FAIL: "
                  << e.what() << "\n";
        return EXIT_FAILURE;
    }
}
