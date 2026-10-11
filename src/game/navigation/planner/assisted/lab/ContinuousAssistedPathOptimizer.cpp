#include "src/game/navigation/planner/assisted/lab/ContinuousAssistedPathOptimizer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "src/world/navigation/NavigationObstacleGeometry.h"

namespace game::navigation::planner::assisted::lab
{
namespace
{

constexpr double Epsilon = 1.0e-9;

bool finite3(const glm::dvec3& value)
{
    return
        std::isfinite(value.x) &&
        std::isfinite(value.y) &&
        std::isfinite(value.z);
}

glm::dvec3 normalizedOr(
    const glm::dvec3& value,
    const glm::dvec3& fallback
)
{
    const double n = glm::length(value);
    if (std::isfinite(n) && n > Epsilon)
        return value / n;
    return fallback;
}

struct CubicSample
{
    glm::dvec3 position {0.0};
    glm::dvec3 first {0.0};
    glm::dvec3 second {0.0};
};

CubicSample sampleCubic(
    const glm::dvec3& p0,
    const glm::dvec3& p1,
    const glm::dvec3& p2,
    const glm::dvec3& p3,
    double t
)
{
    const double u = 1.0 - t;

    CubicSample out;
    out.position =
        (u*u*u) * p0 +
        (3.0*u*u*t) * p1 +
        (3.0*u*t*t) * p2 +
        (t*t*t) * p3;

    out.first =
        (3.0*u*u) * (p1-p0) +
        (6.0*u*t) * (p2-p1) +
        (3.0*t*t) * (p3-p2);

    out.second =
        (6.0*u) * (p2 - 2.0*p1 + p0) +
        (6.0*t) * (p3 - 2.0*p2 + p1);

    return out;
}

double curvatureOf(const CubicSample& sample)
{
    const double speed = glm::length(sample.first);
    if (!std::isfinite(speed) || speed <= 1.0e-8)
        return std::numeric_limits<double>::infinity();

    return
        glm::length(
            glm::cross(sample.first, sample.second)
        ) /
        (speed * speed * speed);
}

struct Variables
{
    glm::dvec3 join {0.0};
    glm::dvec3 joinDirectionRaw {1.0, 0.0, 0.0};
    double startHandleMeters = 1.0;
    double joinHandleMeters = 1.0;
    double goalHandleMeters = 1.0;
};

struct Evaluation
{
    bool finite = false;
    bool feasible = false;

    double violation = std::numeric_limits<double>::infinity();
    double objective = std::numeric_limits<double>::infinity();

    double length = 0.0;
    double maxCurvature = 0.0;
    double lateralAccelerationEnergy = 0.0;
    double curvatureVariationEnergy = 0.0;

    glm::dvec3 joinDirection {1.0, 0.0, 0.0};
    std::array<glm::dvec3, 6> control {};
    std::vector<glm::dvec3> points;
};

glm::dvec3 deterministicPerpendicular(
    const glm::dvec3& tangent
)
{
    const glm::dvec3 seed =
        std::abs(tangent.y) < 0.8
            ? glm::dvec3(0.0, 1.0, 0.0)
            : glm::dvec3(1.0, 0.0, 0.0);

    return normalizedOr(
        glm::cross(tangent, seed),
        glm::dvec3(0.0, 0.0, 1.0)
    );
}

double effectiveMinimumRadius(const Request& request)
{
    const double comfort =
        std::max(0.0, request.minimumComfortRadiusMeters);

    const double dynamic =
        request.maxLateralAccelerationMps2 > Epsilon &&
        request.designSpeedMps > 0.0
            ? request.designSpeedMps *
                request.designSpeedMps /
                request.maxLateralAccelerationMps2
            : 0.0;

    return std::max(comfort, dynamic);
}

Evaluation evaluate(
    const Request& request,
    const Variables& variables,
    std::size_t* objectiveEvaluations
)
{
    if (objectiveEvaluations)
        ++(*objectiveEvaluations);

    Evaluation out;

    const glm::dvec3 startTangent =
        normalizedOr(
            request.start.tangent,
            glm::dvec3(0.0, 0.0, -1.0)
        );
    const glm::dvec3 goalTangent =
        normalizedOr(
            request.goal.tangent,
            glm::dvec3(0.0, 0.0, -1.0)
        );
    const glm::dvec3 joinTangent =
        normalizedOr(
            variables.joinDirectionRaw,
            startTangent
        );

    const double startHandle =
        std::max(1.0, variables.startHandleMeters);
    const double joinHandle =
        std::max(1.0, variables.joinHandleMeters);
    const double goalHandle =
        std::max(1.0, variables.goalHandleMeters);

    const glm::dvec3 startCurve =
        request.start.positionMeters +
        startTangent *
            std::max(
                0.0,
                request.protectedStartStraightMeters
            );

    const glm::dvec3 goalCurve =
        request.goal.positionMeters -
        goalTangent *
            std::max(
                0.0,
                request.protectedGoalStraightMeters
            );

    out.control[0] = startCurve;
    out.control[1] =
        startCurve +
        startTangent * startHandle;
    out.control[2] =
        variables.join -
        joinTangent * joinHandle;
    out.control[3] =
        variables.join +
        joinTangent * joinHandle;
    out.control[4] =
        goalCurve -
        goalTangent * goalHandle;
    out.control[5] = goalCurve;
    out.joinDirection = joinTangent;

    const std::size_t samplesPerBezier =
        std::max<std::size_t>(
            24,
            request.samplesPerBezier
        );

    out.points.reserve(samplesPerBezier * 2 + 3);

    const double minimumRadius =
        effectiveMinimumRadius(request);
    const double maxCurvatureAllowed =
        minimumRadius > Epsilon
            ? 1.0 / minimumRadius
            : std::numeric_limits<double>::infinity();

    const double referenceLength =
        std::max(
            1.0,
            glm::length(
                request.goal.positionMeters -
                request.start.positionMeters
            )
        );
    const double lateralAccelerationScale =
        std::max(
            Epsilon,
            request.maxLateralAccelerationMps2
        );
    const double curvatureScale =
        minimumRadius > Epsilon
            ? 1.0 / minimumRadius
            : 1.0 / referenceLength;
    const double curvatureDerivativeScale =
        std::max(
            1.0e-15,
            curvatureScale / referenceLength
        );

    double previousCurvature = 0.0;
    bool havePreviousCurvature = false;
    glm::dvec3 previousPoint {0.0};
    bool havePreviousPoint = false;

    double maxViolation = 0.0;

    const auto sampleSegment =
        [&](const glm::dvec3& p0,
            const glm::dvec3& p1,
            const glm::dvec3& p2,
            const glm::dvec3& p3,
            bool skipFirst)
        {
            for (std::size_t i = 0;
                 i <= samplesPerBezier;
                 ++i)
            {
                if (skipFirst && i == 0)
                    continue;

                const double t =
                    static_cast<double>(i) /
                    static_cast<double>(samplesPerBezier);

                const CubicSample sample =
                    sampleCubic(p0,p1,p2,p3,t);

                if (!finite3(sample.position) ||
                    !finite3(sample.first) ||
                    !finite3(sample.second))
                {
                    maxViolation =
                        std::numeric_limits<double>::infinity();
                    return false;
                }

                const double curvature =
                    curvatureOf(sample);

                if (!std::isfinite(curvature))
                {
                    maxViolation =
                        std::numeric_limits<double>::infinity();
                    return false;
                }

                if (std::isfinite(maxCurvatureAllowed) &&
                    curvature > maxCurvatureAllowed)
                {
                    const double normalized =
                        (curvature - maxCurvatureAllowed) /
                        std::max(
                            maxCurvatureAllowed,
                            1.0e-12
                        );
                    maxViolation =
                        std::max(maxViolation, normalized);
                }

                if (havePreviousPoint)
                {
                    const double ds =
                        glm::length(
                            sample.position -
                            previousPoint
                        );
                    out.length += ds;

                    const double lateralAcceleration =
                        request.designSpeedMps *
                        request.designSpeedMps *
                        curvature;
                    const double normalizedLateralAcceleration =
                        lateralAcceleration /
                        lateralAccelerationScale;

                    out.lateralAccelerationEnergy +=
                        normalizedLateralAcceleration *
                        normalizedLateralAcceleration *
                        (ds / referenceLength);

                    if (havePreviousCurvature &&
                        ds > 1.0e-6)
                    {
                        const double derivative =
                            (curvature -
                             previousCurvature) / ds;
                        const double normalizedDerivative =
                            derivative /
                            curvatureDerivativeScale;

                        out.curvatureVariationEnergy +=
                            normalizedDerivative *
                            normalizedDerivative *
                            (ds / referenceLength);
                    }

                    if (!world::navigation::
                        segmentClearOfNavigationObstacles(
                            previousPoint,
                            sample.position,
                            request.obstacles,
                            request.hullRadiusMeters))
                    {
                        maxViolation =
                            std::max(maxViolation, 1.0);
                    }
                }

                out.maxCurvature =
                    std::max(
                        out.maxCurvature,
                        curvature
                    );

                out.points.push_back(sample.position);
                previousPoint = sample.position;
                previousCurvature = curvature;
                havePreviousPoint = true;
                havePreviousCurvature = true;
            }

            return true;
        };

    if (!sampleSegment(
            out.control[0],
            out.control[1],
            out.control[2],
            variables.join,
            false))
    {
        return out;
    }

    if (!sampleSegment(
            variables.join,
            out.control[3],
            out.control[4],
            out.control[5],
            true))
    {
        return out;
    }

    // Protected straight collision checks are hard constraints too.
    if (!world::navigation::
        segmentClearOfNavigationObstacles(
            request.start.positionMeters,
            out.control[0],
            request.obstacles,
            request.hullRadiusMeters))
    {
        maxViolation = std::max(maxViolation, 1.0);
    }

    if (!world::navigation::
        segmentClearOfNavigationObstacles(
            out.control[5],
            request.goal.positionMeters,
            request.obstacles,
            request.hullRadiusMeters))
    {
        maxViolation = std::max(maxViolation, 1.0);
    }

    out.finite =
        std::isfinite(out.length) &&
        std::isfinite(out.maxCurvature) &&
        std::isfinite(out.lateralAccelerationEnergy) &&
        std::isfinite(out.curvatureVariationEnergy) &&
        std::isfinite(maxViolation);

    if (!out.finite)
        return out;

    out.violation = maxViolation;
    out.feasible = maxViolation <= 1.0e-7;

    out.objective =
        request.weights.pathLength *
            (out.length / referenceLength) +
        request.weights.lateralAccelerationEnergy *
            out.lateralAccelerationEnergy +
        request.weights.curvatureVariation *
            out.curvatureVariationEnergy;

    return out;
}

bool better(
    const Evaluation& candidate,
    const Evaluation& current
)
{
    if (!candidate.finite)
        return false;
    if (!current.finite)
        return true;

    if (candidate.feasible != current.feasible)
        return candidate.feasible;

    if (candidate.feasible)
        return
            candidate.objective + 1.0e-9 <
            current.objective;

    if (candidate.violation + 1.0e-9 <
        current.violation)
    {
        return true;
    }

    if (std::abs(
            candidate.violation -
            current.violation
        ) <= 1.0e-9)
    {
        return
            candidate.objective + 1.0e-9 <
            current.objective;
    }

    return false;
}

Variables initialGuess(const Request& request)
{
    const glm::dvec3 startTangent =
        normalizedOr(
            request.start.tangent,
            glm::dvec3(0.0, 0.0, -1.0)
        );
    const glm::dvec3 goalTangent =
        normalizedOr(
            request.goal.tangent,
            glm::dvec3(0.0, 0.0, -1.0)
        );

    const glm::dvec3 startCurve =
        request.start.positionMeters +
        startTangent *
            std::max(
                0.0,
                request.protectedStartStraightMeters
            );

    const glm::dvec3 goalCurve =
        request.goal.positionMeters -
        goalTangent *
            std::max(
                0.0,
                request.protectedGoalStraightMeters
            );

    const glm::dvec3 chord =
        goalCurve - startCurve;
    const double chordLength =
        std::max(1.0, glm::length(chord));

    glm::dvec3 joinDirection =
        normalizedOr(
            chord,
            goalTangent
        );

    glm::dvec3 side =
        glm::cross(
            startTangent,
            joinDirection
        );

    if (glm::length(side) <= 1.0e-6)
        side =
            deterministicPerpendicular(startTangent);

    side = normalizedOr(
        side,
        deterministicPerpendicular(startTangent)
    );

    const double minimumRadius =
        std::max(
            1.0,
            effectiveMinimumRadius(request)
        );

    Variables out;
    out.join =
        0.5 * (startCurve + goalCurve) +
        side *
            std::max(
                2.0 * minimumRadius,
                0.25 * chordLength
            );

    out.joinDirectionRaw = joinDirection;

    const double handle =
        std::max(
            2.0 * minimumRadius,
            0.25 * chordLength
        );

    out.startHandleMeters = handle;
    out.joinHandleMeters = handle;
    out.goalHandleMeters = handle;
    return out;
}

} // namespace

Result ContinuousAssistedPathOptimizer::optimize(
    const Request& request
)
{
    Result result;

    if (!finite3(request.start.positionMeters) ||
        !finite3(request.start.tangent) ||
        !finite3(request.goal.positionMeters) ||
        !finite3(request.goal.tangent) ||
        request.hullRadiusMeters < 0.0 ||
        request.designSpeedMps < 0.0 ||
        request.maxLateralAccelerationMps2 <= 0.0 ||
        request.minimumComfortRadiusMeters < 0.0)
    {
        result.message =
            "invalid continuous Assisted optimizer request";
        return result;
    }

    Variables variables =
        initialGuess(request);

    std::size_t objectiveEvaluations = 0;
    Evaluation current =
        evaluate(
            request,
            variables,
            &objectiveEvaluations
        );

    const glm::dvec3 startCurve =
        request.start.positionMeters +
        normalizedOr(
            request.start.tangent,
            glm::dvec3(0.0,0.0,-1.0)
        ) *
        std::max(
            0.0,
            request.protectedStartStraightMeters
        );

    const glm::dvec3 goalCurve =
        request.goal.positionMeters -
        normalizedOr(
            request.goal.tangent,
            glm::dvec3(0.0,0.0,-1.0)
        ) *
        std::max(
            0.0,
            request.protectedGoalStraightMeters
        );

    const double scale =
        std::max(
            100.0,
            glm::length(goalCurve - startCurve)
        );

    std::array<double,9> steps = {
        0.20*scale,
        0.20*scale,
        0.20*scale,
        0.50,
        0.50,
        0.50,
        0.15*scale,
        0.15*scale,
        0.15*scale
    };

    std::size_t acceptedSteps = 0;

    auto get =
        [&](const Variables& v, std::size_t index)
        {
            switch(index)
            {
                case 0: return v.join.x;
                case 1: return v.join.y;
                case 2: return v.join.z;
                case 3: return v.joinDirectionRaw.x;
                case 4: return v.joinDirectionRaw.y;
                case 5: return v.joinDirectionRaw.z;
                case 6: return v.startHandleMeters;
                case 7: return v.joinHandleMeters;
                default:return v.goalHandleMeters;
            }
        };

    auto set =
        [&](Variables& v,
            std::size_t index,
            double value)
        {
            switch(index)
            {
                case 0: v.join.x=value; break;
                case 1: v.join.y=value; break;
                case 2: v.join.z=value; break;
                case 3: v.joinDirectionRaw.x=value; break;
                case 4: v.joinDirectionRaw.y=value; break;
                case 5: v.joinDirectionRaw.z=value; break;
                case 6:
                    v.startHandleMeters =
                        std::max(1.0,value);
                    break;
                case 7:
                    v.joinHandleMeters =
                        std::max(1.0,value);
                    break;
                case 8:
                    v.goalHandleMeters =
                        std::max(1.0,value);
                    break;
            }
        };

    for (std::size_t iteration=0;
         iteration <
            std::max<std::size_t>(
                1,
                request.maxPatternIterations
            );
         ++iteration)
    {
        bool improved = false;

        for (std::size_t axis=0;
             axis<steps.size();
             ++axis)
        {
            Variables bestVariables = variables;
            Evaluation bestEvaluation = current;

            for (const double direction :
                 {-1.0, 1.0})
            {
                Variables candidate = variables;
                set(
                    candidate,
                    axis,
                    get(candidate,axis) +
                        direction * steps[axis]
                );

                const Evaluation evaluation =
                    evaluate(
                        request,
                        candidate,
                        &objectiveEvaluations
                    );

                if (better(
                        evaluation,
                        bestEvaluation))
                {
                    bestVariables = candidate;
                    bestEvaluation = evaluation;
                }
            }

            if (better(bestEvaluation,current))
            {
                variables = bestVariables;
                current = bestEvaluation;
                improved = true;
                ++acceptedSteps;
            }
        }

        if (!improved)
        {
            for (double& step : steps)
                step *= 0.70;
        }

        const double maxStep =
            *std::max_element(
                steps.begin(),
                steps.end()
            );

        if (maxStep < 1.0e-3)
            break;
    }

    result.valid = current.feasible;
    result.message =
        result.valid
            ? "continuous constrained Assisted path ready"
            : "continuous optimizer did not reach a feasible path";

    result.joinMeters = variables.join;
    result.joinTangent = current.joinDirection;

    result.firstControl1Meters =
        current.control[1];
    result.firstControl2Meters =
        current.control[2];
    result.secondControl1Meters =
        current.control[3];
    result.secondControl2Meters =
        current.control[4];

    result.sampledCenterlineMeters =
        std::move(current.points);

    result.diagnostics.objectiveEvaluations =
        objectiveEvaluations;
    result.diagnostics.acceptedSteps =
        acceptedSteps;
    result.diagnostics.pathLengthMeters =
        current.length;
    result.diagnostics.maxCurvaturePerMeter =
        current.maxCurvature;
    result.diagnostics.minimumRadiusMeters =
        current.maxCurvature > 1.0e-12
            ? 1.0 / current.maxCurvature
            : std::numeric_limits<double>::infinity();
    result.diagnostics.lateralAccelerationEnergy =
        current.lateralAccelerationEnergy;
    result.diagnostics.curvatureVariationEnergy =
        current.curvatureVariationEnergy;
    result.diagnostics.objective =
        current.objective;
    result.diagnostics.maxConstraintViolation =
        current.violation;

    return result;
}

} // namespace game::navigation::planner::assisted::lab
