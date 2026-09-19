#include "src/world/navigation/local/PhysicalManeuverHorizon.h"
#include "src/game/navigation/NavigationExecutionSafetyProbeBuilder.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
using Horizon = world::navigation::PhysicalManeuverHorizon;
using SafetyProbes = game::navigation::NavigationExecutionSafetyProbeBuilder;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void requireNear(
    double actual,
    double expected,
    double tolerance,
    const std::string& message
)
{
    if (std::abs(actual - expected) > tolerance)
        throw std::runtime_error(message);
}

Horizon::Result evaluate(double speed)
{
    Horizon::Query query;
    query.speedMetersPerSecond = speed;
    query.accelerationMagnitudeMetersPerSecond2 = 0.0;
    query.snapshotAgeSeconds = 0.0;
    query.controlResponseReserveSeconds = 0.5;
    query.brakingAccelerationMetersPerSecond2 = 2.0;
    query.turnDistanceMeters = 10.0;
    query.safetyMarginMeters = 5.0;
    query.minimumDistanceMeters = 1.0;
    query.minimumLookAheadSeconds = 3.0;
    return Horizon::evaluate(query);
}

void testDistanceAndTimeGrowWithPhysicalStoppingNeed()
{
    const auto slow = evaluate(10.0);
    require(slow.valid, "10 m/s horizon must be valid");
    requireNear(slow.responseDistanceMeters, 5.0, 1.0e-12,
                "response coast distance mismatch");
    requireNear(slow.brakingDistanceMeters, 25.0, 1.0e-12,
                "braking distance mismatch");
    requireNear(slow.distanceMeters, 45.0, 1.0e-12,
                "physical horizon distance mismatch");
    requireNear(slow.lookAheadSeconds, 5.5, 1.0e-12,
                "dynamic look-ahead must include response + braking time");

    const auto fast = evaluate(20.0);
    require(fast.valid, "20 m/s horizon must be valid");
    requireNear(fast.distanceMeters, 125.0, 1.0e-12,
                "20 m/s physical horizon mismatch");
    requireNear(fast.lookAheadSeconds, 10.5, 1.0e-12,
                "20 m/s look-ahead mismatch");
    require(
        fast.distanceMeters > 2.0 * slow.distanceMeters,
        "distance horizon must reflect quadratic braking-distance growth"
    );
    require(
        fast.lookAheadSeconds > slow.lookAheadSeconds,
        "temporal horizon must grow with speed"
    );
}

void testResponseAccelerationIsConservative()
{
    Horizon::Query query;
    query.speedMetersPerSecond = 10.0;
    query.accelerationMagnitudeMetersPerSecond2 = 2.0;
    query.snapshotAgeSeconds = 0.25;
    query.controlResponseReserveSeconds = 0.75;
    query.brakingAccelerationMetersPerSecond2 = 2.0;
    query.minimumLookAheadSeconds = 1.0;

    const auto result = Horizon::evaluate(query);
    require(result.valid, "accelerating response horizon must be valid");
    requireNear(result.responseSeconds, 1.0, 1.0e-12,
                "response interval mismatch");
    requireNear(result.speedAtBrakeMetersPerSecond, 12.0, 1.0e-12,
                "speed-at-brake upper bound mismatch");
    requireNear(result.responseDistanceMeters, 11.0, 1.0e-12,
                "accelerating response distance mismatch");
    requireNear(result.brakingDistanceMeters, 36.0, 1.0e-12,
                "accelerating braking distance mismatch");
    requireNear(result.distanceMeters, 47.0, 1.0e-12,
                "accelerating total horizon mismatch");
    requireNear(result.lookAheadSeconds, 7.0, 1.0e-12,
                "accelerating temporal horizon mismatch");
}

void testExecutionSafetyProbeMathIsPureAndBounded()
{
    SafetyProbes::StoppingReserveQuery stopping;
    stopping.positionMapMeters = {10.0, 20.0, 30.0};
    stopping.velocityMapMetersPerSecond = {10.0, 0.0, 0.0};
    stopping.controlResponseReserveSeconds = 0.5;
    stopping.brakingAccelerationMetersPerSecond2 = 2.0;

    const auto a = SafetyProbes::buildStoppingReserve(stopping);
    const auto b = SafetyProbes::buildStoppingReserve(stopping);

    require(a.valid && a.active && b.valid && b.active,
            "stopping reserve probe must be valid and active");
    requireNear(a.distanceMeters, 30.0, 1.0e-12,
                "stopping reserve distance mismatch");
    requireNear(a.endMapMeters.x, 40.0, 1.0e-12,
                "stopping reserve endpoint mismatch");
    requireNear(a.endMapMeters.x, b.endMapMeters.x, 0.0,
                "same immutable input must produce identical probe output");

    SafetyProbes::ConstantAccelerationQuery forecast;
    forecast.positionMapMeters = {0.0, 0.0, 0.0};
    forecast.velocityMapMetersPerSecond = {10.0, 0.0, 0.0};
    forecast.accelerationMapMetersPerSecond2 = {2.0, 0.0, 0.0};
    forecast.durationSeconds = 2.0;
    forecast.maximumDistanceMeters = 15.0;

    const auto direct =
        SafetyProbes::buildConstantAccelerationProbe(forecast);
    require(direct.valid && direct.active,
            "constant acceleration probe must be active");
    requireNear(glm::length(direct.endMapMeters), 15.0, 1.0e-12,
                "constant acceleration probe must respect local horizon bound");

    const auto sampled =
        SafetyProbes::buildSampledConstantAccelerationForecast(forecast);
    require(sampled.valid && sampled.active,
            "sampled execution forecast must be active");
    require(sampled.pointCount ==
                SafetyProbes::kExecutedForecastSamples + 1,
            "sampled forecast point count changed");
    requireNear(
        glm::length(
            sampled.pointsMapMeters[sampled.pointCount - 1]
        ),
        15.0,
        1.0e-12,
        "sampled forecast endpoint must share the same bounded kinematics"
    );
}

}

int main()
{
    try
    {
        testDistanceAndTimeGrowWithPhysicalStoppingNeed();
        testResponseAccelerationIsConservative();
        testExecutionSafetyProbeMathIsPureAndBounded();
        std::cout << "PHYSICAL MANEUVER HORIZON TESTS: PASS\n";
        std::cout << " - distance horizon grows with response + v^2/(2a)\n";
        std::cout << " - dynamic look-ahead grows with response + braking time\n";
        std::cout << " - execution safety probes are pure bounded kinematics\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr << "PHYSICAL MANEUVER HORIZON TESTS: FAIL: "
                  << error.what() << "\n";
        return EXIT_FAILURE;
    }
}
