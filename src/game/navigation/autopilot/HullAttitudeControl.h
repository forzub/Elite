#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

#include <glm/glm.hpp>

namespace game::navigation::autopilot
{

// Pure angular actuator controller.
//
// WORKING CONTRACT — this controller is currently part of the known-good
// tunnel-following path. Do not merge roll into the pitch/yaw planar channel
// or add route/dock geometry here without a specific failing case.
//
// Pitch/yaw and roll are separate channels:
//   * pitch/yaw share only the nose-steering planar authority;
//   * roll uses an independent longitudinal-axis authority.
//
// No route, position, speed, Assisted course lag or engine state is allowed.
class HullAttitudeControl final
{
public:
    struct Request
    {
        glm::dvec2 pitchYawErrorRad {0.0};
        double rollErrorRad = 0.0;

        glm::dvec2 pitchYawRateRadPerSec {0.0};
        glm::dvec2 desiredPitchYawRateRadPerSec {0.0};
        double rollRateRadPerSec = 0.0;
        double desiredRollRateRadPerSec = 0.0;

        glm::dvec2 maxPitchYawRateRadPerSec {0.0};
        double maxRollRateRadPerSec = 0.0;

        double pitchYawAngularAccelerationAuthorityRadPerSec2 = 0.0;
        double rollAngularAccelerationAuthorityRadPerSec2 = 0.0;
        double deltaSeconds = 0.0;
    };

    struct Result
    {
        bool valid = false;
        glm::dvec2 pitchYawInput {0.0};
        double rollInput = 0.0;
    };

    [[nodiscard]] static Result evaluate(const Request& request) noexcept
    {
        Result out;

        if (!(std::isfinite(request.deltaSeconds) &&
              request.deltaSeconds > 0.0) ||
            !(std::isfinite(
                request.pitchYawAngularAccelerationAuthorityRadPerSec2) &&
              request.pitchYawAngularAccelerationAuthorityRadPerSec2 > 0.0) ||
            !(std::isfinite(
                request.rollAngularAccelerationAuthorityRadPerSec2) &&
              request.rollAngularAccelerationAuthorityRadPerSec2 > 0.0))
        {
            return out;
        }

        out.pitchYawInput = predictivePlanarInput(
            request.pitchYawErrorRad,
            request.pitchYawRateRadPerSec,
            request.desiredPitchYawRateRadPerSec,
            request.maxPitchYawRateRadPerSec,
            request.pitchYawAngularAccelerationAuthorityRadPerSec2,
            request.deltaSeconds
        );

        out.rollInput = predictiveAxisInput(
            request.rollErrorRad,
            request.rollRateRadPerSec,
            request.desiredRollRateRadPerSec,
            request.maxRollRateRadPerSec,
            request.rollAngularAccelerationAuthorityRadPerSec2,
            request.deltaSeconds
        );

        out.valid =
            finiteVec(out.pitchYawInput) &&
            std::isfinite(out.rollInput);

        return out;
    }

private:
    [[nodiscard]] static bool finiteVec(const glm::dvec2& v) noexcept
    {
        return std::isfinite(v.x) && std::isfinite(v.y);
    }

    [[nodiscard]] static double directionalRateLimit(
        const glm::dvec2& direction,
        const glm::dvec2& maxRate
    ) noexcept
    {
        double limit = std::numeric_limits<double>::infinity();
        for (int axis = 0; axis < 2; ++axis)
        {
            const double component = std::abs(direction[axis]);
            if (component <= 1.0e-12)
                continue;
            if (maxRate[axis] <= 1.0e-9)
                return 0.0;
            limit = std::min(limit, maxRate[axis] / component);
        }
        return std::isfinite(limit)
            ? std::max(0.0, limit)
            : std::numeric_limits<double>::infinity();
    }

    [[nodiscard]] static glm::dvec2 predictivePlanarInput(
        const glm::dvec2& error,
        const glm::dvec2& rate,
        const glm::dvec2& desiredRate,
        const glm::dvec2& maxRate,
        double authority,
        double dt
    ) noexcept
    {
        constexpr double AngleDeadband = 0.0015;
        constexpr double RateDeadband = 0.004;

        const double angle = glm::length(error);
        const double rateMagnitude = glm::length(rate);

        if (!std::isfinite(angle) ||
            !std::isfinite(rateMagnitude) ||
            !finiteVec(desiredRate))
        {
            return {};
        }

        const glm::dvec2 boundedDesiredRate = {
            std::clamp(
                desiredRate.x,
                -std::max(0.0, maxRate.x),
                std::max(0.0, maxRate.x)
            ),
            std::clamp(
                desiredRate.y,
                -std::max(0.0, maxRate.y),
                std::max(0.0, maxRate.y)
            )
        };

        if (angle <= AngleDeadband &&
            glm::length(rate - boundedDesiredRate) <= RateDeadband)
        {
            return {};
        }

        if (angle <= 1.0e-12)
        {
            glm::dvec2 requested =
                (boundedDesiredRate - rate) / (authority * dt);
            const double magnitude = glm::length(requested);
            if (magnitude > 1.0)
                requested /= magnitude;
            return requested;
        }

        const glm::dvec2 direction = error / angle;
        const double rateTowardTarget = glm::dot(rate, direction);
        const double reactionTravel =
            std::max(0.0, rateTowardTarget) * dt;
        const double brakingAngleBudget =
            std::max(0.0, angle - reactionTravel);
        const double brakingLimitedRate =
            std::sqrt(2.0 * authority * brakingAngleBudget);

        const double targetMagnitude =
            std::min(
                directionalRateLimit(direction, maxRate),
                brakingLimitedRate
            );

        // Follow the moving course continuously. The feed-forward rate is the
        // angular velocity of the desired VELOCITY direction; the braking
        // envelope contributes only the additional rate needed to remove
        // residual pose error. This avoids treating every frame on a smooth
        // arc as a fresh stationary attitude target.
        glm::dvec2 targetRate =
            boundedDesiredRate + direction * targetMagnitude;
        targetRate.x = std::clamp(
            targetRate.x,
            -std::max(0.0, maxRate.x),
            std::max(0.0, maxRate.x)
        );
        targetRate.y = std::clamp(
            targetRate.y,
            -std::max(0.0, maxRate.y),
            std::max(0.0, maxRate.y)
        );

        glm::dvec2 requested =
            (targetRate - rate) / (authority * dt);
        const double magnitude = glm::length(requested);
        if (magnitude > 1.0)
            requested /= magnitude;
        return requested;
    }

    [[nodiscard]] static double predictiveAxisInput(
        double error,
        double rate,
        double desiredRate,
        double maxRate,
        double authority,
        double dt
    ) noexcept
    {
        constexpr double AngleDeadband = 0.0015;
        constexpr double RateDeadband = 0.004;

        if (!std::isfinite(error) ||
            !std::isfinite(rate) ||
            !std::isfinite(desiredRate) ||
            !std::isfinite(maxRate))
        {
            return 0.0;
        }

        const double boundedDesiredRate =
            std::clamp(
                desiredRate,
                -std::max(0.0, maxRate),
                std::max(0.0, maxRate)
            );

        const double angle = std::abs(error);
        if (angle <= AngleDeadband &&
            std::abs(rate - boundedDesiredRate) <= RateDeadband)
        {
            return 0.0;
        }

        if (angle <= 1.0e-12)
        {
            return std::clamp(
                (boundedDesiredRate - rate) / (authority * dt),
                -1.0,
                1.0
            );
        }

        const double direction =
            error >= 0.0 ? 1.0 : -1.0;
        const double rateTowardTarget =
            rate * direction;
        const double reactionTravel =
            std::max(0.0, rateTowardTarget) * dt;
        const double brakingAngleBudget =
            std::max(0.0, angle - reactionTravel);
        const double brakingLimitedRate =
            std::sqrt(2.0 * authority * brakingAngleBudget);

        const double fallbackRate =
            direction *
            std::min(
                std::max(0.0, maxRate),
                brakingLimitedRate
            );

        // If an upstream guidance layer supplied a desired roll rate, that
        // rate already encodes how quickly the hull should settle toward the
        // frame-up target. Track it directly instead of stacking another
        // bang-bang rate on top. The predictive envelope remains the fallback
        // for callers that provide only an angle target.
        const double targetRate =
            std::abs(boundedDesiredRate) > 1.0e-9
                ? boundedDesiredRate
                : fallbackRate;

        return std::clamp(
            (targetRate - rate) / (authority * dt),
            -1.0,
            1.0
        );
    }
};

} // namespace game::navigation::autopilot
