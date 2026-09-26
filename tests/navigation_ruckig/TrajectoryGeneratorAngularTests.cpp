#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include <glm/glm.hpp>

#include "src/world/navigation/TrajectoryGenerator.h"

namespace
{

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

double length(const glm::dvec3& value)
{
    return std::sqrt(glm::dot(value, value));
}

void testRotatingTerminalAngularProgramIsPhysicallyBounded()
{
    world::navigation::TrajectoryGenerationRequest request;
    request.systemId = 0;
    request.frameId = "angular-program-test";
    request.startUniverseTimeSeconds = 1000.0;

    request.vehicle.collisionRadiusMeters = 1.0;
    request.vehicle.preferredClearanceMeters = 0.0;
    request.vehicle.maxSpeedMps = 20.0;
    request.vehicle.maxForwardAccelerationMps2 = 5.0;
    request.vehicle.maxBrakingAccelerationMps2 = 5.0;
    request.vehicle.maxLateralAccelerationMps2 = 5.0;
    request.vehicle.maxAngularVelocityRadPerSecond = 1.0;
    request.vehicle.maxAngularAccelerationRadPerSecond2 = 0.5;

    request.pathPointsMeters = {
        glm::dvec3(0.0, 0.0, 0.0),
        glm::dvec3(100.0, 0.0, 0.0),
        glm::dvec3(200.0, 0.0, 0.0)
    };

    request.initialVelocityMps = glm::dvec3(0.0);
    request.initialAccelerationMps2 = glm::dvec3(0.0);
    request.hasInitialOrientation = true;
    request.initialForward = glm::dvec3(0.0, 0.0, -1.0);
    request.initialUp = glm::dvec3(0.0, 1.0, 0.0);
    request.hasInitialAngularVelocity = true;
    request.initialAngularVelocityRadPerSecond = glm::dvec3(0.0);

    request.hasTerminalVelocity = true;
    request.terminalVelocityMps = glm::dvec3(1.0, 0.0, 0.0);
    request.hasTerminalOrientation = true;
    request.terminalForward = glm::dvec3(1.0, 0.0, 0.0);
    request.terminalUp = glm::dvec3(0.0, 1.0, 0.0);
    request.terminalOrientationBlendDistanceMeters = 180.0;
    request.hasTerminalAngularVelocity = true;
    request.terminalAngularVelocityRadPerSecond =
        glm::dvec3(0.0, 0.2, 0.0);

    const auto result =
        world::navigation::TrajectoryGenerator::generate(request);

    require(
        result.ready(),
        "rotating-terminal trajectory failed: " +
            result.trajectory.message
    );
    require(
        result.trajectory.angularKinematicsAuthored,
        "trajectory did not publish authored angular kinematics"
    );
    require(
        result.trajectory.samples.size() > 4,
        "trajectory did not contain enough samples"
    );

    const double maxOmega =
        request.vehicle.maxAngularVelocityRadPerSecond;
    const double maxAlpha =
        request.vehicle.maxAngularAccelerationRadPerSecond2;

    for (std::size_t i = 0;
         i < result.trajectory.samples.size();
         ++i)
    {
        const auto& sample =
            result.trajectory.samples[i];

        require(
            length(sample.angularVelocityRadPerSecond) <=
                maxOmega + 1.0e-6,
            "angular speed exceeded vehicle capability"
        );

        if (i == 0)
            continue;

        const auto& previous =
            result.trajectory.samples[i - 1];
        const double dt =
            sample.timeOffsetSeconds -
            previous.timeOffsetSeconds;
        require(dt > 0.0, "trajectory sample time is not monotonic");

        const double alpha =
            length(
                sample.angularVelocityRadPerSecond -
                previous.angularVelocityRadPerSecond
            ) / dt;

        require(
            alpha <= maxAlpha + 1.0e-5,
            "angular acceleration exceeded vehicle capability"
        );
    }

    const auto& terminal =
        result.trajectory.samples.back();
    const glm::dvec3 terminalForward =
        terminal.orientation *
        glm::dvec3(0.0, 0.0, -1.0);

    const double terminalForwardError =
        std::acos(
            std::clamp(
                glm::dot(
                    glm::normalize(terminalForward),
                    glm::normalize(request.terminalForward)
                ),
                -1.0,
                1.0
            )
        );

    require(
        terminalForwardError <=
            0.08726646259971647 + 1.0e-6,
        "terminal orientation missed the five-degree capture envelope"
    );
    require(
        length(
            terminal.angularVelocityRadPerSecond -
            request.terminalAngularVelocityRadPerSecond
        ) <= 0.05 + 1.0e-6,
        "terminal angular velocity was not matched"
    );
}

} // namespace

int main()
{
    try
    {
        testRotatingTerminalAngularProgramIsPhysicallyBounded();
        std::cout
            << "TRAJECTORY GENERATOR ANGULAR TESTS: PASS\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "TRAJECTORY GENERATOR ANGULAR TESTS: FAIL: "
            << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
