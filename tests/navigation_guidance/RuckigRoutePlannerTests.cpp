#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <sstream>
#include <string>
#include <vector>

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

double pointSegmentDistance(
    const glm::dvec3& p,
    const glm::dvec3& a,
    const glm::dvec3& b
)
{
    const glm::dvec3 ab = b - a;
    const double denom = glm::dot(ab, ab);
    if (denom <= 1.0e-12)
        return glm::length(p - a);

    const double u = std::clamp(
        glm::dot(p - a, ab) / denom,
        0.0,
        1.0
    );
    return glm::length(p - (a + ab * u));
}

double pointPolylineDistance(
    const glm::dvec3& p,
    const std::vector<glm::dvec3>& polyline
)
{
    require(polyline.size() >= 2, "polyline has too few points");
    double best = std::numeric_limits<double>::infinity();
    for (std::size_t i = 1; i < polyline.size(); ++i)
    {
        best = std::min(
            best,
            pointSegmentDistance(
                p,
                polyline[i - 1],
                polyline[i]
            )
        );
    }
    return best;
}

void testStraightRouteUsesRuckigWithMovingStartAndFinish()
{
    auto request = baseRequest();
    request.pathPointsMeters = {
        glm::dvec3(0.0, 0.0, 0.0),
        glm::dvec3(500.0, 0.0, 0.0)
    };
    request.vehicle.maxSpeedMps = 10.0;
    request.initialVelocityMps = glm::dvec3(10.0, 0.0, 0.0);
    request.hasTerminalVelocity = true;
    request.terminalVelocityMps = glm::dvec3(10.0, 0.0, 0.0);
    request.pointSpeedConstraints.push_back({500.0, 10.0});

    const auto result = game::navigation::RuckigRoutePlanner::plan(request);
    require(result.ready(), "straight moving-terminal Ruckig route failed");
    require(result.diagnostics.ruckigLegAttempts > 0,
        "straight route reported no Ruckig solve attempts");
    require(result.diagnostics.ruckigLegSuccesses > 0,
        "straight route reported no successful Ruckig leg");
    require(result.trajectory.samples.size() > 3,
        "straight Ruckig route was not sampled");
    require(
        glm::length(
            result.trajectory.samples.front().velocityMps -
            request.initialVelocityMps
        ) < 1.0e-6,
        "straight route changed the requested moving start velocity"
    );
    require(glm::length(
        result.trajectory.samples.back().positionMeters -
        request.pathPointsMeters.back()) < 1.0e-9,
        "straight route changed the requested terminal");
    require(
        glm::length(
            result.trajectory.samples.back().velocityMps -
            request.terminalVelocityMps
        ) < 1.0e-6,
        "straight route stopped instead of honoring moving terminal velocity"
    );
    require(
        result.diagnostics.maxSpeedMps <= 10.0 + 1.0e-6,
        "steady 10 m/s transit accelerated above the authored test speed"
    );
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
    require(std::abs(firstCorner.sourcePathProgressMeters - 300.0) < 0.5,
        "time-sampled path progress lost the coarse waypoint neighborhood");
    require(firstCorner.speedMps > 1.0,
        "clear coarse corner was needlessly converted to a stop point");
    require(result.diagnostics.maxCurvaturePerMeter > 0.0,
        "continuous Ruckig corner reported no curvature");
}

void testBlockedWideBlendShrinksBeforeStopping()
{
    auto request = baseRequest();
    request.pathPointsMeters = {
        glm::dvec3(0.0, 0.0, 0.0),
        glm::dvec3(100.0, 0.0, 0.0),
        glm::dvec3(100.0, 100.0, 0.0),
        glm::dvec3(200.0, 100.0, 0.0)
    };

    world::navigation::NavigationObstacle blocker;
    blocker.id = "wide-corner-cut-blocker";
    blocker.shape = world::navigation::NavigationObstacleShape::Box;
    blocker.centerMeters = glm::dvec3(90.0, 15.0, 0.0);
    blocker.halfExtentsMeters = glm::dvec3(5.0, 5.0, 20.0);
    request.obstacles.push_back(blocker);

    const auto result = game::navigation::RuckigRoutePlanner::plan(request);
    require(result.ready(), "adaptive corner-blend route failed");
    requireSweptClear(result.trajectory, request);

    const auto& corner = sampleNearestSourceProgress(
        result.trajectory,
        100.0
    );
    require(std::abs(corner.sourcePathProgressMeters - 100.0) < 0.5,
        "time-sampled corner route lost the topology-vertex neighborhood");
    require(corner.speedMps > 0.75,
        "one blocked wide chord incorrectly forced a full stop");
}

void testTightCornerNeverTradesSafetyForThroughSpeed()
{
    auto request = baseRequest();
    request.pathPointsMeters = {
        glm::dvec3(0.0, 0.0, 0.0),
        glm::dvec3(100.0, 0.0, 0.0),
        glm::dvec3(100.0, 100.0, 0.0),
        glm::dvec3(200.0, 100.0, 0.0)
    };

    world::navigation::NavigationObstacle blocker;
    blocker.id = "tight-corner-blocker";
    blocker.shape = world::navigation::NavigationObstacleShape::Box;
    blocker.centerMeters = glm::dvec3(95.0, 5.0, 0.0);
    blocker.halfExtentsMeters = glm::dvec3(2.8, 2.8, 20.0);
    request.obstacles.push_back(blocker);

    const auto result = game::navigation::RuckigRoutePlanner::plan(request);
    require(result.ready(), "tight-corner Ruckig route failed");
    requireSweptClear(result.trajectory, request);

    // Do not prescribe a stop merely because this fixture looks tight.
    // If the solver demonstrates a collision-free continuous passage, that is
    // better than StopTurnGo. The invariant here is safety, not zero speed.
    const auto& corner = sampleNearestSourceProgress(
        result.trajectory,
        100.0
    );
    require(std::isfinite(corner.speedMps),
        "tight-corner route produced non-finite speed");
}

void testDefaultWallDetourKeepsMovingThroughShallowCorners()
{
    auto request = baseRequest();
    request.vehicle.collisionRadiusMeters = 13.0;
    request.vehicle.maxSpeedMps = 10.0;
    request.vehicle.maxLateralAccelerationMps2 = 2.0;

    request.pathPointsMeters = {
        glm::dvec3(0.0, 0.0, 0.0),
        glm::dvec3(110.0, -27.0, 0.0),
        glm::dvec3(190.0, -27.0, 0.0),
        glm::dvec3(300.0, 0.0, 0.0)
    };

    world::navigation::NavigationObstacle wall;
    wall.id = "viewer-wall";
    wall.shape = world::navigation::NavigationObstacleShape::Box;
    wall.centerMeters = glm::dvec3(150.0, 0.0, 0.0);
    wall.halfExtentsMeters = glm::dvec3(25.0, 12.0, 30.0);
    request.obstacles.push_back(wall);

    const double firstProgress =
        glm::length(
            request.pathPointsMeters[1] -
            request.pathPointsMeters[0]
        );
    const double secondProgress =
        firstProgress +
        glm::length(
            request.pathPointsMeters[2] -
            request.pathPointsMeters[1]
        );
    const double totalProgress =
        secondProgress +
        glm::length(
            request.pathPointsMeters[3] -
            request.pathPointsMeters[2]
        );
    const glm::dvec3 firstDirection = glm::normalize(
        request.pathPointsMeters[1] -
        request.pathPointsMeters[0]
    );
    const glm::dvec3 lastDirection = glm::normalize(
        request.pathPointsMeters.back() -
        request.pathPointsMeters[
            request.pathPointsMeters.size() - 2
        ]
    );

    request.initialVelocityMps = firstDirection * 10.0;
    request.hasTerminalVelocity = true;
    request.terminalVelocityMps = lastDirection * 10.0;
    request.pointSpeedConstraints.push_back({totalProgress, 10.0});

    const auto result = game::navigation::RuckigRoutePlanner::plan(request);
    require(result.ready(), "viewer wall detour Ruckig route failed");
    requireSweptClear(result.trajectory, request);

    const auto& firstCorner =
        sampleNearestSourceProgress(result.trajectory, firstProgress);
    const auto& secondCorner =
        sampleNearestSourceProgress(result.trajectory, secondProgress);

    require(firstCorner.speedMps > 0.75,
        "first shallow wall corner still became StopTurnGo");
    require(secondCorner.speedMps > 0.75,
        "second shallow wall corner still became StopTurnGo");

    require(
        result.executionGuidePointsMeters.size() >
            request.pathPointsMeters.size(),
        "default wall route did not create a rounded execution guide"
    );

    double minimumSpeed = std::numeric_limits<double>::infinity();
    for (const auto& sample : result.trajectory.samples)
        minimumSpeed = std::min(minimumSpeed, sample.speedMps);

    if (minimumSpeed < 7.5)
    {
        std::ostringstream message;
        message.setf(std::ios::fixed);
        message.precision(3);
        message
            << "default wall calculated curve contains a major unnecessary braking dip: min_speed="
            << minimumSpeed
            << " m/s";
        require(false, message.str());
    }

    double maximumGuideDeviation = 0.0;
    for (const auto& sample : result.trajectory.samples)
    {
        maximumGuideDeviation = std::max(
            maximumGuideDeviation,
            pointPolylineDistance(
                sample.positionMeters,
                result.executionGuidePointsMeters
            )
        );
    }

    require(
        maximumGuideDeviation <= 1.5,
        "default wall Ruckig reference bows visibly away from execution guide"
    );

    for (std::size_t i = 1; i < result.trajectory.samples.size(); ++i)
    {
        require(
            result.trajectory.samples[i].positionMeters.x + 0.05 >=
                result.trajectory.samples[i - 1].positionMeters.x,
            "default wall calculated curve loops/backtracks along route direction"
        );
    }
}

void testInitialAccelerationIsPreservedAtTrajectoryStart()
{
    auto request = baseRequest();
    request.pathPointsMeters = {
        glm::dvec3(0.0, 0.0, 0.0),
        glm::dvec3(180.0, 0.0, 0.0)
    };
    request.initialVelocityMps = glm::dvec3(6.0, 0.0, 0.0);
    request.initialAccelerationMps2 = glm::dvec3(1.5, 0.0, 0.0);
    request.pointSpeedConstraints.push_back({180.0, 0.0});

    const auto result = game::navigation::RuckigRoutePlanner::plan(request);
    require(result.ready(), "initial-acceleration Ruckig route failed");
    require(
        glm::length(
            result.trajectory.samples.front().accelerationMps2 -
            request.initialAccelerationMps2
        ) < 1.0e-6,
        "Ruckig route lost the requested initial acceleration state"
    );
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
        {"straight route keeps moving start and finish", testStraightRouteUsesRuckigWithMovingStartAndFinish},
        {"diagonal stopped leg stays on coarse chord", testDiagonalStoppedLegStaysOnCoarseChord},
        {"clear corner keeps through velocity", testClearCornerGetsContinuousRuckigWaypointVelocity},
        {"blocked wide blend shrinks before stop", testBlockedWideBlendShrinksBeforeStopping},
        {"tight corner preserves safety", testTightCornerNeverTradesSafetyForThroughSpeed},
        {"default wall shallow corners stay moving", testDefaultWallDetourKeepsMovingThroughShallowCorners},
        {"initial acceleration is preserved", testInitialAccelerationIsPreservedAtTrajectoryStart},
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
