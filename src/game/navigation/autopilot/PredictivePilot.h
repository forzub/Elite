#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "src/game/navigation/LocalFlightControlLaw.h"
#include "src/game/navigation/autopilot/HullAttitudeControl.h"
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

        // Angular velocity of the desired NAVIGATION COURSE (velocity
        // direction), in map coordinates. This is feed-forward for the hull
        // actuator; route ownership remains with desiredVelocityMapMps.
        glm::dvec3 desiredCourseAngularRateMapRadPerSec {0.0};

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
        bool terminalAttitudeHold = false;
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

        // Navigation course is owned by desired velocity. The hull is an
        // actuator: pitch/yaw must still rotate the real body toward that
        // desired course, otherwise Assisted physics would drag an initially
        // correct velocity vector toward a misaligned nose.
        const double desiredSpeed =
            finiteLength(request.desiredVelocityMapMps);
        const glm::dvec3 desiredCourse =
            desiredSpeed > 1.0e-9
                ? request.desiredVelocityMapMps / desiredSpeed
                : normalizedOr(
                    request.desiredForwardMap,
                    forward
                  );

        const glm::dvec3 actuatorForward =
            request.terminalAttitudeHold
                ? normalizedOr(
                    request.desiredForwardMap,
                    desiredCourse
                  )
                : desiredCourse;

        HullPoseGuidance::Request pitchYawPoseRequest;
        pitchYawPoseRequest.currentForward = forward;
        pitchYawPoseRequest.currentRight = right;
        pitchYawPoseRequest.currentUp = up;
        pitchYawPoseRequest.targetForward = actuatorForward;
        pitchYawPoseRequest.targetUp = up;

        const auto pitchYawPose =
            HullPoseGuidance::evaluate(pitchYawPoseRequest);
        if (!pitchYawPose.valid)
            return {};

        // Roll remains an independent hull channel.
        HullPoseGuidance::Request rollRequest;
        rollRequest.currentForward = forward;
        rollRequest.currentRight = right;
        rollRequest.currentUp = up;
        rollRequest.targetForward = forward;
        rollRequest.targetUp = request.desiredUpMap;

        const auto rollPose =
            HullPoseGuidance::evaluate(rollRequest);
        if (!rollPose.valid)
            return {};

        const glm::dvec2 pitchYawError = {
            pitchYawPose.pitchYawErrorLocalRad.x,
            pitchYawPose.pitchYawErrorLocalRad.y
        };

        const double configuredAngularAuthority =
            game::ship::angularAccelerationLimitRadPerSec2(params);

        HullAttitudeControl::Request attitudeRequest;
        attitudeRequest.pitchYawErrorRad = pitchYawError;
        attitudeRequest.rollErrorRad = rollPose.rollErrorRad;
        attitudeRequest.pitchYawRateRadPerSec = {
            finiteOrZero(request.pitchRateRadPerSec),
            finiteOrZero(request.yawRateRadPerSec)
        };

        const glm::dvec3 desiredCourseAngularRateMap =
            finiteVecOrZero(
                request.desiredCourseAngularRateMapRadPerSec
            );
        attitudeRequest.desiredPitchYawRateRadPerSec = {
            glm::dot(desiredCourseAngularRateMap, right),
            glm::dot(desiredCourseAngularRateMap, up)
        };

        attitudeRequest.rollRateRadPerSec =
            finiteOrZero(request.rollRateRadPerSec);
        attitudeRequest.maxPitchYawRateRadPerSec = {
            std::max(0.0, static_cast<double>(params.maxPitchRate)),
            std::max(0.0, static_cast<double>(params.maxYawRate))
        };
        attitudeRequest.maxRollRateRadPerSec =
            std::max(0.0, static_cast<double>(params.maxRollRate));
        attitudeRequest.angularAccelerationAuthorityRadPerSec2 =
            configuredAngularAuthority;
        attitudeRequest.deltaSeconds = dt;

        const auto attitude =
            HullAttitudeControl::evaluate(attitudeRequest);
        if (!attitude.valid)
            return {};

        out.pitchInput =
            static_cast<float>(attitude.pitchYawInput.x);
        out.yawInput =
            static_cast<float>(attitude.pitchYawInput.y);
        out.rollInput =
            static_cast<float>(attitude.rollInput);

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
            // Below the END/autobrake envelope the velocity direction is no
            // longer a useful navigation observable. Remove the remaining
            // authored STOP residual with the same ordinary keypad RCS
            // controls available to the player; do not wake a main engine and
            // do not use the legacy direct-navigation actuator seam.
            const double rcsAuthority =
                game::ship::manoeuvreAccelerationLimitMps2(params);
            if (rcsAuthority > 1.0e-12)
            {
                glm::dvec3 wantedAcceleration =
                    (request.desiredVelocityMapMps -
                     request.actualVelocityMapMps) / dt;

                const double wantedMagnitude =
                    glm::length(wantedAcceleration);
                if (std::isfinite(wantedMagnitude) &&
                    wantedMagnitude > rcsAuthority)
                {
                    wantedAcceleration *=
                        rcsAuthority / wantedMagnitude;
                }

                out.forwardInput = finiteClamp(
                    glm::dot(wantedAcceleration, forward) /
                    rcsAuthority
                );
                out.strafeInput = finiteClamp(
                    glm::dot(wantedAcceleration, right) /
                    rcsAuthority
                );
                out.liftInput = finiteClamp(
                    glm::dot(wantedAcceleration, up) /
                    rcsAuthority
                );
            }
            out.targetSpeedRate = 0.0f;
        }
        else if (request.law == LocalFlightControlLaw::Assisted)
        {
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
                    desiredCourse
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

    [[nodiscard]] static glm::dvec3 finiteVecOrZero(
        const glm::dvec3& value
    ) noexcept
    {
        return
            std::isfinite(value.x) &&
            std::isfinite(value.y) &&
            std::isfinite(value.z)
                ? value
                : glm::dvec3(0.0);
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


};

} // namespace game::navigation::autopilot
