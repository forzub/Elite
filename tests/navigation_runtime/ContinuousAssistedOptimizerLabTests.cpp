#include <cmath>
#include <cstdlib>
#include <iostream>

#include <glm/glm.hpp>

#include "src/game/navigation/planner/assisted/lab/ContinuousAssistedPathOptimizer.h"

namespace
{
int fail(const char* message)
{
    std::cerr
        << "CONTINUOUS ASSISTED OPTIMIZER LAB: FAIL: "
        << message << "\n";
    return EXIT_FAILURE;
}

bool nearDirection(
    const glm::dvec3& actual,
    const glm::dvec3& expected,
    double cosine
)
{
    const double la = glm::length(actual);
    const double le = glm::length(expected);
    if (la <= 1.0e-9 || le <= 1.0e-9)
        return false;

    return
        glm::dot(actual / la, expected / le) >= cosine;
}
}

int main()
{
    using namespace
        game::navigation::planner::assisted::lab;

    // Case 1: reversed heading. This still requires a real turn and preserves
    // a true authored comfort-radius floor, but maxSpeed^2/a_lat is no longer
    // a hard geometric radius.
    Request reversed;
    reversed.start.positionMeters =
        {0.0, 0.0, 0.0};
    reversed.start.tangent =
        {1.0, 0.0, 0.0};
    reversed.start.speedMps = 0.0;

    reversed.goal.positionMeters =
        {-6000.0, 0.0, 0.0};
    reversed.goal.tangent =
        {-1.0, 0.0, 0.0};
    reversed.goal.speedMps = 0.0;

    reversed.hullRadiusMeters = 17.3;
    reversed.designSpeedMps = 80.0;
    reversed.acceleratingMps2 = 20.0;
    reversed.brakingMps2 = 20.0;
    reversed.maxLateralAccelerationMps2 = 20.0;
    reversed.minimumComfortRadiusMeters = 500.0;

    reversed.protectedStartStraightMeters = 150.0;
    reversed.protectedGoalStraightMeters = 700.0;

    reversed.samplesPerBezier = 128;
    reversed.maxPatternIterations = 700;

    const Result turn =
        ContinuousAssistedPathOptimizer::optimize(
            reversed
        );

    if (!turn.valid)
        return fail("reversed-heading free-space problem was not feasible");

    if (turn.sampledCenterlineMeters.size() < 64)
        return fail("optimizer returned too few centerline samples");

    if (turn.segments.size() < 2)
        return fail("optimizer returned no usable transition geometry");

    if (turn.diagnostics.minimumRadiusMeters +
            1.0e-3 <
        reversed.minimumComfortRadiusMeters)
    {
        return fail("optimized path violated authored comfort radius");
    }

    const glm::dvec3 startCurve =
        reversed.start.positionMeters +
        glm::normalize(reversed.start.tangent) *
            reversed.protectedStartStraightMeters;
    const glm::dvec3 goalCurve =
        reversed.goal.positionMeters -
        glm::normalize(reversed.goal.tangent) *
            reversed.protectedGoalStraightMeters;

    if (!nearDirection(
            turn.firstControl1Meters - startCurve,
            reversed.start.tangent,
            0.999999))
    {
        return fail("start tangent boundary condition was not preserved");
    }

    if (!nearDirection(
            goalCurve - turn.secondControl2Meters,
            reversed.goal.tangent,
            0.999999))
    {
        return fail("goal tangent boundary condition was not preserved");
    }

    if (!(turn.diagnostics.pathLengthMeters > 6000.0))
        return fail("reversed-heading solution did not form a real turn");

    if (!(turn.diagnostics.pathLengthMeters < 30000.0))
        return fail("optimizer escaped into an absurdly long turn");

    if (!(turn.diagnostics.estimatedTravelTimeSeconds > 0.0))
        return fail("optimizer did not produce a physical travel-time estimate");

    if (!(turn.diagnostics.maxConstraintViolation <= 1.0e-7))
        return fail("optimizer reported residual hard-constraint violation");

    // Case 2: high cruise ceiling. This is the regression for the 80-110 km
    // loops observed in production. The optimizer must be allowed to slow
    // locally instead of inflating all turns to R=Vmax^2/a_lat.
    Request highSpeed;
    highSpeed.start.positionMeters =
        {0.0, 0.0, 0.0};
    highSpeed.start.tangent =
        {1.0, 0.0, 0.0};
    highSpeed.start.speedMps = 0.0;

    highSpeed.goal.positionMeters =
        {12000.0, 5000.0, 0.0};
    highSpeed.goal.tangent =
        {1.0, 0.0, 0.0};
    highSpeed.goal.speedMps = 0.0;

    highSpeed.hullRadiusMeters = 17.3;
    highSpeed.designSpeedMps = 400.0;
    highSpeed.acceleratingMps2 = 20.0;
    highSpeed.brakingMps2 = 20.0;
    highSpeed.maxLateralAccelerationMps2 = 20.0;
    highSpeed.minimumComfortRadiusMeters = 0.0;

    highSpeed.protectedStartStraightMeters = 500.0;
    highSpeed.protectedGoalStraightMeters = 700.0;

    highSpeed.samplesPerBezier = 128;
    highSpeed.maxPatternIterations = 500;

    const Result fast =
        ContinuousAssistedPathOptimizer::optimize(
            highSpeed
        );

    if (!fast.valid)
        return fail("high-speed free-space problem was not feasible");

    if (!(fast.diagnostics.lowerBoundLengthMeters > 10000.0))
        return fail("high-speed lower-bound diagnostic is invalid");

    if (!(fast.diagnostics.stretchRatio < 2.5))
        return fail("high-speed optimizer still prefers a giant detour");

    if (!(fast.diagnostics.pathLengthMeters < 30000.0))
        return fail("high-speed optimizer exceeded route-length sanity bound");

    if (!(fast.diagnostics.estimatedTravelTimeSeconds < 180.0))
        return fail("high-speed optimizer produced an implausibly slow route");

    if (!(fast.diagnostics.maxConstraintViolation <= 1.0e-7))
        return fail("high-speed optimizer retained hard-constraint violation");

    if (!std::isfinite(fast.diagnostics.maxLateralUtilization) ||
        fast.diagnostics.maxLateralUtilization < 0.0)
    {
        return fail("high-speed optimizer produced invalid lateral-utilization diagnostics");
    }

    if (!std::isfinite(fast.diagnostics.brakingDemandEnergy) ||
        fast.diagnostics.brakingDemandEnergy < 0.0)
    {
        return fail("high-speed optimizer produced invalid braking-demand diagnostics");
    }

    std::cout
        << "CONTINUOUS ASSISTED OPTIMIZER LAB: PASS\n"
        << " reversed_length_m="
        << turn.diagnostics.pathLengthMeters
        << " reversed_stretch="
        << turn.diagnostics.stretchRatio
        << " high_speed_length_m="
        << fast.diagnostics.pathLengthMeters
        << " high_speed_lower_bound_m="
        << fast.diagnostics.lowerBoundLengthMeters
        << " high_speed_stretch="
        << fast.diagnostics.stretchRatio
        << " high_speed_eta_s="
        << fast.diagnostics.estimatedTravelTimeSeconds
        << " high_speed_peak_lat_util="
        << fast.diagnostics.maxLateralUtilization
        << " high_speed_brake_energy="
        << fast.diagnostics.brakingDemandEnergy
        << " objective_evaluations="
        << (turn.diagnostics.objectiveEvaluations +
            fast.diagnostics.objectiveEvaluations)
        << "\n";

    return EXIT_SUCCESS;
}
