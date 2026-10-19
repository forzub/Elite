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

    // Regress the high-speed Assisted handoff: do not count braking AND
    // curvature radius twice as independent initial straight distances.
    // Keep Newtonian/legacy's stricter formula unchanged.
    const double fullLegacyLead =
        OrdinaryRoutePolicy::requiredStartLeadMeters(
            500.0, 500.0, 15.0, 24.5166333333, 0.5, 30.0
        );
    const double assistedLead =
        OrdinaryRoutePolicy::requiredAssistedStartLeadMeters(
            500.0, 500.0, 15.0, 24.5166333333, 0.5, 30.0
        );
    const double designSpeed = 400.0;
    const double expectedBrakingMeters =
        (500.0 * 500.0 - designSpeed * designSpeed) /
            (2.0 * 15.0);
    const double expectedTurnRadiusMeters =
        designSpeed * designSpeed / 24.5166333333;
    if (!near(assistedLead,
              expectedBrakingMeters + expectedTurnRadiusMeters,
              1.0e-5))
        return fail("Assisted straight still double-counted braking/radius");
    if (!near(fullLegacyLead, 2.0 * assistedLead, 1.0e-5))
        return fail("Newtonian/legacy start lead changed");

    // Assisted retains a real measured geometry minimum even at rest.
    if (!near(
            OrdinaryRoutePolicy::requiredAssistedStartLeadMeters(
                0.0, 500.0, 15.0, 24.5166333333, 0.5, 30.0
            ), 1000.0))
        return fail("stationary Assisted start lost the 1 km minimum");

    // HUD/manual tunnel readability is geometric, not speed dependent.
    // With 500 m sparse frames and a 30 degree maximum heading change, a
    // quarter-turn must span at least three frame intervals.
    const double tunnelRadius =
        OrdinaryRoutePolicy::requiredGuidanceTunnelRadiusMeters(
            500.0,
            30.0
        );
    const double expectedTunnelRadius =
        500.0 / 0.52359877559829887308;
    if (!near(tunnelRadius, expectedTunnelRadius, 1.0e-6))
        return fail("tunnel-safe radius did not follow frame spacing");

    const double capitalShipRadius =
        OrdinaryRoutePolicy::requiredGuidanceTunnelRadiusMeters(
            500.0,
            1000.0
        );
    if (!(capitalShipRadius > tunnelRadius))
        return fail("tunnel-safe radius did not scale with vehicle length");

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
