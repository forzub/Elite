#include <cmath>
#include <cstdlib>
#include <iostream>

#include "src/game/navigation/OrdinaryRoutePolicy.h"

namespace
{
bool near(double a, double b, double eps = 1.0e-6)
{
    return std::abs(a - b) <= eps;
}

int fail(const char* message)
{
    std::cerr << "ORDINARY ROUTE POLICY TESTS: FAIL: "
              << message << std::endl;
    return EXIT_FAILURE;
}
}

int main()
{
    using game::navigation::OrdinaryRoutePolicy;

    // Missing stage speed limit means 100% of the route limit.
    if (!near(
            OrdinaryRoutePolicy::stageSpeedLimitMps(400.0, 0.0),
            400.0))
    {
        return fail("default stage speed was not 100 percent");
    }

    if (!near(
            OrdinaryRoutePolicy::stageSpeedLimitMps(400.0, 220.0),
            220.0))
    {
        return fail("authored stage speed limit was not applied");
    }

    // A moving start must demand more lead than a stopped start when the
    // incoming speed requires a larger-radius turn/braking budget.
    const double stoppedLead =
        OrdinaryRoutePolicy::requiredStartLeadMeters(
            0.0, 400.0, 45.0, 20.0, 0.5, 30.0
        );
    const double movingLead =
        OrdinaryRoutePolicy::requiredStartLeadMeters(
            300.0, 400.0, 45.0, 20.0, 0.5, 30.0
        );
    if (!(movingLead > stoppedLead))
        return fail("moving-start lead did not grow with speed");

    // Terminal approach must be derived from braking physics, not a fixed
    // multi-kilometre constant.
    const double approach =
        OrdinaryRoutePolicy::requiredTerminalApproachMeters(
            400.0, 18.0, 45.0, 300.0, 30.0
        );
    const double expectedBraking =
        (400.0 * 400.0 - 18.0 * 18.0) / (2.0 * 45.0);
    if (!(approach >= expectedBraking * 1.25 - 1.0e-6))
        return fail("terminal approach does not cover braking distance");

    // Ordinary candidate preference is distance first; traversal time only
    // breaks equal-distance ties.
    if (!OrdinaryRoutePolicy::preferCandidate(
            1000.0, 20.0,
            1200.0, 10.0))
    {
        return fail("shorter route lost to faster longer route");
    }

    if (OrdinaryRoutePolicy::preferCandidate(
            1200.0, 5.0,
            1000.0, 50.0))
    {
        return fail("longer route beat shorter route on time");
    }

    std::cout << "ORDINARY ROUTE POLICY TESTS: PASS" << std::endl;
    return EXIT_SUCCESS;
}
