#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

#include <glm/glm.hpp>

#include "src/game/navigation/RuckigRoutePlanner.h"
#include "src/world/navigation/NavigationObstacleGeometry.h"
#include "src/world/navigation/TrajectoryGenerator.h"

namespace
{

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

world::navigation::NavigationVehicleProfile vehicle()
{
    world::navigation::NavigationVehicleProfile out;
    out.collisionRadiusMeters = 2.0;
    out.preferredClearanceMeters = 0.0;
    out.maxSpeedMps = 80.0;
    out.maxForwardAccelerationMps2 = 20.0;
    out.maxBrakingAccelerationMps2 = 20.0;
    out.maxLateralAccelerationMps2 = 10.0;
    out.maxAngularVelocityRadPerSecond = 1.0;
    out.maxAngularAccelerationRadPerSecond2 = 1.0;
    return out;
}

world::navigation::TrajectoryGenerationRequest baseRequest()
{
    world::navigation::TrajectoryGenerationRequest request;
    request.systemId = 0;
    request.frameId = "ruckig-route-test";
    request.startUniverseTimeSeconds = 1000.0;
    request.vehicle = vehicle();
    return request;
}

const world::navigation::TrajectorySample& sampleNearestSourceProgress(
    const world::navigation::Trajectory& trajectory,
    double sourceProgressMeters
)
{
    require(!trajectory.samples.empty(), "trajectory has no samples");
    const world::navigation::TrajectorySample* best = &trajectory.samples.front();
    double error = std::numeric_limits<double>::infinity();
    for (const auto& sample : trajectory.samples)
    {
        const double current = std::abs(
            sample.sourcePathProgressMeters - sourceProgressMeters
        );
        if (current < error)
        {
            error = current;
            best = &sample;
        }
    }
    return *best;
}

void requireSweptClear(
    const world::navigation::Trajectory& trajectory,
    const world::navigation::TrajectoryGenerationRequest& request
)
{
    for (std::size_t i = 1; i < trajectory.samples.size(); ++i)
    {
        require(
            world::navigation::segmentClearOfNavigationObstacles(
                trajectory.samples[i - 1].positionMeters,
                trajectory.samples[i].positionMeters,
                request.obstacles,
                request.vehicle.collisionRadiusMeters,
                request.vehicle.preferredClearanceMeters
            ),
            "accepted Ruckig route contains an obstacle-cutting chord"
        );
    }
}

void testStraightRouteUsesRuckigAndStopsAtTerminal()
{
    auto request = baseRequest();
    request.pathPointsMeters = {
        glm::dvec3(0.0, 0.0, 0.0),
        glm::dvec3(500.0, 0.0, 0.0)
    };
    request.pointSpeedConstraints.push_back({500.0, 0.0});

    const auto result = game::navigation::RuckigRoutePlanner::plan(request);
    require(result.ready(), "straight Ruckig route failed");
    require(result.diagnostics.ruckigLegAttempts > 0,
        "straight route reported no Ruckig solve attempts");
    require(result.diagnostics.ruckigLegSuccesses > 0,
        "straight route reported no successful Ruckig leg");
    require(result.trajectory.samples.size() > 3,
        "straight Ruckig route was not sampled");
    require(glm::length(result.trajectory.samples.front().positionMeters) < 1.0e-9,
        "straight route changed the requested start");
    require(glm::length(
        result.trajectory.samples.back().positionMeters -
        request.pathPointsMeters.back()) < 1.0e-9,
        "straight route changed the requested terminal");
    require(result.trajectory.samples.back().speedMps < 1.0e-6,
        "straight Ruckig route did not stop at terminal");
}

void testDiagonalStoppedLegStaysOnCoarseChord()
{
    auto request = baseRequest();
    const glm::dvec3 start(-120.0, 75.0, -40.0);
    const glm::dvec3 end(430.0, 365.0, 510.0);
    request.pathPointsMeters = {start, end};
    request.pointSpeedConstraints.push_back({glm::length(end - start), 0.0});

    const auto result = game::navigation::RuckigRoutePlanner::plan(request);
    require(result.ready(), "diagonal rest-to-rest Ruckig leg failed");

    const glm::dvec3 chord = end - start;
    const double chord2 = glm::dot(chord, chord);
    for (const auto& sample : result.trajectory.samples)
    {
        const double u = std::clamp(
            glm::dot(sample.positionMeters - start, chord) / chord2,
            0.0,
            1.0
        );
        const glm::dvec3 nearest = start + chord * u;
        require(glm::length(sample.positionMeters - nearest) < 1.0e-5,
            "world-axis Ruckig synchronization bowed a stopped leg off its coarse chord");
    }
}

void testClearCornerGetsContinuousRuckigWaypointVelocity()
{
    auto request = baseRequest();
    request.pathPointsMeters = {
        glm::dvec3(-300.0, 0.0, 0.0),
        glm::dvec3(0.0, 0.0, 0.0),
        glm::dvec3(0.0, 300.0, 0.0),
        glm::dvec3(250.0, 550.0, 0.0)
    };

    const auto result = game::navigation::RuckigRoutePlanner::plan(request);
    require(result.ready(), "clear-corner Ruckig route failed");

    const auto& firstCorner = sampleNearestSourceProgress(
        result.trajectory,
        300.0
    );
    require(std::abs(firstCorner.sourcePathProgressMeters - 300.0) < 1.0e-5,
        "Ruckig route lost the coarse waypoint progress marker");
    require(firstCorner.speedMps > 1.0,
        "clear coarse corner was needlessly converted to a stop point");
    require(result.diagnostics.maxCurvaturePerMeter > 0.0,
        "continuous Ruckig corner reported no curvature");
}

void testBlockedCornerBlendFallsBackToSafeStop()
{
    auto request = baseRequest();
    request.pathPointsMeters = {
        glm::dvec3(0.0, 0.0, 0.0),
        glm::dvec3(100.0, 0.0, 0.0),
        glm::dvec3(100.0, 100.0, 0.0),
        glm::dvec3(200.0, 100.0, 0.0)
    };

    world::navigation::NavigationObstacle blocker;
    blocker.id = "corner-cut-blocker";
    blocker.shape = world::navigation::NavigationObstacleShape::Box;
    blocker.centerMeters = glm::dvec3(90.0, 15.0, 0.0);
    blocker.halfExtentsMeters = glm::dvec3(5.0, 5.0, 20.0);
    request.obstacles.push_back(blocker);

    const auto result = game::navigation::RuckigRoutePlanner::plan(request);
    require(result.ready(), "blocked-corner Ruckig fallback route failed");
    requireSweptClear(result.trajectory, request);

    const auto& corner = sampleNearestSourceProgress(
        result.trajectory,
        100.0
    );
    require(std::abs(corner.sourcePathProgressMeters - 100.0) < 1.0e-5,
        "blocked-corner route lost the topology vertex");
    require(corner.speedMps < 1.0e-5,
        "unsafe diagonal corner blend was not relaxed to a stop point");
}

void testImpossibleInitialBrakingIsRejectedBeforePlanning()
{
    auto request = baseRequest();
    request.vehicle.maxBrakingAccelerationMps2 = 1.0;
    request.pathPointsMeters = {
        glm::dvec3(0.0, 0.0, 0.0),
        glm::dvec3(20.0, 0.0, 0.0)
    };
    request.initialVelocityMps = glm::dvec3(50.0, 0.0, 0.0);
    request.pointSpeedConstraints.push_back({20.0, 0.0});

    const auto result = game::navigation::RuckigRoutePlanner::plan(request);
    require(!result.ready(), "impossible braking state was accepted");
    require(result.trajectory.status ==
            world::navigation::TrajectoryStatus::InitialStateInfeasible,
        "impossible braking state has the wrong failure class");
}

} // namespace

int main()
{
    const struct
    {
        const char* name;
        void (*fn)();
    } tests[] = {
        {"straight route uses Ruckig", testStraightRouteUsesRuckigAndStopsAtTerminal},
        {"diagonal stopped leg stays on coarse chord", testDiagonalStoppedLegStaysOnCoarseChord},
        {"clear corner keeps through velocity", testClearCornerGetsContinuousRuckigWaypointVelocity},
        {"blocked corner falls back to stop", testBlockedCornerBlendFallsBackToSafeStop},
        {"impossible braking is rejected", testImpossibleInitialBrakingIsRejectedBeforePlanning},
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
            std::cerr << "[FAIL] " << test.name
                      << ": " << error.what() << '\n';
        }
    }

    std::cout << passed << '/' << (sizeof(tests) / sizeof(tests[0]))
              << " focused Ruckig route tests passed\n";
    return passed == (sizeof(tests) / sizeof(tests[0]))
        ? EXIT_SUCCESS
        : EXIT_FAILURE;
}
