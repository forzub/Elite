#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "src/world/navigation/GeometricPathPlanner.h"
#include "src/world/navigation/NavigationObstacleGeometry.h"

namespace
{
using world::navigation::GeometricPathPlanner;
using world::navigation::GeometricPathRequest;
using world::navigation::NavigationObstacle;
using world::navigation::NavigationObstacleShape;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void testClearDirectPath()
{
    GeometricPathRequest request;
    request.startMeters = {-500.0, 20.0, 10.0};
    request.goalMeters = {700.0, -30.0, 80.0};
    request.params.agentRadiusMeters = 15.0;

    const auto plan = GeometricPathPlanner::plan(request);
    require(plan.valid, "clear geometric path was rejected");
    require(!plan.obstacleDetourUsed, "clear path invented a detour");
    require(plan.pointsMeters.size() == 2, "clear path gained waypoints");
    require(glm::length(plan.pointsMeters.front() - request.startMeters) < 1e-12,
        "clear path changed start");
    require(glm::length(plan.pointsMeters.back() - request.goalMeters) < 1e-12,
        "clear path changed goal");
}

void testRotatedObbDetour()
{
    NavigationObstacle obstacle;
    obstacle.id = "rotated-box";
    obstacle.shape = NavigationObstacleShape::Box;
    obstacle.centerMeters = {0.0, 0.0, 0.0};
    obstacle.halfExtentsMeters = {180.0, 420.0, 240.0};
    obstacle.localToWorldBasis = glm::dmat3(glm::rotate(
        glm::dmat4(1.0),
        glm::radians(35.0),
        glm::dvec3(0.0, 0.0, 1.0)
    ));

    GeometricPathRequest request;
    request.startMeters = {-1200.0, 0.0, 0.0};
    request.goalMeters = {1200.0, 0.0, 0.0};
    request.params.agentRadiusMeters = 20.0;
    request.obstacles.push_back(obstacle);

    const auto plan = GeometricPathPlanner::plan(request);
    require(plan.valid, "rotated OBB detour was not built");
    require(plan.obstacleDetourUsed, "rotated OBB was ignored");
    require(plan.pointsMeters.size() >= 3, "rotated OBB has no bypass waypoint");

    for (std::size_t i = 1; i < plan.pointsMeters.size(); ++i)
    {
        require(
            world::navigation::segmentClearOfNavigationObstacles(
                plan.pointsMeters[i - 1],
                plan.pointsMeters[i],
                request.obstacles,
                request.params.agentRadiusMeters
            ),
            "detour emitted a segment through rotated OBB"
        );
    }
}

void testSphereBoxCapsuleKernel()
{
    NavigationObstacle sphere;
    sphere.shape = NavigationObstacleShape::Sphere;
    sphere.radiusMeters = 100.0;
    require(
        world::navigation::segmentIntersectsNavigationObstacle(
            {-300.0, 0.0, 0.0},
            {300.0, 0.0, 0.0},
            sphere
        ),
        "sphere collision kernel lost"
    );

    NavigationObstacle box;
    box.shape = NavigationObstacleShape::Box;
    box.halfExtentsMeters = {50.0, 120.0, 80.0};
    box.localToWorldBasis = glm::dmat3(glm::rotate(
        glm::dmat4(1.0),
        glm::radians(45.0),
        glm::dvec3(0.0, 0.0, 1.0)
    ));
    require(
        world::navigation::segmentIntersectsNavigationObstacle(
            {-300.0, 0.0, 0.0},
            {300.0, 0.0, 0.0},
            box
        ),
        "OBB collision kernel lost"
    );

    NavigationObstacle capsule;
    capsule.shape = NavigationObstacleShape::Capsule;
    capsule.radiusMeters = 60.0;
    capsule.capsuleHalfLengthMeters = 250.0;
    require(
        world::navigation::segmentIntersectsNavigationObstacle(
            {-200.0, 0.0, 0.0},
            {200.0, 0.0, 0.0},
            capsule
        ),
        "capsule collision kernel lost"
    );
    require(
        !world::navigation::segmentIntersectsNavigationObstacle(
            {-200.0, 200.0, 0.0},
            {200.0, 200.0, 0.0},
            capsule
        ),
        "capsule collision kernel is over-inflated"
    );
}

void testDeterministicInputPure()
{
    NavigationObstacle box;
    box.id = "box";
    box.shape = NavigationObstacleShape::Box;
    box.centerMeters = {-200.0, 0.0, 0.0};
    box.halfExtentsMeters = {180.0, 260.0, 220.0};
    box.localToWorldBasis = glm::dmat3(glm::rotate(
        glm::dmat4(1.0),
        glm::radians(28.0),
        glm::dvec3(0.0, 0.0, 1.0)
    ));

    NavigationObstacle capsule;
    capsule.id = "capsule";
    capsule.shape = NavigationObstacleShape::Capsule;
    capsule.centerMeters = {500.0, -120.0, 0.0};
    capsule.radiusMeters = 130.0;
    capsule.capsuleHalfLengthMeters = 260.0;

    GeometricPathRequest request;
    request.startMeters = {-1400.0, 0.0, 0.0};
    request.goalMeters = {1400.0, 0.0, 0.0};
    request.params.agentRadiusMeters = 24.0;
    request.obstacles = {box, capsule};

    const auto before = request;
    const auto baseline = GeometricPathPlanner::plan(request);
    require(baseline.valid, "determinism fixture produced no route");

    for (int run = 0; run < 5; ++run)
    {
        const auto repeated = GeometricPathPlanner::plan(request);
        require(repeated.valid == baseline.valid, "validity changed between runs");
        require(repeated.obstacleDetourUsed == baseline.obstacleDetourUsed,
            "detour flag changed between runs");
        require(repeated.pointsMeters.size() == baseline.pointsMeters.size(),
            "waypoint count changed between runs");
        for (std::size_t i = 0; i < baseline.pointsMeters.size(); ++i)
        {
            require(
                glm::length(repeated.pointsMeters[i] - baseline.pointsMeters[i]) < 1e-12,
                "waypoint geometry changed between runs"
            );
        }
    }

    require(
        glm::length(request.startMeters - before.startMeters) < 1e-12 &&
        glm::length(request.goalMeters - before.goalMeters) < 1e-12 &&
        request.obstacles.size() == before.obstacles.size() &&
        glm::length(
            request.obstacles.front().centerMeters -
            before.obstacles.front().centerMeters
        ) < 1e-12,
        "planner mutated input request"
    );
}

} // namespace

int main()
{
    try
    {
        testClearDirectPath();
        testRotatedObbDetour();
        testSphereBoxCapsuleKernel();
        testDeterministicInputPure();
        std::cout << "[PASS] current geometric path planner focused contract\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr << "[FAIL] geometric path planner: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
