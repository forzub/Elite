#include <cmath>
#include <cstdlib>
#include <iostream>

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
        glm::dot(actual/la, expected/le) >= cosine;
}
}

int main()
{
    using namespace game::navigation::planner::assisted::lab;

    Request request;
    request.start.positionMeters =
        glm::dvec3(0.0, 0.0, 0.0);
    request.start.tangent =
        glm::dvec3(1.0, 0.0, 0.0);
    request.start.speedMps = 0.0;

    // Deliberately place the goal behind the initial tangent. There is no
    // obstacle and no discrete "turn angle" input. The optimizer must derive
    // non-zero curvature from the constrained minimum problem itself.
    request.goal.positionMeters =
        glm::dvec3(-6000.0, 0.0, 0.0);
    request.goal.tangent =
        glm::dvec3(-1.0, 0.0, 0.0);
    request.goal.speedMps = 0.0;

    request.hullRadiusMeters = 17.3;
    request.designSpeedMps = 80.0;
    request.maxLateralAccelerationMps2 = 20.0;
    request.minimumComfortRadiusMeters = 500.0;

    request.protectedStartStraightMeters = 150.0;
    request.protectedGoalStraightMeters = 700.0;

    request.samplesPerBezier = 128;
    request.maxPatternIterations = 700;

    const Result result =
        ContinuousAssistedPathOptimizer::optimize(request);

    if (!result.valid)
        return fail("reversed-heading free-space problem was not feasible");

    if (result.sampledCenterlineMeters.size() < 64)
        return fail("optimizer returned too few centerline samples");

    const double requiredRadius =
        std::max(
            request.minimumComfortRadiusMeters,
            request.designSpeedMps *
                request.designSpeedMps /
                request.maxLateralAccelerationMps2
        );

    if (result.diagnostics.minimumRadiusMeters +
            1.0e-3 <
        requiredRadius)
    {
        return fail("optimized path violated minimum-radius constraint");
    }

    const glm::dvec3 startCurve =
        request.start.positionMeters +
        glm::normalize(request.start.tangent) *
            request.protectedStartStraightMeters;
    const glm::dvec3 goalCurve =
        request.goal.positionMeters -
        glm::normalize(request.goal.tangent) *
            request.protectedGoalStraightMeters;

    if (!nearDirection(
            result.firstControl1Meters - startCurve,
            request.start.tangent,
            0.999999))
    {
        return fail("start tangent boundary condition was not preserved");
    }

    if (!nearDirection(
            goalCurve - result.secondControl2Meters,
            request.goal.tangent,
            0.999999))
    {
        return fail("goal tangent boundary condition was not preserved");
    }

    if (!(result.diagnostics.pathLengthMeters > 6000.0))
        return fail("reversed-heading solution did not form a real turn");

    if (!(result.diagnostics.pathLengthMeters < 30000.0))
        return fail("optimizer escaped into an absurdly long low-curvature loop");

    if (!(result.diagnostics.maxConstraintViolation <= 1.0e-7))
        return fail("optimizer reported residual hard-constraint violation");

    std::cout
        << "CONTINUOUS ASSISTED OPTIMIZER LAB: PASS\n"
        << " objective_evaluations="
        << result.diagnostics.objectiveEvaluations
        << " accepted_steps="
        << result.diagnostics.acceptedSteps
        << " length_m="
        << result.diagnostics.pathLengthMeters
        << " min_radius_m="
        << result.diagnostics.minimumRadiusMeters
        << " objective="
        << result.diagnostics.objective
        << "\n";

    return EXIT_SUCCESS;
}
