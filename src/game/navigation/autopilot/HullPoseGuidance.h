#pragma once

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>

namespace game::navigation::autopilot
{

// Pure, axis-separated hull-pose geometry.
//
// Contract:
//   * forward alignment owns pitch/yaw only;
//   * tunnel up/down alignment owns roll only.
//
// No SO(3) combined-error solve is allowed here because this craft is not an
// airplane: rolling the hull around its longitudinal axis must not alter the
// commanded flight direction or corridor capture.
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
        glm::dvec3 targetUpForRoll {0.0, 1.0, 0.0};

        // Local body axes: X=pitch, Y=yaw, Z=roll.
        // pitchYawErrorLocalRad.z is always exactly zero.
        glm::dvec3 pitchYawErrorLocalRad {0.0};

        double rollErrorRad = 0.0;
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

        const glm::dvec3 currentRight =
            normalizedOr(
                glm::cross(currentForward, currentUp),
                request.currentRight
            );

        out.targetForward =
            normalizedOr(request.targetForward, currentForward);

        // ----- Pitch/yaw channel: forward vector only. -----
        out.forwardErrorRad =
            angleBetween(currentForward, out.targetForward);

        const glm::dvec3 forwardCross =
            glm::cross(currentForward, out.targetForward);
        const double forwardCrossLength = glm::length(forwardCross);

        glm::dvec3 forwardErrorMap(0.0);
        if (out.forwardErrorRad > 1.0e-12 &&
            std::isfinite(forwardCrossLength) &&
            forwardCrossLength > 1.0e-12)
        {
            forwardErrorMap =
                forwardCross / forwardCrossLength *
                out.forwardErrorRad;
        }
        else if (out.forwardErrorRad > 3.14159265358979323846 - 1.0e-9)
        {
            // 180 degrees has no unique cross-product axis. Choose current up
            // deterministically; still never introduce roll.
            forwardErrorMap = currentUp * out.forwardErrorRad;
        }

        out.pitchYawErrorLocalRad = {
            glm::dot(forwardErrorMap, currentRight),
            glm::dot(forwardErrorMap, currentUp),
            0.0
        };

        // ----- Roll channel: up/down around CURRENT longitudinal axis only. -----
        glm::dvec3 targetUp =
            request.targetUp -
            currentForward * glm::dot(request.targetUp, currentForward);
        out.targetUpForRoll =
            normalizedOr(targetUp, currentUp);

        out.rollErrorRad =
            std::atan2(
                glm::dot(
                    currentForward,
                    glm::cross(currentUp, out.targetUpForRoll)
                ),
                std::clamp(
                    glm::dot(currentUp, out.targetUpForRoll),
                    -1.0,
                    1.0
                )
            );

        out.upErrorRad = std::abs(out.rollErrorRad);

        out.valid =
            finiteVec(out.targetForward) &&
            finiteVec(out.targetUpForRoll) &&
            finiteVec(out.pitchYawErrorLocalRad) &&
            std::isfinite(out.rollErrorRad) &&
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
