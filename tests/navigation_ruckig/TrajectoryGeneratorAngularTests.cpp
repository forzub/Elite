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

    const auto& penultimate =
        result.trajectory.samples[result.trajectory.samples.size() - 2];
    const auto& terminal =
        result.trajectory.samples.back();
    const double terminalDt =
        terminal.timeOffsetSeconds -
        penultimate.timeOffsetSeconds;
    require(
        length(
            penultimate.angularVelocityRadPerSecond -
            request.terminalAngularVelocityRadPerSecond
        ) <= maxAlpha * terminalDt + 1.0e-6,
        "angular planner deferred terminal omega correction to the final sample"
    );
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


void testTranslationSlowsWhenAngularTerminalNeedsMoreTime()
{
    world::navigation::TrajectoryGenerationRequest request;
    request.systemId = 0;
    request.frameId = "angular-time-relaxation-test";
    request.startUniverseTimeSeconds = 2000.0;

    request.vehicle.collisionRadiusMeters = 1.0;
    request.vehicle.preferredClearanceMeters = 0.0;
    request.vehicle.maxSpeedMps = 400.0;
    request.vehicle.maxForwardAccelerationMps2 = 200.0;
    request.vehicle.maxBrakingAccelerationMps2 = 200.0;
    request.vehicle.maxLateralAccelerationMps2 = 200.0;
    request.vehicle.maxAngularVelocityRadPerSecond = 0.6;
    request.vehicle.maxAngularAccelerationRadPerSecond2 = 0.15;

    // The translationally fastest solution is only a few seconds long, while
    // the hull needs materially longer to rotate from -Z to +X and arrive
    // with the requested terminal omega. This must be solved by slowing the
    // same route, not by rejecting the navigation task or widening alpha.
    request.pathPointsMeters = {
        glm::dvec3(0.0, 0.0, 0.0),
        glm::dvec3(200.0, 0.0, 0.0),
        glm::dvec3(400.0, 0.0, 0.0)
    };

    request.initialVelocityMps = glm::dvec3(0.0);
    request.initialAccelerationMps2 = glm::dvec3(0.0);
    request.hasInitialOrientation = true;
    request.initialForward = glm::dvec3(0.0, 0.0, -1.0);
    request.initialUp = glm::dvec3(0.0, 1.0, 0.0);
    request.hasInitialAngularVelocity = true;
    request.initialAngularVelocityRadPerSecond = glm::dvec3(0.0);

    request.hasTerminalVelocity = true;
    request.terminalVelocityMps = glm::dvec3(0.0);
    request.hasTerminalOrientation = true;
    request.terminalForward = glm::dvec3(1.0, 0.0, 0.0);
    request.terminalUp = glm::dvec3(0.0, 1.0, 0.0);
    request.terminalOrientationBlendDistanceMeters = 400.0;
    request.hasTerminalAngularVelocity = true;
    request.terminalAngularVelocityRadPerSecond =
        glm::dvec3(0.0, 0.1, 0.0);

    const auto result =
        world::navigation::TrajectoryGenerator::generate(request);

    require(
        result.ready(),
        "angular-time relaxation failed: " + result.trajectory.message
    );
    require(
        result.trajectory.message.find("angular-speed-relaxed") !=
            std::string::npos,
        "planner did not slow translation for the angular boundary"
    );
    require(
        result.trajectory.durationSeconds > 4.0,
        "angular-time relaxation did not materially increase route time"
    );

    const auto& terminal = result.trajectory.samples.back();
    require(
        length(
            terminal.angularVelocityRadPerSecond -
            request.terminalAngularVelocityRadPerSecond
        ) <= 0.05 + 1.0e-6,
        "relaxed route still missed terminal angular velocity"
    );
}

void testAuthoredPathGeometryIsNotRedrawn()
{
    world::navigation::TrajectoryGenerationRequest request;
    request.systemId = 0;
    request.frameId = "authored-geometry-test";
    request.vehicle.collisionRadiusMeters = 1.0;
    request.vehicle.maxSpeedMps = 50.0;
    request.vehicle.maxForwardAccelerationMps2 = 10.0;
    request.vehicle.maxBrakingAccelerationMps2 = 10.0;
    request.vehicle.maxLateralAccelerationMps2 = 10.0;
    request.vehicle.maxAngularVelocityRadPerSecond = 2.0;
    request.vehicle.maxAngularAccelerationRadPerSecond2 = 2.0;
    request.pathGeometryAlreadyAuthored = true;
    request.pathPointsMeters = {
        {0.0, 0.0, 0.0},
        {100.0, 0.0, 0.0},
        {200.0, 0.0, 20.0},
        {300.0, 0.0, 60.0}
    };

    const auto expected = request.pathPointsMeters;
    const auto result =
        world::navigation::TrajectoryGenerator::generate(request);

    require(
        result.ready(),
        "authored geometry trajectory failed: " +
            result.trajectory.message
    );
    require(
        result.executionGuidePointsMeters.size() == expected.size(),
        "trajectory layer changed authored geometry point count"
    );
    for (std::size_t i = 0; i < expected.size(); ++i)
    {
        require(
            length(
                result.executionGuidePointsMeters[i] -
                expected[i]
            ) <= 1.0e-9,
            "trajectory layer moved an authored geometry point"
        );
    }

    for (const auto& sample : result.trajectory.samples)
    {
        require(
            length(sample.accelerationMps2) <=
                15.0,
            "authored route generated an implausible acceleration spike"
        );
    }
}

void testAuthoredArcStaysInsideAccelerationEnvelope()
{
    world::navigation::TrajectoryGenerationRequest request;
    request.systemId = 0;
    request.frameId = "authored-arc-envelope-test";
    request.vehicle.collisionRadiusMeters = 1.0;
    request.vehicle.maxSpeedMps = 100.0;
    request.vehicle.maxForwardAccelerationMps2 = 10.0;
    request.vehicle.maxBrakingAccelerationMps2 = 10.0;
    request.vehicle.maxLateralAccelerationMps2 = 5.0;
    request.vehicle.maxAngularVelocityRadPerSecond = 2.0;
    request.vehicle.maxAngularAccelerationRadPerSecond2 = 2.0;
    request.pathGeometryAlreadyAuthored = true;

    constexpr double Radius = 1000.0;
    constexpr int Samples = 65;
    for (int i = 0; i < Samples; ++i)
    {
        const double a =
            (0.5 * 3.14159265358979323846) *
            static_cast<double>(i) /
            static_cast<double>(Samples - 1);
        request.pathPointsMeters.push_back({
            Radius * std::sin(a),
            0.0,
            Radius * (1.0 - std::cos(a))
        });
    }

    const auto result =
        world::navigation::TrajectoryGenerator::generate(request);

    require(
        result.ready(),
        "authored circular route exceeded acceleration envelope: " +
            result.trajectory.message
    );

    for (std::size_t i=0;i<result.trajectory.samples.size();++i)
    {
        const auto& sample=result.trajectory.samples[i];
        const glm::dvec3 tangent =
            length(sample.velocityMps) > 1.0e-9
                ? glm::normalize(sample.velocityMps)
                : glm::normalize(
                      sample.orientation *
                      glm::dvec3(0.0,0.0,-1.0)
                  );
        const double along =
            glm::dot(sample.accelerationMps2, tangent);
        const glm::dvec3 lateral =
            sample.accelerationMps2 - tangent * along;
        const double lateralMagnitude=length(lateral);

        const auto sampleDiagnostic=[&]()
        {
            return
                " sample=" + std::to_string(i) +
                " speed=" + std::to_string(sample.speedMps) +
                " along=" + std::to_string(along) +
                " lateral=" + std::to_string(lateralMagnitude) +
                " accel=(" +
                    std::to_string(sample.accelerationMps2.x) + "," +
                    std::to_string(sample.accelerationMps2.y) + "," +
                    std::to_string(sample.accelerationMps2.z) + ")" +
                " tangent=(" +
                    std::to_string(tangent.x) + "," +
                    std::to_string(tangent.y) + "," +
                    std::to_string(tangent.z) + ")";
        };

        require(
            along <= request.vehicle.maxForwardAccelerationMps2 + 1.0e-5,
            "authored arc exceeded forward acceleration" +
                sampleDiagnostic()
        );
        require(
            along >= -request.vehicle.maxBrakingAccelerationMps2 - 1.0e-5,
            "authored arc exceeded braking acceleration" +
                sampleDiagnostic()
        );
        require(
            lateralMagnitude <=
                request.vehicle.maxLateralAccelerationMps2 + 1.0e-5,
            "authored arc exceeded lateral acceleration" +
                sampleDiagnostic()
        );
    }
}

void testLongStraightCruisesBeforeLocalTurnAndStop()
{
    world::navigation::TrajectoryGenerationRequest request;
    request.systemId = 0;
    request.frameId = "local-speed-keyframes";
    request.vehicle.collisionRadiusMeters = 1.0;
    request.vehicle.maxSpeedMps = 500.0;
    request.vehicle.maxForwardAccelerationMps2 = 20.0;
    request.vehicle.maxBrakingAccelerationMps2 = 20.0;
    request.vehicle.maxLateralAccelerationMps2 = 5.0;
    request.vehicle.maxAngularVelocityRadPerSecond = 5.0;
    request.vehicle.maxAngularAccelerationRadPerSecond2 = 10.0;
    request.pathPointsMeters = {
        {0.0, 0.0, 0.0},
        {10000.0, 0.0, 0.0},
        {10010.0, 0.0, 1.0},
        {10020.0, 0.0, 3.0},
        {10030.0, 0.0, 6.0},
        {10040.0, 0.0, 10.0}
    };
    const auto result =
        world::navigation::TrajectoryGenerator::generate(request);
    require(result.ready(),
            "keyframed route failed: " + result.trajectory.message);
    double straightPeak = 0.0;
    double turnPeak = 0.0;
    for (const auto& sample : result.trajectory.samples)
    {
        if (sample.pathProgressMeters < 9900.0)
            straightPeak = std::max(straightPeak, sample.speedMps);
        else
            turnPeak = std::max(turnPeak, sample.speedMps);
        require(sample.speedMps <= 400.001,
                "ordinary cruise exceeded 0.8 ship maximum");
    }
    require(straightPeak > 250.0,
            "local slow turn capped the long straight");
    require(turnPeak < straightPeak * 0.8,
            "ship did not brake before the local turn");
    require(result.trajectory.samples.back().speedMps < 0.01,
            "route did not stop at HOLD");
}

} // namespace

int main()
{
    try
    {
        testRotatingTerminalAngularProgramIsPhysicallyBounded();
        testTranslationSlowsWhenAngularTerminalNeedsMoreTime();
        testAuthoredPathGeometryIsNotRedrawn();
        testAuthoredArcStaysInsideAccelerationEnvelope();
        testLongStraightCruisesBeforeLocalTurnAndStop();
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
