#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include <glm/glm.hpp>

#include "src/world/navigation/GuidanceTunnel.h"
#include "src/world/navigation/NavigationOrientation.h"

namespace
{

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

world::navigation::NavigationVehicleProfile testVehicle()
{
    world::navigation::NavigationVehicleProfile vehicle;
    vehicle.collisionRadiusMeters = 20.0;
    vehicle.maxSpeedMps = 180.0;
    vehicle.maxForwardAccelerationMps2 = 20.0;
    vehicle.maxBrakingAccelerationMps2 = 20.0;
    vehicle.maxLateralAccelerationMps2 = 10.0;
    vehicle.maxAngularVelocityRadPerSecond = 1.0;
    vehicle.maxAngularAccelerationRadPerSecond2 = 1.0;
    return vehicle;
}

world::navigation::Trajectory makeStraightTrajectory(
    double lengthMeters,
    double sampleSpacingMeters,
    double speedMps
)
{
    world::navigation::Trajectory trajectory;
    trajectory.status = world::navigation::TrajectoryStatus::Ready;
    trajectory.systemId = 0;
    trajectory.frameId = "bounded-reconnect-test";
    trajectory.startUniverseTimeSeconds = 1000.0;
    trajectory.lengthMeters = lengthMeters;
    trajectory.durationSeconds = speedMps > 0.0
        ? lengthMeters / speedMps
        : 0.0;

    const glm::dquat orientation =
        world::navigation::orientationForForwardUp(
            glm::dvec3(1.0, 0.0, 0.0),
            glm::dvec3(0.0, 1.0, 0.0)
        );

    const int steps = static_cast<int>(std::ceil(
        lengthMeters / sampleSpacingMeters
    ));
    for (int i = 0; i <= steps; ++i)
    {
        const double progress = std::min(
            lengthMeters,
            sampleSpacingMeters * static_cast<double>(i)
        );
        world::navigation::TrajectorySample sample;
        sample.pathProgressMeters = progress;
        sample.sourcePathProgressMeters = progress;
        sample.positionMeters = glm::dvec3(progress, 0.0, 0.0);
        sample.orientation = orientation;
        sample.speedMps = speedMps;
        sample.timeOffsetSeconds = speedMps > 0.0
            ? progress / speedMps
            : 0.0;
        sample.universeTimeSeconds =
            trajectory.startUniverseTimeSeconds + sample.timeOffsetSeconds;
        trajectory.samples.push_back(sample);
    }
    return trajectory;
}

world::navigation::GuidanceTunnelRequest baseRequest(
    const world::navigation::Trajectory& trajectory
)
{
    world::navigation::GuidanceTunnelRequest request;
    request.trajectory = &trajectory;
    request.buildMode =
        world::navigation::GuidanceTunnelBuildMode::ReconnectCurrentPose;
    request.currentPositionMeters = glm::dvec3(0.0, 18.0, 0.0);
    request.currentOrientation = trajectory.samples.front().orientation;
    request.currentVelocityMps = glm::dvec3(100.0, 0.0, 0.0);
    request.terminalPositionMeters = trajectory.samples.back().positionMeters;
    request.terminalOrientation = trajectory.samples.back().orientation;
    request.gateSpacingMeters = 50.0;
    request.gateWidthMeters = 120.0;
    request.gateHeightMeters = 180.0;
    request.lateralToleranceMeters = 30.0;
    request.verticalToleranceMeters = 40.0;
    request.startCaptureDistanceMeters = 300.0;
    request.terminalAlignmentDistanceMeters = 1000.0;
    request.vehicle = testVehicle();
    request.curveSampleSpacingMeters = 14.0;
    request.curveChordErrorMeters = 0.15;
    request.maxSmoothSupportLevel = 1;
    request.minimumTurnRadiusMeters = 0.0;
    request.reconnectPlanningLatencySeconds = 0.75;
    return request;
}

void testLongReconnectUsesPhysicsBoundedHorizonAndKeepsRealTerminal()
{
    const auto trajectory = makeStraightTrajectory(12000.0, 100.0, 100.0);
    const auto request = baseRequest(trajectory);

    const auto tunnel =
        world::navigation::GuidanceTunnelBuilder::build(request);

    require(tunnel.valid, "bounded reconnect did not build");
    require(tunnel.reconnectSolveBounded,
        "long stable-terminal reconnect still solved the whole route");
    require(!tunnel.reconnectFullSolveFallback,
        "bounded reconnect unexpectedly fell back to full solve");

    const double brakingDistance =
        100.0 * 100.0 /
        (2.0 * request.vehicle.maxBrakingAccelerationMps2);
    const double latencyDistance =
        100.0 * request.reconnectPlanningLatencySeconds;
    require(tunnel.reconnectLookaheadMeters + 1.0e-6 >=
            brakingDistance + latencyDistance,
        "bounded reconnect horizon does not cover braking plus solve latency");
    require(tunnel.reconnectSolveEndProgressMeters < 3000.0,
        "bounded reconnect consumed an implausibly large fraction of long route");
    require(tunnel.reconnectSolveEndProgressMeters + 5000.0 <
            trajectory.samples.back().pathProgressMeters,
        "bounded reconnect left no substantial immutable accepted tail");

    require(glm::length(
        tunnel.gates.front().positionMeters - request.currentPositionMeters
    ) <= 1.0e-9,
        "bounded reconnect does not start at current hull position");
    require(glm::length(
        tunnel.gates.back().positionMeters -
        trajectory.samples.back().positionMeters
    ) <= 1.0e-9,
        "local solve endpoint was mistaken for the real docking terminal");

    bool sawStitchedTail = false;
    for (const auto& gate : tunnel.gates)
    {
        if (gate.sourceTrajectoryProgressMeters >
            tunnel.reconnectSolveEndProgressMeters + 1000.0)
        {
            sawStitchedTail = true;
            break;
        }
    }
    require(sawStitchedTail,
        "accepted trajectory tail was not stitched after local reconnect");
}

void testMovedTerminalKeepsSafeFullReconnectFallback()
{
    const auto trajectory = makeStraightTrajectory(3000.0, 100.0, 80.0);
    auto request = baseRequest(trajectory);
    request.currentVelocityMps = glm::dvec3(60.0, 0.0, 0.0);
    request.terminalPositionMeters += glm::dvec3(0.0, 25.0, 0.0);

    const auto tunnel =
        world::navigation::GuidanceTunnelBuilder::build(request);

    require(tunnel.valid, "moved-terminal fallback did not build");
    require(!tunnel.reconnectSolveBounded,
        "materially moved terminal incorrectly used immutable-tail shortcut");
    require(tunnel.reconnectFullSolveFallback,
        "moved terminal did not report the full reconnect fallback");
    require(glm::length(
        tunnel.gates.back().positionMeters - request.terminalPositionMeters
    ) <= 1.0e-9,
        "full reconnect fallback lost the live terminal position");
}

void testShortRemainingRouteDoesNotInventLocalTerminal()
{
    const auto trajectory = makeStraightTrajectory(700.0, 50.0, 50.0);
    auto request = baseRequest(trajectory);
    request.currentPositionMeters = glm::dvec3(450.0, 10.0, 0.0);
    request.currentVelocityMps = glm::dvec3(50.0, 0.0, 0.0);

    const auto tunnel =
        world::navigation::GuidanceTunnelBuilder::build(request);

    require(tunnel.valid, "short remaining reconnect did not build");
    require(!tunnel.reconnectSolveBounded,
        "short remaining route was needlessly split into a local horizon");
    require(glm::length(
        tunnel.gates.back().positionMeters - request.terminalPositionMeters
    ) <= 1.0e-9,
        "short reconnect changed the actual terminal");
}

} // namespace

int main()
{
    const struct
    {
        const char* name;
        void (*fn)();
    } tests[] = {
        {
            "long reconnect uses bounded horizon and keeps terminal",
            testLongReconnectUsesPhysicsBoundedHorizonAndKeepsRealTerminal
        },
        {
            "moved terminal uses full reconnect fallback",
            testMovedTerminalKeepsSafeFullReconnectFallback
        },
        {
            "short remaining route keeps actual terminal",
            testShortRemainingRouteDoesNotInventLocalTerminal
        }
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

    std::cout << passed << "/"
              << (sizeof(tests) / sizeof(tests[0]))
              << " local-horizon guidance tests passed\n";
    return passed == (sizeof(tests) / sizeof(tests[0]))
        ? EXIT_SUCCESS
        : EXIT_FAILURE;
}
