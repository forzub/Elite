#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

#include <glm/glm.hpp>

#include "src/game/navigation/autopilot/AutopilotControlApi.h"
#include "src/game/navigation/autopilot/HullAttitudeControl.h"
#include "src/game/navigation/autopilot/HullPoseGuidance.h"
#include "src/game/ship/core/ShipDynamics.h"

namespace game::navigation::autopilot
{

// Receding-horizon Newtonian controller.
//
// Navigation owns desired velocity/reference acceleration. This controller
// searches a short physically-constrained future and chooses the first
// thrust-vector action that minimizes velocity-reference error, overspeed,
// unnecessary thrust and unnecessary reorientation.
//
// The hull is NOT commanded to follow the route tangent. Hull + TVC exist only
// to realize the selected main-thrust vector. Translational RCS is reserved for
// the final precision-stop residue.
class NewtonianOptimalController final
{
public:
    [[nodiscard]] AutopilotControlCommand update(
        const AutopilotControlRequest& request,
        const ShipParams& params
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

        const double actualSpeed =
            finiteLength(request.actualVelocityMapMps);

        constexpr double PrecisionStopEntrySpeedMps = 0.50;
        const bool precisionStop =
            request.stopRequested &&
            actualSpeed <= PrecisionStopEntrySpeedMps;

        if (precisionStop)
        {
            auto out = precisionStopControl(
                request,
                params,
                forward,
                right,
                up,
                dt
            );
            remember(request);
            return out;
        }

        state_.secondsUntilOptimize -= dt;
        bool forceOptimize =
            !state_.initialized ||
            request.stopRequested != state_.lastStopRequested ||
            request.terminalAttitudeHold;

        // An orient-only optimum is deliberately allowed to start a flip
        // before braking is due. As soon as that orientation has been reached,
        // re-solve immediately instead of idling until the 100 ms MPC period
        // expires.
        if (state_.action.valid &&
            state_.action.throttle01 <= 1.0e-6 &&
            finiteLength(request.desiredLinearAccelerationMapMps2) >
                1.0e-6)
        {
            const double alignment =
                glm::dot(
                    forward,
                    normalizedOr(
                        state_.action.targetThrustDirectionMap,
                        forward
                    )
                );
            if (alignment > 0.995)
                forceOptimize = true;
        }

        if (forceOptimize || state_.secondsUntilOptimize <= 0.0)
        {
            state_.action = optimize(request, params);
            state_.secondsUntilOptimize = OptimizationPeriodSeconds;
        }

        // At true rest with no translational objective, orchestration may still
        // require a launch/safety attitude before route handoff.
        if (actualSpeed <= 0.5 &&
            finiteLength(request.desiredVelocityMapMps) <= 0.5 &&
            finiteLength(request.desiredLinearAccelerationMapMps2) <= 1.0e-6 &&
            request.previewCount == 0 &&
            !request.stopRequested)
        {
            state_.action.targetThrustDirectionMap =
                normalizedOr(request.desiredForwardMap, forward);
            state_.action.throttle01 = 0.0;
            state_.action.valid = true;
        }

        auto out = realizeAction(
            request,
            params,
            state_.action,
            forward,
            right,
            up,
            dt
        );

        remember(request);
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

        const double main =
            game::ship::forwardMainAccelerationLimitMps2(params);
        out.targetSpeedChangeRateMps2 = main;
        out.brakingResponseMps2 = main;
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
    static constexpr double OptimizationPeriodSeconds = 0.10;
    static constexpr std::size_t BeamWidth = 24;
    static constexpr std::size_t CandidateCount = 6;
    static constexpr std::size_t MaximumDepth =
        AutopilotPreviewCapacity;

    struct Action
    {
        glm::dvec3 targetThrustDirectionMap {0.0, 0.0, -1.0};
        double throttle01 = 0.0;
        bool valid = false;
    };

    struct SearchNode
    {
        glm::dvec3 velocityMapMps {0.0};
        glm::dvec3 thrustDirectionMap {0.0, 0.0, -1.0};
        double hullSlewRateRadPerSec = 0.0;
        double cost = 0.0;
        Action firstAction {};
    };

    struct GimbalDemand
    {
        float rightInput = 0.0f;
        float upInput = 0.0f;
        double servedAngleRad = 0.0;
        glm::dvec3 commandedDirectionMap {0.0, 0.0, -1.0};
    };

    struct State
    {
        bool initialized = false;
        bool lastStopRequested = false;
        glm::dvec3 previousVelocityMapMps {0.0};
        double secondsUntilOptimize = 0.0;
        Action action {};
    };

    [[nodiscard]] static AutopilotReferencePreviewSample currentReference(
        const AutopilotControlRequest& request
    ) noexcept
    {
        AutopilotReferencePreviewSample out;
        out.timeOffsetSeconds = 0.0;
        out.desiredVelocityMapMps =
            finiteVecOrZero(request.desiredVelocityMapMps);
        out.desiredLinearAccelerationMapMps2 =
            finiteVecOrZero(
                request.desiredLinearAccelerationMapMps2
            );
        return out;
    }

    [[nodiscard]] static AutopilotReferencePreviewSample referenceAt(
        const AutopilotControlRequest& request,
        std::size_t depth
    ) noexcept
    {
        const std::size_t count =
            std::min(
                request.previewCount,
                AutopilotPreviewCapacity
            );
        if (count == 0)
            return currentReference(request);

        const std::size_t index =
            std::min(depth, count - 1);
        auto out = request.preview[index];
        out.desiredVelocityMapMps =
            finiteVecOrZero(out.desiredVelocityMapMps);
        out.desiredLinearAccelerationMapMps2 =
            finiteVecOrZero(
                out.desiredLinearAccelerationMapMps2
            );
        return out;
    }

    [[nodiscard]] static AutopilotReferencePreviewSample finalReference(
        const AutopilotControlRequest& request
    ) noexcept
    {
        const std::size_t count =
            std::min(
                request.previewCount,
                AutopilotPreviewCapacity
            );
        return
            count > 0
                ? referenceAt(request, count - 1)
                : currentReference(request);
    }

    [[nodiscard]] static double stepSeconds(
        const AutopilotControlRequest& request,
        std::size_t depth
    ) noexcept
    {
        const std::size_t count =
            std::min(
                request.previewCount,
                AutopilotPreviewCapacity
            );
        if (count == 0)
            return 0.50;

        const double now =
            depth == 0
                ? 0.0
                : request.preview[
                    std::min(depth - 1, count - 1)
                  ].timeOffsetSeconds;
        const double next =
            request.preview[
                std::min(depth, count - 1)
            ].timeOffsetSeconds;

        return std::clamp(next - now, 0.10, 1.25);
    }

    [[nodiscard]] static Action optimize(
        const AutopilotControlRequest& request,
        const ShipParams& params
    ) noexcept
    {
        const double mainAuthority =
            game::ship::forwardMainAccelerationLimitMps2(params);
        if (!(mainAuthority > 1.0e-9))
            return {};

        const glm::dvec3 initialThrustDirection =
            normalizedOr(request.forwardMap, {0.0, 0.0, -1.0});
        const double initialHullRate =
            std::sqrt(
                finiteOrZero(request.pitchRateRadPerSec) *
                    finiteOrZero(request.pitchRateRadPerSec) +
                finiteOrZero(request.yawRateRadPerSec) *
                    finiteOrZero(request.yawRateRadPerSec)
            );

        std::array<SearchNode, BeamWidth> beam {};
        std::size_t beamCount = 1;
        beam[0].velocityMapMps =
            finiteVecOrZero(request.actualVelocityMapMps);
        beam[0].thrustDirectionMap = initialThrustDirection;
        beam[0].hullSlewRateRadPerSec = initialHullRate;

        const std::size_t previewCount =
            std::min(
                request.previewCount,
                AutopilotPreviewCapacity
            );
        const std::size_t depthCount =
            previewCount > 0
                ? std::min(previewCount, MaximumDepth)
                : std::size_t(4);

        for (std::size_t depth = 0; depth < depthCount; ++depth)
        {
            const auto reference = referenceAt(request, depth);
            const auto future = finalReference(request);
            const double step = stepSeconds(request, depth);

            std::array<
                SearchNode,
                BeamWidth * CandidateCount
            > expanded {};
            std::size_t expandedCount = 0;

            for (std::size_t nodeIndex = 0;
                 nodeIndex < beamCount;
                 ++nodeIndex)
            {
                const auto& node = beam[nodeIndex];
                const auto candidates = candidateActions(
                    node,
                    reference,
                    future,
                    request,
                    params,
                    mainAuthority
                );

                for (const auto& action : candidates)
                {
                    SearchNode next =
                        simulate(
                            node,
                            action,
                            reference,
                            request,
                            params,
                            mainAuthority,
                            step
                        );

                    if (depth == 0)
                        next.firstAction = action;
                    else
                        next.firstAction = node.firstAction;

                    if (expandedCount < expanded.size())
                        expanded[expandedCount++] = next;
                }
            }

            std::sort(
                expanded.begin(),
                expanded.begin() +
                    static_cast<std::ptrdiff_t>(expandedCount),
                [](const SearchNode& a, const SearchNode& b)
                {
                    return a.cost < b.cost;
                }
            );

            beamCount =
                std::min(BeamWidth, expandedCount);
            for (std::size_t i = 0; i < beamCount; ++i)
                beam[i] = expanded[i];
        }

        if (beamCount == 0 || !beam[0].firstAction.valid)
            return {};

        return beam[0].firstAction;
    }

    [[nodiscard]] static std::array<Action, CandidateCount>
    candidateActions(
        const SearchNode& node,
        const AutopilotReferencePreviewSample& reference,
        const AutopilotReferencePreviewSample& future,
        const AutopilotControlRequest& request,
        const ShipParams&,
        double mainAuthority
    ) noexcept
    {
        std::array<Action, CandidateCount> actions {};

        const glm::dvec3 velocityError =
            reference.desiredVelocityMapMps -
            node.velocityMapMps;
        const glm::dvec3 trackingAcceleration =
            reference.desiredLinearAccelerationMapMps2 +
            velocityError / 1.25;
        const double trackingMagnitude =
            finiteLength(trackingAcceleration);
        const glm::dvec3 trackingDirection =
            normalizedOr(
                trackingAcceleration,
                node.thrustDirectionMap
            );
        const double trackingThrottle =
            std::clamp(
                trackingMagnitude /
                    std::max(mainAuthority, 1.0e-9),
                0.0,
                1.0
            );

        glm::dvec3 futureDelta =
            future.desiredVelocityMapMps -
            node.velocityMapMps;
        if (request.stopRequested)
            futureDelta = -node.velocityMapMps;
        const glm::dvec3 futureDirection =
            normalizedOr(
                futureDelta,
                trackingDirection
            );

        // 0: hold attitude and coast.
        actions[0] = {
            node.thrustDirectionMap,
            0.0,
            true
        };

        // 1: pre-orient toward the current optimal correction with main off.
        actions[1] = {
            trackingDirection,
            0.0,
            true
        };

        // 2: physically-sized tracking burn.
        actions[2] = {
            trackingDirection,
            trackingThrottle,
            true
        };

        // 3: full-authority tracking burn.
        actions[3] = {
            trackingDirection,
            1.0,
            true
        };

        // 4: pre-orient for a future slow-down/turn before it becomes current.
        actions[4] = {
            futureDirection,
            0.0,
            true
        };

        // 5: full burn along the future delta-v direction.
        actions[5] = {
            futureDirection,
            1.0,
            true
        };

        return actions;
    }

    [[nodiscard]] static SearchNode simulate(
        const SearchNode& node,
        const Action& action,
        const AutopilotReferencePreviewSample& reference,
        const AutopilotControlRequest& request,
        const ShipParams& params,
        double mainAuthority,
        double dt
    ) noexcept
    {
        SearchNode next = node;

        const glm::dvec3 targetDirection =
            normalizedOr(
                action.targetThrustDirectionMap,
                node.thrustDirectionMap
            );

        const double hullRateLimit =
            std::max(
                0.0,
                game::ship::maximumAngularSpeedRadPerSec(params)
            );
        const double angularAuthority =
            std::max(
                0.0,
                game::ship::angularAccelerationLimitRadPerSec2(params)
            );
        const double gimbalRate =
            game::ship::mainGimbalRateRadPerSec(params);

        const double separation =
            angleBetween(
                node.thrustDirectionMap,
                targetDirection
            );

        const double desiredHullRate =
            separation > 1.0e-9
                ? hullRateLimit
                : 0.0;
        const double maximumRateDelta =
            angularAuthority * dt;
        double hullRate =
            node.hullSlewRateRadPerSec;
        if (hullRate < desiredHullRate)
            hullRate =
                std::min(
                    desiredHullRate,
                    hullRate + maximumRateDelta
                );
        else
            hullRate =
                std::max(
                    desiredHullRate,
                    hullRate - maximumRateDelta
                );

        const double reachableAngle =
            std::max(
                0.0,
                (0.5 *
                    (node.hullSlewRateRadPerSec + hullRate) +
                 gimbalRate) * dt
            );

        next.thrustDirectionMap =
            slewDirection(
                node.thrustDirectionMap,
                targetDirection,
                reachableAngle
            );
        next.hullSlewRateRadPerSec = hullRate;

        const double throttle =
            std::clamp(action.throttle01, 0.0, 1.0);
        const glm::dvec3 acceleration =
            next.thrustDirectionMap *
            (mainAuthority * throttle);
        next.velocityMapMps =
            node.velocityMapMps + acceleration * dt;

        const glm::dvec3 velocityError =
            next.velocityMapMps -
            reference.desiredVelocityMapMps;
        const double velocityError2 =
            glm::dot(velocityError, velocityError);

        const double actualSpeed =
            finiteLength(next.velocityMapMps);
        const double desiredSpeed =
            finiteLength(reference.desiredVelocityMapMps);
        const double overspeed =
            std::max(0.0, actualSpeed - desiredSpeed);

        const glm::dvec3 accelerationError =
            acceleration -
            reference.desiredLinearAccelerationMapMps2;
        const double accelerationError2 =
            glm::dot(accelerationError, accelerationError);

        const double reorientation =
            angleBetween(
                node.thrustDirectionMap,
                next.thrustDirectionMap
            );

        double stageCost =
            1.0 * velocityError2 +
            4.0 * overspeed * overspeed +
            0.08 * accelerationError2 +
            2.0 * throttle * throttle +
            150.0 * reorientation * reorientation +
            2.0 * hullRate * hullRate;

        if (request.stopRequested)
            stageCost += 6.0 * actualSpeed * actualSpeed;

        next.cost += stageCost * dt;
        return next;
    }

    [[nodiscard]] static AutopilotControlCommand realizeAction(
        const AutopilotControlRequest& request,
        const ShipParams& params,
        const Action& action,
        const glm::dvec3& forward,
        const glm::dvec3& right,
        const glm::dvec3& up,
        double dt
    ) noexcept
    {
        AutopilotControlCommand out;

        if (request.terminalAttitudeHold)
        {
            applyAttitude(
                out,
                request,
                params,
                normalizedOr(request.desiredForwardMap, forward),
                normalizedOr(request.desiredUpMap, up),
                dt
            );
            return out;
        }

        if (!action.valid)
        {
            applyAttitude(
                out,
                request,
                params,
                forward,
                up,
                dt
            );
            return out;
        }

        const glm::dvec3 targetThrustDirection =
            normalizedOr(
                action.targetThrustDirectionMap,
                forward
            );

        const auto gimbal =
            solveGimbalDemand(
                targetThrustDirection,
                forward,
                right,
                up,
                params
            );

        out.mainGimbalRightInput = gimbal.rightInput;
        out.mainGimbalUpInput = gimbal.upInput;

        const glm::dvec3 targetHullForward =
            hullAxisForResidualGimbal(
                targetThrustDirection,
                forward,
                gimbal.servedAngleRad
            );

        // While moving, roll around the thrust axis has no navigation meaning.
        // Preserve the current up direction and damp angular velocity instead
        // of making the hull chase route/tunnel roll.
        applyAttitude(
            out,
            request,
            params,
            targetHullForward,
            up,
            dt
        );

        const double alignment =
            std::max(
                0.0,
                glm::dot(
                    gimbal.commandedDirectionMap,
                    targetThrustDirection
                )
            );

        out.targetSpeedRate =
            finiteClamp(
                std::clamp(action.throttle01, 0.0, 1.0) *
                alignment
            );

        out.forwardInput = 0.0f;
        out.strafeInput = 0.0f;
        out.liftInput = 0.0f;

        out.navigationAccelerationDemandValid = false;
        out.navigationVelocityTargetValid = false;
        out.navigationPrecisionTranslationOnly = false;
        return out;
    }

    static void applyAttitude(
        AutopilotControlCommand& out,
        const AutopilotControlRequest& request,
        const ShipParams& params,
        const glm::dvec3& targetForward,
        const glm::dvec3& targetUp,
        double dt
    ) noexcept
    {
        HullPoseGuidance::Request poseRequest;
        poseRequest.currentForward =
            normalizedOr(request.forwardMap, {0.0, 0.0, -1.0});
        poseRequest.currentRight =
            normalizedOr(request.rightMap, {1.0, 0.0, 0.0});
        poseRequest.currentUp =
            normalizedOr(request.upMap, {0.0, 1.0, 0.0});
        poseRequest.targetForward =
            normalizedOr(targetForward, poseRequest.currentForward);
        poseRequest.targetUp =
            normalizedOr(targetUp, poseRequest.currentUp);

        const auto pose = HullPoseGuidance::evaluate(poseRequest);
        if (!pose.valid)
            return;

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
        attitudeRequest.desiredPitchYawRateRadPerSec = {0.0, 0.0};
        attitudeRequest.rollRateRadPerSec =
            finiteOrZero(request.rollRateRadPerSec);
        attitudeRequest.desiredRollRateRadPerSec = 0.0;
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
            return;

        out.pitchInput =
            finiteClamp(attitude.pitchYawInput.x);
        out.yawInput =
            finiteClamp(attitude.pitchYawInput.y);
        out.rollInput =
            finiteClamp(attitude.rollInput);
    }

    [[nodiscard]] static AutopilotControlCommand precisionStopControl(
        const AutopilotControlRequest& request,
        const ShipParams& params,
        const glm::dvec3& forward,
        const glm::dvec3& right,
        const glm::dvec3& up,
        double dt
    ) noexcept
    {
        AutopilotControlCommand out;

        applyAttitude(
            out,
            request,
            params,
            request.terminalAttitudeHold
                ? request.desiredForwardMap
                : forward,
            request.terminalAttitudeHold
                ? request.desiredUpMap
                : up,
            dt
        );

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

        return out;
    }

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
            game::ship::mainGimbalYawLimitRad(params);
        const double pitchLimit =
            game::ship::mainGimbalPitchLimitRad(params);

        const glm::dvec3 desired =
            normalizedOr(desiredDirection, forward);

        if (yawLimit <= 1.0e-12 && pitchLimit <= 1.0e-12)
        {
            out.commandedDirectionMap = forward;
            return out;
        }

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
        out.commandedDirectionMap =
            normalizedOr(
                forward +
                    right * std::tan(commandedYaw) +
                    up * std::tan(commandedPitch),
                forward
            );

        out.servedAngleRad =
            angleBetween(forward, out.commandedDirectionMap);
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

    [[nodiscard]] static glm::dvec3 slewDirection(
        const glm::dvec3& from,
        const glm::dvec3& to,
        double maxAngleRad
    ) noexcept
    {
        const glm::dvec3 a =
            normalizedOr(from, {0.0, 0.0, -1.0});
        const glm::dvec3 b =
            normalizedOr(to, a);
        const double cosine =
            std::clamp(glm::dot(a, b), -1.0, 1.0);
        const double angle = std::acos(cosine);

        if (angle <= 1.0e-12 || maxAngleRad >= angle)
            return b;
        if (maxAngleRad <= 0.0)
            return a;

        if (cosine <= -0.999999)
        {
            glm::dvec3 axis =
                glm::cross(a, glm::dvec3(0.0, 1.0, 0.0));
            if (finiteLength(axis) <= 1.0e-9)
                axis = glm::cross(a, glm::dvec3(1.0, 0.0, 0.0));
            axis = normalizedOr(axis, {0.0, 0.0, 1.0});
            return normalizedOr(
                a * std::cos(maxAngleRad) +
                    glm::cross(axis, a) * std::sin(maxAngleRad),
                a
            );
        }

        const glm::dvec3 tangent =
            normalizedOr(
                b - a * cosine,
                b
            );
        return normalizedOr(
            a * std::cos(maxAngleRad) +
                tangent * std::sin(maxAngleRad),
            a
        );
    }

    [[nodiscard]] static double angleBetween(
        const glm::dvec3& a,
        const glm::dvec3& b
    ) noexcept
    {
        const double la = finiteLength(a);
        const double lb = finiteLength(b);
        if (la <= 1.0e-12 || lb <= 1.0e-12)
            return 0.0;
        return std::acos(
            std::clamp(
                glm::dot(a / la, b / lb),
                -1.0,
                1.0
            )
        );
    }

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

    void remember(const AutopilotControlRequest& request) noexcept
    {
        state_.initialized = true;
        state_.lastStopRequested = request.stopRequested;
        state_.previousVelocityMapMps =
            finiteVecOrZero(request.actualVelocityMapMps);
    }

    State state_ {};
};

} // namespace game::navigation::autopilot
