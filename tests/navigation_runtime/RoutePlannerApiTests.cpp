#include "src/game/navigation/planner/RoutePlannerApi.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void requireNear(double actual, double expected, double tolerance, const char* message)
{
    if (std::abs(actual - expected) > tolerance)
        throw std::runtime_error(message);
}

} // namespace

int main()
{
    try
    {
        game::navigation::planner::RoutePlanRequest request;
        request.startMeters = {5000.0, 1200.0, 0.0};
        request.goalMeters = {0.0, 0.0, 0.0};
        request.terminalOutward = {1.0, 0.0, 0.0};
        request.terminalReferenceDistanceMeters = 300.0;
        request.agentRadiusMeters = 10.0;
        request.maxSpeedMps = 120.0;
        request.acceleratingMps2 = 8.0;
        request.brakingMps2 = 8.0;
        request.lateralMps2 = 5.0;
        request.roundTurns = false;
        request.gateSpacingMeters = 500.0;
        request.terminalGateSpacingMeters = 250.0;

        const auto plan =
            game::navigation::planner::RoutePlanner::plan(request);

        require(plan.valid(), "generic RoutePlanner API did not produce a route");
        require(
            plan.executionGates.size() >= 2,
            "generic RoutePlanner API returned no executable geometry"
        );
        requireNear(
            glm::length(
                plan.executionGates.back().positionMeters -
                request.goalMeters
            ),
            0.0,
            1.0e-6,
            "generic RoutePlanner changed the requested terminal point"
        );

        std::cout << "ROUTE PLANNER API TESTS: PASS\n";
        std::cout << " - generic caller reaches exact goal without docking-private types\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "ROUTE PLANNER API TESTS: FAIL: "
                  << error.what() << "\n";
        return 1;
    }
}
