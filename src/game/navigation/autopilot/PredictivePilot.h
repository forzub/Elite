#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include <glm/glm.hpp>

#include "src/game/navigation/LocalFlightControlLaw.h"
#include "src/game/ship/core/ShipControlState.h"
#include "src/game/ship/core/ShipDynamics.h"
#include "src/game/ship/core/ShipParams.h"

namespace game::navigation::autopilot
{

class PredictivePilot final
{
public:
    struct State
    {
        bool initialized = false;

        glm::dvec3 previousVelocityMapMps {0.0};
        double previousPitchRateRadPerSec = 0.0;
        double previousYawRateRadPerSec = 0.0;
        double previousRollRateRadPerSec = 0.0;

        float previousPitchInput = 0.0f;
        float previousYawInput = 0.0f;
        float previousRollInput = 0.0f;
        float previousTargetSpeedRate = 0.0f;

        double effectivePitchAuthorityRadPerSec2 = 0.0;
        double effectiveYawAuthorityRadPerSec2 = 0.0;
        double effectiveRollAuthorityRadPerSec2 = 0.0;

        // Measured first-order Assisted response estimate. This is not a
        // planner property; it is learned from the real craft while V2 flies.
        double assistedCourseResponseSeconds = 2.0;
        double assistedSpeedResponseMps2 = 0.0;

        double lastMeasuredCourseErrorRad = 0.0;
        bool hasMeasuredCourseError = false;
    };

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
        const ShipParams& params,
        State& state
    ) noexcept
    {
        const double dt =
            std::max(
                1.0e-4,
                std::isfinite(request.deltaSeconds)
                    ? request.deltaSeconds
                    : 0.0
            );

        const glm::dvec3 forward =
            normalizedOr(request.forwardMap, {0.0, 0.0, -1.0});
        const glm::dvec3 right =
            normalizedOr(request.rightMap, {1.0, 0.0, 0.0});
        const glm::dvec3 up =
            normalizedOr(request.upMap, {0.0, 1.0, 0.0});

        updateIdentification(request, params, forward, dt, state);

        ShipControlState out;

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

        const double configuredAngularAuthority =
            game::ship::angularAccelerationLimitRadPerSec2(params);

        const double pitchAuthority =
            learnedAuthority(
                state.effectivePitchAuthorityRadPerSec2,
                configuredAngularAuthority
            );
        const double yawAuthority =
            learnedAuthority(
                state.effectiveYawAuthorityRadPerSec2,
                configuredAngularAuthority
            );
        const double rollAuthority =
            learnedAuthority(
                state.effectiveRollAuthorityRadPerSec2,
                configuredAngularAuthority
            );

        out.pitchInput = static_cast<float>(
            choosePredictiveAxisInput(
                glm::dot(rotationErrorMap, right),
                finiteOrZero(request.pitchRateRadPerSec),
                std::max(0.0, static_cast<double>(params.maxPitchRate)),
                pitchAuthority,
                dt
            )
        );
        out.yawInput = static_cast<float>(
            choosePredictiveAxisInput(
                glm::dot(rotationErrorMap, up),
                finiteOrZero(request.yawRateRadPerSec),
                std::max(0.0, static_cast<double>(params.maxYawRate)),
                yawAuthority,
                dt
            )
        );
        out.rollInput = static_cast<float>(
            choosePredictiveAxisInput(
                glm::dot(rotationErrorMap, forward),
                finiteOrZero(request.rollRateRadPerSec),
                std::max(0.0, static_cast<double>(params.maxRollRate)),
                rollAuthority,
                dt
            )
        );

        const double actualSpeed = finiteLength(request.actualVelocityMapMps);
        constexpr double PrecisionStopEntrySpeedMps = 0.50;
        const bool precisionStop =
            request.stopRequested &&
            actualSpeed <= PrecisionStopEntrySpeedMps;

        if (request.stopRequested && !precisionStop)
        {
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
            const double configuredTargetRate = std::max(
                static_cast<double>(
                    params.assistedMinimumTargetSpeedChangeRateMps2
                ),
                maxSpeed *
                    static_cast<double>(
                        params.assistedTargetSpeedChangeRateFractionPerSecond
                    )
            );
            const double learnedTargetRate =
                state.assistedSpeedResponseMps2 > 1.0e-6
                    ? state.assistedSpeedResponseMps2
                    : configuredTargetRate;
            const double effectiveTargetRate =
                std::clamp(
                    learnedTargetRate,
                    std::max(1.0, configuredTargetRate * 0.25),
                    std::max(1.0, configuredTargetRate * 2.0)
                );

            const double horizon = std::clamp(
                state.assistedCourseResponseSeconds * 0.35,
                0.35,
                1.25
            );

            out.targetSpeedRate = finiteClamp(
                (desiredSpeed - actualForwardSpeed) /
                (effectiveTargetRate * horizon)
            );
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

        out.navigationAccelerationDemandValid = false;
        out.navigationVelocityTargetValid = false;
        out.navigationPrecisionTranslationOnly = false;

        state.initialized = true;
        state.previousVelocityMapMps = request.actualVelocityMapMps;
        state.previousPitchRateRadPerSec =
            finiteOrZero(request.pitchRateRadPerSec);
        state.previousYawRateRadPerSec =
            finiteOrZero(request.yawRateRadPerSec);
        state.previousRollRateRadPerSec =
            finiteOrZero(request.rollRateRadPerSec);
        state.previousPitchInput = out.pitchInput;
        state.previousYawInput = out.yawInput;
        state.previousRollInput = out.rollInput;
        state.previousTargetSpeedRate = out.targetSpeedRate;

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

    [[nodiscard]] static double learnedAuthority(
        double learned,
        double configured
    ) noexcept
    {
        if (!(configured > 1.0e-9))
            return 0.0;
        if (!(learned > 1.0e-9))
            return configured;
        return std::clamp(
            learned,
            configured * 0.25,
            configured * 1.50
        );
    }

    static void updateAuthorityEstimate(
        double previousInput,
        double previousRate,
        double currentRate,
        double dt,
        double configuredAuthority,
        double& estimate
    ) noexcept
    {
        if (!(dt > 1.0e-5) ||
            std::abs(previousInput) < 0.20 ||
            !(configuredAuthority > 1.0e-9))
        {
            return;
        }

        const double measured =
            std::abs((currentRate - previousRate) / dt) /
            std::max(0.20, std::abs(previousInput));
        if (!std::isfinite(measured) || measured <= 1.0e-6)
            return;

        const double bounded =
            std::clamp(
                measured,
                configuredAuthority * 0.20,
                configuredAuthority * 2.0
            );
        if (!(estimate > 1.0e-9))
            estimate = bounded;
        else
            estimate = estimate * 0.92 + bounded * 0.08;
    }

    static void updateIdentification(
        const Request& request,
        const ShipParams& params,
        const glm::dvec3& forward,
        double dt,
        State& state
    ) noexcept
    {
        const double configuredAngularAuthority =
            game::ship::angularAccelerationLimitRadPerSec2(params);

        if (state.initialized)
        {
            updateAuthorityEstimate(
                state.previousPitchInput,
                state.previousPitchRateRadPerSec,
                finiteOrZero(request.pitchRateRadPerSec),
                dt,
                configuredAngularAuthority,
                state.effectivePitchAuthorityRadPerSec2
            );
            updateAuthorityEstimate(
                state.previousYawInput,
                state.previousYawRateRadPerSec,
                finiteOrZero(request.yawRateRadPerSec),
                dt,
                configuredAngularAuthority,
                state.effectiveYawAuthorityRadPerSec2
            );
            updateAuthorityEstimate(
                state.previousRollInput,
                state.previousRollRateRadPerSec,
                finiteOrZero(request.rollRateRadPerSec),
                dt,
                configuredAngularAuthority,
                state.effectiveRollAuthorityRadPerSec2
            );

            if (std::abs(state.previousTargetSpeedRate) > 0.15)
            {
                const double previousForwardSpeed =
                    glm::dot(state.previousVelocityMapMps, forward);
                const double currentForwardSpeed =
                    glm::dot(request.actualVelocityMapMps, forward);
                const double measuredResponse =
                    std::abs(
                        (currentForwardSpeed - previousForwardSpeed) / dt
                    ) /
                    std::max(
                        0.15,
                        std::abs(
                            static_cast<double>(
                                state.previousTargetSpeedRate
                            )
                        )
                    );
                if (std::isfinite(measuredResponse) &&
                    measuredResponse > 1.0e-4)
                {
                    if (!(state.assistedSpeedResponseMps2 > 1.0e-6))
                        state.assistedSpeedResponseMps2 = measuredResponse;
                    else
                        state.assistedSpeedResponseMps2 =
                            state.assistedSpeedResponseMps2 * 0.95 +
                            measuredResponse * 0.05;
                }
            }
        }

        const double speed = finiteLength(request.actualVelocityMapMps);
        if (speed > 0.5)
        {
            const glm::dvec3 velocityDir =
                request.actualVelocityMapMps / speed;
            const double courseError = std::acos(
                std::clamp(
                    glm::dot(velocityDir, forward),
                    -1.0,
                    1.0
                )
            );

            if (state.hasMeasuredCourseError && dt > 1.0e-5)
            {
                const double closureRate =
                    (state.lastMeasuredCourseErrorRad - courseError) / dt;
                if (courseError > 0.01 && closureRate > 1.0e-4)
                {
                    const double measuredTau =
                        courseError / closureRate;
                    if (std::isfinite(measuredTau))
                    {
                        const double boundedTau =
                            std::clamp(measuredTau, 0.25, 5.0);
                        state.assistedCourseResponseSeconds =
                            state.assistedCourseResponseSeconds * 0.95 +
                            boundedTau * 0.05;
                    }
                }
            }

            state.lastMeasuredCourseErrorRad = courseError;
            state.hasMeasuredCourseError = true;
        }
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

    [[nodiscard]] static double choosePredictiveAxisInput(
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

        // Physical angular braking law restored from the original predictive
        // pilot. The commanded angular rate is limited to the largest rate
        // that can still be arrested inside the remaining angle:
        //
        //     omega^2 <= 2 * alpha * theta
        //
        // This makes counter-steer begin BEFORE the target attitude instead
        // of after overshoot.
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
    }};

} // namespace game::navigation::autopilot
