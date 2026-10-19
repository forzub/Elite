#include "src/game/navigation/planner/assisted/lab/ContinuousAssistedPathOptimizer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "src/world/navigation/NavigationObstacleGeometry.h"

namespace game::navigation::planner::assisted::lab
{
namespace
{

constexpr double Epsilon = 1.0e-9;
constexpr double MinimumUsefulSpeedMps = 0.25;

bool finite3(const glm::dvec3& value) noexcept
{
    return
        std::isfinite(value.x) &&
        std::isfinite(value.y) &&
        std::isfinite(value.z);
}

glm::dvec3 normalizedOr(
    const glm::dvec3& value,
    const glm::dvec3& fallback
) noexcept
{
    const double n = glm::length(value);
    if (std::isfinite(n) && n > Epsilon)
        return value / n;

    const double f = glm::length(fallback);
    if (std::isfinite(f) && f > Epsilon)
        return fallback / f;

    return {0.0, 0.0, -1.0};
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
) noexcept
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

double curvatureOf(const CubicSample& sample) noexcept
{
    const double derivativeLength =
        glm::length(sample.first);

    if (!std::isfinite(derivativeLength) ||
        derivativeLength <= 1.0e-8)
    {
        return std::numeric_limits<double>::infinity();
    }

    return
        glm::length(
            glm::cross(sample.first, sample.second)
        ) /
        (derivativeLength *
         derivativeLength *
         derivativeLength);
}

struct CurvatureShapeStats
{
    bool finite = true;
    double turnRadians = 0.0;
    double curvatureUniformity = 0.0;
};

CurvatureShapeStats curvatureShapeOfCubic(
    const glm::dvec3& p0,
    const glm::dvec3& p1,
    const glm::dvec3& p2,
    const glm::dvec3& p3,
    std::size_t samples
) noexcept
{
    CurvatureShapeStats out;
    samples = std::max<std::size_t>(24, samples);

    double firstMoment = 0.0;   // integral kappa ds == turn angle
    double secondMoment = 0.0;  // integral kappa^2 ds
    double thirdMoment = 0.0;   // integral kappa^3 ds

    CubicSample previous =
        sampleCubic(p0, p1, p2, p3, 0.0);
    double previousCurvature = curvatureOf(previous);

    if (!std::isfinite(previousCurvature))
    {
        out.finite = false;
        return out;
    }

    for (std::size_t i = 1; i <= samples; ++i)
    {
        const double t =
            static_cast<double>(i) /
            static_cast<double>(samples);
        const CubicSample current =
            sampleCubic(p0, p1, p2, p3, t);
        const double currentCurvature =
            curvatureOf(current);
        const double ds =
            glm::length(
                current.position -
                previous.position
            );

        if (!std::isfinite(currentCurvature) ||
            !std::isfinite(ds))
        {
            out.finite = false;
            return out;
        }

        if (ds > Epsilon)
        {
            const double kappa =
                0.5 *
                (previousCurvature +
                 currentCurvature);
            const double kappa2 = kappa * kappa;

            firstMoment += kappa * ds;
            secondMoment += kappa2 * ds;
            thirdMoment += kappa2 * kappa * ds;
        }

        previous = current;
        previousCurvature = currentCurvature;
    }

    out.turnRadians = firstMoment;

    // Use turn angle dtheta = kappa ds as the measure. Straight portions
    // therefore carry zero weight without any arbitrary "is this a turn?"
    // threshold. For constant curvature:
    //
    //   C * A / B^2 - 1 == 0
    //
    // where A=int(k ds), B=int(k^2 ds), C=int(k^3 ds).
    // Flattened/elliptical turns have a positive dimensionless variance.
    if (firstMoment > Epsilon &&
        secondMoment > Epsilon)
    {
        const double raw =
            thirdMoment *
            firstMoment /
            (secondMoment * secondMoment) -
            1.0;

        out.curvatureUniformity =
            std::max(0.0, raw);
    }

    out.finite =
        std::isfinite(out.turnRadians) &&
        std::isfinite(out.curvatureUniformity);
    return out;
}

glm::dvec3 deterministicPerpendicular(
    const glm::dvec3& tangent
) noexcept
{
    const glm::dvec3 t =
        normalizedOr(tangent, {1.0, 0.0, 0.0});

    const glm::dvec3 seed =
        std::abs(t.y) < 0.8
            ? glm::dvec3(0.0, 1.0, 0.0)
            : glm::dvec3(1.0, 0.0, 0.0);

    return normalizedOr(
        glm::cross(t, seed),
        {0.0, 0.0, 1.0}
    );
}

struct Variables
{
    glm::dvec3 midpoint {0.0};
    glm::dvec3 cruiseDirectionRaw {1.0, 0.0, 0.0};

    double halfStraightMeters = 0.0;

    double startBoundaryHandleMeters = 1.0;
    double startCruiseHandleMeters = 1.0;
    double goalCruiseHandleMeters = 1.0;
    double goalBoundaryHandleMeters = 1.0;
};

struct SamplePoint
{
    glm::dvec3 position {0.0};
    double curvaturePerMeter = 0.0;
};

struct Evaluation
{
    bool finite = false;
    bool feasible = false;

    double violation =
        std::numeric_limits<double>::infinity();
    double objective =
        std::numeric_limits<double>::infinity();

    double length = 0.0;
    double lowerBoundLength = 0.0;
    double stretchRatio = 0.0;
    double estimatedTravelTimeSeconds = 0.0;

    double maxCurvature = 0.0;
    double maxLateralUtilization = 0.0;
    double lateralAccelerationEnergy = 0.0;
    double brakingDemandEnergy = 0.0;

    double totalCurvatureRadians = 0.0;
    double curvatureConcentrationEnergy = 0.0;
    double curvatureUniformityEnergy = 0.0;
    double maxAngularVelocityUtilization = 0.0;
    double maxAngularAccelerationUtilization = 0.0;
    double angularVelocityEnergy = 0.0;
    double angularAccelerationEnergy = 0.0;

    double curvatureVariationEnergy = 0.0;

    glm::dvec3 cruiseDirection {1.0, 0.0, 0.0};
    glm::dvec3 firstEnd {0.0};
    glm::dvec3 secondStart {0.0};

    std::array<glm::dvec3, 8> control {};
    std::vector<SamplePoint> samples;
};

double speedLimitForCurvature(
    const Request& request,
    double curvaturePerMeter
) noexcept
{
    double limit =
        std::max(0.0, request.designSpeedMps);

    if (curvaturePerMeter > 1.0e-12 &&
        request.maxLateralAccelerationMps2 > Epsilon)
    {
        limit =
            std::min(
                limit,
                std::sqrt(
                    request.maxLateralAccelerationMps2 /
                    curvaturePerMeter
                )
            );
    }

    // Following a curved centerline also rotates the hull tangent. Lateral
    // acceleration alone is therefore insufficient: v*kappa must remain
    // inside the real angular-rate authority as well.
    if (curvaturePerMeter > 1.0e-12 &&
        request.maxAngularVelocityRadPerSecond > Epsilon)
    {
        limit =
            std::min(
                limit,
                request.maxAngularVelocityRadPerSecond /
                    curvaturePerMeter
            );
    }

    return std::max(0.0, limit);
}

double fallbackLongitudinalAuthority(
    double authored,
    double lateral
) noexcept
{
    if (std::isfinite(authored) && authored > Epsilon)
        return authored;
    if (std::isfinite(lateral) && lateral > Epsilon)
        return lateral;
    return 1.0;
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
            {0.0, 0.0, -1.0}
        );
    const glm::dvec3 goalTangent =
        normalizedOr(
            request.goal.tangent,
            startTangent
        );
    const glm::dvec3 cruiseDirection =
        normalizedOr(
            variables.cruiseDirectionRaw,
            normalizedOr(
                request.goal.positionMeters -
                    request.start.positionMeters,
                startTangent
            )
        );

    out.cruiseDirection = cruiseDirection;

    const double startProtected =
        std::max(
            0.0,
            request.protectedStartStraightMeters
        );
    const double goalProtected =
        std::max(
            0.0,
            request.protectedGoalStraightMeters
        );

    const glm::dvec3 startCurve =
        request.start.positionMeters +
        startTangent * startProtected;
    const glm::dvec3 goalCurve =
        request.goal.positionMeters -
        goalTangent * goalProtected;

    const double halfStraight =
        std::max(0.0, variables.halfStraightMeters);

    const glm::dvec3 firstEnd =
        variables.midpoint -
        cruiseDirection * halfStraight;
    const glm::dvec3 secondStart =
        variables.midpoint +
        cruiseDirection * halfStraight;

    out.firstEnd = firstEnd;
    out.secondStart = secondStart;

    const double h0 =
        std::max(
            1.0,
            variables.startBoundaryHandleMeters
        );
    const double h1 =
        std::max(
            1.0,
            variables.startCruiseHandleMeters
        );
    const double h2 =
        std::max(
            1.0,
            variables.goalCruiseHandleMeters
        );
    const double h3 =
        std::max(
            1.0,
            variables.goalBoundaryHandleMeters
        );

    // First cubic.
    out.control[0] = startCurve;
    out.control[1] = startCurve + startTangent * h0;
    out.control[2] = firstEnd - cruiseDirection * h1;
    out.control[3] = firstEnd;

    // Second cubic.
    out.control[4] = secondStart;
    out.control[5] = secondStart + cruiseDirection * h2;
    out.control[6] = goalCurve - goalTangent * h3;
    out.control[7] = goalCurve;

    const double directInterior =
        glm::length(goalCurve - startCurve);

    out.lowerBoundLength =
        startProtected +
        directInterior +
        goalProtected;

    if (!(std::isfinite(out.lowerBoundLength) &&
          out.lowerBoundLength > Epsilon))
    {
        return out;
    }

    const std::size_t samplesPerBezier =
        std::max<std::size_t>(
            24,
            request.samplesPerBezier
        );

    out.samples.reserve(
        samplesPerBezier * 2 + 8
    );

    double maxViolation = 0.0;

    const double maxCurvatureAllowed =
        request.minimumComfortRadiusMeters > Epsilon
            ? 1.0 /
                request.minimumComfortRadiusMeters
            : std::numeric_limits<double>::infinity();

    const auto appendSample =
        [&](const glm::dvec3& position,
            double curvature) -> bool
        {
            if (!finite3(position) ||
                !std::isfinite(curvature))
            {
                maxViolation =
                    std::numeric_limits<double>::infinity();
                return false;
            }

            if (!out.samples.empty())
            {
                const glm::dvec3 previous =
                    out.samples.back().position;
                const double ds =
                    glm::length(position - previous);

                if (!(std::isfinite(ds)))
                {
                    maxViolation =
                        std::numeric_limits<double>::infinity();
                    return false;
                }

                if (ds > 1.0e-8)
                {
                    out.length += ds;

                    if (!world::navigation::
                        segmentClearOfNavigationObstacles(
                            previous,
                            position,
                            request.obstacles,
                            request.hullRadiusMeters
                        ))
                    {
                        maxViolation =
                            std::max(maxViolation, 1.0);
                    }
                }
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

            out.maxCurvature =
                std::max(out.maxCurvature, curvature);

            out.samples.push_back(
                {position, curvature}
            );
            return true;
        };

    // Include the protected start straight in the sampled speed/collision
    // model. This is where a moving craft may physically brake before the
    // first real turn.
    if (!appendSample(
            request.start.positionMeters,
            0.0
        ))
    {
        return out;
    }

    if (glm::length(
            startCurve -
            request.start.positionMeters
        ) > 1.0e-8)
    {
        if (!appendSample(startCurve, 0.0))
            return out;
    }

    const auto sampleBezier =
        [&](const glm::dvec3& p0,
            const glm::dvec3& p1,
            const glm::dvec3& p2,
            const glm::dvec3& p3,
            bool skipFirst) -> bool
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
                    sampleCubic(
                        p0, p1, p2, p3, t
                    );

                const double curvature =
                    curvatureOf(sample);

                if (!std::isfinite(curvature))
                {
                    maxViolation =
                        std::numeric_limits<double>::infinity();
                    return false;
                }

                if (!appendSample(
                        sample.position,
                        curvature
                    ))
                {
                    return false;
                }
            }

            return true;
        };

    if (!sampleBezier(
            out.control[0],
            out.control[1],
            out.control[2],
            out.control[3],
            glm::length(startCurve -
                        request.start.positionMeters) <= 1.0e-8))
    {
        return out;
    }

    // The middle segment is deliberately straight. It is the key structural
    // difference from the old "two giant Beziers fill the whole leg" model.
    if (glm::length(secondStart - firstEnd) > 1.0e-8)
    {
        if (!appendSample(secondStart, 0.0))
            return out;
    }

    if (!sampleBezier(
            out.control[4],
            out.control[5],
            out.control[6],
            out.control[7],
            true))
    {
        return out;
    }

    // Score each continuous cubic turn against its own mean curvature.
    // This is deliberately per-turn rather than global: two legitimate turns
    // may use different radii, while each should still avoid the flattened
    // "long shallow shoulders + tight nose" shape.
    const CurvatureShapeStats firstShape =
        curvatureShapeOfCubic(
            out.control[0],
            out.control[1],
            out.control[2],
            out.control[3],
            samplesPerBezier
        );
    const CurvatureShapeStats secondShape =
        curvatureShapeOfCubic(
            out.control[4],
            out.control[5],
            out.control[6],
            out.control[7],
            samplesPerBezier
        );

    if (!firstShape.finite || !secondShape.finite)
        return out;

    const double shapeTurn =
        firstShape.turnRadians +
        secondShape.turnRadians;

    if (shapeTurn > Epsilon)
    {
        out.curvatureUniformityEnergy =
            (
                firstShape.turnRadians *
                    firstShape.curvatureUniformity +
                secondShape.turnRadians *
                    secondShape.curvatureUniformity
            ) /
            shapeTurn;
    }

    if (glm::length(
            request.goal.positionMeters -
            goalCurve
        ) > 1.0e-8)
    {
        if (!appendSample(
                request.goal.positionMeters,
                0.0
            ))
        {
            return out;
        }
    }

    if (out.samples.size() < 2)
        return out;

    out.stretchRatio =
        out.length /
        std::max(1.0, out.lowerBoundLength);

    const double accelerating =
        fallbackLongitudinalAuthority(
            request.acceleratingMps2,
            request.maxLateralAccelerationMps2
        );
    const double braking =
        fallbackLongitudinalAuthority(
            request.brakingMps2,
            request.maxLateralAccelerationMps2
        );

    const std::size_t count = out.samples.size();
    std::vector<double> speedLimit(count, 0.0);
    std::vector<double> backwardCap(count, 0.0);
    std::vector<double> speed(count, 0.0);

    for (std::size_t i = 0; i < count; ++i)
    {
        speedLimit[i] =
            speedLimitForCurvature(
                request,
                out.samples[i].curvaturePerMeter
            );
    }

    backwardCap.back() =
        std::min(
            speedLimit.back(),
            std::max(0.0, request.goal.speedMps)
        );

    for (std::size_t reverse = count - 1;
         reverse > 0;
         --reverse)
    {
        const std::size_t i = reverse - 1;
        const double ds =
            glm::length(
                out.samples[i + 1].position -
                out.samples[i].position
            );

        const double maxBefore =
            std::sqrt(
                std::max(
                    0.0,
                    backwardCap[i + 1] *
                        backwardCap[i + 1] +
                    2.0 * braking * ds
                )
            );

        backwardCap[i] =
            std::min(speedLimit[i], maxBefore);
    }

    const double requestedStartSpeed =
        std::max(0.0, request.start.speedMps);

    if (requestedStartSpeed >
        backwardCap.front() + 0.25)
    {
        const double normalized =
            (requestedStartSpeed -
             backwardCap.front()) /
            std::max(
                1.0,
                request.designSpeedMps
            );
        maxViolation =
            std::max(maxViolation, normalized);
    }

    speed.front() =
        std::min(
            requestedStartSpeed,
            backwardCap.front()
        );

    for (std::size_t i = 0; i + 1 < count; ++i)
    {
        const double ds =
            glm::length(
                out.samples[i + 1].position -
                out.samples[i].position
            );

        const double reachable =
            std::sqrt(
                std::max(
                    0.0,
                    speed[i] * speed[i] +
                    2.0 * accelerating * ds
                )
            );

        speed[i + 1] =
            std::min(
                backwardCap[i + 1],
                reachable
            );
    }

    // Curvature-rate authority is part of the speed profile, not merely a
    // diagnostic. If adjacent samples ask the hull to change path angular
    // rate faster than its real attitude authority, lower the local speed
    // ceilings and rebuild the longitudinal profile. This lets geometry and
    // ETA compete naturally: a sharp curvature transition is still possible,
    // but it becomes slow unless the optimizer broadens/smooths it.
    if (request.maxAngularAccelerationRadPerSecond2 > Epsilon)
    {
        constexpr int AngularProfileIterations = 10;
        constexpr double AngularReserve = 0.995;

        for (int iteration = 0;
             iteration < AngularProfileIterations;
             ++iteration)
        {
            bool adjusted = false;

            for (std::size_t i = 0; i + 1 < count; ++i)
            {
                const double ds =
                    glm::length(
                        out.samples[i + 1].position -
                        out.samples[i].position
                    );
                if (ds <= 1.0e-9)
                    continue;

                const double sumSpeed =
                    speed[i] + speed[i + 1];
                if (sumSpeed <= MinimumUsefulSpeedMps)
                    continue;

                const double dt =
                    2.0 * ds / sumSpeed;
                if (dt <= Epsilon)
                    continue;

                const double omega0 =
                    speed[i] *
                    out.samples[i].curvaturePerMeter;
                const double omega1 =
                    speed[i + 1] *
                    out.samples[i + 1].curvaturePerMeter;
                const double demandedAlpha =
                    std::abs(omega1 - omega0) / dt;

                if (demandedAlpha <=
                    request.maxAngularAccelerationRadPerSecond2 *
                        (1.0 + 1.0e-6))
                {
                    continue;
                }

                // Scaling both endpoint speeds by s scales demanded angular
                // acceleration approximately by s^2 because omega scales with
                // s while traversal time grows as 1/s.
                const double scale =
                    AngularReserve *
                    std::sqrt(
                        request.maxAngularAccelerationRadPerSecond2 /
                        demandedAlpha
                    );

                speedLimit[i] *= scale;
                speedLimit[i + 1] *= scale;
                adjusted = true;
            }

            if (!adjusted)
                break;

            backwardCap.back() =
                std::min(
                    speedLimit.back(),
                    std::max(0.0, request.goal.speedMps)
                );

            for (std::size_t reverse = count - 1;
                 reverse > 0;
                 --reverse)
            {
                const std::size_t i = reverse - 1;
                const double ds =
                    glm::length(
                        out.samples[i + 1].position -
                        out.samples[i].position
                    );
                const double maxBefore =
                    std::sqrt(
                        std::max(
                            0.0,
                            backwardCap[i + 1] *
                                backwardCap[i + 1] +
                            2.0 * braking * ds
                        )
                    );
                backwardCap[i] =
                    std::min(speedLimit[i], maxBefore);
            }

            speed.front() =
                std::min(
                    requestedStartSpeed,
                    backwardCap.front()
                );

            for (std::size_t i = 0; i + 1 < count; ++i)
            {
                const double ds =
                    glm::length(
                        out.samples[i + 1].position -
                        out.samples[i].position
                    );
                const double reachable =
                    std::sqrt(
                        std::max(
                            0.0,
                            speed[i] * speed[i] +
                            2.0 * accelerating * ds
                        )
                    );
                speed[i + 1] =
                    std::min(
                        backwardCap[i + 1],
                        reachable
                    );
            }
        }
    }

    // Angular-rate smoothing may lower the admissible speed before the first
    // curve. A moving craft cannot teleport down to that cap, so preserve the
    // original hot-start feasibility check after the final shaped profile.
    if (requestedStartSpeed >
        backwardCap.front() + 0.25)
    {
        const double normalized =
            (requestedStartSpeed -
             backwardCap.front()) /
            std::max(1.0, request.designSpeedMps);
        maxViolation =
            std::max(maxViolation, normalized);
    }

    double travelTime = 0.0;
    double maxLateralUtilization = 0.0;
    double lateralEnergy = 0.0;
    double brakingDemandEnergy = 0.0;

    double totalCurvature = 0.0;
    double curvatureConcentration = 0.0;
    double maxAngularVelocityUtilization = 0.0;
    double maxAngularAccelerationUtilization = 0.0;
    double angularVelocityEnergy = 0.0;
    double angularAccelerationEnergy = 0.0;

    double curvatureVariation = 0.0;

    const double referenceLength =
        std::max(1.0, out.lowerBoundLength);
    const double hullScale =
        std::max(1.0, 2.0 * request.hullRadiusMeters);

    for (std::size_t i = 0; i + 1 < count; ++i)
    {
        const double ds =
            glm::length(
                out.samples[i + 1].position -
                out.samples[i].position
            );

        if (ds <= 1.0e-9)
            continue;

        const double sumSpeed =
            speed[i] + speed[i + 1];

        double segmentTime = 0.0;
        if (sumSpeed > MinimumUsefulSpeedMps)
        {
            segmentTime = 2.0 * ds / sumSpeed;
        }
        else
        {
            const double authority =
                std::max(
                    Epsilon,
                    std::min(accelerating, braking)
                );
            segmentTime =
                2.0 * std::sqrt(ds / authority);
        }
        travelTime += segmentTime;

        const double curvature =
            0.5 *
            (out.samples[i].curvaturePerMeter +
             out.samples[i + 1].curvaturePerMeter);

        totalCurvature += curvature * ds;
        curvatureConcentration +=
            curvature * curvature * ds * hullScale;

        const double segmentSpeed =
            0.5 * (speed[i] + speed[i + 1]);

        const double lateralAcceleration =
            segmentSpeed *
            segmentSpeed *
            curvature;

        const double normalizedLateral =
            std::max(
                0.0,
                lateralAcceleration /
                    std::max(
                        Epsilon,
                        request.maxLateralAccelerationMps2
                    )
            );

        maxLateralUtilization =
            std::max(
                maxLateralUtilization,
                normalizedLateral
            );

        // Convex dynamic-reserve cost. There is deliberately no preferred
        // radius here: a tight curve is cheap when flown slowly, while a broad
        // curve can still be expensive when it consumes most lateral authority.
        const double lateralSquared =
            normalizedLateral * normalizedLateral;
        lateralEnergy +=
            lateralSquared *
            lateralSquared *
            (ds / referenceLength);

        if (request.maxAngularVelocityRadPerSecond > Epsilon)
        {
            const double angularRate =
                segmentSpeed * curvature;
            const double normalizedAngularRate =
                angularRate /
                request.maxAngularVelocityRadPerSecond;
            const double angularRateSquared =
                normalizedAngularRate *
                normalizedAngularRate;

            maxAngularVelocityUtilization =
                std::max(
                    maxAngularVelocityUtilization,
                    normalizedAngularRate
                );
            angularVelocityEnergy +=
                angularRateSquared *
                angularRateSquared *
                (ds / referenceLength);
        }

        if (request.maxAngularAccelerationRadPerSecond2 > Epsilon &&
            segmentTime > Epsilon)
        {
            const double angularRate0 =
                speed[i] *
                out.samples[i].curvaturePerMeter;
            const double angularRate1 =
                speed[i + 1] *
                out.samples[i + 1].curvaturePerMeter;
            const double angularAcceleration =
                std::abs(angularRate1 - angularRate0) /
                segmentTime;
            const double normalizedAngularAcceleration =
                angularAcceleration /
                request.maxAngularAccelerationRadPerSecond2;
            const double angularAccelerationSquared =
                normalizedAngularAcceleration *
                normalizedAngularAcceleration;

            maxAngularAccelerationUtilization =
                std::max(
                    maxAngularAccelerationUtilization,
                    normalizedAngularAcceleration
                );
            angularAccelerationEnergy +=
                angularAccelerationSquared *
                angularAccelerationSquared *
                (ds / referenceLength);

            // The profile above must normally keep this at or below real
            // authority. Remaining excess means the sampled geometry cannot be
            // executed exactly even after slowing down and is therefore a hard
            // dynamic violation, not a comfort preference.
            if (normalizedAngularAcceleration > 1.0 + 1.0e-4)
            {
                maxViolation =
                    std::max(
                        maxViolation,
                        normalizedAngularAcceleration - 1.0
                    );
            }
        }

        // Penalize only demanded braking, not ordinary full-throttle
        // acceleration. A candidate that must spend kilometres braking at the
        // physical limit therefore loses to a slightly longer route that keeps
        // useful stopping reserve.
        if (speed[i] > speed[i + 1] + 1.0e-9)
        {
            const double demandedBraking =
                (speed[i] * speed[i] -
                 speed[i + 1] * speed[i + 1]) /
                (2.0 * ds);
            const double normalizedBraking =
                std::max(
                    0.0,
                    demandedBraking /
                        std::max(Epsilon, braking)
                );
            const double brakingSquared =
                normalizedBraking * normalizedBraking;

            brakingDemandEnergy +=
                brakingSquared *
                brakingSquared *
                (ds / referenceLength);
        }

        const double deltaCurvature =
            out.samples[i + 1].curvaturePerMeter -
            out.samples[i].curvaturePerMeter;

        const double normalizedCurvatureJump =
            deltaCurvature * referenceLength;

        curvatureVariation +=
            normalizedCurvatureJump *
            normalizedCurvatureJump *
            (ds / referenceLength);
    }

    out.estimatedTravelTimeSeconds = travelTime;
    out.maxLateralUtilization = maxLateralUtilization;
    out.lateralAccelerationEnergy = lateralEnergy;
    out.brakingDemandEnergy = brakingDemandEnergy;

    out.totalCurvatureRadians = totalCurvature;
    out.curvatureConcentrationEnergy = curvatureConcentration;
    out.maxAngularVelocityUtilization =
        maxAngularVelocityUtilization;
    out.maxAngularAccelerationUtilization =
        maxAngularAccelerationUtilization;
    out.angularVelocityEnergy = angularVelocityEnergy;
    out.angularAccelerationEnergy = angularAccelerationEnergy;

    out.curvatureVariationEnergy = curvatureVariation;

    out.finite =
        std::isfinite(out.length) &&
        std::isfinite(out.lowerBoundLength) &&
        std::isfinite(out.stretchRatio) &&
        std::isfinite(out.estimatedTravelTimeSeconds) &&
        std::isfinite(out.maxCurvature) &&
        std::isfinite(out.maxLateralUtilization) &&
        std::isfinite(out.lateralAccelerationEnergy) &&
        std::isfinite(out.brakingDemandEnergy) &&
        std::isfinite(out.totalCurvatureRadians) &&
        std::isfinite(out.curvatureConcentrationEnergy) &&
        std::isfinite(out.curvatureUniformityEnergy) &&
        std::isfinite(out.maxAngularVelocityUtilization) &&
        std::isfinite(out.maxAngularAccelerationUtilization) &&
        std::isfinite(out.angularVelocityEnergy) &&
        std::isfinite(out.angularAccelerationEnergy) &&
        std::isfinite(out.curvatureVariationEnergy) &&
        std::isfinite(maxViolation);

    if (!out.finite)
        return out;

    out.violation = maxViolation;
    out.feasible = maxViolation <= 1.0e-7;

    const double idealTime =
        out.lowerBoundLength /
        std::max(
            1.0,
            request.designSpeedMps
        );

    const double normalizedTime =
        out.estimatedTravelTimeSeconds /
        std::max(0.1, idealTime);

    const double excessStretch =
        std::max(
            0.0,
            out.stretchRatio - 1.0
        );

    const double excessLengthPenalty =
        excessStretch * excessStretch;

    // The optimizer is not minimizing radius or path length. It minimizes
    // travel time while paying for control authority consumed by the route.
    // The fourth power keeps low utilization nearly free and makes operation
    // near the physical limit progressively expensive without introducing a
    // magic "good radius".
    const double peakLateralSquared =
        out.maxLateralUtilization *
        out.maxLateralUtilization;
    const double peakLateralStress =
        peakLateralSquared *
        peakLateralSquared;

    const double peakAngularSquared =
        out.maxAngularVelocityUtilization *
        out.maxAngularVelocityUtilization;
    const double peakAngularStress =
        peakAngularSquared *
        peakAngularSquared;

    // Curvature regularity is intentionally capped because it is a numerical
    // smoothness preference, not a reason to buy tens of kilometres of detour.
    const double boundedCurvatureVariation =
        std::min(
            4.0,
            out.curvatureVariationEnergy
        );

    out.objective =
        request.weights.travelTime *
            normalizedTime +
        request.weights.pathLength *
            excessLengthPenalty +
        request.weights.lateralAccelerationEnergy *
            out.lateralAccelerationEnergy +
        request.weights.peakLateralUtilization *
            peakLateralStress +
        request.weights.brakingDemand *
            out.brakingDemandEnergy +
        request.weights.totalCurvature *
            out.totalCurvatureRadians +
        request.weights.curvatureConcentration *
            out.curvatureConcentrationEnergy +
        request.weights.curvatureUniformity *
            out.curvatureUniformityEnergy +
        request.weights.angularVelocityDemand *
            (out.angularVelocityEnergy +
             0.5 * peakAngularStress) +
        request.weights.angularAccelerationDemand *
            out.angularAccelerationEnergy +
        request.weights.curvatureVariation *
            boundedCurvatureVariation;

    return out;
}

bool better(
    const Evaluation& candidate,
    const Evaluation& current
) noexcept
{
    if (!candidate.finite)
        return false;
    if (!current.finite)
        return true;

    if (candidate.feasible != current.feasible)
        return candidate.feasible;

    if (candidate.feasible)
    {
        return
            candidate.objective + 1.0e-9 <
            current.objective;
    }

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

// Passive bounded telemetry over EVERY evaluated candidate. No new geometry
// evaluations, no changes to feasibility, objective, or search decisions.
struct CandidateAudit
{
    static constexpr std::size_t Retained = 5;
    std::size_t feasible = 0;
    std::size_t infeasible = 0;

    std::array<CandidateComparison, Retained> shortest {};
    std::array<CandidateComparison, Retained> fastest {};
    std::array<CandidateComparison, Retained> bestScore {};
    std::size_t shortestCount = 0;
    std::size_t fastestCount = 0;
    std::size_t bestScoreCount = 0;

    static void retain(
        std::array<CandidateComparison, Retained>& top,
        std::size_t& count,
        const CandidateComparison& candidate,
        int metric
    ) noexcept
    {
        const auto valueFor =
            [metric](const CandidateComparison& item)
            {
                return metric == 0
                    ? item.lengthMeters
                    : (metric == 1
                        ? item.estimatedTravelTimeSeconds
                        : item.objective);
            };

        const double value = valueFor(candidate);
        std::size_t position = 0;
        while (position < count &&
               value >= valueFor(top[position]))
        {
            ++position;
        }
        if (position == Retained)
            return;

        if (count < Retained)
            ++count;
        for (std::size_t i = count - 1; i > position; --i)
            top[i] = top[i - 1];
        top[position] = candidate;
    }

    void observe(
        const Evaluation& evaluation,
        std::size_t evaluationIndex
    ) noexcept
    {
        if (!evaluation.finite || !evaluation.feasible)
        {
            ++infeasible;
            return;
        }

        ++feasible;
        CandidateComparison item;
        item.evaluationIndex = evaluationIndex;
        item.lengthMeters = evaluation.length;
        item.estimatedTravelTimeSeconds =
            evaluation.estimatedTravelTimeSeconds;
        item.objective = evaluation.objective;
        item.minimumRadiusMeters =
            evaluation.maxCurvature > 1.0e-12
                ? 1.0 / evaluation.maxCurvature
                : 0.0;
        item.totalCurvatureRadians =
            evaluation.totalCurvatureRadians;

        retain(shortest, shortestCount, item, 0);
        retain(fastest, fastestCount, item, 1);
        retain(bestScore, bestScoreCount, item, 2);
    }

    void publish(Diagnostics& diagnostics) const
    {
        diagnostics.feasibleEvaluations = feasible;
        diagnostics.infeasibleEvaluations = infeasible;
        diagnostics.shortestFeasible.assign(
            shortest.begin(), shortest.begin() + shortestCount
        );
        diagnostics.fastestFeasible.assign(
            fastest.begin(), fastest.begin() + fastestCount
        );
        diagnostics.lowestObjectiveFeasible.assign(
            bestScore.begin(), bestScore.begin() + bestScoreCount
        );
    }
};

double geometryScale(const Request& request) noexcept
{
    const glm::dvec3 startTangent =
        normalizedOr(
            request.start.tangent,
            {0.0, 0.0, -1.0}
        );
    const glm::dvec3 goalTangent =
        normalizedOr(
            request.goal.tangent,
            startTangent
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

    return
        std::max(
            100.0,
            glm::length(goalCurve - startCurve)
        );
}

Variables makeSeed(
    const Request& request,
    const glm::dvec3& offset
)
{
    const glm::dvec3 startTangent =
        normalizedOr(
            request.start.tangent,
            {0.0, 0.0, -1.0}
        );
    const glm::dvec3 goalTangent =
        normalizedOr(
            request.goal.tangent,
            startTangent
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
    const double scale =
        std::max(100.0, glm::length(chord));
    const glm::dvec3 cruiseDirection =
        normalizedOr(chord, goalTangent);

    const double comfortScale =
        std::max(
            0.0,
            request.minimumComfortRadiusMeters
        );

    const double transitionHandle =
        std::clamp(
            std::max(
                0.08 * scale,
                0.75 * comfortScale
            ),
            25.0,
            0.25 * scale
        );

    Variables out;
    out.midpoint =
        0.5 * (startCurve + goalCurve) +
        offset;
    out.cruiseDirectionRaw = cruiseDirection;

    // Give the candidate a real straight middle by default. Pattern search may
    // shrink this to zero if a single continuous transition is actually best.
    out.halfStraightMeters =
        0.18 * scale;

    out.startBoundaryHandleMeters =
        transitionHandle;
    out.startCruiseHandleMeters =
        transitionHandle;
    out.goalCruiseHandleMeters =
        transitionHandle;
    out.goalBoundaryHandleMeters =
        transitionHandle;

    return out;
}

std::vector<Variables> buildSeeds(
    const Request& request
)
{
    const double scale = geometryScale(request);

    const glm::dvec3 chord =
        request.goal.positionMeters -
        request.start.positionMeters;
    const glm::dvec3 chordDirection =
        normalizedOr(
            chord,
            request.start.tangent
        );

    const glm::dvec3 sideA =
        deterministicPerpendicular(chordDirection);
    const glm::dvec3 sideB =
        normalizedOr(
            glm::cross(chordDirection, sideA),
            {0.0, 1.0, 0.0}
        );

    std::vector<Variables> seeds;
    seeds.reserve(13);

    const auto add =
        [&](const glm::dvec3& offset)
        {
            seeds.push_back(
                makeSeed(request, offset)
            );
        };

    add({0.0, 0.0, 0.0});

    for (const double fraction :
         {0.10, 0.20, 0.35})
    {
        const double distance =
            fraction * scale;
        add(sideA * distance);
        add(-sideA * distance);
        add(sideB * distance);
        add(-sideB * distance);
    }

    return seeds;
}

struct SearchResult
{
    Variables variables;
    Evaluation evaluation;
    std::size_t acceptedSteps = 0;
};

SearchResult patternOptimize(
    const Request& request,
    const Variables& initial,
    std::size_t iterations,
    std::size_t* objectiveEvaluations,
    CandidateAudit* audit
)
{
    SearchResult out;
    out.variables = initial;
    out.evaluation =
        evaluate(
            request,
            out.variables,
            objectiveEvaluations
        );
    audit->observe(out.evaluation, *objectiveEvaluations);

    const double scale = geometryScale(request);

    std::array<double,11> steps = {
        0.08 * scale,
        0.08 * scale,
        0.08 * scale,

        0.20,
        0.20,
        0.20,

        0.08 * scale,

        0.06 * scale,
        0.06 * scale,
        0.06 * scale,
        0.06 * scale
    };

    const double maxHalfStraight =
        0.60 * scale;
    const double maxHandle =
        0.60 * scale;

    const auto get =
        [&](const Variables& value,
            std::size_t index) -> double
        {
            switch (index)
            {
                case 0: return value.midpoint.x;
                case 1: return value.midpoint.y;
                case 2: return value.midpoint.z;

                case 3: return value.cruiseDirectionRaw.x;
                case 4: return value.cruiseDirectionRaw.y;
                case 5: return value.cruiseDirectionRaw.z;

                case 6: return value.halfStraightMeters;

                case 7:
                    return value.startBoundaryHandleMeters;
                case 8:
                    return value.startCruiseHandleMeters;
                case 9:
                    return value.goalCruiseHandleMeters;
                default:
                    return value.goalBoundaryHandleMeters;
            }
        };

    const auto set =
        [&](Variables& value,
            std::size_t index,
            double scalar)
        {
            switch (index)
            {
                case 0:
                    value.midpoint.x = scalar;
                    break;
                case 1:
                    value.midpoint.y = scalar;
                    break;
                case 2:
                    value.midpoint.z = scalar;
                    break;

                case 3:
                    value.cruiseDirectionRaw.x = scalar;
                    break;
                case 4:
                    value.cruiseDirectionRaw.y = scalar;
                    break;
                case 5:
                    value.cruiseDirectionRaw.z = scalar;
                    break;

                case 6:
                    value.halfStraightMeters =
                        std::clamp(
                            scalar,
                            0.0,
                            maxHalfStraight
                        );
                    break;

                case 7:
                    value.startBoundaryHandleMeters =
                        std::clamp(
                            scalar,
                            1.0,
                            maxHandle
                        );
                    break;
                case 8:
                    value.startCruiseHandleMeters =
                        std::clamp(
                            scalar,
                            1.0,
                            maxHandle
                        );
                    break;
                case 9:
                    value.goalCruiseHandleMeters =
                        std::clamp(
                            scalar,
                            1.0,
                            maxHandle
                        );
                    break;
                default:
                    value.goalBoundaryHandleMeters =
                        std::clamp(
                            scalar,
                            1.0,
                            maxHandle
                        );
                    break;
            }
        };

    for (std::size_t iteration = 0;
         iteration < iterations;
         ++iteration)
    {
        bool improved = false;

        for (std::size_t axis = 0;
             axis < steps.size();
             ++axis)
        {
            Variables bestVariables = out.variables;
            Evaluation bestEvaluation = out.evaluation;

            for (const double direction :
                 {-1.0, 1.0})
            {
                Variables candidate = out.variables;

                set(
                    candidate,
                    axis,
                    get(candidate, axis) +
                        direction * steps[axis]
                );

                const Evaluation evaluation =
                    evaluate(
                        request,
                        candidate,
                        objectiveEvaluations
                    );
                audit->observe(evaluation, *objectiveEvaluations);

                if (better(
                        evaluation,
                        bestEvaluation
                    ))
                {
                    bestVariables = candidate;
                    bestEvaluation = evaluation;
                }
            }

            if (better(
                    bestEvaluation,
                    out.evaluation
                ))
            {
                out.variables = bestVariables;
                out.evaluation = bestEvaluation;
                improved = true;
                ++out.acceptedSteps;
            }
        }

        if (!improved)
        {
            for (double& step : steps)
                step *= 0.68;
        }

        const double largestStep =
            *std::max_element(
                steps.begin(),
                steps.end()
            );

        if (largestStep < 1.0e-3)
            break;
    }

    return out;
}

OptimizedSegment cubicSegment(
    const glm::dvec3& p0,
    const glm::dvec3& p1,
    const glm::dvec3& p2,
    const glm::dvec3& p3
)
{
    OptimizedSegment out;
    out.kind = OptimizedSegmentKind::CubicBezier;
    out.startMeters = p0;
    out.control1Meters = p1;
    out.control2Meters = p2;
    out.endMeters = p3;
    return out;
}

OptimizedSegment lineSegment(
    const glm::dvec3& a,
    const glm::dvec3& b
)
{
    OptimizedSegment out;
    out.kind = OptimizedSegmentKind::Line;
    out.startMeters = a;
    out.endMeters = b;
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
        !std::isfinite(request.start.speedMps) ||
        !std::isfinite(request.goal.speedMps) ||
        request.start.speedMps < 0.0 ||
        request.goal.speedMps < 0.0 ||
        request.hullRadiusMeters < 0.0 ||
        request.designSpeedMps <= 0.0 ||
        request.acceleratingMps2 < 0.0 ||
        request.brakingMps2 < 0.0 ||
        request.maxLateralAccelerationMps2 <= 0.0 ||
        request.maxAngularVelocityRadPerSecond < 0.0 ||
        request.maxAngularAccelerationRadPerSecond2 < 0.0 ||
        request.minimumComfortRadiusMeters < 0.0)
    {
        result.message =
            "invalid continuous Assisted optimizer request";
        return result;
    }

    const auto seeds = buildSeeds(request);
    result.diagnostics.seedCount = seeds.size();

    struct RankedSeed
    {
        Variables variables;
        Evaluation evaluation;
    };

    std::size_t objectiveEvaluations = 0;
    CandidateAudit candidateAudit;
    std::vector<RankedSeed> ranked;
    ranked.reserve(seeds.size());

    for (const auto& seed : seeds)
    {
        Evaluation evaluation =
            evaluate(request, seed, &objectiveEvaluations);
        candidateAudit.observe(evaluation, objectiveEvaluations);
        ranked.push_back({seed, std::move(evaluation)});
    }

    std::stable_sort(
        ranked.begin(),
        ranked.end(),
        [](const RankedSeed& a,
           const RankedSeed& b)
        {
            return better(a.evaluation, b.evaluation);
        }
    );

    const std::size_t selectedCount =
        std::min<std::size_t>(
            4,
            ranked.size()
        );

    const std::size_t totalIterations =
        std::max<std::size_t>(
            1,
            request.maxPatternIterations
        );

    const std::size_t perSeedIterations =
        std::max<std::size_t>(
            12,
            totalIterations /
                std::max<std::size_t>(
                    1,
                    selectedCount
                )
        );

    SearchResult bestSearch;
    bool haveBest = false;

    for (std::size_t i = 0;
         i < selectedCount;
         ++i)
    {
        SearchResult candidate =
            patternOptimize(
                request,
                ranked[i].variables,
                perSeedIterations,
                &objectiveEvaluations,
                &candidateAudit
            );

        if (!haveBest ||
            better(
                candidate.evaluation,
                bestSearch.evaluation
            ))
        {
            bestSearch = std::move(candidate);
            haveBest = true;
        }
    }

    if (!haveBest)
    {
        result.message =
            "continuous optimizer produced no candidate";
        return result;
    }

    // One final local refinement around the best topology. Total work stays
    // bounded by the authored iteration budget order-of-magnitude rather than
    // multiplying the full old budget by every seed.
    SearchResult refined =
        patternOptimize(
            request,
            bestSearch.variables,
            std::max<std::size_t>(
                12,
                totalIterations / 2
            ),
            &objectiveEvaluations,
            &candidateAudit
        );

    if (better(
            refined.evaluation,
            bestSearch.evaluation
        ))
    {
        bestSearch.acceptedSteps +=
            refined.acceptedSteps;
        bestSearch.variables =
            refined.variables;
        bestSearch.evaluation =
            std::move(refined.evaluation);
    }

    const Evaluation& current =
        bestSearch.evaluation;
    const Variables& variables =
        bestSearch.variables;

    result.valid = current.feasible;
    result.message =
        result.valid
            ? "continuous time/reserve optimized Assisted path ready"
            : "continuous optimizer did not reach a feasible path";

    result.joinMeters = variables.midpoint;
    result.joinTangent = current.cruiseDirection;

    result.firstControl1Meters =
        current.control[1];
    result.firstControl2Meters =
        current.control[2];
    result.secondControl1Meters =
        current.control[5];
    result.secondControl2Meters =
        current.control[6];

    result.sampledCenterlineMeters.reserve(
        current.samples.size()
    );
    for (const auto& sample : current.samples)
        result.sampledCenterlineMeters.push_back(
            sample.position
        );

    if (result.valid)
    {
        result.segments.push_back(
            cubicSegment(
                current.control[0],
                current.control[1],
                current.control[2],
                current.control[3]
            )
        );

        if (glm::length(
                current.secondStart -
                current.firstEnd
            ) > 1.0e-6)
        {
            result.segments.push_back(
                lineSegment(
                    current.firstEnd,
                    current.secondStart
                )
            );
        }

        result.segments.push_back(
            cubicSegment(
                current.control[4],
                current.control[5],
                current.control[6],
                current.control[7]
            )
        );
    }

    candidateAudit.publish(result.diagnostics);
    result.diagnostics.objectiveEvaluations =
        objectiveEvaluations;
    result.diagnostics.acceptedSteps =
        bestSearch.acceptedSteps;
    result.diagnostics.pathLengthMeters =
        current.length;
    result.diagnostics.lowerBoundLengthMeters =
        current.lowerBoundLength;
    result.diagnostics.stretchRatio =
        current.stretchRatio;
    result.diagnostics.estimatedTravelTimeSeconds =
        current.estimatedTravelTimeSeconds;
    result.diagnostics.maxCurvaturePerMeter =
        current.maxCurvature;
    result.diagnostics.minimumRadiusMeters =
        current.maxCurvature > 1.0e-12
            ? 1.0 / current.maxCurvature
            : std::numeric_limits<double>::infinity();
    result.diagnostics.maxLateralUtilization =
        current.maxLateralUtilization;
    result.diagnostics.lateralAccelerationEnergy =
        current.lateralAccelerationEnergy;
    result.diagnostics.brakingDemandEnergy =
        current.brakingDemandEnergy;
    result.diagnostics.totalCurvatureRadians =
        current.totalCurvatureRadians;
    result.diagnostics.curvatureConcentrationEnergy =
        current.curvatureConcentrationEnergy;
    result.diagnostics.curvatureUniformityEnergy =
        current.curvatureUniformityEnergy;
    result.diagnostics.maxAngularVelocityUtilization =
        current.maxAngularVelocityUtilization;
    result.diagnostics.maxAngularAccelerationUtilization =
        current.maxAngularAccelerationUtilization;
    result.diagnostics.angularVelocityEnergy =
        current.angularVelocityEnergy;
    result.diagnostics.angularAccelerationEnergy =
        current.angularAccelerationEnergy;
    result.diagnostics.curvatureVariationEnergy =
        current.curvatureVariationEnergy;
    result.diagnostics.objective =
        current.objective;
    result.diagnostics.maxConstraintViolation =
        current.violation;

    return result;
}

} // namespace game::navigation::planner::assisted::lab
