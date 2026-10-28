#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <limits>

#include <glm/glm.hpp>

#include "src/game/navigation/autopilot/AutopilotControlApi.h"
#include "src/game/navigation/autopilot/HullAttitudeControl.h"
#include "src/game/navigation/autopilot/HullPoseGuidance.h"
#include "src/game/ship/core/ShipDynamics.h"
#include "src/world/navigation/TrajectoryGenerator.h"

namespace game::navigation::autopilot
{

// Ballistic Newtonian pilot.
//
// Navigation owns desired velocity, the profile feed-forward and the corridor
// reference. This pilot flies the Newtonian doctrine with explicit phases:
//
//   COAST  velocity vector formed, main off; only the trims needed to stay
//          inside the safe tube (translational RCS).
//   FLIP   planned hull rotation at a calm preferred rate, main = 0; the
//          craft keeps its velocity while the hull turns around it.
//   ALIGN  rotation arrested and the ACHIEVABLE thrust vector (hull + nozzle
//          cone) brought onto the burn direction; main stays gated.
//   BURN   main thrust through the burn gate; the burn direction is latched,
//          small errors go to nozzle vectoring and RCS, not to the hull.
//
// The corridor centreline is a reference, not a rail: lateral correction is
// triggered by the PREDICTED cross-track (position + lateral velocity over a
// few seconds) leaving a safe tube, never by a small present offset.
// Correction means: nothing -> RCS -> nozzle while burning -> hull rotation
// only when the wall cannot be avoided otherwise.
class NewtonianOptimalController final
{
public:
    enum class Phase : int
    {
        Coast = 0,
        Flip = 1,
        Align = 2,
        Burn = 3,
        Hold = 4,
    };

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

        // At true rest with no translational objective, orchestration may
        // still require a launch/safety attitude before route handoff.
        const bool restAttitude =
            actualSpeed <= 0.5 &&
            finiteLength(request.desiredVelocityMapMps) <= 0.5 &&
            finiteLength(request.desiredLinearAccelerationMapMps2) <=
                1.0e-6 &&
            request.previewCount == 0 &&
            !request.stopRequested;

        AutopilotControlCommand out;
        glm::dvec3 traceTarget = forward;
        decision_ = {};
        decision_.thrustDirection = forward;
        decision_.hullTarget = forward;
        if (precisionStop)
        {
            out = precisionStopControl(
                request,
                params,
                forward,
                right,
                up,
                dt
            );
            state_.phase = Phase::Hold;
        }
        else if (request.terminalAttitudeHold || restAttitude)
        {
            traceTarget =
                normalizedOr(request.desiredForwardMap, forward);
            applyAttitude(
                out,
                request,
                params,
                traceTarget,
                request.terminalAttitudeHold
                    ? normalizedOr(request.desiredUpMap, up)
                    : up,
                dt
            );
            state_.phase = Phase::Hold;
        }
        else
        {
            out = ballisticControl(
                request,
                params,
                forward,
                right,
                up,
                dt,
                traceTarget
            );
        }

        // Every RCS translation this pilot commands is deliberate trajectory
        // delta-v; the manual keypad release-cancellation must not undo it.
        out.rcsKeepsTranslationDeltaV = true;
        out.navigationAccelerationDemandValid = false;
        out.navigationVelocityTargetValid = false;
        out.navigationPrecisionTranslationOnly = false;

        (void)traceTarget;
        traceTick(request, out, dt);
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
        out.thrustReversalLeadSeconds = plannedFlipLeadSeconds(params);
        return out;
    }

    void reset() noexcept
    {
        state_ = {};
    }

    // Coast time needed ahead of a braking burn opposite the current thrust
    // axis: a calm 180 degree flip, the align/settle time and a margin.
    [[nodiscard]] static double plannedFlipLeadSeconds(
        const ShipParams& params
    ) noexcept
    {
        constexpr double Pi = 3.14159265358979323846;
        const double turn = plannedTurnSeconds(Pi, params);
        return std::isfinite(turn) ? turn + FlipLeadMarginSeconds : 0.0;
    }

private:
    // Half of installed RCS stays reserved for attitude and nozzle
    // counter-torque; the rest may trim the trajectory.
    static constexpr double RcsTrajectoryShare = 0.60;
    // A velocity error the RCS budget clears within this time is RCS work;
    // turning the whole hull for it is not allowed.
    static constexpr double RcsDeltaVHorizonSeconds = 5.0;
    static constexpr double LongitudinalResponseSeconds = 2.0;
    // Station keeping when the reference itself is (nearly) at rest.
    static constexpr double HoldSpeedMps = 2.0;
    static constexpr double HoldResponseSeconds = 4.0;
    // Safe tube: correction starts when the PREDICTED cross-track leaves it
    // and stops once the prediction is back well inside.
    static constexpr double LateralPredictionSeconds = 4.0;
    static constexpr double LateralResponseSeconds = 2.0;
    static constexpr double SafeTubeFraction = 0.35;
    static constexpr double TubeReleaseFraction = 0.20;
    static constexpr double TubeAimFraction = 0.10;
    static constexpr double WallMarginFraction = 0.85;
    static constexpr double OvershootSlackFraction = 0.30;
    static constexpr double RetroOnlySpeedMps = 20.0;
    static constexpr double DefaultCorridorToleranceMeters = 50.0;
    // Planned turns use a calm share of the installed capability.
    static constexpr double PreferredRateShare = 0.50;
    static constexpr double PreferredAccelerationShare = 0.50;
    static constexpr double AlignSettleSeconds = 1.0;
    static constexpr double FlipLeadMarginSeconds = 1.5;
    static constexpr double DegToRad = 3.14159265358979323846 / 180.0;
    static constexpr double FlipEntryRad = 25.0 * DegToRad;
    static constexpr double FlipExitRad = 12.0 * DegToRad;
    // Burn gate on the ACHIEVABLE thrust error (hull error beyond the cone).
    static constexpr double BurnGateFullRad = 2.0 * DegToRad;
    static constexpr double BurnGateLimitedRad = 10.0 * DegToRad;
    static constexpr double BurnRateGateRadPerSec = 0.12;
    // The latched burn attitude changes only when the planned braking/burn
    // vector itself changes substantially.
    static constexpr double BurnRelatchRad = 10.0 * DegToRad;
    static constexpr double MinTurnTargetDwellSeconds = 2.0;

    struct State
    {
        bool initialized = false;
        glm::dvec3 previousVelocityMapMps {0.0};
        Phase phase = Phase::Coast;
        bool lateralCorrection = false;
        bool mainRequested = false;
        bool anticipating = false;
        bool burnLatched = false;
        glm::dvec3 burnDirectionMap {0.0, 0.0, -1.0};
        glm::dvec3 burnHullTargetMap {0.0, 0.0, -1.0};
        bool turnTargetValid = false;
        glm::dvec3 turnTargetMap {0.0, 0.0, -1.0};
        double turnTargetAgeSeconds = 0.0;
        double traceSeconds = 0.0;
        int traceFlags = 0;
    };

    // DEV decision-trace record of the last tick; never read by control.
    struct Decision
    {
        int chosenPreview = -1;
        int chosenReason = 0; // 0 none, 1 curve, 2 delta-v burn
        double chosenStartSeconds = 0.0;
        glm::dvec3 crossTrack {0.0};
        glm::dvec3 lateralVelocity {0.0};
        double speedError = 0.0;
        double feedForwardAlong = 0.0;
        double feedForwardLateral = 0.0;
        glm::dvec3 correction {0.0};
        glm::dvec3 demand {0.0};
        glm::dvec3 thrustDirection {0.0, 0.0, -1.0};
        glm::dvec3 hullTarget {0.0, 0.0, -1.0};
        double hullErrorRad = 0.0;
        double thrustErrorRad = 0.0;
        double gimbalLimitRad = 0.0;
        int gate = 0;       // 0 n/a, 1 full, 2 limited, 3 closed
        int gateReason = 0; // 1 thrust error, 2 hull rate, 3 wall side
        bool urgent = false;
        bool anticipate = false;
        bool curveAnticipation = false;
        bool latched = false;
        bool mainNeeded = false;
        bool braking = false;
        bool lateralCorrection = false;
        bool emergency = false;
        bool stationKeeping = false;
        bool profileGap = false;
    };

    struct GimbalDemandResult
    {
        float rightInput = 0.0f;
        float upInput = 0.0f;
        glm::dvec3 commandedDirectionMap {0.0, 0.0, -1.0};
    };

    [[nodiscard]] AutopilotControlCommand ballisticControl(
        const AutopilotControlRequest& request,
        const ShipParams& params,
        const glm::dvec3& forward,
        const glm::dvec3& right,
        const glm::dvec3& up,
        double dt,
        glm::dvec3& traceTarget
    ) noexcept
    {
        AutopilotControlCommand out;

        const double mainAuthority =
            std::max(0.0, game::ship::forwardMainAccelerationLimitMps2(params));
        const double rcsAuthority =
            std::max(0.0, game::ship::manoeuvreAccelerationLimitMps2(params));
        const double rcsBudget = RcsTrajectoryShare * rcsAuthority;
        const double gimbalLimit =
            std::max(
                game::ship::mainGimbalPitchLimitRad(params),
                game::ship::mainGimbalYawLimitRad(params)
            );

        const glm::dvec3 velocity =
            finiteVecOrZero(request.actualVelocityMapMps);
        const glm::dvec3 desiredVelocity =
            finiteVecOrZero(request.desiredVelocityMapMps);
        const glm::dvec3 feedForward =
            finiteVecOrZero(request.desiredLinearAccelerationMapMps2);
        const glm::dvec3 tangent =
            normalizedOr(
                desiredVelocity,
                normalizedOr(request.desiredForwardMap, forward)
            );
        const double tolerance =
            request.corridorToleranceMeters > 0.0
                ? request.corridorToleranceMeters
                : DefaultCorridorToleranceMeters;

        // The reference point left behind (an outward overshoot of a broken
        // corner clamps the route projection at its vertex): that along-track
        // error is corrected like a cross-track one, and the profile speed
        // of the stale reference is not chased.
        glm::dvec3 offset {0.0};
        double overshoot = 0.0;
        if (request.positionReferenceValid)
        {
            offset =
                finiteVecOrZero(request.actualPositionMapMeters) -
                finiteVecOrZero(request.desiredPositionMapMeters);
            overshoot =
                std::max(
                    0.0,
                    glm::dot(offset, tangent) -
                        OvershootSlackFraction * tolerance
                );
        }

        // ----- Longitudinal: follow the speed profile, deadband in coast.
        const double speedError =
            glm::dot(desiredVelocity - velocity, tangent);
        const double feedForwardAlong = glm::dot(feedForward, tangent);
        const glm::dvec3 feedForwardLateral =
            feedForward - tangent * feedForwardAlong;
        double longitudinal =
            feedForwardAlong + speedError / LongitudinalResponseSeconds;
        const double speedDeadband =
            0.5 + 0.005 * finiteLength(velocity);
        if (std::abs(speedError) < speedDeadband &&
            std::abs(feedForwardAlong) <= 1.0e-3)
            longitudinal = 0.0;
        // Speed error and profile acceleration of opposite sign (under-speed
        // while the profile decelerates, over-speed while it accelerates):
        // stop the profile burn, never reverse it (that would need the hull
        // turned around). The profile is a function of POSITION and only
        // "comes down" while the craft keeps moving, so the gap is closed
        // with RCS instead of stalling short of the next stop.
        const bool profileGapOnRcs =
            speedError * feedForwardAlong < 0.0 &&
            std::abs(speedError) > speedDeadband;
        if (profileGapOnRcs)
            longitudinal =
                std::clamp(
                    speedError / LongitudinalResponseSeconds,
                    -rcsBudget,
                    rcsBudget
                );
        if (overshoot > 0.0)
            longitudinal = 0.0;

        // ----- Lateral: predictive safe tube around the corridor reference.
        const glm::dvec3 relativeVelocity = velocity - desiredVelocity;
        glm::dvec3 lateralVelocity =
            relativeVelocity -
            tangent * glm::dot(relativeVelocity, tangent);
        glm::dvec3 lateral = feedForwardLateral;
        bool lateralEmergency = false;
        glm::dvec3 crossTrack {0.0};
        if (request.positionReferenceValid)
        {
            crossTrack = offset - tangent * glm::dot(offset, tangent);
            if (overshoot > 0.0)
            {
                crossTrack += tangent * overshoot;
                lateralVelocity += tangent * glm::dot(velocity, tangent);
            }
            const glm::dvec3 predicted =
                crossTrack + lateralVelocity * LateralPredictionSeconds;
            const double predictedDistance = finiteLength(predicted);

            if (!state_.lateralCorrection &&
                predictedDistance > SafeTubeFraction * tolerance)
                state_.lateralCorrection = true;
            else if (state_.lateralCorrection &&
                     predictedDistance <= TubeReleaseFraction * tolerance)
                state_.lateralCorrection = false;

            if (state_.lateralCorrection)
            {
                const double aimRadius = TubeAimFraction * tolerance;
                const glm::dvec3 aim =
                    predictedDistance > aimRadius
                        ? predicted * (aimRadius / predictedDistance)
                        : predicted;
                lateral +=
                    (aim - predicted) /
                    (LateralPredictionSeconds * LateralResponseSeconds);
            }

            // Escalate to the main engine only when RCS can no longer stop
            // the outward drift before the corridor wall.
            const double crossDistance = finiteLength(crossTrack);
            const double outward =
                crossDistance > 1.0e-6
                    ? std::max(
                        0.0,
                        glm::dot(lateralVelocity, crossTrack / crossDistance)
                      )
                    : 0.0;
            const double margin =
                std::max(0.0, WallMarginFraction * tolerance - crossDistance);
            lateralEmergency =
                outward > 1.0e-6 &&
                (rcsBudget <= 1.0e-9 ||
                 outward * outward / (2.0 * rcsBudget) > margin);
        }
        else
        {
            lateral -= lateralVelocity / LongitudinalResponseSeconds;
        }

        glm::dvec3 demand = tangent * longitudinal + lateral;

        // Station keeping: the reference is (nearly) at rest and stays so
        // over the preview. Its tangent is ambiguous there, so the full
        // position error is trimmed out with RCS alone. A stop the route
        // flies through (a broken corner) is NOT held: the craft must pass
        // its vertex for the reference to move on.
        bool referenceMovesAhead = false;
        for (std::size_t k = 0;
             k < std::min(request.previewCount, AutopilotPreviewCapacity);
             ++k)
            referenceMovesAhead =
                referenceMovesAhead ||
                finiteLength(request.preview[k].desiredVelocityMapMps) >=
                    HoldSpeedMps;
        const bool stationKeeping =
            !referenceMovesAhead &&
            request.positionReferenceValid &&
            rcsBudget > 1.0e-9 &&
            finiteLength(desiredVelocity) < HoldSpeedMps &&
            std::abs(feedForwardAlong) <= rcsBudget &&
            finiteLength(velocity) <= rcsBudget * RcsDeltaVHorizonSeconds;
        if (stationKeeping)
            demand =
                -offset / (HoldResponseSeconds * HoldResponseSeconds) -
                velocity * (2.0 / HoldResponseSeconds);

        // ----- Anticipate the next main burn from the ACTUAL velocity: the
        // delta-v each preview sample requires and the latest time that burn
        // may start. Turn the hull early enough to reach it aligned; if that
        // time has already passed, the burn is current.
        decision_ = {};
        bool anticipate = false;
        bool urgent = false;
        glm::dvec3 upcomingDirection = forward;
        glm::dvec3 urgentDemand {0.0};
        const double plannedBraking =
            world::navigation::TrajectoryGenerationPolicy::
                DefaultPlanningAuthorityFraction * mainAuthority;
        const bool moving = finiteLength(velocity) > 1.0;
        const glm::dvec3 motionDirection = normalizedOr(velocity, tangent);
        const std::size_t previewCount =
            std::min(request.previewCount, AutopilotPreviewCapacity);
        double earliestStart = std::numeric_limits<double>::infinity();
        for (std::size_t k = 0;
             k < previewCount && plannedBraking > 1.0e-9;
             ++k)
        {
            const auto& sample = request.preview[k];
            // A curve ahead whose path-normal acceleration RCS cannot carry
            // is a main burn from the moment the curve begins: the thrust
            // vector must already point there when the curve starts.
            const glm::dvec3 sampleFeedForward =
                finiteVecOrZero(sample.desiredLinearAccelerationMapMps2);
            const glm::dvec3 sampleTangent =
                normalizedOr(sample.desiredVelocityMapMps, motionDirection);
            const glm::dvec3 sampleCurveAcceleration =
                sampleFeedForward -
                sampleTangent * glm::dot(sampleFeedForward, sampleTangent);
            if (moving && finiteLength(sampleCurveAcceleration) > rcsBudget)
            {
                const double curveStart =
                    k == 0
                        ? 0.0
                        : std::max(
                              0.0, request.preview[k - 1].timeOffsetSeconds);
                if (curveStart < earliestStart)
                {
                    decision_.chosenPreview = static_cast<int>(k);
                    decision_.chosenReason = 1;
                    earliestStart = curveStart;
                    upcomingDirection =
                        normalizedOr(sampleFeedForward, forward);
                    urgentDemand = sampleFeedForward;
                }
            }
            glm::dvec3 deltaV =
                finiteVecOrZero(sample.desiredVelocityMapMps) - velocity;
            // At speed only the speed REDUCTION is planned here: the retro
            // burn along the present motion; a course change after a corner
            // is flown once slow. When already slow the full delta-v (the
            // new segment direction) is the burn to prepare.
            if (finiteLength(velocity) > RetroOnlySpeedMps)
                deltaV =
                    -motionDirection *
                    std::max(0.0, -glm::dot(deltaV, motionDirection));
            // Delta-v the RCS alone delivers before that sample needs no
            // main burn and no hull turn.
            const double required = finiteLength(deltaV);
            if (required <=
                rcsBudget *
                    std::max(RcsDeltaVHorizonSeconds,
                             sample.timeOffsetSeconds))
                continue;
            const double latestStart =
                std::max(0.0, sample.timeOffsetSeconds) -
                required / plannedBraking;
            if (latestStart >= earliestStart)
                continue;
            decision_.chosenPreview = static_cast<int>(k);
            decision_.chosenReason = 2;
            earliestStart = latestStart;
            upcomingDirection = deltaV / required;
            urgentDemand =
                deltaV / std::max(0.5, sample.timeOffsetSeconds);
        }
        if (std::isfinite(earliestStart))
        {
            const double angle = angleBetween(forward, upcomingDirection);
            const double lead =
                plannedTurnSeconds(angle, params) + FlipLeadMarginSeconds +
                (state_.anticipating ? 1.0 : 0.0);
            urgent = moving && earliestStart <= 0.0;
            // Stays true once the hull is aligned, so a conflicting burn
            // cannot turn the hull away again before the planned one starts.
            anticipate = !urgent && earliestStart <= lead;
        }
        state_.anticipating = anticipate;
        if (urgent && !stationKeeping &&
            finiteLength(urgentDemand) > finiteLength(demand))
            demand = urgentDemand + lateral;

        // ----- Is this main-engine work? (hysteresis while already burning)
        const double hold = state_.mainRequested ? 0.5 : 1.0;
        if (stationKeeping)
        {
            urgent = false;
            anticipate = false;
        }
        bool mainNeeded =
            !stationKeeping &&
            mainAuthority > 1.0e-9 &&
            (urgent ||
             (longitudinal != 0.0 && !profileGapOnRcs &&
              (std::abs(feedForwardAlong) > hold * rcsBudget ||
               std::abs(speedError) >
                   hold * rcsBudget * RcsDeltaVHorizonSeconds)) ||
             finiteLength(feedForwardLateral) > hold * rcsBudget ||
             lateralEmergency ||
             (rcsBudget <= 1.0e-9 && finiteLength(demand) > 0.05));
        const glm::dvec3 burnDirection = normalizedOr(demand, tangent);
        // A planned flip for braking wins over continuing to ACCELERATE along
        // the motion. A braking burn is finished first, and a wall-avoidance
        // burn is never dropped.
        if (anticipate && mainNeeded && !lateralEmergency &&
            glm::dot(burnDirection, velocity) > 0.0 &&
            angleBetween(burnDirection, upcomingDirection) > FlipEntryRad)
            mainNeeded = false;
        state_.mainRequested = mainNeeded;

        const double hullRate =
            std::hypot(
                finiteOrZero(request.pitchRateRadPerSec),
                finiteOrZero(request.yawRateRadPerSec)
            );

        Phase phase = Phase::Coast;
        glm::dvec3 attitudeTarget = forward;
        double throttle = 0.0;
        glm::dvec3 delivered {0.0};
        if (mainNeeded)
        {
            if (!state_.burnLatched ||
                angleBetween(burnDirection, state_.burnDirectionMap) >
                    BurnRelatchRad)
            {
                // A burn the nozzle cone already serves keeps the hull where
                // it is; otherwise the hull is brought onto the burn vector.
                state_.burnDirectionMap = burnDirection;
                state_.burnHullTargetMap =
                    angleBetween(forward, burnDirection) <= 0.5 * gimbalLimit
                        ? forward
                        : burnDirection;
                state_.burnLatched = true;
            }
            // On a curve the required thrust vector itself turns with the
            // path: the hull follows it continuously instead of holding a
            // latched braking attitude, and its rate is not a gate.
            const bool curving =
                finiteLength(feedForwardLateral) > rcsBudget;
            attitudeTarget =
                curving ? burnDirection : state_.burnHullTargetMap;
            traceTarget = burnDirection;

            const double hullError = angleBetween(forward, burnDirection);
            const double thrustError =
                std::max(0.0, hullError - gimbalLimit);
            const double wanted =
                std::clamp(finiteLength(demand) / mainAuthority, 0.0, 1.0);
            const auto gimbal =
                solveGimbalDemand(burnDirection, forward, right, up, params);

            decision_.hullErrorRad = hullError;
            decision_.thrustErrorRad = thrustError;
            decision_.gateReason =
                thrustError > BurnGateFullRad ? 1
                : (!curving && hullRate > BurnRateGateRadPerSec) ? 2 : 0;
            decision_.gate = 3;
            if (thrustError <= BurnGateFullRad &&
                (curving || hullRate <= BurnRateGateRadPerSec))
            {
                throttle = wanted;
                decision_.gate = 1;
            }
            else if (thrustError <= BurnGateLimitedRad)
            {
                decision_.gate = 2;
                // Limited burn: its side push must stay within what RCS can
                // cancel, and never push toward a near corridor wall.
                const double sidePush =
                    mainAuthority * std::sin(thrustError);
                throttle =
                    sidePush > 1.0e-9
                        ? std::min(wanted, 0.8 * rcsBudget / sidePush)
                        : wanted;
                const glm::dvec3 side =
                    gimbal.commandedDirectionMap -
                    burnDirection *
                        glm::dot(gimbal.commandedDirectionMap, burnDirection);
                if (request.positionReferenceValid &&
                    finiteLength(crossTrack) > 0.6 * tolerance &&
                    glm::dot(side, crossTrack) > 0.0)
                {
                    throttle = 0.0;
                    decision_.gate = 3;
                    decision_.gateReason = 3;
                }
            }

            if (throttle > 1.0e-6)
            {
                phase = Phase::Burn;
                out.mainGimbalRightInput = gimbal.rightInput;
                out.mainGimbalUpInput = gimbal.upInput;
                delivered =
                    gimbal.commandedDirectionMap * (mainAuthority * throttle);
            }
            else
            {
                throttle = 0.0;
                phase = turnPhase(hullError);
            }
        }
        else
        {
            state_.burnLatched = false;
            if (anticipate)
            {
                attitudeTarget = upcomingDirection;
                traceTarget = upcomingDirection;
                phase = turnPhase(angleBetween(forward, upcomingDirection));
            }
        }
        // A started turn is not re-aimed elsewhere before it had time to
        // matter: alternating targets would keep the hull turning forever
        // and never open the burn gate.
        if (phase == Phase::Flip || phase == Phase::Align)
        {
            if (state_.turnTargetValid &&
                angleBetween(attitudeTarget, state_.turnTargetMap) >
                    FlipEntryRad &&
                state_.turnTargetAgeSeconds < MinTurnTargetDwellSeconds)
            {
                attitudeTarget = state_.turnTargetMap;
                phase = turnPhase(angleBetween(forward, attitudeTarget));
            }
            else
            {
                if (!state_.turnTargetValid ||
                    angleBetween(attitudeTarget, state_.turnTargetMap) >
                        FlipEntryRad)
                    state_.turnTargetAgeSeconds = 0.0;
                state_.turnTargetMap = attitudeTarget;
            }
            state_.turnTargetValid = true;
            state_.turnTargetAgeSeconds += dt;
        }
        else
        {
            state_.turnTargetValid = false;
        }
        state_.phase = phase;
        decision_.crossTrack = crossTrack;
        decision_.lateralVelocity = lateralVelocity;
        decision_.speedError = speedError;
        decision_.feedForwardAlong = feedForwardAlong;
        decision_.feedForwardLateral = finiteLength(feedForwardLateral);
        decision_.correction = lateral - feedForwardLateral;
        decision_.demand = demand;
        decision_.thrustDirection = mainNeeded ? burnDirection
            : anticipate ? upcomingDirection : forward;
        decision_.hullTarget = attitudeTarget;
        if (!mainNeeded)
            decision_.hullErrorRad = angleBetween(forward, attitudeTarget);
        decision_.gimbalLimitRad = gimbalLimit;
        decision_.urgent = urgent;
        decision_.anticipate = anticipate;
        decision_.curveAnticipation =
            (urgent || anticipate) && decision_.chosenReason == 1;
        decision_.chosenStartSeconds =
            std::isfinite(earliestStart) ? earliestStart : -1.0;
        decision_.latched = state_.burnLatched;
        decision_.mainNeeded = mainNeeded;
        decision_.braking =
            mainNeeded && glm::dot(burnDirection, velocity) < 0.0;
        decision_.lateralCorrection = state_.lateralCorrection;
        decision_.emergency = lateralEmergency;
        decision_.stationKeeping = stationKeeping;
        decision_.profileGap = profileGapOnRcs;
        state_.traceFlags =
            (overshoot > 0.0 ? 1 : 0) | (mainNeeded ? 2 : 0) |
            (anticipate ? 4 : 0) | (urgent ? 8 : 0) |
            (stationKeeping ? 16 : 0) |
            (state_.lateralCorrection ? 32 : 0) |
            (lateralEmergency ? 64 : 0);

        // While moving, roll around the thrust axis has no navigation
        // meaning: keep the current up and only damp roll rate.
        if (phase == Phase::Flip)
            applyFlipRate(out, request, params, attitudeTarget, up, dt);
        else
            applyAttitude(out, request, params, attitudeTarget, up, dt);

        out.targetSpeedRate = finiteClamp(throttle);

        // RCS takes everything the main burn does not deliver, within its
        // trajectory budget. Dribbles below 1% of authority are dropped.
        if (rcsAuthority > 1.0e-12)
        {
            glm::dvec3 remainder = demand - delivered;
            const double magnitude = finiteLength(remainder);
            if (magnitude > rcsBudget && magnitude > 1.0e-12)
                remainder *= rcsBudget / magnitude;
            if (finiteLength(remainder) < 0.01 * rcsAuthority)
                remainder = glm::dvec3(0.0);
            out.forwardInput =
                finiteClamp(glm::dot(remainder, forward) / rcsAuthority);
            out.strafeInput =
                finiteClamp(glm::dot(remainder, right) / rcsAuthority);
            out.liftInput =
                finiteClamp(glm::dot(remainder, up) / rcsAuthority);
        }
        return out;
    }

    [[nodiscard]] Phase turnPhase(double hullErrorRad) const noexcept
    {
        const bool flipping = state_.phase == Phase::Flip;
        return hullErrorRad > FlipEntryRad ||
                   (flipping && hullErrorRad > FlipExitRad)
            ? Phase::Flip
            : Phase::Align;
    }

    [[nodiscard]] static double preferredTurnRateRadPerSec(
        const ShipParams& params
    ) noexcept
    {
        // Physical limits, including the hull-load rate bound.
        const double installed =
            std::min(
                game::ship::pitchRateLimitRadPerSec(params),
                game::ship::yawRateLimitRadPerSec(params)
            );
        return std::isfinite(installed)
            ? PreferredRateShare * std::max(0.0, installed)
            : 0.0;
    }

    [[nodiscard]] static double preferredTurnAccelerationRadPerSec2(
        const ShipParams& params
    ) noexcept
    {
        return PreferredAccelerationShare *
            std::max(
                0.0,
                game::ship::angularAccelerationLimitRadPerSec2(params)
            );
    }

    // Calm rest-to-rest turn time at the preferred rate/acceleration,
    // plus the align/settle time before a burn may start.
    [[nodiscard]] static double plannedTurnSeconds(
        double angleRad,
        const ShipParams& params
    ) noexcept
    {
        const double angle = std::max(0.0, angleRad);
        const double rate = preferredTurnRateRadPerSec(params);
        const double acceleration =
            preferredTurnAccelerationRadPerSec2(params);
        if (!(rate > 1.0e-9 && acceleration > 1.0e-9))
            return std::numeric_limits<double>::infinity();
        const double turn =
            angle <= rate * rate / acceleration
                ? 2.0 * std::sqrt(angle / acceleration)
                : angle / rate + rate / acceleration;
        return turn + AlignSettleSeconds;
    }

    // Planned flip: track a calm trapezoidal rate profile toward the target
    // instead of the pose servo's full-authority bang. Roll is unchanged.
    static void applyFlipRate(
        AutopilotControlCommand& out,
        const AutopilotControlRequest& request,
        const ShipParams& params,
        const glm::dvec3& targetForward,
        const glm::dvec3& targetUp,
        double dt
    ) noexcept
    {
        applyAttitude(out, request, params, targetForward, targetUp, dt);

        HullPoseGuidance::Request poseRequest;
        poseRequest.currentForward =
            normalizedOr(request.forwardMap, {0.0, 0.0, -1.0});
        poseRequest.currentRight =
            normalizedOr(request.rightMap, {1.0, 0.0, 0.0});
        poseRequest.currentUp =
            normalizedOr(request.upMap, {0.0, 1.0, 0.0});
        poseRequest.targetForward =
            normalizedOr(targetForward, poseRequest.currentForward);
        poseRequest.targetUp = poseRequest.currentUp;
        const auto pose = HullPoseGuidance::evaluate(poseRequest);
        const double authority =
            game::ship::angularAccelerationLimitRadPerSec2(params);
        if (!pose.valid || !(authority > 1.0e-9))
            return;

        const glm::dvec2 error {
            pose.pitchYawErrorLocalRad.x,
            pose.pitchYawErrorLocalRad.y
        };
        const double angle = glm::length(error);
        if (!(angle > 1.0e-9))
            return;
        const glm::dvec2 axis = error / angle;

        const double rateLimit = preferredTurnRateRadPerSec(params);
        const double acceleration =
            preferredTurnAccelerationRadPerSec2(params);
        const double remaining =
            std::max(0.0, angle - 0.5 * FlipExitRad);
        const double desiredRate =
            std::min(rateLimit, std::sqrt(2.0 * acceleration * remaining));

        const glm::dvec2 rate {
            finiteOrZero(request.pitchRateRadPerSec),
            finiteOrZero(request.yawRateRadPerSec)
        };
        constexpr double RateResponseSeconds = 0.5;
        glm::dvec2 wanted =
            (axis * desiredRate - rate) * (2.0 / RateResponseSeconds);
        const double magnitude = glm::length(wanted);
        if (magnitude > acceleration && magnitude > 1.0e-12)
            wanted *= acceleration / magnitude;
        out.pitchInput = finiteClamp(wanted.x / authority);
        out.yawInput = finiteClamp(wanted.y / authority);
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
        // Same critically damped hull servo as Assisted. The old one-tick
        // predictive law demanded the target rate within 20 ms; with strong
        // installed RCS and replicated (delayed) rates it limit-cycled.
        attitudeRequest.pitchYawResponseSeconds = 0.50;
        attitudeRequest.rollResponseSeconds = 0.50;

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

    [[nodiscard]] static GimbalDemandResult solveGimbalDemand(
        const glm::dvec3& desiredDirection,
        const glm::dvec3& forward,
        const glm::dvec3& right,
        const glm::dvec3& up,
        const ShipParams& params
    ) noexcept
    {
        GimbalDemandResult out;

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
        return out;
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

    // DEV diagnostic only. ELITE_NEWTONIAN_POLICY_TRACE=<tsv path> writes a
    // decision row at 10 Hz (and on every event tick);
    // ELITE_NEWTONIAN_EVENT_TRACE=<tsv path> writes discrete decision events.
    // No effect on control when unset.
    static std::FILE* openTrace(const char* variable, const char* header)
    {
        const char* path = std::getenv(variable);
        if (!path || !*path)
            return nullptr;
        std::FILE* f = std::fopen(path, "w");
        if (f)
            std::fputs(header, f);
        return f;
    }

    void traceTick(
        const AutopilotControlRequest& request,
        const AutopilotControlCommand& out,
        double dt
    ) noexcept
    {
        static std::FILE* file = openTrace(
            "ELITE_NEWTONIAN_POLICY_TRACE",
            "t\tcurve\ts_m\tref_x\tref_y\tref_z\ttan_x\ttan_y\ttan_z\t"
            "radius_m\tprofile_mps\tref_mps\tspeed_mps\tpreview_k\t"
            "preview_reason\tpreview_start_s\txtrack_m\tpred1_m\tpred2_m\t"
            "pred3_m\tpred4_m\tlat_v_mps\tlong_err_mps\tff_along\tff_lat\t"
            "corr_acc\tdemand_acc\tfwd_x\tfwd_y\tfwd_z\tomega\t"
            "thrust_x\tthrust_y\tthrust_z\thull_x\thull_y\thull_z\t"
            "hull_err_deg\tthrust_err_deg\tgimbal_r\tgimbal_u\t"
            "gimbal_lim_deg\tthrottle\trcs_f\trcs_s\trcs_l\tpitch\tyaw\t"
            "roll\tphase\turgent\tanticipate\tcurve_ant\tlatch\tgate\t"
            "gate_reason\ttube\temergency\tcoast\tflip\tbraking\t"
            "station\tprofile_gap\ttarget_changed\ttarget_delta_deg\t"
            "thrust_delta_deg\tstop\ttol_m\n");
        static std::FILE* events = openTrace(
            "ELITE_NEWTONIAN_EVENT_TRACE", "t\ts_m\tevent\tdetail\n");
        state_.traceSeconds += dt;
        if (!file && !events)
            return;

        constexpr double Deg = 57.2957795130823208768;
        const double t = state_.traceSeconds;
        const double s = request.traceRouteProgressMeters;
        const Decision& d = decision_;
        TraceMemory& m = traceMemory_;
        const int phase = static_cast<int>(state_.phase);
        const bool curving = request.traceCurvaturePerMeter > 1.0e-6;
        const double targetDelta =
            m.valid ? angleBetween(m.hullTarget, d.hullTarget) * Deg : 0.0;
        const double thrustDelta =
            m.valid ? angleBetween(m.thrustDirection, d.thrustDirection) * Deg
                    : 0.0;
        const double crossDistance = finiteLength(d.crossTrack);
        const double tol = request.corridorToleranceMeters;
        const bool violation =
            request.positionReferenceValid && tol > 0.0 && crossDistance > tol;

        bool eventTick = false;
        char detail[160];
        auto event = [&](const char* name)
        {
            eventTick = true;
            if (events)
                std::fprintf(events, "%.3f\t%.1f\t%s\t%s\n", t, s, name, detail);
        };
        if (m.valid)
        {
            if (curving != m.curving)
            {
                std::snprintf(detail, sizeof(detail),
                    "curve=%zu radius=%.0f speed=%.1f",
                    request.traceCurveIndex,
                    curving ? 1.0 / request.traceCurvaturePerMeter : 0.0,
                    finiteLength(request.actualVelocityMapMps));
                event(curving ? "ENTER_CURVE" : "LEAVE_CURVE");
            }
            if (targetDelta > 25.0)
            {
                std::snprintf(detail, sizeof(detail),
                    "hull_target_delta=%.1f phase=%d", targetDelta, phase);
                event("TARGET_SWITCH");
            }
            if (d.urgent != m.urgent)
            {
                std::snprintf(detail, sizeof(detail), "preview=%d reason=%d",
                    d.chosenPreview, d.chosenReason);
                event(d.urgent ? "URGENT_ON" : "URGENT_OFF");
            }
            const bool open = d.gate == 1 || d.gate == 2;
            const bool wasOpen = m.gate == 1 || m.gate == 2;
            if (open != wasOpen && (d.gate != 0 || m.gate != 0))
            {
                std::snprintf(detail, sizeof(detail),
                    "gate=%d reason=%d thrust_err=%.1f hull_err=%.1f",
                    d.gate, d.gateReason, d.thrustErrorRad * Deg,
                    d.hullErrorRad * Deg);
                event(open ? "BURN_GATE_OPEN" : "BURN_GATE_CLOSE");
            }
            if (d.latched != m.latched)
            {
                detail[0] = '\0';
                event(d.latched ? "LATCH_SET" : "LATCH_CLEAR");
            }
            if (phase != m.phase)
            {
                std::snprintf(detail, sizeof(detail), "%d->%d", m.phase, phase);
                event("PHASE_CHANGE");
            }
            if (thrustDelta > 90.0 && (d.mainNeeded || d.anticipate))
            {
                std::snprintf(detail, sizeof(detail), "thrust_delta=%.1f",
                    thrustDelta);
                event("COMMAND_REVERSAL");
            }
            const double pitch = out.pitchInput;
            const double yaw = out.yawInput;
            if ((pitch * m.pitchInput < -0.09 &&
                 std::abs(pitch) > 0.3 && std::abs(m.pitchInput) > 0.3) ||
                (yaw * m.yawInput < -0.09 &&
                 std::abs(yaw) > 0.3 && std::abs(m.yawInput) > 0.3))
            {
                std::snprintf(detail, sizeof(detail),
                    "rotation pitch %.2f->%.2f yaw %.2f->%.2f",
                    m.pitchInput, pitch, m.yawInput, yaw);
                event("COMMAND_REVERSAL");
            }
            if (violation != m.corridorViolation)
            {
                std::snprintf(detail, sizeof(detail), "xtrack=%.1f tol=%.1f",
                    crossDistance, tol);
                event(violation ? "CORRIDOR_VIOLATION" : "CORRIDOR_RECOVERED");
            }
        }

        if (file && (eventTick || m.tick % 5 == 0))
        {
            const glm::dvec3 v = finiteVecOrZero(request.actualVelocityMapMps);
            const glm::dvec3 vDes =
                finiteVecOrZero(request.desiredVelocityMapMps);
            const glm::dvec3 tangent =
                normalizedOr(vDes, request.desiredForwardMap);
            const glm::dvec3 ref = request.desiredPositionMapMeters;
            const glm::dvec3 fwd = request.forwardMap;
            const double omega = std::sqrt(
                request.pitchRateRadPerSec * request.pitchRateRadPerSec +
                request.yawRateRadPerSec * request.yawRateRadPerSec +
                request.rollRateRadPerSec * request.rollRateRadPerSec);
            auto pred = [&](double seconds)
            {
                return finiteLength(d.crossTrack + d.lateralVelocity * seconds);
            };
            const bool coast = state_.phase == Phase::Coast;
            const bool flip = state_.phase == Phase::Flip;
            std::fprintf(file,
                "%.3f\t%zu\t%.1f\t%.1f\t%.1f\t%.1f\t%.4f\t%.4f\t%.4f\t"
                "%.0f\t%.2f\t%.2f\t%.2f\t%d\t%d\t%.2f\t%.2f\t%.2f\t%.2f\t"
                "%.2f\t%.2f\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t"
                "%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t"
                "%.4f\t%.2f\t%.2f\t%.3f\t%.3f\t%.2f\t%.3f\t%.3f\t%.3f\t"
                "%.3f\t%.3f\t%.3f\t%.3f\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t"
                "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%.1f\t%.1f\t%d\t%.1f\n",
                t, request.traceCurveIndex, s, ref.x, ref.y, ref.z,
                tangent.x, tangent.y, tangent.z,
                curving ? 1.0 / request.traceCurvaturePerMeter : 0.0,
                request.traceProfileSpeedMps, finiteLength(vDes),
                finiteLength(v), d.chosenPreview, d.chosenReason,
                d.chosenStartSeconds,
                request.positionReferenceValid ? crossDistance : 0.0,
                pred(1.0), pred(2.0), pred(3.0), pred(4.0),
                finiteLength(d.lateralVelocity), d.speedError,
                d.feedForwardAlong, d.feedForwardLateral,
                finiteLength(d.correction), finiteLength(d.demand),
                fwd.x, fwd.y, fwd.z, omega,
                d.thrustDirection.x, d.thrustDirection.y, d.thrustDirection.z,
                d.hullTarget.x, d.hullTarget.y, d.hullTarget.z,
                d.hullErrorRad * Deg, d.thrustErrorRad * Deg,
                out.mainGimbalRightInput, out.mainGimbalUpInput,
                d.gimbalLimitRad * Deg, out.targetSpeedRate,
                out.forwardInput, out.strafeInput, out.liftInput,
                out.pitchInput, out.yawInput, out.rollInput,
                phase, d.urgent ? 1 : 0, d.anticipate ? 1 : 0,
                d.curveAnticipation ? 1 : 0, d.latched ? 1 : 0, d.gate,
                d.gateReason, d.lateralCorrection ? 1 : 0,
                d.emergency ? 1 : 0, coast ? 1 : 0, flip ? 1 : 0,
                d.braking ? 1 : 0, d.stationKeeping ? 1 : 0,
                d.profileGap ? 1 : 0, targetDelta > 1.0 ? 1 : 0,
                targetDelta, thrustDelta,
                request.stopRequested ? 1 : 0, tol);
        }
        if (file)
            std::fflush(file);
        if (events)
            std::fflush(events);

        m.valid = true;
        m.hullTarget = d.hullTarget;
        m.thrustDirection = d.thrustDirection;
        m.phase = phase;
        m.urgent = d.urgent;
        m.latched = d.latched;
        m.gate = d.gate;
        m.curving = curving;
        m.corridorViolation = violation;
        m.pitchInput = out.pitchInput;
        m.yawInput = out.yawInput;
        ++m.tick;
    }

    void remember(const AutopilotControlRequest& request) noexcept
    {
        state_.initialized = true;
        state_.previousVelocityMapMps =
            finiteVecOrZero(request.actualVelocityMapMps);
    }

    State state_ {};
    Decision decision_ {};
    struct TraceMemory
    {
        bool valid = false;
        glm::dvec3 hullTarget {0.0, 0.0, -1.0};
        glm::dvec3 thrustDirection {0.0, 0.0, -1.0};
        int phase = -1;
        bool urgent = false;
        bool latched = false;
        int gate = 0;
        bool curving = false;
        bool corridorViolation = false;
        double pitchInput = 0.0;
        double yawInput = 0.0;
        int tick = 0;
    } traceMemory_ {};
};

} // namespace game::navigation::autopilot
