#pragma once

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace game::navigation::autopilot
{

// Pure hull-attitude geometry.
//
// Responsibility:
//   current rigid-body basis + authored target forward/up
//       -> orthonormal target basis + exact SO(3) error.
//
// It knows nothing about route position, corridor capture, speed, engines,
// Assisted course lag, angular authority or ShipControlState.
class HullPoseGuidance final
{
public:
    struct Request
    {
        glm::dvec3 currentForward {0.0, 0.0, -1.0};
        glm::dvec3 currentRight {1.0, 0.0, 0.0};
        glm::dvec3 currentUp {0.0, 1.0, 0.0};

        glm::dvec3 targetForward {0.0, 0.0, -1.0};
        glm::dvec3 targetUp {0.0, 1.0, 0.0};
    };

    struct Result
    {
        bool valid = false;

        glm::dvec3 targetForward {0.0, 0.0, -1.0};
        glm::dvec3 targetRight {1.0, 0.0, 0.0};
        glm::dvec3 targetUp {0.0, 1.0, 0.0};

        glm::dvec3 rotationErrorMapRad {0.0};
        glm::dvec3 rotationErrorLocalRad {0.0};

        double forwardErrorRad = 0.0;
        double upErrorRad = 0.0;
    };

    [[nodiscard]] static Result evaluate(const Request& request) noexcept
    {
        Result out;

        const glm::dvec3 currentForward =
            normalizedOr(request.currentForward, {0.0, 0.0, -1.0});

        glm::dvec3 currentUp =
            request.currentUp -
            currentForward * glm::dot(request.currentUp, currentForward);
        currentUp = normalizedOr(currentUp, {0.0, 1.0, 0.0});

        glm::dvec3 currentRight =
            normalizedOr(
                glm::cross(currentForward, currentUp),
                request.currentRight
            );
        currentUp =
            normalizedOr(
                glm::cross(currentRight, currentForward),
                currentUp
            );

        out.targetForward =
            normalizedOr(request.targetForward, currentForward);

        glm::dvec3 targetUp =
            request.targetUp -
            out.targetForward *
                glm::dot(request.targetUp, out.targetForward);
        out.targetUp = normalizedOr(targetUp, currentUp);

        out.targetRight =
            normalizedOr(
                glm::cross(out.targetForward, out.targetUp),
                currentRight
            );
        out.targetUp =
            normalizedOr(
                glm::cross(out.targetRight, out.targetForward),
                out.targetUp
            );

        const glm::dquat current = glm::normalize(
            glm::quat_cast(
                glm::dmat3(
                    currentRight,
                    currentUp,
                    -currentForward
                )
            )
        );
        const glm::dquat target = glm::normalize(
            glm::quat_cast(
                glm::dmat3(
                    out.targetRight,
                    out.targetUp,
                    -out.targetForward
                )
            )
        );

        glm::dquat delta =
            glm::normalize(target * glm::conjugate(current));
        if (delta.w < 0.0)
            delta = -delta;

        const glm::dvec3 vectorPart(delta.x, delta.y, delta.z);
        const double vectorLength = glm::length(vectorPart);

        if (std::isfinite(vectorLength) && vectorLength > 1.0e-12)
        {
            const double angle =
                2.0 * std::atan2(
                    vectorLength,
                    std::clamp(delta.w, 0.0, 1.0)
                );
            if (!std::isfinite(angle))
                return {};

            out.rotationErrorMapRad =
                vectorPart * (angle / vectorLength);
        }

        out.rotationErrorLocalRad = {
            glm::dot(out.rotationErrorMapRad, currentRight),
            glm::dot(out.rotationErrorMapRad, currentUp),
            glm::dot(out.rotationErrorMapRad, currentForward)
        };

        out.forwardErrorRad =
            angleBetween(currentForward, out.targetForward);
        out.upErrorRad =
            angleBetween(currentUp, out.targetUp);

        out.valid =
            finiteVec(out.targetForward) &&
            finiteVec(out.targetRight) &&
            finiteVec(out.targetUp) &&
            finiteVec(out.rotationErrorMapRad) &&
            finiteVec(out.rotationErrorLocalRad) &&
            std::isfinite(out.forwardErrorRad) &&
            std::isfinite(out.upErrorRad);

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
        const double la = glm::length(a);
        const double lb = glm::length(b);
        if (!(std::isfinite(la) && std::isfinite(lb)) ||
            la <= 1.0e-12 ||
            lb <= 1.0e-12)
        {
            return 0.0;
        }

        return std::acos(
            std::clamp(
                glm::dot(a / la, b / lb),
                -1.0,
                1.0
            )
        );
    }
};

} // namespace game::navigation::autopilot
