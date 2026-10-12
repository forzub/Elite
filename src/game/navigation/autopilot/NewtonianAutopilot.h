#pragma once

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>

#include "src/game/navigation/autopilot/AutopilotControlApi.h"
#include "src/game/navigation/autopilot/HullAttitudeControl.h"
#include "src/game/navigation/autopilot/HullPoseGuidance.h"
#include "src/game/ship/core/ShipDynamics.h"

namespace game::navigation::autopilot
{

// Newtonian virtual pilot.
//
// Ordinary route translation doctrine:
//   * the main engine owns meaningful delta-v;
//   * bounded/rate-limited main-nozzle TVC covers small acceleration-angle
//     error before the hull is asked to rotate the remainder;
//   * main thrust stays off while the usable thrust axis points away from the demand;
//   * translational RCS is NOT used to substitute for an unaligned main engine;
//   * RCS translation is reserved for the explicit precision-stop residue.
//
// This module has the exact same public API as AssistedAutopilot.  No engine
// topology, slot ids or concrete actuator state cross this boundary.
class NewtonianAutopilot final
{
public:
    [[nodiscard]] AutopilotControlCommand update(
        const AutopilotControlRequest& request,
        const ShipParams& params
    ) noexcept
    {
        AutopilotControlCommand out;

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

        const double actualSpeed =
            finiteLength(request.actualVelocityMapMps);
        const double desiredSpeed =
            finiteLength(request.desiredVelocityMapMps);
        const double demandedAcceleration =
            finiteLength(request.desiredLinearAccelerationMapMps2);

        constexpr double PrecisionStopEntrySpeedMps = 0.50;
        const bool precisionStop =
            request.stopRequested &&
            actualSpeed <= PrecisionStopEntrySpeedMps;

        glm::dvec3 targetForward = forward;
        bool followingCourseInsteadOfThrust = false;
        GimbalDemand gimbal {};

        if (request.terminalAttitudeHold)
        {
            targetForward =
                normalizedOr(request.desiredForwardMap, forward);
        }
        else if (request.stopRequested && !precisionStop)
        {
            // Explicit BrakeToStop owns hull alignment downstream. Do not
            // fight it with a simultaneous route-attitude or TVC command.
            targetForward = forward;
        }
        else if (demandedAcceleration > 1.0e-9)
        {
            const glm::dvec3 accelerationDirection =
                request.desiredLinearAccelerationMapMps2 /
                demandedAcceleration;

            // Let the real nozzle cover the portion of the acceleration-angle
            // error that fits inside its physical gimbal envelope. The hull is
            // asked to rotate only the unserved remainder.
            gimbal = solveGimbalDemand(
                accelerationDirection,
                forward,
                right,
                up,
                params
            );
            out.mainGimbalRightInput = gimbal.rightInput;
            out.mainGimbalUpInput = gimbal.upInput;

            targetForward =
                hullAxisForResidualGimbal(
                    accelerationDirection,
                    forward,
                    gimbal.servedAngleRad
                );
        }
        else if (desiredSpeed > 1.0e-9)
        {
            // Coasting requires no thrust. Keep the hull ready on the authored
            // course without changing inertial velocity.
            targetForward =
                request.desiredVelocityMapMps / desiredSpeed;
            followingCourseInsteadOfThrust = true;
        }
        else if (actualSpeed <= 0.5)
        {
            // At (near) rest there is no velocity course and there may be no
            // translational acceleration demand yet.  The orchestration layer
            // may still require a launch/safety-course attitude before route
            // handoff.  In that zero-speed state desiredForwardMap is the only
            // physically meaningful course observable, so acquire it with
            // attitude controls while keeping main thrust off.
            targetForward =
                normalizedOr(request.desiredForwardMap, forward);
        }

        HullPoseGuidance::Request poseRequest;
        poseRequest.currentForward = forward;
        poseRequest.currentRight = right;
        poseRequest.currentUp = up;
        poseRequest.targetForward = targetForward;
        poseRequest.targetUp = request.desiredUpMap;

        const auto pose = HullPoseGuidance::evaluate(poseRequest);
        if (!pose.valid)
            return {};

        HullAttitudeControl::Request attitudeRequest;
        attitudeRequest.pitchYawErrorRad = {
            pose.pitchYawErrorLocalRad.x,
            pose.pitchYawErrorLocalRad.y
        };
        attitudeRequest.rollErrorRad = pose.rollErrorRad;
        attitudeRequest.pitchYawRateRadPerSec = {
            finiteOrZero(request.pitchRateRadPerSec),
            finiteOrZero(request.yawRateRadPerSec)
        };

        // Course angular feed-forward is meaningful while coasting on the
        // route tangent.  When the hull is being aimed at a thrust vector, the
        // acceleration vector itself is authoritative and no course-rate leak
        // is allowed to steer the hull as though it were Assisted.
        const glm::dvec3 desiredAngularRate =
            followingCourseInsteadOfThrust
                ? finiteVecOrZero(
                      request.desiredCourseAngularRateMapRadPerSec
                  )
                : glm::dvec3(0.0);
        attitudeRequest.desiredPitchYawRateRadPerSec = {
            glm::dot(desiredAngularRate, right),
            glm::dot(desiredAngularRate, up)
        };
        attitudeRequest.rollRateRadPerSec =
            finiteOrZero(request.rollRateRadPerSec);
        attitudeRequest.desiredRollRateRadPerSec =
            finiteOrZero(request.desiredRollRateRadPerSec);
        attitudeRequest.maxPitchYawRateRadPerSec = {
            std::max(0.0, static_cast<double>(params.maxPitchRate)),
            std::max(0.0, static_cast<double>(params.maxYawRate))
        };
        attitudeRequest.maxRollRateRadPerSec =
            std::max(0.0, static_cast<double>(params.maxRollRate));

        const double angularAuthority =
            std::max(
                1.0e-9,
                game::ship::angularAccelerationLimitRadPerSec2(params)
            );
        attitudeRequest.pitchYawAngularAccelerationAuthorityRadPerSec2 =
            angularAuthority;
        attitudeRequest.rollAngularAccelerationAuthorityRadPerSec2 =
            angularAuthority;
        attitudeRequest.deltaSeconds = dt;

        const auto attitude =
            HullAttitudeControl::evaluate(attitudeRequest);
        if (!attitude.valid)
            return {};

        out.pitchInput =
            finiteClamp(attitude.pitchYawInput.x);
        out.yawInput =
            finiteClamp(attitude.pitchYawInput.y);
        out.rollInput =
            finiteClamp(attitude.rollInput);

        if (request.stopRequested && !precisionStop)
        {
            // END/autobrake is the ordinary Newtonian turn-and-burn contract.
            out.velocityAlignmentCommand =
                VelocityAlignmentMode::BrakeToStop;
        }
        else if (precisionStop)
        {
            // Only the final low-speed residue is delegated to translational
            // RCS.  This is the one place where RCS is allowed to own delta-v.
            const double rcsAuthority =
                game::ship::manoeuvreAccelerationLimitMps2(params);
            if (rcsAuthority > 1.0e-12)
            {
                glm::dvec3 wantedAcceleration =
                    (request.desiredVelocityMapMps -
                     request.actualVelocityMapMps) / dt;

                const double wantedMagnitude =
                    finiteLength(wantedAcceleration);
                if (wantedMagnitude > rcsAuthority)
                    wantedAcceleration *=
                        rcsAuthority / wantedMagnitude;

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
        }
        else if (demandedAcceleration > 1.0e-9)
        {
            const double mainAuthority =
                game::ship::forwardMainAccelerationLimitMps2(params);
            if (mainAuthority > 1.0e-9)
            {
                // No angular threshold/hack: thrust rises continuously from
                // zero as the real main axis becomes aligned with the demanded
                // acceleration vector.  Negative projection means the engine
                // would increase the error, so it remains off.
                const double alignedDemand =
                    std::max(
                        0.0,
                        glm::dot(
                            request.desiredLinearAccelerationMapMps2,
                            forward
                        )
                    );
                out.targetSpeedRate =
                    finiteClamp(
                        std::min(
                            1.0,
                            alignedDemand / mainAuthority
                        )
                    );
            }

            // Deliberately zero. Ordinary Newtonian route following may not
            // make RCS fight a locomotive-scale velocity error.
            out.forwardInput = 0.0f;
            out.strafeInput = 0.0f;
            out.liftInput = 0.0f;
        }

        out.navigationAccelerationDemandValid = false;
        out.navigationVelocityTargetValid = false;
        out.navigationPrecisionTranslationOnly = false;

        state_.initialized = true;
        state_.previousVelocityMapMps =
            finiteVecOrZero(request.actualVelocityMapMps);

        return out;
    }

    [[nodiscard]] AutopilotControlTelemetry telemetry(
        const ShipParams& params
    ) const noexcept
    {
        AutopilotControlTelemetry out;
        out.initialized = state_.initialized;
        out.previousVelocityMapMps = state_.previousVelocityMapMps;

        const double angularAuthority =
            game::ship::angularAccelerationLimitRadPerSec2(params);
        const double angularRate =
            game::ship::maximumAngularSpeedRadPerSec(params);
        out.courseResponseSeconds =
            angularAuthority > 1.0e-9 && angularRate > 1.0e-9
                ? angularRate / angularAuthority
                : 2.0;

        out.targetSpeedChangeRateMps2 =
            game::ship::forwardMainAccelerationLimitMps2(params);
        out.brakingResponseMps2 =
            game::ship::forwardMainAccelerationLimitMps2(params);
        out.commandResponsePerSecond =
            std::max(
                1.0e-6,
                static_cast<double>(params.throttleAccel) > 0.0
                    ? static_cast<double>(params.throttleAccel)
                    : static_cast<double>(
                        params.fallbackThrottleResponsePerSecond
                      )
            );
        return out;
    }

    void reset() noexcept
    {
        state_ = {};
    }

private:
    struct GimbalDemand
    {
        float rightInput = 0.0f;
        float upInput = 0.0f;
        double servedAngleRad = 0.0;
    };

    [[nodiscard]] static GimbalDemand solveGimbalDemand(
        const glm::dvec3& desiredDirection,
        const glm::dvec3& forward,
        const glm::dvec3& right,
        const glm::dvec3& up,
        const ShipParams& params
    ) noexcept
    {
        GimbalDemand out;

        const double yawLimit =
            std::max(
                0.0,
                static_cast<double>(params.mainGimbalYawLimitRad)
            );
        const double pitchLimit =
            std::max(
                0.0,
                static_cast<double>(params.mainGimbalPitchLimitRad)
            );

        if (yawLimit <= 1.0e-12 && pitchLimit <= 1.0e-12)
            return out;

        const glm::dvec3 desired =
            normalizedOr(desiredDirection, forward);

        const double localForward =
            glm::dot(desired, forward);
        const double localRight =
            glm::dot(desired, right);
        const double localUp =
            glm::dot(desired, up);

        const double desiredYaw =
            std::atan2(localRight, localForward);
        const double desiredPitch =
            std::atan2(
                localUp,
                std::sqrt(
                    localForward * localForward +
                    localRight * localRight
                )
            );

        double right01 =
            yawLimit > 1.0e-12
                ? desiredYaw / yawLimit
                : 0.0;
        double up01 =
            pitchLimit > 1.0e-12
                ? desiredPitch / pitchLimit
                : 0.0;

        // Same elliptical envelope as the physical actuator. Equal pitch/yaw
        // limits therefore form a true cone and diagonal commands cannot gain
        // extra resultant travel.
        const double normalizedLength =
            std::sqrt(right01 * right01 + up01 * up01);
        if (normalizedLength > 1.0)
        {
            right01 /= normalizedLength;
            up01 /= normalizedLength;
        }

        right01 = std::clamp(right01, -1.0, 1.0);
        up01 = std::clamp(up01, -1.0, 1.0);

        out.rightInput = finiteClamp(right01);
        out.upInput = finiteClamp(up01);

        const double commandedYaw = right01 * yawLimit;
        const double commandedPitch = up01 * pitchLimit;
        const glm::dvec3 commandedDirection =
            normalizedOr(
                forward +
                    right * std::tan(commandedYaw) +
                    up * std::tan(commandedPitch),
                forward
            );

        out.servedAngleRad =
            std::acos(
                std::clamp(
                    glm::dot(forward, commandedDirection),
                    -1.0,
                    1.0
                )
            );
        return out;
    }

    [[nodiscard]] static glm::dvec3 hullAxisForResidualGimbal(
        const glm::dvec3& desiredDirection,
        const glm::dvec3& currentForward,
        double servedAngleRad
    ) noexcept
    {
        const glm::dvec3 desired =
            normalizedOr(desiredDirection, currentForward);
        const glm::dvec3 current =
            normalizedOr(currentForward, {0.0, 0.0, -1.0});

        const double cosine =
            std::clamp(glm::dot(desired, current), -1.0, 1.0);
        const double separation = std::acos(cosine);

        if (separation <= 1.0e-9)
            return current;

        if (servedAngleRad >= separation - 1.0e-9)
            return current;

        // Near an exact 180-degree flip there is no unique great-circle side.
        // Let HullPoseGuidance choose its established deterministic flip rather
        // than inventing a TVC-side preference.
        if (cosine <= -0.999999)
            return desired;

        const double moveFromDesired =
            std::clamp(servedAngleRad, 0.0, separation);
        const glm::dvec3 tangent =
            normalizedOr(
                current - desired * cosine,
                current
            );

        return normalizedOr(
            desired * std::cos(moveFromDesired) +
                tangent * std::sin(moveFromDesired),
            desired
        );
    }

    struct State
    {
        bool initialized = false;
        glm::dvec3 previousVelocityMapMps {0.0};
    };

    [[nodiscard]] static double finiteOrZero(double value) noexcept
    {
        return std::isfinite(value) ? value : 0.0;
    }

    [[nodiscard]] static double finiteLength(
        const glm::dvec3& value
    ) noexcept
    {
        const double magnitude = glm::length(value);
        return std::isfinite(magnitude) ? magnitude : 0.0;
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
        const double magnitude = glm::length(value);
        if (std::isfinite(magnitude) && magnitude > 1.0e-12)
            return value / magnitude;

        const double fallbackMagnitude = glm::length(fallback);
        return fallbackMagnitude > 1.0e-12
            ? fallback / fallbackMagnitude
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

    State state_ {};
};

} // namespace game::navigation::autopilot
