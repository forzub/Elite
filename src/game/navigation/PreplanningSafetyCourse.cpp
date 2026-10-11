#include "src/game/navigation/PreplanningSafetyCourse.h"

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
constexpr double MinimumSceneRayMeters = 5000.0;
constexpr double SceneExitMarginMeters = 1500.0;
constexpr double ArcSampleSpacingMeters = 150.0;
constexpr int ArcSampleCap = 96;

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
    const double length2 = glm::dot(value, value);
    if (std::isfinite(length2) && length2 > Epsilon)
        return value / std::sqrt(length2);

    const double fallbackLength2 = glm::dot(fallback, fallback);
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

double sceneRayLength(
    const PreplanningSafetyCourseRequest& request,
    const glm::dvec3& origin,
    double minimumMeters
) noexcept
{
    double out =
        std::max(MinimumSceneRayMeters, minimumMeters);

    for (const auto& obstacle : request.obstacles)
    {
        if (!obstacle.finite())
            continue;

        const double inflation =
            world::navigation::navigationObstacleInflationMeters(
                obstacle,
                request.hullRadiusMeters,
                request.additionalClearanceMeters
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

bool clearSegment(
    const PreplanningSafetyCourseRequest& request,
    const glm::dvec3& a,
    const glm::dvec3& b
) noexcept
{
    return world::navigation::segmentClearOfNavigationObstacles(
        a,
        b,
        request.obstacles,
        request.hullRadiusMeters,
        request.additionalClearanceMeters
    );
}

struct CandidateCheck
{
    bool clear = false;
    double angleRad = 0.0;
    double radiusMeters = 0.0;
};

CandidateCheck courseTransferClear(
    const PreplanningSafetyCourseRequest& request,
    const glm::dvec3& currentCourse,
    const glm::dvec3& candidateCourse,
    double speedMps
) noexcept
{
    CandidateCheck out;
    const glm::dvec3 from =
        normalizedOr(currentCourse, {0.0, 0.0, -1.0});
    const glm::dvec3 to =
        normalizedOr(candidateCourse, from);

    out.angleRad = angleBetween(from, to);

    if (speedMps <= 0.5 || out.angleRad <= 1.0e-5)
    {
        const double ray =
            sceneRayLength(request, request.positionMeters, 0.0);
        out.clear = clearSegment(
            request,
            request.positionMeters,
            request.positionMeters + to * ray
        );
        return out;
    }

    if (request.courseAccelerationMps2 <= Epsilon ||
        request.maxAngularVelocityRadPerSecond <= Epsilon)
    {
        return out;
    }

    const double accelerationRadius =
        speedMps * speedMps /
        request.courseAccelerationMps2;
    const double angularRadius =
        speedMps /
        request.maxAngularVelocityRadPerSecond;

    out.radiusMeters =
        std::max(
            1.0,
            std::max(accelerationRadius, angularRadius)
        );

    // Before an inertial-course change can be established the hull still has
    // finite slew authority. Reserving the whole bounded slew distance is
    // intentionally conservative: if this straight is already blocked, the
    // manoeuvre is not accepted as a "safe course".
    const double slewSeconds =
        out.angleRad /
        request.maxAngularVelocityRadPerSecond;
    const double preTurnMeters =
        speedMps * slewSeconds;

    glm::dvec3 arcStart = request.positionMeters;
    if (preTurnMeters > 1.0e-6)
    {
        const glm::dvec3 preTurnEnd =
            request.positionMeters +
            from * preTurnMeters;
        if (!clearSegment(
                request,
                request.positionMeters,
                preTurnEnd
            ))
        {
            return out;
        }
        arcStart = preTurnEnd;
    }

    glm::dvec3 normal = glm::cross(from, to);
    if (glm::dot(normal, normal) <= Epsilon)
        normal = deterministicPerpendicular(from);
    normal = normalizedOr(
        normal,
        deterministicPerpendicular(from)
    );

    const glm::dvec3 radial =
        normalizedOr(
            glm::cross(from, normal),
            deterministicPerpendicular(from)
        );
    const glm::dvec3 center =
        arcStart - radial * out.radiusMeters;

    const double arcLength =
        out.radiusMeters * out.angleRad;
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

    glm::dvec3 previous = arcStart;
    glm::dvec3 end = arcStart;

    for (int i = 1; i <= samples; ++i)
    {
        const double t =
            static_cast<double>(i) /
            static_cast<double>(samples);
        const glm::dvec3 rotatedRadial =
            rotateAroundAxis(
                radial,
                normal,
                out.angleRad * t
            );
        end =
            center +
            rotatedRadial * out.radiusMeters;

        if (!clearSegment(request, previous, end))
            return out;

        previous = end;
    }

    const double ray =
        sceneRayLength(
            request,
            end,
            arcLength + preTurnMeters
        );

    out.clear =
        clearSegment(
            request,
            end,
            end + to * ray
        );
    return out;
}

} // namespace

PreplanningSafetyCourseResult
PreplanningSafetyCourse::evaluate(
    const PreplanningSafetyCourseRequest& request
)
{
    PreplanningSafetyCourseResult out;

    if (!finite3(request.positionMeters) ||
        !finite3(request.velocityMps) ||
        !finite3(request.preferredDirection) ||
        request.hullRadiusMeters < 0.0 ||
        request.additionalClearanceMeters < 0.0 ||
        request.courseAccelerationMps2 < 0.0 ||
        request.brakingAccelerationMps2 < 0.0 ||
        request.maxAngularVelocityRadPerSecond < 0.0)
    {
        out.message = "invalid preplanning safety-course request";
        return out;
    }

    out.speedMps = glm::length(request.velocityMps);
    const glm::dvec3 preferred =
        normalizedOr(
            request.preferredDirection,
            {0.0, 0.0, -1.0}
        );
    out.currentCourse =
        out.speedMps > 0.5
            ? request.velocityMps / out.speedMps
            : preferred;
    out.safeCourse = out.currentCourse;

    const double braking =
        std::max(Epsilon, request.brakingAccelerationMps2);
    out.stopDistanceMeters =
        out.speedMps * out.speedMps /
        (2.0 * braking);

    if (request.newtonian &&
        out.speedMps > 0.5 &&
        request.maxAngularVelocityRadPerSecond > Epsilon)
    {
        // Newtonian braking needs the hull to flip before the main bank can
        // remove velocity. Reserve that ballistic translation too.
        out.stopDistanceMeters +=
            out.speedMps *
            (glm::pi<double>() /
             request.maxAngularVelocityRadPerSecond);
    }

    out.sceneRayMeters =
        sceneRayLength(
            request,
            request.positionMeters,
            out.stopDistanceMeters + SceneExitMarginMeters
        );

    if (out.speedMps <= 0.5)
    {
        out.valid = true;
        out.currentCourseSafe = true;
        out.safeCourseFound = true;
        out.physicallyRecoverable = true;
        out.safeCourse = preferred;
        out.message = "stationary start is safe for planning";
        return out;
    }

    if (clearSegment(
            request,
            request.positionMeters,
            request.positionMeters +
                out.currentCourse * out.sceneRayMeters
        ))
    {
        out.valid = true;
        out.currentCourseSafe = true;
        out.safeCourseFound = true;
        out.physicallyRecoverable = true;
        out.message = "current ballistic course clears static planning scene";
        return out;
    }

    const double stopRay =
        std::max(
            0.0,
            out.stopDistanceMeters +
                std::max(25.0, request.hullRadiusMeters)
        );
    out.stopFallbackClear =
        stopRay <= 1.0e-6 ||
        clearSegment(
            request,
            request.positionMeters,
            request.positionMeters +
                out.currentCourse * stopRay
        );

    struct Best
    {
        bool valid = false;
        double score = std::numeric_limits<double>::infinity();
        glm::dvec3 direction {0.0, 0.0, -1.0};
        CandidateCheck check;
    } best;

    const auto consider =
        [&](const glm::dvec3& direction)
        {
            if (!finite3(direction) ||
                glm::dot(direction, direction) <= Epsilon)
            {
                return;
            }

            const glm::dvec3 candidate =
                normalizedOr(direction, out.currentCourse);
            const CandidateCheck check =
                courseTransferClear(
                    request,
                    out.currentCourse,
                    candidate,
                    out.speedMps
                );
            if (!check.clear)
                return;

            const double goalAngle =
                angleBetween(candidate, preferred);

            // Minimal physical course change dominates. Goal preference only
            // breaks otherwise-similar escape directions.
            const double score =
                check.angleRad * 10.0 +
                goalAngle;

            if (!best.valid || score < best.score)
            {
                best.valid = true;
                best.score = score;
                best.direction = candidate;
                best.check = check;
            }
        };

    consider(preferred);

    const glm::dvec3 basisX =
        deterministicPerpendicular(out.currentCourse);
    const glm::dvec3 basisY =
        normalizedOr(
            glm::cross(out.currentCourse, basisX),
            {0.0, 1.0, 0.0}
        );

    constexpr std::array<double,9> RingDegrees {
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
                out.currentCourse * c +
                radial * s
            );
        }

        // Rings are ordered by turn angle. Once this ring produced a valid
        // candidate no wider turn can beat it on the primary criterion.
        if (best.valid &&
            best.check.angleRad <=
                angle + 1.0e-6)
        {
            break;
        }
    }

    out.valid = true;

    if (best.valid)
    {
        out.safeCourseFound = true;
        out.physicallyRecoverable = true;
        out.safeCourse = best.direction;
        out.turnAngleRad = best.check.angleRad;
        out.turnRadiusMeters = best.check.radiusMeters;
        out.message = "alternate collision-free preplanning course found";
        return out;
    }

    out.physicallyRecoverable = out.stopFallbackClear;
    out.message =
        out.stopFallbackClear
            ? "no safe ballistic course; collision-free stop required"
            : "no proven safe course or stopping corridor";
    return out;
}

} // namespace game::navigation
