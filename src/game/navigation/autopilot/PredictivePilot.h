#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "src/game/navigation/LocalFlightControlLaw.h"
#include "src/game/navigation/autopilot/HullPoseGuidance.h"
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
        double assistedAccelerationResponseMps2 = 0.0;
        double assistedBrakingResponseMps2 = 0.0;

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
        bool terminalAttitudeHold = false;
        double angularTrackingResponseSeconds = 0.0;
        double deltaSeconds = 0.0;
    };

    [[nodiscard]] static double effectiveAssistedTargetSpeedChangeRateMps2(
        const ShipParams& params,
        const State& state
    ) noexcept
    {
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

        return std::clamp(
            learnedTargetRate,
            std::max(1.0, configuredTargetRate * 0.25),
            std::max(1.0, configuredTargetRate * 2.0)
        );
    }

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

        HullPoseGuidance::Request poseRequest;
        poseRequest.currentForward = forward;
        poseRequest.currentRight = right;
        poseRequest.currentUp = up;
        poseRequest.targetForward = request.desiredForwardMap;
        poseRequest.targetUp = request.desiredUpMap;

        const auto pose = HullPoseGuidance::evaluate(poseRequest);
        if (!pose.valid)
            return {};

        const glm::dvec3 desiredForward = pose.targetForward;
        const glm::dvec3 desiredUp = pose.targetUpForRoll;

        const double configuredAngularAuthority =
            game::ship::angularAccelerationLimitRadPerSec2(params);

        // Pitch/yaw and roll are intentionally independent control channels.
        // HullPoseGuidance guarantees forward error never leaks into roll and
        // roll alignment never changes the nose-steering error.
        const glm::dvec3 localRotationError(
            pose.pitchYawErrorLocalRad.x,
            pose.pitchYawErrorLocalRad.y,
            pose.rollErrorRad
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
            request.terminalAttitudeHold
                ? chooseTerminalHoldRotationInput(
                    localRotationError,
                    currentLocalRate,
                    maxLocalRate,
                    configuredAngularAuthority,
                    dt
                  )
                : choosePredictiveRotationInput(
                    localRotationError,
                    currentLocalRate,
                    desiredLocalRate,
                    desiredLocalAcceleration,
                    maxLocalRate,
                    configuredAngularAuthority,
                    request.angularTrackingResponseSeconds,
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

            const double effectiveTargetRate =
                effectiveAssistedTargetSpeedChangeRateMps2(
                    params,
                    state
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

                    double& directionalEstimate =
                        state.previousTargetSpeedRate < 0.0f
                            ? state.assistedBrakingResponseMps2
                            : state.assistedAccelerationResponseMps2;
                    if (!(directionalEstimate > 1.0e-6))
                        directionalEstimate = measuredResponse;
                    else
                        directionalEstimate =
                            directionalEstimate * 0.90 +
                            measuredResponse * 0.10;
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

    [[nodiscard]] static glm::dvec3 chooseTerminalHoldRotationInput(
        const glm::dvec3& rotationErrorLocalRad,
        const glm::dvec3& angularRateLocalRadPerSec,
        const glm::dvec3& maxRateLocalRadPerSec,
        double angularAuthorityRadPerSec2,
        double dt
    ) noexcept
    {
        const double angle = glm::length(rotationErrorLocalRad);
        if (!std::isfinite(angle) ||
            !(dt > 0.0) ||
            angularAuthorityRadPerSec2 <= 1.0e-9)
        {
            return glm::dvec3(0.0);
        }

        constexpr double AngleDeadbandRad = 0.0015;
        constexpr double RateDeadbandRadPerSec = 0.004;
        const double rateMagnitude =
            glm::length(angularRateLocalRadPerSec);

        if (angle <= AngleDeadbandRad &&
            rateMagnitude <= RateDeadbandRadPerSec)
        {
            return glm::dvec3(0.0);
        }

        glm::dvec3 targetRate(0.0);
        if (angle > AngleDeadbandRad)
        {
            const glm::dvec3 direction =
                rotationErrorLocalRad / angle;
            const double directionalLimit =
                directionalRateLimit(direction, maxRateLocalRadPerSec);
            const double stoppingEnvelopeRate =
                std::sqrt(
                    std::max(
                        0.0,
                        2.0 * angularAuthorityRadPerSec2 * angle
                    )
                );
            const double targetMagnitude =
                std::min(
                    directionalLimit,
                    stoppingEnvelopeRate
                );
            targetRate = direction * targetMagnitude;
        }

        const glm::dvec3 rateError =
            targetRate - angularRateLocalRadPerSec;
        const double rateErrorMagnitude = glm::length(rateError);
        if (!std::isfinite(rateErrorMagnitude) ||
            rateErrorMagnitude <= 1.0e-12)
        {
            return glm::dvec3(0.0);
        }

        // Use exactly the time full angular authority would need to close the
        // current rate error. At rest with a large attitude error this yields
        // full acceleration; as the stopping envelope collapses near target,
        // the sign reverses early enough to kill angular rate before capture.
        const double responseSeconds =
            std::max(
                dt,
                rateErrorMagnitude /
                    angularAuthorityRadPerSec2
            );
        glm::dvec3 requestedAcceleration =
            rateError / responseSeconds;

        const double requestedMagnitude =
            glm::length(requestedAcceleration);
        if (requestedMagnitude > angularAuthorityRadPerSec2)
        {
            requestedAcceleration *=
                angularAuthorityRadPerSec2 /
                requestedMagnitude;
        }

        return requestedAcceleration /
            angularAuthorityRadPerSec2;
    }

    [[nodiscard]] static glm::dvec3 choosePredictiveRotationInput(
        const glm::dvec3& rotationErrorLocalRad,
        const glm::dvec3& angularRateLocalRadPerSec,
        const glm::dvec3& desiredAngularRateLocalRadPerSec,
        const glm::dvec3& desiredAngularAccelerationLocalRadPerSec2,
        const glm::dvec3& maxRateLocalRadPerSec,
        double angularAuthorityRadPerSec2,
        double trackingResponseSeconds,
        double dt
    ) noexcept
    {
        // Restore the proven hull-attitude controller.  Course-response tau is
        // the delay between hull heading and Assisted velocity direction; it
        // must never be used as the body rotation servo time constant.
        (void)desiredAngularRateLocalRadPerSec;
        (void)desiredAngularAccelerationLocalRadPerSec2;
        (void)trackingResponseSeconds;

        constexpr double AngleDeadbandRad = 0.0015;
        constexpr double RateDeadbandRadPerSec = 0.004;

        const double remainingAngle = glm::length(rotationErrorLocalRad);
        const double rateMagnitude = glm::length(angularRateLocalRadPerSec);

        if (!std::isfinite(remainingAngle) ||
            !std::isfinite(rateMagnitude) ||
            !(dt > 0.0) ||
            angularAuthorityRadPerSec2 <= 1.0e-9)
        {
            return glm::dvec3(0.0);
        }

        if (remainingAngle <= AngleDeadbandRad &&
            rateMagnitude <= RateDeadbandRadPerSec)
        {
            return glm::dvec3(0.0);
        }

        if (remainingAngle <= 1.0e-12)
        {
            const glm::dvec3 requested =
                -angularRateLocalRadPerSec /
                (angularAuthorityRadPerSec2 * dt);
            const double magnitude = glm::length(requested);
            return magnitude > 1.0
                ? requested / magnitude
                : requested;
        }

        const glm::dvec3 direction =
            rotationErrorLocalRad / remainingAngle;
        const double rateTowardTarget =
            glm::dot(angularRateLocalRadPerSec, direction);

        // Fixed-step physical braking envelope.  Account for the angular
        // travel that occurs during this simulation step before new torque can
        // change the rate.  This is the controller that previously passed the
        // 5/15/45/90 degree closed-loop no-overshoot tests.
        const double reactionTravel =
            std::max(0.0, rateTowardTarget) * dt;
        const double brakingAngleBudget =
            std::max(0.0, remainingAngle - reactionTravel);
        const double brakingLimitedRate =
            std::sqrt(
                2.0 * angularAuthorityRadPerSec2 *
                brakingAngleBudget
            );

        const double rateLimit =
            directionalRateLimit(direction, maxRateLocalRadPerSec);
        const double targetRateMagnitude =
            std::min(rateLimit, brakingLimitedRate);
        const glm::dvec3 targetRate =
            direction * targetRateMagnitude;

        glm::dvec3 requestedInput =
            (targetRate - angularRateLocalRadPerSec) /
            (angularAuthorityRadPerSec2 * dt);

        const double inputMagnitude = glm::length(requestedInput);
        if (inputMagnitude > 1.0)
            requestedInput /= inputMagnitude;

        return requestedInput;
    }
};

} // namespace game::navigation::autopilot
