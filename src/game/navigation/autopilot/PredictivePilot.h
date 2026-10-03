#pragma once

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>

#include "src/game/navigation/LocalFlightControlLaw.h"
#include "src/game/ship/core/ShipControlState.h"
#include "src/game/ship/core/ShipDynamics.h"
#include "src/game/ship/core/ShipParams.h"

namespace game::navigation::autopilot
{

// Second-generation virtual pilot.
//
// This controller is intentionally independent of ShipControlAdapter and of the
// legacy navigation actuator seam. Its input is a desired vehicle state; its
// only output is the same ordinary ShipControlState a human pilot can produce.
//
// Attitude control is predictive: it converts orientation error into a bounded
// target angular rate from the physical braking envelope
//
//      omega_target^2 <= 2 * alpha * angle_remaining
//
// and then accelerates or counter-steers through ordinary pitch/yaw/roll keys.
// Translation in Assisted mode is closed around measured forward speed rather
// than around the persistent setpoint, so releasing +/- occurs only after the
// real craft has reached the requested speed.
class PredictivePilot final
{
public:
    struct Request
    {
        LocalFlightControlLaw law = defaultLocalFlightControlLaw();

        glm::dvec3 desiredVelocityMapMps {0.0};
        glm::dvec3 desiredLinearAccelerationMapMps2 {0.0};

        glm::dvec3 desiredForwardMap {0.0, 0.0, -1.0};
        glm::dvec3 desiredUpMap {0.0, 1.0, 0.0};

        glm::dvec3 actualVelocityMapMps {0.0};
        glm::dvec3 forwardMap {0.0, 0.0, -1.0};
        glm::dvec3 rightMap {1.0, 0.0, 0.0};
        glm::dvec3 upMap {0.0, 1.0, 0.0};

        double pitchRateRadPerSec = 0.0;
        double yawRateRadPerSec = 0.0;
        double rollRateRadPerSec = 0.0;

        bool stopRequested = false;
        double deltaSeconds = 0.0;
    };

    [[nodiscard]] static ShipControlState make(
        const Request& request,
        const ShipParams& params
    ) noexcept
    {
        ShipControlState out;

        const glm::dvec3 forward =
            normalizedOr(request.forwardMap, {0.0, 0.0, -1.0});
        const glm::dvec3 right =
            normalizedOr(request.rightMap, {1.0, 0.0, 0.0});
        const glm::dvec3 up =
            normalizedOr(request.upMap, {0.0, 1.0, 0.0});

        const glm::dvec3 desiredForward =
            normalizedOr(request.desiredForwardMap, forward);
        glm::dvec3 desiredUp =
            request.desiredUpMap -
            desiredForward * glm::dot(request.desiredUpMap, desiredForward);
        desiredUp = normalizedOr(desiredUp, up);

        const glm::dvec3 rotationErrorMap =
            orientationErrorVector(
                forward,
                up,
                desiredForward,
                desiredUp
            );

        const double angularAuthority =
            game::ship::angularAccelerationLimitRadPerSec2(params);
        const double dt =
            std::max(
                1.0e-4,
                std::isfinite(request.deltaSeconds)
                    ? request.deltaSeconds
                    : 0.0
            );

        if (angularAuthority > 1.0e-9)
        {
            const double pitchError =
                glm::dot(rotationErrorMap, right);
            const double yawError =
                glm::dot(rotationErrorMap, up);
            const double rollError =
                glm::dot(rotationErrorMap, forward);

            out.pitchInput = static_cast<float>(
                predictiveAxisInput(
                    pitchError,
                    finiteOrZero(request.pitchRateRadPerSec),
                    std::max(0.0, static_cast<double>(params.maxPitchRate)),
                    angularAuthority,
                    dt
                )
            );
            out.yawInput = static_cast<float>(
                predictiveAxisInput(
                    yawError,
                    finiteOrZero(request.yawRateRadPerSec),
                    std::max(0.0, static_cast<double>(params.maxYawRate)),
                    angularAuthority,
                    dt
                )
            );
            out.rollInput = static_cast<float>(
                predictiveAxisInput(
                    rollError,
                    finiteOrZero(request.rollRateRadPerSec),
                    std::max(0.0, static_cast<double>(params.maxRollRate)),
                    angularAuthority,
                    dt
                )
            );
        }

        const double actualSpeed = finiteLength(request.actualVelocityMapMps);
        constexpr double PrecisionStopEntrySpeedMps = 0.50;
        const bool precisionStop =
            request.stopRequested &&
            actualSpeed <= PrecisionStopEntrySpeedMps;

        if (request.stopRequested && !precisionStop)
        {
            // END is an ordinary human control in both laws. Assisted uses its
            // real reverse-main authority when installed; Newtonian owns the
            // normal turn-and-burn alignment sequence.
            out.velocityAlignmentCommand =
                VelocityAlignmentMode::BrakeToStop;
        }
        else if (precisionStop && dt > 1.0e-9)
        {
            const double rcsAuthority =
                game::ship::manoeuvreAccelerationLimitMps2(params);
            if (rcsAuthority > 1.0e-9)
            {
                glm::dvec3 wantedAcceleration =
                    (request.desiredVelocityMapMps -
                     request.actualVelocityMapMps) / dt;
                const double wantedMagnitude =
                    glm::length(wantedAcceleration);
                if (std::isfinite(wantedMagnitude) &&
                    wantedMagnitude > rcsAuthority)
                {
                    wantedAcceleration *= rcsAuthority / wantedMagnitude;
                }

                out.forwardInput = finiteClamp(
                    glm::dot(wantedAcceleration, forward) / rcsAuthority
                );
                out.strafeInput = finiteClamp(
                    glm::dot(wantedAcceleration, right) / rcsAuthority
                );
                out.liftInput = finiteClamp(
                    glm::dot(wantedAcceleration, up) / rcsAuthority
                );
            }
        }
        else if (request.law == LocalFlightControlLaw::Assisted)
        {
            const double desiredSpeed =
                finiteLength(request.desiredVelocityMapMps);
            const double actualForwardSpeed =
                std::max(
                    0.0,
                    finiteOrZero(
                        glm::dot(request.actualVelocityMapMps, forward)
                    )
                );

            const double maxSpeed =
                game::ship::controlledSpeedLimitMps(params);
            const double targetRateMps2 = std::max(
                static_cast<double>(
                    params.assistedMinimumTargetSpeedChangeRateMps2
                ),
                maxSpeed *
                    static_cast<double>(
                        params.assistedTargetSpeedChangeRateFractionPerSecond
                    )
            );

            // Do not chase the hidden Assisted setpoint. A human observes what
            // the craft is actually doing, presses +/- and releases when the
            // real speed reaches the target. The 0.75 s horizon makes that
            // behavior continuous instead of full-key bang/bang.
            constexpr double SpeedPredictionHorizonSeconds = 0.75;
            if (targetRateMps2 > 1.0e-9)
            {
                out.targetSpeedRate = finiteClamp(
                    (desiredSpeed - actualForwardSpeed) /
                    (targetRateMps2 * SpeedPredictionHorizonSeconds)
                );
            }
        }
        else
        {
            const double authority =
                game::ship::forwardMainAccelerationLimitMps2(params);
            if (authority > 1.0e-9)
            {
                out.targetSpeedRate = static_cast<float>(
                    std::clamp(
                        glm::dot(
                            request.desiredLinearAccelerationMapMps2,
                            forward
                        ) / authority,
                        0.0,
                        1.0
                    )
                );
            }
        }

        // Absolute V2 invariant: this pilot never falls through to direct
        // navigation actuator control.
        out.navigationAccelerationDemandValid = false;
        out.navigationVelocityTargetValid = false;
        out.navigationPrecisionTranslationOnly = false;
        return out;
    }

private:
    [[nodiscard]] static double finiteOrZero(double value) noexcept
    {
        return std::isfinite(value) ? value : 0.0;
    }

    [[nodiscard]] static double finiteLength(
        const glm::dvec3& value
    ) noexcept
    {
        const double length = glm::length(value);
        return std::isfinite(length) ? length : 0.0;
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

    [[nodiscard]] static float finiteClamp(double value) noexcept
    {
        return static_cast<float>(
            std::clamp(
                std::isfinite(value) ? value : 0.0,
                -1.0,
                1.0
            )
        );
    }

    [[nodiscard]] static glm::dvec3 orientationErrorVector(
        const glm::dvec3& forward,
        const glm::dvec3& up,
        const glm::dvec3& desiredForward,
        const glm::dvec3& desiredUp
    ) noexcept
    {
        glm::dvec3 rotation(0.0);

        const double forwardDot =
            std::clamp(glm::dot(forward, desiredForward), -1.0, 1.0);
        const double forwardAngle = std::acos(forwardDot);
        glm::dvec3 forwardAxis = glm::cross(forward, desiredForward);
        double axisLength = glm::length(forwardAxis);

        if (forwardAngle > 1.0e-9)
        {
            if (!(axisLength > 1.0e-12))
            {
                forwardAxis = glm::cross(forward, up);
                axisLength = glm::length(forwardAxis);
            }
            if (axisLength > 1.0e-12)
                rotation += forwardAxis / axisLength * forwardAngle;
        }

        glm::dvec3 currentUpProjected =
            up - desiredForward * glm::dot(up, desiredForward);
        glm::dvec3 desiredUpProjected =
            desiredUp -
            desiredForward * glm::dot(desiredUp, desiredForward);

        const double currentUpLength = glm::length(currentUpProjected);
        const double desiredUpLength = glm::length(desiredUpProjected);
        if (currentUpLength > 1.0e-12 && desiredUpLength > 1.0e-12)
        {
            currentUpProjected /= currentUpLength;
            desiredUpProjected /= desiredUpLength;
            const double sinRoll = glm::dot(
                glm::cross(currentUpProjected, desiredUpProjected),
                desiredForward
            );
            const double cosRoll = std::clamp(
                glm::dot(currentUpProjected, desiredUpProjected),
                -1.0,
                1.0
            );
            rotation += desiredForward * std::atan2(sinRoll, cosRoll);
        }

        return rotation;
    }

    [[nodiscard]] static double predictiveAxisInput(
        double angleErrorRad,
        double angularRateRadPerSec,
        double maxRateRadPerSec,
        double angularAuthorityRadPerSec2,
        double dt
    ) noexcept
    {
        constexpr double AngleDeadbandRad = 0.0015;
        constexpr double RateDeadbandRadPerSec = 0.004;
        constexpr double ResponseHorizonSeconds = 0.18;

        if (!std::isfinite(angleErrorRad) ||
            !std::isfinite(angularRateRadPerSec) ||
            angularAuthorityRadPerSec2 <= 1.0e-9)
        {
            return 0.0;
        }

        if (std::abs(angleErrorRad) <= AngleDeadbandRad &&
            std::abs(angularRateRadPerSec) <= RateDeadbandRadPerSec)
        {
            return 0.0;
        }

        // The target angular rate is exactly the largest rate that can still
        // be stopped inside the remaining angle at full available authority.
        const double brakingLimitedRate = std::sqrt(
            std::max(
                0.0,
                2.0 * angularAuthorityRadPerSec2 *
                    std::abs(angleErrorRad)
            )
        );
        const double rateLimit =
            maxRateRadPerSec > 1.0e-9
                ? maxRateRadPerSec
                : brakingLimitedRate;
        const double desiredRate =
            std::copysign(
                std::min(rateLimit, brakingLimitedRate),
                angleErrorRad
            );

        const double horizon =
            std::max(
                ResponseHorizonSeconds,
                std::min(0.50, dt * 4.0)
            );
        const double requiredAcceleration =
            (desiredRate - angularRateRadPerSec) / horizon;

        return std::clamp(
            requiredAcceleration / angularAuthorityRadPerSec2,
            -1.0,
            1.0
        );
    }
};

} // namespace game::navigation::autopilot
