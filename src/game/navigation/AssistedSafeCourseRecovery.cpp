#include "src/game/navigation/AssistedSafeCourseRecovery.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include <glm/gtc/constants.hpp>

#include "src/world/navigation/NavigationObstacleGeometry.h"

namespace game::navigation
{
namespace
{
constexpr double Epsilon = 1.0e-9;
constexpr double MovingSpeedEpsilonMps = 0.5;
constexpr double MinimumSceneRayMeters = 5000.0;
constexpr double SceneExitMarginMeters = 1500.0;
constexpr double ArcSampleSpacingMeters = 150.0;
constexpr int ArcSampleCap = 128;

bool finite3(const glm::dvec3& v) noexcept
{
    return std::isfinite(v.x) &&
        std::isfinite(v.y) &&
        std::isfinite(v.z);
}

glm::dvec3 normalizedOr(
    const glm::dvec3& value,
    const glm::dvec3& fallback
) noexcept
{
    const double valueLength2 = glm::dot(value, value);
    if (std::isfinite(valueLength2) &&
        valueLength2 > Epsilon)
    {
        return value / std::sqrt(valueLength2);
    }

    const double fallbackLength2 =
        glm::dot(fallback, fallback);
    if (std::isfinite(fallbackLength2) &&
        fallbackLength2 > Epsilon)
    {
        return fallback / std::sqrt(fallbackLength2);
    }

    return {0.0, 0.0, -1.0};
}

double angleBetween(
    const glm::dvec3& a,
    const glm::dvec3& b
) noexcept
{
    return std::acos(
        std::clamp(
            glm::dot(
                normalizedOr(a, {0.0, 0.0, -1.0}),
                normalizedOr(b, {0.0, 0.0, -1.0})
            ),
            -1.0,
            1.0
        )
    );
}

glm::dvec3 deterministicPerpendicular(
    const glm::dvec3& direction
) noexcept
{
    const glm::dvec3 d =
        normalizedOr(direction, {0.0, 0.0, -1.0});
    const glm::dvec3 seed =
        std::abs(d.y) < 0.8
            ? glm::dvec3(0.0, 1.0, 0.0)
            : glm::dvec3(1.0, 0.0, 0.0);

    return normalizedOr(
        glm::cross(d, seed),
        {1.0, 0.0, 0.0}
    );
}

glm::dvec3 rotateAroundAxis(
    const glm::dvec3& value,
    const glm::dvec3& axis,
    double angle
) noexcept
{
    const glm::dvec3 n =
        normalizedOr(axis, {0.0, 1.0, 0.0});
    const double c = std::cos(angle);
    const double s = std::sin(angle);

    return
        value * c +
        glm::cross(n, value) * s +
        n * glm::dot(n, value) * (1.0 - c);
}

bool clearSegment(
    const glm::dvec3& a,
    const glm::dvec3& b,
    const std::vector<world::navigation::NavigationObstacle>&
        obstacles,
    double hullRadiusMeters,
    double additionalClearanceMeters
) noexcept
{
    return world::navigation::segmentClearOfNavigationObstacles(
        a,
        b,
        obstacles,
        hullRadiusMeters,
        additionalClearanceMeters
    );
}

double sceneRayLength(
    const glm::dvec3& origin,
    double minimumMeters,
    const std::vector<world::navigation::NavigationObstacle>&
        obstacles,
    double hullRadiusMeters,
    double additionalClearanceMeters
) noexcept
{
    double out =
        std::max(MinimumSceneRayMeters, minimumMeters);

    for (const auto& obstacle : obstacles)
    {
        if (!obstacle.finite())
            continue;

        const double inflation =
            world::navigation::navigationObstacleInflationMeters(
                obstacle,
                hullRadiusMeters,
                additionalClearanceMeters
            );

        out = std::max(
            out,
            glm::length(obstacle.centerMeters - origin) +
                obstacle.conservativeRadiusMeters() +
                inflation +
                SceneExitMarginMeters
        );
    }

    return out;
}

struct Candidate
{
    bool valid = false;

    glm::dvec3 course {0.0, 0.0, -1.0};
    glm::dvec3 axis {0.0, 1.0, 0.0};
    glm::dvec3 centerMeters {0.0};
    glm::dvec3 endPositionMeters {0.0};

    double angleRad = 0.0;
    double radiusMeters = 0.0;
    double angularRateRadPerSecond = 0.0;
    double durationSeconds = 0.0;
    double sceneRayMeters = 0.0;
};

Candidate proveCandidate(
    const AssistedSafeCourseRecoveryRequest& request,
    const glm::dvec3& fromCourse,
    const glm::dvec3& toCourse,
    double speedMps
) noexcept
{
    Candidate out;

    if (speedMps <= MovingSpeedEpsilonMps ||
        request.courseAccelerationMps2 <= Epsilon ||
        request.maxCourseAngularVelocityRadPerSecond <= Epsilon)
    {
        return out;
    }

    const glm::dvec3 from =
        normalizedOr(fromCourse, {0.0, 0.0, -1.0});
    const glm::dvec3 to =
        normalizedOr(toCourse, from);

    out.angleRad = angleBetween(from, to);
    out.course = to;

    if (out.angleRad <= 1.0e-6)
    {
        const double ray =
            sceneRayLength(
                request.positionMeters,
                0.0,
                request.obstacles,
                request.hullRadiusMeters,
                request.additionalClearanceMeters
            );

        if (!clearSegment(
                request.positionMeters,
                request.positionMeters + to * ray,
                request.obstacles,
                request.hullRadiusMeters,
                request.additionalClearanceMeters
            ))
        {
            return Candidate {};
        }

        out.valid = true;
        out.axis = deterministicPerpendicular(from);
        out.centerMeters = request.positionMeters;
        out.endPositionMeters = request.positionMeters;
        out.radiusMeters = std::numeric_limits<double>::infinity();
        out.angularRateRadPerSecond = 0.0;
        out.durationSeconds = 0.0;
        out.sceneRayMeters = ray;
        return out;
    }

    const double accelerationLimitedRate =
        request.courseAccelerationMps2 / speedMps;
    const double angularRate =
        std::min(
            accelerationLimitedRate,
            request.maxCourseAngularVelocityRadPerSecond
        );

    if (!std::isfinite(angularRate) ||
        angularRate <= Epsilon)
    {
        return out;
    }

    glm::dvec3 axis = glm::cross(from, to);
    if (glm::dot(axis, axis) <= Epsilon)
        axis = deterministicPerpendicular(from);
    axis = normalizedOr(
        axis,
        deterministicPerpendicular(from)
    );

    const double radius = speedMps / angularRate;
    const glm::dvec3 radial =
        normalizedOr(
            glm::cross(from, axis),
            deterministicPerpendicular(from)
        );
    const glm::dvec3 center =
        request.positionMeters - radial * radius;

    const double arcLength =
        radius * out.angleRad;
    const int samples =
        std::clamp(
            static_cast<int>(
                std::ceil(
                    arcLength /
                    ArcSampleSpacingMeters
                )
            ),
            8,
            ArcSampleCap
        );

    glm::dvec3 previous = request.positionMeters;
    glm::dvec3 end = previous;

    for (int i = 1; i <= samples; ++i)
    {
        const double t =
            static_cast<double>(i) /
            static_cast<double>(samples);
        const glm::dvec3 rotatedRadial =
            rotateAroundAxis(
                radial,
                axis,
                out.angleRad * t
            );
        end = center + rotatedRadial * radius;

        if (!clearSegment(
                previous,
                end,
                request.obstacles,
                request.hullRadiusMeters,
                request.additionalClearanceMeters
            ))
        {
            return Candidate {};
        }

        previous = end;
    }

    const double ray =
        sceneRayLength(
            end,
            arcLength,
            request.obstacles,
            request.hullRadiusMeters,
            request.additionalClearanceMeters
        );

    if (!clearSegment(
            end,
            end + to * ray,
            request.obstacles,
            request.hullRadiusMeters,
            request.additionalClearanceMeters
        ))
    {
        return Candidate {};
    }

    out.valid = true;
    out.axis = axis;
    out.centerMeters = center;
    out.endPositionMeters = end;
    out.radiusMeters = radius;
    out.angularRateRadPerSecond = angularRate;
    out.durationSeconds =
        out.angleRad / angularRate;
    out.sceneRayMeters = ray;

    return out;
}

double directedPositionProgressAngle(
    const AssistedSafeCourseRecoveryPlan& plan,
    const glm::dvec3& actualPositionMeters
) noexcept
{
    const glm::dvec3 startRadial =
        normalizedOr(
            plan.startPositionMeters -
                plan.turnCenterMeters,
            deterministicPerpendicular(plan.initialCourse)
        );
    const glm::dvec3 actualRadial =
        normalizedOr(
            actualPositionMeters -
                plan.turnCenterMeters,
            startRadial
        );
    const glm::dvec3 axis =
        normalizedOr(
            plan.turnAxis,
            deterministicPerpendicular(plan.initialCourse)
        );

    const double x =
        std::clamp(
            glm::dot(startRadial, actualRadial),
            -1.0,
            1.0
        );
    const double y =
        glm::dot(
            axis,
            glm::cross(startRadial, actualRadial)
        );

    double angle = std::atan2(y, x);
    if (angle < 0.0)
        angle = 0.0;

    return std::clamp(
        angle,
        0.0,
        std::max(0.0, plan.turnAngleRad)
    );
}

bool remainingTrajectoryClear(
    const AssistedSafeCourseRecoveryPlan& plan,
    double progressAngleRad,
    const glm::dvec3& actualPositionMeters,
    const std::vector<world::navigation::NavigationObstacle>&
        obstacles
)
{
    glm::dvec3 previous = actualPositionMeters;

    const double clampedProgress =
        std::clamp(
            progressAngleRad,
            0.0,
            std::max(0.0, plan.turnAngleRad)
        );
    const double remainingAngle =
        std::max(
            0.0,
            plan.turnAngleRad - clampedProgress
        );

    if (remainingAngle > 1.0e-9)
    {
        const double remainingMeters =
            plan.turnRadiusMeters * remainingAngle;
        const int samples =
            std::clamp(
                static_cast<int>(
                    std::ceil(
                        remainingMeters /
                        ArcSampleSpacingMeters
                    )
                ),
                4,
                ArcSampleCap
            );

        for (int i = 1; i <= samples; ++i)
        {
            const double angle =
                clampedProgress +
                remainingAngle *
                    static_cast<double>(i) /
                    static_cast<double>(samples);
            const double nominalTime =
                angle /
                plan.courseAngularRateRadPerSecond;

            const auto future =
                AssistedSafeCourseRecovery::sample(
                    plan,
                    nominalTime
                );
            if (!future.valid)
                return false;

            if (!clearSegment(
                    previous,
                    future.positionMeters,
                    obstacles,
                    plan.hullRadiusMeters,
                    plan.additionalClearanceMeters
                ))
            {
                return false;
            }

            previous = future.positionMeters;
        }
    }

    const double ray =
        sceneRayLength(
            previous,
            plan.sceneRayMeters,
            obstacles,
            plan.hullRadiusMeters,
            plan.additionalClearanceMeters
        );

    return clearSegment(
        previous,
        previous + plan.targetCourse * ray,
        obstacles,
        plan.hullRadiusMeters,
        plan.additionalClearanceMeters
    );
}

} // namespace

AssistedSafeCourseRecoveryPlan
AssistedSafeCourseRecovery::plan(
    const AssistedSafeCourseRecoveryRequest& request
)
{
    AssistedSafeCourseRecoveryPlan out;

    if (!finite3(request.positionMeters) ||
        !finite3(request.velocityMps) ||
        !finite3(request.preferredDirection) ||
        request.hullRadiusMeters < 0.0 ||
        request.additionalClearanceMeters < 0.0 ||
        request.courseAccelerationMps2 < 0.0 ||
        request.brakingAccelerationMps2 < 0.0 ||
        request.maxCourseAngularVelocityRadPerSecond < 0.0)
    {
        out.message =
            "invalid Assisted safe-course recovery request";
        return out;
    }

    out.startPositionMeters = request.positionMeters;
    out.startVelocityMps = request.velocityMps;
    out.speedMps = glm::length(request.velocityMps);
    out.hullRadiusMeters = request.hullRadiusMeters;
    out.additionalClearanceMeters =
        request.additionalClearanceMeters;
    out.obstacles = request.obstacles;

    const glm::dvec3 preferred =
        normalizedOr(
            request.preferredDirection,
            {0.0, 0.0, -1.0}
        );

    if (out.speedMps <= MovingSpeedEpsilonMps)
    {
        out.disposition =
            AssistedSafeCourseRecoveryDisposition::
                CurrentCourseSafe;
        out.initialCourse = preferred;
        out.targetCourse = preferred;
        out.targetVelocityMps = glm::dvec3(0.0);
        out.endPositionMeters = request.positionMeters;
        out.message =
            "stationary Assisted craft needs no moving-course recovery";
        return out;
    }

    out.initialCourse =
        request.velocityMps / out.speedMps;
    out.targetCourse = out.initialCourse;
    out.targetVelocityMps = request.velocityMps;

    const double braking =
        std::max(Epsilon, request.brakingAccelerationMps2);
    out.stopDistanceMeters =
        out.speedMps * out.speedMps /
        (2.0 * braking);

    out.sceneRayMeters =
        sceneRayLength(
            request.positionMeters,
            out.stopDistanceMeters + SceneExitMarginMeters,
            request.obstacles,
            request.hullRadiusMeters,
            request.additionalClearanceMeters
        );

    if (clearSegment(
            request.positionMeters,
            request.positionMeters +
                out.initialCourse * out.sceneRayMeters,
            request.obstacles,
            request.hullRadiusMeters,
            request.additionalClearanceMeters
        ))
    {
        out.disposition =
            AssistedSafeCourseRecoveryDisposition::
                CurrentCourseSafe;
        out.endPositionMeters = request.positionMeters;
        out.message =
            "current Assisted ballistic course is already safe";
        return out;
    }

    const double stopRay =
        out.stopDistanceMeters +
        std::max(25.0, request.hullRadiusMeters);
    out.stopCorridorClear =
        clearSegment(
            request.positionMeters,
            request.positionMeters +
                out.initialCourse * stopRay,
            request.obstacles,
            request.hullRadiusMeters,
            request.additionalClearanceMeters
        );

    struct Best
    {
        bool valid = false;
        double score = std::numeric_limits<double>::infinity();
        Candidate candidate;
    } best;

    const auto consider =
        [&](const glm::dvec3& direction)
        {
            if (!finite3(direction) ||
                glm::dot(direction, direction) <= Epsilon)
            {
                return;
            }

            const Candidate candidate =
                proveCandidate(
                    request,
                    out.initialCourse,
                    direction,
                    out.speedMps
                );
            if (!candidate.valid)
                return;

            const double goalAngle =
                angleBetween(
                    candidate.course,
                    preferred
                );

            // Primary objective: smallest physical change to the existing
            // inertial course. Goal preference only breaks similar escapes.
            const double score =
                candidate.angleRad * 10.0 +
                goalAngle;

            if (!best.valid || score < best.score)
            {
                best.valid = true;
                best.score = score;
                best.candidate = candidate;
            }
        };

    consider(preferred);

    const glm::dvec3 basisX =
        deterministicPerpendicular(out.initialCourse);
    const glm::dvec3 basisY =
        normalizedOr(
            glm::cross(out.initialCourse, basisX),
            {0.0, 1.0, 0.0}
        );

    constexpr std::array<double, 9> RingDegrees {
        15.0, 30.0, 45.0, 60.0, 75.0,
        90.0, 120.0, 150.0, 180.0
    };
    constexpr int AzimuthSamples = 16;

    for (const double degrees : RingDegrees)
    {
        const double angle =
            degrees * glm::pi<double>() / 180.0;
        const double c = std::cos(angle);
        const double s = std::sin(angle);
        const int samples =
            degrees >= 179.9 ? 1 : AzimuthSamples;

        for (int i = 0; i < samples; ++i)
        {
            const double phi =
                2.0 * glm::pi<double>() *
                static_cast<double>(i) /
                static_cast<double>(samples);

            const glm::dvec3 radial =
                basisX * std::cos(phi) +
                basisY * std::sin(phi);

            consider(
                out.initialCourse * c +
                radial * s
            );
        }

        if (best.valid &&
            best.candidate.angleRad <= angle + 1.0e-6)
        {
            break;
        }
    }

    if (!best.valid)
    {
        out.disposition =
            AssistedSafeCourseRecoveryDisposition::
                StopFallback;
        out.message =
            out.stopCorridorClear
                ? "no constant-speed Assisted escape arc; stop corridor is clear"
                : "no proven Assisted escape arc; emergency stop is the only fallback";
        return out;
    }

    out.disposition =
        AssistedSafeCourseRecoveryDisposition::Maneuver;
    out.targetCourse = best.candidate.course;
    out.targetVelocityMps =
        out.targetCourse * out.speedMps;
    out.turnAxis = best.candidate.axis;
    out.turnCenterMeters =
        best.candidate.centerMeters;
    out.turnAngleRad =
        best.candidate.angleRad;
    out.turnRadiusMeters =
        best.candidate.radiusMeters;
    out.courseAngularRateRadPerSecond =
        best.candidate.angularRateRadPerSecond;
    out.durationSeconds =
        best.candidate.durationSeconds;
    out.sceneRayMeters =
        best.candidate.sceneRayMeters;
    out.endPositionMeters =
        best.candidate.endPositionMeters;

    out.trackingPositionEnvelopeMeters =
        std::max(
            1.0,
            request.additionalClearanceMeters * 0.5
        );
    out.trackingVelocityEnvelopeMps =
        std::max(
            0.5,
            std::sqrt(
                2.0 *
                std::max(0.0, request.courseAccelerationMps2) *
                out.trackingPositionEnvelopeMeters
            )
        );

    out.message =
        "committed Assisted safe-course maneuver proven";

    return out;
}

AssistedSafeCourseRecoverySample
AssistedSafeCourseRecovery::sample(
    const AssistedSafeCourseRecoveryPlan& plan,
    double elapsedSeconds
) noexcept
{
    AssistedSafeCourseRecoverySample out;

    if (!plan.valid() ||
        !plan.requiresManeuver() ||
        !std::isfinite(elapsedSeconds) ||
        plan.speedMps <= MovingSpeedEpsilonMps ||
        plan.courseAngularRateRadPerSecond <= Epsilon ||
        !std::isfinite(plan.turnRadiusMeters) ||
        plan.turnRadiusMeters <= 0.0)
    {
        return out;
    }

    const double elapsed =
        std::max(0.0, elapsedSeconds);
    out.valid = true;
    out.elapsedSeconds = elapsed;

    if (elapsed >= plan.durationSeconds)
    {
        out.turnComplete = true;
        out.turnProgress01 = 1.0;
        out.positionMeters =
            plan.endPositionMeters +
            plan.targetVelocityMps *
                (elapsed - plan.durationSeconds);
        out.velocityMps = plan.targetVelocityMps;
        out.linearAccelerationMps2 = glm::dvec3(0.0);
        return out;
    }

    const double angle =
        plan.courseAngularRateRadPerSecond *
        elapsed;
    const double progress =
        plan.turnAngleRad > Epsilon
            ? angle / plan.turnAngleRad
            : 1.0;

    const glm::dvec3 initialRadial =
        normalizedOr(
            plan.startPositionMeters -
                plan.turnCenterMeters,
            deterministicPerpendicular(plan.initialCourse)
        );

    const glm::dvec3 radial =
        rotateAroundAxis(
            initialRadial,
            plan.turnAxis,
            angle
        );
    const glm::dvec3 course =
        normalizedOr(
            rotateAroundAxis(
                plan.initialCourse,
                plan.turnAxis,
                angle
            ),
            plan.targetCourse
        );

    out.turnProgress01 =
        std::clamp(progress, 0.0, 1.0);
    out.positionMeters =
        plan.turnCenterMeters +
        radial * plan.turnRadiusMeters;
    out.velocityMps =
        course * plan.speedMps;
    out.linearAccelerationMps2 =
        plan.courseAngularRateRadPerSecond *
        glm::cross(
            plan.turnAxis,
            out.velocityMps
        );

    return out;
}

AssistedSafeCourseRecoveryAssessment
AssistedSafeCourseRecovery::assess(
    const AssistedSafeCourseRecoveryPlan& plan,
    const glm::dvec3& actualPositionMeters,
    const glm::dvec3& actualVelocityMps,
    const std::vector<world::navigation::NavigationObstacle>&
        currentObstacles
)
{
    AssistedSafeCourseRecoveryAssessment out;

    if (!plan.valid() ||
        !finite3(actualPositionMeters) ||
        !finite3(actualVelocityMps))
    {
        out.message =
            "invalid Assisted safe-course recovery assessment";
        return out;
    }

    if (plan.disposition ==
        AssistedSafeCourseRecoveryDisposition::
            CurrentCourseSafe)
    {
        out.disposition =
            AssistedSafeCourseRecoveryAssessmentDisposition::
                Complete;
        out.remainingTrajectoryClear = true;
        out.message =
            "no Assisted safe-course maneuver is required";
        return out;
    }

    if (plan.disposition ==
        AssistedSafeCourseRecoveryDisposition::
            StopFallback)
    {
        out.disposition =
            AssistedSafeCourseRecoveryAssessmentDisposition::
                StopFallback;
        out.remainingTrajectoryClear =
            plan.stopCorridorClear;
        out.message = plan.message;
        return out;
    }

    const double actualSpeedMps =
        glm::length(actualVelocityMps);
    if (!std::isfinite(actualSpeedMps) ||
        actualSpeedMps <= MovingSpeedEpsilonMps)
    {
        out.disposition =
            AssistedSafeCourseRecoveryAssessmentDisposition::
                ReplanRequired;
        out.message =
            "Assisted craft lost its moving course before recovery completed";
        return out;
    }

    const glm::dvec3 actualCourse =
        actualVelocityMps / actualSpeedMps;
    const double progressAngleRad =
        directedPositionProgressAngle(
            plan,
            actualPositionMeters
        );

    const double nominalTime =
        progressAngleRad /
        plan.courseAngularRateRadPerSecond;

    out.target =
        sample(
            plan,
            nominalTime
        );
    if (!out.target.valid)
    {
        out.message =
            "committed Assisted maneuver could not be sampled";
        return out;
    }

    out.positionErrorMeters =
        glm::length(
            actualPositionMeters -
            out.target.positionMeters
        );

    // Safety recovery owns COURSE, not cruise-speed recapture. Losing speed is
    // conservative; exceeding the speed used to prove the arc is not.
    const double trackingSpeedMps =
        std::min(
            actualSpeedMps,
            plan.speedMps
        );
    const glm::dvec3 targetCourse =
        normalizedOr(
            out.target.velocityMps,
            plan.targetCourse
        );

    out.target.velocityMps =
        targetCourse * trackingSpeedMps;

    const double requiredCentripetalMps2 =
        plan.turnRadiusMeters > Epsilon
            ? trackingSpeedMps *
                trackingSpeedMps /
                plan.turnRadiusMeters
            : 0.0;

    out.target.linearAccelerationMps2 =
        requiredCentripetalMps2 *
        glm::cross(
            plan.turnAxis,
            targetCourse
        );

    out.velocityErrorMps =
        std::max(
            0.0,
            actualSpeedMps - plan.speedMps
        );

    if (out.positionErrorMeters >
            plan.trackingPositionEnvelopeMeters ||
        out.velocityErrorMps >
            plan.trackingVelocityEnvelopeMps)
    {
        out.disposition =
            AssistedSafeCourseRecoveryAssessmentDisposition::
                ReplanRequired;
        out.message =
            "actual Assisted state left committed maneuver safety envelope";
        return out;
    }

    out.remainingTrajectoryClear =
        remainingTrajectoryClear(
            plan,
            progressAngleRad,
            actualPositionMeters,
            currentObstacles
        );

    if (!out.remainingTrajectoryClear)
    {
        out.disposition =
            AssistedSafeCourseRecoveryAssessmentDisposition::
                ReplanRequired;
        out.message =
            "remaining committed Assisted recovery path is no longer clear";
        return out;
    }

    const double remainingArcMeters =
        plan.turnRadiusMeters *
        std::max(
            0.0,
            plan.turnAngleRad - progressAngleRad
        );

    const double terminalCourseErrorMeters =
        plan.turnRadiusMeters *
        angleBetween(
            actualCourse,
            plan.targetCourse
        );

    if (remainingArcMeters <=
            plan.trackingPositionEnvelopeMeters &&
        terminalCourseErrorMeters <=
            plan.trackingPositionEnvelopeMeters)
    {
        out.target.turnComplete = true;
        out.target.turnProgress01 = 1.0;
        out.target.positionMeters =
            plan.endPositionMeters;
        out.target.velocityMps =
            plan.targetCourse * trackingSpeedMps;
        out.target.linearAccelerationMps2 =
            glm::dvec3(0.0);

        out.disposition =
            AssistedSafeCourseRecoveryAssessmentDisposition::
                Complete;
        out.message =
            "committed Assisted safe course acquired";
        return out;
    }

    out.disposition =
        AssistedSafeCourseRecoveryAssessmentDisposition::
            Continue;
    out.message =
        "committed Assisted safe-course maneuver remains valid";
    return out;
}

} // namespace game::navigation
