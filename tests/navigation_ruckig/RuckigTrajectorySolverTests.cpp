#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include <glm/glm.hpp>

#include "src/game/navigation/GravityFieldSystem.h"
#include "src/game/navigation/RuckigTrajectorySolver.h"

namespace
{
using namespace game::navigation;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

bool near(double a, double b, double tolerance)
{
    return std::abs(a - b) <= tolerance;
}

double distance(const glm::dvec3& a, const glm::dvec3& b)
{
    return glm::length(a - b);
}

RuckigTrajectoryRequest baseRequest()
{
    RuckigTrajectoryRequest request;
    request.systemId = 0;
    request.startUniverseTimeSeconds = 100.0;
    request.horizonSeconds = 12.0;
    request.sampleIntervalSeconds = 0.5;
    request.validationStepSeconds = 0.01;
    request.motionEnvelope.maxProperAccelerationMps2 =
        4.0 * StandardGravityMps2;
    request.motionEnvelope.maxProperJerkMps3 =
        2.0 * StandardGravityMps2;
    return request;
}

void requireEndpoint(
    const RuckigTrajectoryResult& result,
    const RuckigTrajectoryRequest& request,
    double positionTolerance = 1.0e-4,
    double velocityTolerance = 1.0e-5
)
{
    require(result.ok(), "Ruckig solve failed: " + result.prediction.message);
    require(!result.prediction.samples.empty(), "Ruckig solve returned no samples");

    const auto& end = result.prediction.samples.back();
    require(
        near(end.timeOffsetSeconds, request.horizonSeconds, 1.0e-7),
        "Ruckig result did not end at the requested horizon"
    );
    require(
        distance(end.state.positionMeters, request.targetPositionMeters) <=
            positionTolerance,
        "Ruckig endpoint position mismatch"
    );
    require(
        distance(end.state.velocityMps, request.targetVelocityMps) <=
            velocityTolerance,
        "Ruckig endpoint velocity mismatch"
    );
}

void testStationaryLocalTransfer()
{
    auto request = baseRequest();
    request.initialState.positionMeters = glm::dvec3(0.0);
    request.initialState.velocityMps = glm::dvec3(0.0);
    request.initialState.accelerationMps2 = glm::dvec3(0.0);
    request.initialProperAccelerationMps2 = glm::dvec3(0.0);
    request.targetPositionMeters = glm::dvec3(100.0, 0.0, 0.0);
    request.targetVelocityMps = glm::dvec3(0.0);

    const auto result = RuckigTrajectorySolver::solve(request);
    requireEndpoint(result, request);
    require(
        result.prediction.diagnostics.maxAppliedProperAccelerationMps2 <=
            request.motionEnvelope.maxProperAccelerationMps2 * 1.00001,
        "Ruckig candidate exceeded Elite proper-acceleration envelope"
    );
    require(
        result.prediction.diagnostics.maxAppliedProperJerkMps3 <=
            request.motionEnvelope.maxProperJerkMps3 * 1.00001,
        "Ruckig candidate exceeded Elite proper-jerk envelope"
    );
}

void testOrbitalScaleWorldCoordinatesUseLocalFrame()
{
    auto request = baseRequest();
    request.initialState.positionMeters =
        glm::dvec3(1.0e9, -2.0e9, 3.0e9);
    request.initialState.velocityMps =
        glm::dvec3(15000.0, -5000.0, 2000.0);
    request.initialState.accelerationMps2 = glm::dvec3(0.0);
    request.initialProperAccelerationMps2 = glm::dvec3(0.0);

    request.targetVelocityMps = request.initialState.velocityMps;
    request.targetPositionMeters =
        request.initialState.positionMeters +
        request.targetVelocityMps * request.horizonSeconds +
        glm::dvec3(200.0, -40.0, 25.0);

    const auto result = RuckigTrajectorySolver::solve(request);
    requireEndpoint(result, request, 5.0e-4, 1.0e-5);
}

void testGravityCompensatedCoMovingFrame()
{
    auto request = baseRequest();
    request.horizonSeconds = 2.0;
    request.sampleIntervalSeconds = 0.1;
    request.validationStepSeconds = 0.005;

    GravityBody earth;
    earth.id = "earth-test";
    earth.centerMeters = glm::dvec3(0.0);
    earth.radiusMeters = 6371000.0;
    earth.gravitationalParameterM3s2 = 3.986004418e14;
    earth.influenceRadiusMeters = 1.0e9;
    request.gravityBodies.push_back(earth);

    request.initialState.positionMeters =
        glm::dvec3(earth.radiusMeters + 400000.0, 0.0, 0.0);
    request.initialState.velocityMps = glm::dvec3(0.0);
    request.initialState.accelerationMps2 = glm::dvec3(0.0);
    request.initialProperAccelerationMps2 = glm::dvec3(0.0);

    const auto initialGravity = GravityFieldSystem::sample(
        request.initialState.positionMeters,
        request.gravityBodies
    );
    require(
        glm::length(initialGravity.accelerationMps2) > 1.0,
        "gravity test body did not create a material field"
    );

    request.targetPositionMeters =
        request.initialState.positionMeters +
        0.5 * initialGravity.accelerationMps2 *
            request.horizonSeconds * request.horizonSeconds;
    request.targetVelocityMps =
        initialGravity.accelerationMps2 * request.horizonSeconds;

    const auto result = RuckigTrajectorySolver::solve(request);
    requireEndpoint(result, request, 1.0e-4, 1.0e-5);
    require(
        glm::length(result.prediction.samples.front().gravityAccelerationMps2) > 1.0,
        "Ruckig output lost Elite gravity diagnostics"
    );
}

void testInfeasibleHorizonIsRejected()
{
    auto request = baseRequest();
    request.horizonSeconds = 1.0;
    request.sampleIntervalSeconds = 0.1;
    request.validationStepSeconds = 0.01;
    request.motionEnvelope.maxProperAccelerationMps2 = 1.0;
    request.motionEnvelope.maxProperJerkMps3 = 1.0;
    request.initialState.positionMeters = glm::dvec3(0.0);
    request.initialState.velocityMps = glm::dvec3(0.0);
    request.initialState.accelerationMps2 = glm::dvec3(0.0);
    request.targetPositionMeters = glm::dvec3(1000.0, 0.0, 0.0);
    request.targetVelocityMps = glm::dvec3(0.0);

    const auto result = RuckigTrajectorySolver::solve(request);
    require(!result.ok(), "physically infeasible one-second transfer was accepted");
}

void benchmarkSolver()
{
    auto request = baseRequest();
    request.validationStepSeconds = 0.05;
    request.initialState.positionMeters = glm::dvec3(0.0);
    request.initialState.velocityMps = glm::dvec3(0.0);
    request.initialState.accelerationMps2 = glm::dvec3(0.0);
    request.targetPositionMeters = glm::dvec3(100.0, 0.0, 0.0);
    request.targetVelocityMps = glm::dvec3(0.0);

    constexpr int Iterations = 500;
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < Iterations; ++i)
    {
        const auto result = RuckigTrajectorySolver::solve(request);
        require(result.ok(), "Ruckig benchmark solve failed");
    }
    const auto stop = std::chrono::steady_clock::now();
    const double totalUs =
        std::chrono::duration<double, std::micro>(stop - start).count();

    std::cout
        << "RUCKIG BENCHMARK: " << Iterations
        << " solves, total=" << totalUs << " us, avg="
        << (totalUs / Iterations) << " us/solve\n";
}

} // namespace

int main()
{
    try
    {
        testStationaryLocalTransfer();
        std::cout << "[PASS] stationary local transfer\n";

        testOrbitalScaleWorldCoordinatesUseLocalFrame();
        std::cout << "[PASS] orbital-scale co-moving frame\n";

        testGravityCompensatedCoMovingFrame();
        std::cout << "[PASS] gravity-compensated co-moving frame\n";

        testInfeasibleHorizonIsRejected();
        std::cout << "[PASS] infeasible horizon rejected\n";

        benchmarkSolver();
        std::cout << "NAVIGATION RUCKIG SPIKE: PASS\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr << "NAVIGATION RUCKIG SPIKE: FAIL: "
                  << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
