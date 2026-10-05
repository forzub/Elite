#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

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
        glm::dvec3 desiredAngularVelocityMapRadPerSec {0.0};
        glm::dvec3 desiredAngularAccelerationMapRadPerSec2 {0.0};

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
                right,
                up,
                desiredForward,
                desiredUp
            );

        const double configuredAngularAuthority =
            game::ship::angularAccelerationLimitRadPerSec2(params);

        // The real ship has one shared angular-acceleration envelope. Do not
        // solve pitch/yaw/roll as three independent actuators each owning
        // 100% of alpha; ShipController normalizes their combined request.
        // Solve the complete local rotation vector once.
        const glm::dvec3 localRotationError(
            glm::dot(rotationErrorMap, right),
            glm::dot(rotationErrorMap, up),
            glm::dot(rotationErrorMap, forward)
        );
        const glm::dvec3 currentLocalRate(
            finiteOrZero(request.pitchRateRadPerSec),
            finiteOrZero(request.yawRateRadPerSec),
            finiteOrZero(request.rollRateRadPerSec)
        );
        const glm::dvec3 maxLocalRate(
            std::max(0.0, static_cast<double>(params.maxPitchRate)),
            std::max(0.0, static_cast<double>(params.maxYawRate)),
            std::max(0.0, static_cast<double>(params.maxRollRate))
        );

        const glm::dvec3 desiredLocalRate(
            glm::dot(
                request.desiredAngularVelocityMapRadPerSec,
                right
            ),
            glm::dot(
                request.desiredAngularVelocityMapRadPerSec,
                up
            ),
            glm::dot(
                request.desiredAngularVelocityMapRadPerSec,
                forward
            )
        );
        const glm::dvec3 desiredLocalAcceleration(
            glm::dot(
                request.desiredAngularAccelerationMapRadPerSec2,
                right
            ),
            glm::dot(
                request.desiredAngularAccelerationMapRadPerSec2,
                up
            ),
            glm::dot(
                request.desiredAngularAccelerationMapRadPerSec2,
                forward
            )
        );

        const glm::dvec3 angularInput =
            choosePredictiveRotationInput(
                localRotationError,
                currentLocalRate,
                desiredLocalRate,
                desiredLocalAcceleration,
                maxLocalRate,
                configuredAngularAuthority,
                dt
            );

        out.pitchInput = static_cast<float>(angularInput.x);
        out.yawInput = static_cast<float>(angularInput.y);
        out.rollInput = static_cast<float>(angularInput.z);

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
        else if (precisionStop)
        {
            // Final docking HOLD is translationally neutral. Do not wake RCS
            // to chase centimetres/metres after the main velocity has been
            // arrested; only the attitude loop remains active.
            out.targetSpeedRate = 0.0f;
        }
        else if (request.law == LocalFlightControlLaw::Assisted)
        {
            const double desiredSpeed =
                finiteLength(request.desiredVelocityMapMps);
            // Scalar speed control must use actual speed magnitude.
            // During a turn Assisted intentionally allows the velocity vector
            // to lag behind the nose for a short time. Projecting velocity on
            // the new nose direction makes the measured speed collapse even
            // though the craft is still moving fast, which falsely commands
            // full acceleration exactly when the route is asking us to brake.
            const double actualSpeed =
                finiteLength(request.actualVelocityMapMps);

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

            // Track the authored 1-D motion program, not just its
            // instantaneous speed sample. A reference state with v=0 may
            // still require non-zero acceleration toward the next spatial
            // point. Standard feed-forward + feedback:
            //
            //   a_cmd = a_ref + (v_ref - v_actual) / horizon
            //
            // This is what lets a physically stopped boundary launch without
            // inventing a special-case target speed from a future sample.
            const double feedForwardAcceleration =
                glm::dot(
                    request.desiredLinearAccelerationMapMps2,
                    desiredForward
                );
            const double feedbackAcceleration =
                (desiredSpeed - actualSpeed) / horizon;

            out.targetSpeedRate = finiteClamp(
                (feedForwardAcceleration + feedbackAcceleration) /
                effectiveTargetRate
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
                // Learn longitudinal response from scalar speed,
                // not from a heading-dependent projection. Otherwise merely
                // turning the nose looks like a huge acceleration/deceleration
                // event and corrupts the learned speed authority.
                const double previousSpeed =
                    finiteLength(state.previousVelocityMapMps);
                const double currentSpeed =
                    finiteLength(request.actualVelocityMapMps);
                const double measuredResponse =
                    std::abs(
                        (currentSpeed - previousSpeed) / dt
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
        const glm::dvec3& right,
        const glm::dvec3& up,
        const glm::dvec3& desiredForward,
        const glm::dvec3& desiredUp
    ) noexcept
    {
        const glm::dvec3 currentForward =
            normalizedOr(forward, glm::dvec3(0.0, 0.0, -1.0));
        const glm::dvec3 currentUp =
            normalizedOr(
                up - currentForward * glm::dot(up, currentForward),
                glm::dvec3(0.0, 1.0, 0.0)
            );
        const glm::dvec3 currentRight =
            normalizedOr(
                right,
                glm::cross(currentForward, currentUp)
            );

        const glm::dvec3 targetForward =
            normalizedOr(desiredForward, currentForward);
        glm::dvec3 targetUp =
            desiredUp - targetForward * glm::dot(desiredUp, targetForward);
        targetUp = normalizedOr(targetUp, currentUp);
        const glm::dvec3 targetRight =
            normalizedOr(
                glm::cross(targetForward, targetUp),
                currentRight
            );
        targetUp =
            normalizedOr(
                glm::cross(targetRight, targetForward),
                targetUp
            );

        // Exact shortest-arc SO(3) delta. The former "forward error + roll"
        // construction was only a small-angle approximation and can assign
        // the wrong combined delta on simultaneous pitch/yaw/roll turns.
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
                    targetRight,
                    targetUp,
                    -targetForward
                )
            )
        );

        glm::dquat delta =
            glm::normalize(target * glm::conjugate(current));
        if (delta.w < 0.0)
            delta = -delta;

        const glm::dvec3 vectorPart(delta.x, delta.y, delta.z);
        const double vectorLength = glm::length(vectorPart);
        if (!std::isfinite(vectorLength) || vectorLength <= 1.0e-12)
            return glm::dvec3(0.0);

        const double angle =
            2.0 * std::atan2(
                vectorLength,
                std::clamp(delta.w, 0.0, 1.0)
            );
        if (!std::isfinite(angle))
            return glm::dvec3(0.0);

        return vectorPart * (angle / vectorLength);
    }

    [[nodiscard]] static double directionalRateLimit(
        const glm::dvec3& direction,
        const glm::dvec3& maxRate
    ) noexcept
    {
        double limit = std::numeric_limits<double>::infinity();

        for (int axis = 0; axis < 3; ++axis)
        {
            const double component = std::abs(direction[axis]);
            if (component <= 1.0e-12)
                continue;

            if (maxRate[axis] > 1.0e-9)
                limit = std::min(limit, maxRate[axis] / component);
        }

        return std::isfinite(limit)
            ? std::max(0.0, limit)
            : std::numeric_limits<double>::infinity();
    }

    [[nodiscard]] static glm::dvec3 choosePredictiveRotationInput(
        const glm::dvec3& rotationErrorLocalRad,
        const glm::dvec3& angularRateLocalRadPerSec,
        const glm::dvec3& desiredAngularRateLocalRadPerSec,
        const glm::dvec3& desiredAngularAccelerationLocalRadPerSec2,
        const glm::dvec3& maxRateLocalRadPerSec,
        double angularAuthorityRadPerSec2,
        double dt
    ) noexcept
    {
        constexpr double AngleDeadbandRad = 0.0015;
        constexpr double RateDeadbandRadPerSec = 0.004;

        const double remainingAngle = glm::length(rotationErrorLocalRad);
        const glm::dvec3 rateError =
            desiredAngularRateLocalRadPerSec -
            angularRateLocalRadPerSec;
        const double rateErrorMagnitude = glm::length(rateError);

        if (!std::isfinite(remainingAngle) ||
            !std::isfinite(rateErrorMagnitude) ||
            !(dt > 0.0) ||
            angularAuthorityRadPerSec2 <= 1.0e-9)
        {
            return glm::dvec3(0.0);
        }

        const double feedForwardMagnitude =
            glm::length(desiredAngularAccelerationLocalRadPerSec2);
        if (remainingAngle <= AngleDeadbandRad &&
            rateErrorMagnitude <= RateDeadbandRadPerSec &&
            (!std::isfinite(feedForwardMagnitude) ||
             feedForwardMagnitude <= 1.0e-9))
        {
            return glm::dvec3(0.0);
        }

        // The previous controller was time-optimal bang-bang: even a fraction
        // of a degree could demand full torque. That is appropriate for an
        // isolated capture test but wrong for continuous path tracking.
        //
        // Use a critically damped attitude/rate servo whose time scale comes
        // from the real actuator envelope: time required to ramp from zero to
        // the configured angular-rate limit at full angular acceleration.
        // No arbitrary "turn harder" constant is needed.
        const double characteristicRate =
            std::max({
                maxRateLocalRadPerSec.x,
                maxRateLocalRadPerSec.y,
                maxRateLocalRadPerSec.z
            });
        const double responseSeconds =
            std::max(
                dt,
                characteristicRate > 1.0e-9
                    ? characteristicRate /
                        angularAuthorityRadPerSec2
                    : dt
            );
        const double naturalFrequency =
            2.0 / responseSeconds;

        glm::dvec3 requestedAcceleration =
            desiredAngularAccelerationLocalRadPerSec2 +
            rotationErrorLocalRad *
                (naturalFrequency * naturalFrequency) +
            rateError *
                (2.0 * naturalFrequency);

        // Never request more than the shared physical angular envelope.
        const double requestedMagnitude =
            glm::length(requestedAcceleration);
        if (std::isfinite(requestedMagnitude) &&
            requestedMagnitude > angularAuthorityRadPerSec2)
        {
            requestedAcceleration *=
                angularAuthorityRadPerSec2 / requestedMagnitude;
        }

        return requestedAcceleration / angularAuthorityRadPerSec2;
    }
};

} // namespace game::navigation::autopilot
