#include <cstdlib>
#include <iostream>

#include "src/game/navigation/planner/assisted/AssistedRoutePlannerApi.h"

namespace
{
int fail(const char* message)
{
    std::cerr
        << "ASSISTED ROUTE PLANNER V3 TESTS: FAIL: "
        << message << std::endl;
    return EXIT_FAILURE;
}
}

int main()
{
    using namespace game::navigation::planner;
    using namespace game::navigation::planner::assisted;

    Request request;
    request.start.positionMeters = glm::dvec3(0.0, 0.0, 0.0);
    request.start.forward = glm::dvec3(1.0, 0.0, 0.0);
    request.start.speedMps = 0.0;

    // Goal is behind the ship. The planner must author a real departure arc,
    // not a sharp polyline corner or reverse-flight shortcut.
    request.goal.positionMeters = glm::dvec3(-6000.0, 0.0, 0.0);
    request.goal.forward = glm::dvec3(-1.0, 0.0, 0.0);
    request.goal.speedMps = 0.0;

    request.vehicle.radiusMeters = 17.3;
    request.vehicle.lengthMeters = 30.0;
    request.vehicle.maxSpeedMps = 100.0;
    request.vehicle.acceleratingMps2 = 20.0;
    request.vehicle.brakingMps2 = 20.0;
    request.vehicle.lateralMps2 = 20.0;
    request.vehicle.maxAngularVelocityRadPerSecond = 0.5;
    request.vehicle.maxAngularAccelerationRadPerSecond2 = 0.5;

    request.geometry.protectedDepartureStraightMeters = 150.0;
    request.geometry.protectedArrivalStraightMeters = 700.0;
    request.geometry.minimumComfortRadiusMeters = 150.0;
    request.geometry.designSpeedFraction = 0.80;
    request.geometry.departurePlaneSamples = 24;
    request.geometry.departureSweepSamples = 17;
    request.geometry.shortlistCount = 24;

    const auto plan = AssistedRoutePlanner::plan(request);
    if (!plan.valid())
        return fail("ship-facing-away route was rejected");

    if (!plan.initialTurnPresent)
        return fail("ship-facing-away route has no authored departure arc");

    const double hardFloor =
        std::max(150.0, 30.0 * request.vehicle.radiusMeters);

    if (plan.initialTurnRadiusMeters + 1.0e-6 < hardFloor)
        return fail("departure arc violated hard comfort radius");

    bool foundArc = false;
    for (const auto& curve : plan.routeCurves)
    {
        if (curve.kind != RouteCurveKind::CircularArc)
            continue;
        foundArc = true;
        if (curve.arcRadiusMeters + 1.0e-6 < hardFloor)
            return fail("route contains sub-minimum circular arc");
    }

    if (!foundArc)
        return fail("route contains no circular motion primitive");

    // The first visible tunnel segment must leave in front of the ship.
    if (plan.routeCurves.empty())
        return fail("route has no curves");

    const glm::dvec3 firstDirection =
        glm::normalize(
            plan.routeCurves.front().endMeters -
            plan.routeCurves.front().startMeters
        );

    if (glm::dot(firstDirection, request.start.forward) < 0.999)
        return fail("first route segment does not leave in front of ship");

    std::cout
        << "ASSISTED ROUTE PLANNER V3 TESTS: PASS"
        << std::endl;
    return EXIT_SUCCESS;
}
