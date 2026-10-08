#pragma once

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>

namespace game::navigation::autopilot
{

// VERIFIED NAVIGATION CONTRACT.
//
// While the craft is moving, pitch/yaw guidance is defined ONLY by the
// velocity-course error:
//
//     actual velocity direction  ->  desired route course
//
// Hull forward is NOT a navigation reference. It is only an actuator state
// and supplies the body axes used to express pitch/yaw commands.
//
// Do not replace course error with nose-to-tangent error. That recreates the
// old "sharp turn, then return the nose" behaviour on authored curves.
class VelocityCourseGuidance final
{
public:
    struct Request
    {
        glm::dvec3 actualVelocityMapMps {0.0};
        glm::dvec3 desiredCourseMap {0.0, 0.0, -1.0};

        glm::dvec3 bodyForwardMap {0.0, 0.0, -1.0};
        glm::dvec3 bodyRightMap {1.0, 0.0, 0.0};
        glm::dvec3 bodyUpMap {0.0, 1.0, 0.0};

        double velocityDirectionThresholdMps = 0.5;
    };

    struct Result
    {
        bool valid = false;
        glm::dvec3 actualCourseMap {0.0, 0.0, -1.0};
        glm::dvec3 desiredCourseMap {0.0, 0.0, -1.0};
        glm::dvec2 pitchYawErrorLocalRad {0.0};
        double courseErrorRad = 0.0;
    };

    [[nodiscard]] static Result evaluate(
        const Request& request
    ) noexcept
    {
        Result out;

        const glm::dvec3 bodyForward =
            normalizedOr(request.bodyForwardMap, {0.0, 0.0, -1.0});
        const glm::dvec3 bodyRight =
            normalizedOr(request.bodyRightMap, {1.0, 0.0, 0.0});
        const glm::dvec3 bodyUp =
            normalizedOr(request.bodyUpMap, {0.0, 1.0, 0.0});

        const double speed = glm::length(request.actualVelocityMapMps);
        out.actualCourseMap =
            std::isfinite(speed) &&
            speed > std::max(0.0, request.velocityDirectionThresholdMps)
                ? request.actualVelocityMapMps / speed
                : bodyForward;

        out.desiredCourseMap =
            normalizedOr(request.desiredCourseMap, out.actualCourseMap);

        out.courseErrorRad =
            std::acos(
                std::clamp(
                    glm::dot(
                        out.actualCourseMap,
                        out.desiredCourseMap
                    ),
                    -1.0,
                    1.0
                )
            );

        glm::dvec3 errorMap(0.0);
        const glm::dvec3 cross =
            glm::cross(out.actualCourseMap, out.desiredCourseMap);
        const double crossLength = glm::length(cross);

        if (out.courseErrorRad > 1.0e-12 &&
            std::isfinite(crossLength) &&
            crossLength > 1.0e-12)
        {
            errorMap =
                cross / crossLength *
                out.courseErrorRad;
        }
        else if (
            out.courseErrorRad >
                3.14159265358979323846 - 1.0e-9)
        {
            // 180 degrees has no unique axis. Body-up is deterministic and
            // still keeps the navigation reference velocity-based.
            errorMap = bodyUp * out.courseErrorRad;
        }

        out.pitchYawErrorLocalRad = {
            glm::dot(errorMap, bodyRight),
            glm::dot(errorMap, bodyUp)
        };

        out.valid =
            finiteVec(out.actualCourseMap) &&
            finiteVec(out.desiredCourseMap) &&
            finiteVec(out.pitchYawErrorLocalRad) &&
            std::isfinite(out.courseErrorRad);

        return out;
    }

private:
    [[nodiscard]] static bool finiteVec(
        const glm::dvec3& v
    ) noexcept
    {
        return
            std::isfinite(v.x) &&
            std::isfinite(v.y) &&
            std::isfinite(v.z);
    }

    [[nodiscard]] static bool finiteVec(
        const glm::dvec2& v
    ) noexcept
    {
        return std::isfinite(v.x) && std::isfinite(v.y);
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
        return
            std::isfinite(fallbackLength) &&
            fallbackLength > 1.0e-12
                ? fallback / fallbackLength
                : glm::dvec3(0.0, 0.0, -1.0);
    }
};

} // namespace game::navigation::autopilot
