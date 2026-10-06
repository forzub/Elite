#pragma once

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>

namespace game::navigation::autopilot
{

// Pure route-course guidance.
//
// Navigation owns the direction of translational motion, not the decorative
// orientation of the hull. While the craft is moving, route steering therefore
// compares the authored desired course with normalize(actualVelocity).
//
// The hull basis is used only to express the course correction on the real
// pitch/yaw actuators. At effectively zero speed the velocity direction is
// undefined, so currentForward is the deterministic fallback until motion
// exists again.
class VelocityCourseGuidance final
{
public:
    struct Request
    {
        glm::dvec3 actualVelocityMapMps {0.0};
        glm::dvec3 currentForward {0.0, 0.0, -1.0};
        glm::dvec3 currentRight {1.0, 0.0, 0.0};
        glm::dvec3 currentUp {0.0, 1.0, 0.0};
        glm::dvec3 targetCourseMap {0.0, 0.0, -1.0};
        double velocityDirectionThresholdMps = 0.5;
    };

    struct Result
    {
        bool valid = false;
        bool usingVelocityDirection = false;

        glm::dvec3 actualCourseMap {0.0, 0.0, -1.0};
        glm::dvec3 targetCourseMap {0.0, 0.0, -1.0};

        // Local body axes: X=pitch, Y=yaw. No roll component exists here.
        glm::dvec2 pitchYawErrorLocalRad {0.0};
        double courseErrorRad = 0.0;
    };

    [[nodiscard]] static Result evaluate(const Request& request) noexcept
    {
        Result out;

        const glm::dvec3 hullForward =
            normalizedOr(request.currentForward, {0.0, 0.0, -1.0});
        const glm::dvec3 right =
            normalizedOr(request.currentRight, {1.0, 0.0, 0.0});
        const glm::dvec3 up =
            normalizedOr(request.currentUp, {0.0, 1.0, 0.0});

        const double speed = glm::length(request.actualVelocityMapMps);
        const double threshold =
            std::max(0.0, request.velocityDirectionThresholdMps);

        out.usingVelocityDirection =
            std::isfinite(speed) && speed > threshold;

        out.actualCourseMap =
            out.usingVelocityDirection
                ? request.actualVelocityMapMps / speed
                : hullForward;

        out.targetCourseMap =
            normalizedOr(request.targetCourseMap, out.actualCourseMap);

        out.courseErrorRad =
            angleBetween(out.actualCourseMap, out.targetCourseMap);

        const glm::dvec3 cross =
            glm::cross(out.actualCourseMap, out.targetCourseMap);
        const double crossLength = glm::length(cross);

        glm::dvec3 courseErrorMap(0.0);
        if (out.courseErrorRad > 1.0e-12 &&
            std::isfinite(crossLength) &&
            crossLength > 1.0e-12)
        {
            courseErrorMap =
                cross / crossLength * out.courseErrorRad;
        }
        else if (out.courseErrorRad >
                 3.14159265358979323846 - 1.0e-9)
        {
            courseErrorMap = up * out.courseErrorRad;
        }

        out.pitchYawErrorLocalRad = {
            glm::dot(courseErrorMap, right),
            glm::dot(courseErrorMap, up)
        };

        out.valid =
            finiteVec(out.actualCourseMap) &&
            finiteVec(out.targetCourseMap) &&
            std::isfinite(out.pitchYawErrorLocalRad.x) &&
            std::isfinite(out.pitchYawErrorLocalRad.y) &&
            std::isfinite(out.courseErrorRad);

        return out;
    }

private:
    [[nodiscard]] static bool finiteVec(const glm::dvec3& v) noexcept
    {
        return
            std::isfinite(v.x) &&
            std::isfinite(v.y) &&
            std::isfinite(v.z);
    }

    [[nodiscard]] static glm::dvec3 normalizedOr(
        const glm::dvec3& value,
        const glm::dvec3& fallback
    ) noexcept
    {
        const double length = glm::length(value);
        if (std::isfinite(length) && length > 1.0e-12)
            return value / length;

        const double fallbackLength = glm::length(fallback);
        return fallbackLength > 1.0e-12
            ? fallback / fallbackLength
            : glm::dvec3(0.0, 0.0, -1.0);
    }

    [[nodiscard]] static double angleBetween(
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
};

} // namespace game::navigation::autopilot
