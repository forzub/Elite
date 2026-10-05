#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <iterator>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/AcceptedManeuverProgram.h"
#include "src/game/navigation/AcceptedManeuverProgramBuilder.h"
#include "src/game/navigation/ManeuverCapabilityAdapters.h"
#include "src/game/navigation/ManeuverTrackingController.h"
#include "src/game/navigation/DockingAutomaticRecoveryPolicy.h"
#include "src/game/navigation/autopilot/PredictivePilot.h"
#include "src/game/navigation/autopilot/RouteFollowerApi.h"
#include "src/game/navigation/planner/RoutePlannerApi.h"
#include "src/game/ship/core/ShipControlState.h"
#include "src/game/ship/core/ShipDynamics.h"
#include "src/game/ship/core/ShipParams.h"
#include "src/world/navigation/TrajectoryGenerator.h"
#include "src/world/navigation/NavigationVehicleProfile.h"

namespace game::navigation::autopilot
{

// Client-side virtual pilot.
//
// It has no network/server/task semantics.  Its only executable product is
// ShipControlState -- the same control surface used by the human input mapper.
// Route geometry stays entirely on the client.
class ClientRouteAutopilot final
{
public:
    struct State
    {
        bool active = false;
        std::uint64_t requestSerial = 0;
        std::size_t currentPage = 0;
        std::size_t currentSpatialSegment = 0;

        // AcceptedManeuverProgram pages are fixed-capacity storage only.
        // Execution owns one continuous spatial reference assembled from all
        // pages, with their duplicated boundary sample removed.
        std::vector<AcceptedManeuverProgram::ReferenceSample>
            continuousSamples;
        std::vector<double> continuousProgressMeters;
        std::vector<double> nominalSpeedProfileMps;
        std::vector<double> runtimeSpeedProfileMps;
        std::vector<double> runtimeLongitudinalAccelerationMps2;
        std::vector<double> checkpointProgressMeters;
        std::size_t currentContinuousSegment = 0;
        std::size_t nextCheckpointIndex = 1;
        std::uint64_t speedProfileRevision = 0;
        bool holdAtTerminal = false;
        double trackingPositionToleranceMeters = 0.0;

        // Authoritative spatial geometry. Accepted maneuver samples still own
        // timing/speed/attitude feed-forward, but they no longer define the
        // route shape when Planner supplied parametric curves.
        std::vector<planner::RouteCurveSegment> routeCurves;
        std::vector<double> curveSampleStartProgressMeters;
        std::vector<double> curveSampleEndProgressMeters;
        glm::dvec3 routeUpReference {0.0};

        RouteFollowerPolicy followerPolicy {};
        PredictivePilot::State pilotState {};
        std::vector<AcceptedManeuverProgram> programs;
    };

    struct Output
    {
        bool valid = false;
        bool complete = false;
        ShipControlState control {};
        double crossTrackErrorMeters = 0.0;
        double remainingDistanceMeters = 0.0;
        double targetSpeedMps = 0.0;
        double targetLongitudinalAccelerationMps2 = 0.0;
        double crossTrackClosingSpeedMps = 0.0;
        double crossTrackCaptureSpeedMps = 0.0;
        double forwardErrorRad = 0.0;
        double upErrorRad = 0.0;
        double courseLeadDistanceMeters = 0.0;
        double predictedCrossTrackMeters = 0.0;
        double centeringDeadbandMeters = 0.0;
        double routeCurvaturePerMeter = 0.0;
        double routeRadiusMeters = 0.0;
        double exactRemainingRouteMeters = 0.0;
        double desiredCourseAngularRateRadPerSec = 0.0;
        double actualAngularRateRadPerSec = 0.0;
        double coursePhaseLeadAngleRad = 0.0;
        double courseResponseSeconds = 0.0;
        double effectiveBrakingAuthorityMps2 = 0.0;
        double requiredTerminalStopDistanceMeters = 0.0;
        double turnSpeedCeilingMps = 0.0;
        double distanceToTurnMeters = 0.0;
        double crossTrackCorrectionAngleRad = 0.0;
        double desiredCaptureAngularRateRadPerSec = 0.0;
        double signedRollErrorRad = 0.0;
        double desiredRollRateRadPerSec = 0.0;
        bool terminalBrakeActive = false;
        bool terminalAttitudeCaptureActive = false;
        bool brakeAttitudeLockActive = false;
        std::size_t routeCurveIndex = 0;
        std::size_t pageIndex = 0;
        std::size_t segmentIndex = 0;
        std::size_t checkpointIndex = 0;
        std::uint64_t speedProfileRevision = 0;
        bool terminalHold = false;
    };

    static void stop(State& state) noexcept
    {
        state = {};
    }

    [[nodiscard]] static bool start(
        State& state,
        const planner::RoutePlan& plan,
        const RouteFollowerAgentState& initialAgent,
        LocalFlightControlLaw law,
        const ShipParams& params,
        double acceptedAtUniverseTimeSeconds,
        std::uint64_t requestSerial,
        double trackingPositionToleranceMeters,
        const glm::dvec3& routeUpReference = glm::dvec3(0.0),
        bool holdAtTerminal = false
    )
    {
        auto programs = buildPrograms(
            plan,
            initialAgent,
            law,
            params,
            acceptedAtUniverseTimeSeconds,
            requestSerial,
            trackingPositionToleranceMeters,
            routeUpReference
        );
        if (programs.empty())
            return false;

        state = {};
        state.active = true;
        state.requestSerial = requestSerial;
        state.holdAtTerminal = holdAtTerminal;
        state.trackingPositionToleranceMeters =
            std::max(0.0, trackingPositionToleranceMeters);
        state.routeUpReference = routeUpReference;
        state.routeCurves = plan.routeCurves;
        if (state.routeCurves.empty())
            state.routeCurves = buildFallbackRouteCurves(plan.executionGates);
        state.programs = std::move(programs);
        if (!buildContinuousReference(
                state.programs,
                state.continuousSamples,
                state.continuousProgressMeters
            ) ||
            !initializeRuntimeProfile(
                state.continuousSamples,
                state.nominalSpeedProfileMps,
                state.runtimeSpeedProfileMps,
                state.runtimeLongitudinalAccelerationMps2
            ) ||
            !buildCheckpointProgress(
                plan.gates,
                state.continuousSamples,
                state.continuousProgressMeters,
                state.checkpointProgressMeters
            ))
        {
            state = {};
            return false;
        }
        if (!buildCurveSampleProgressMap(
                state.routeCurves,
                state.continuousSamples,
                state.continuousProgressMeters,
                state.curveSampleStartProgressMeters,
                state.curveSampleEndProgressMeters
            ))
        {
            state = {};
            return false;
        }

        state.nextCheckpointIndex =
            state.checkpointProgressMeters.size() > 1 ? 1 : 0;
        return true;
    }

    [[nodiscard]] static ShipControlState stabilize(
        const RouteFollowerAgentState& agent,
        LocalFlightControlLaw law,
        const ShipParams& params,
        PredictivePilot::State& pilotState,
        double deltaSeconds
    ) noexcept
    {
        PredictivePilot::Request request;
        request.law = law;
        request.desiredVelocityMapMps = glm::dvec3(0.0);
        request.desiredLinearAccelerationMapMps2 = glm::dvec3(0.0);
        request.desiredForwardMap = agent.forwardMap;
        request.desiredUpMap = agent.upMap;
        request.actualVelocityMapMps = agent.velocityMapMetersPerSecond;
        request.forwardMap = agent.forwardMap;
        request.rightMap = agent.rightMap;
        request.upMap = agent.upMap;
        request.pitchRateRadPerSec = agent.pitchRateRadPerSec;
        request.yawRateRadPerSec = agent.yawRateRadPerSec;
        request.rollRateRadPerSec = agent.rollRateRadPerSec;
        request.stopRequested = true;
        request.deltaSeconds = deltaSeconds;
        return PredictivePilot::make(request, params, pilotState);
    }

    [[nodiscard]] static Output update(
        State& state,
        const RouteFollowerAgentState& agent,
        LocalFlightControlLaw law,
        const ShipParams& params,
        double universeTimeSeconds,
        double deltaSeconds
    ) noexcept
    {
        (void)universeTimeSeconds;

        Output out;
        if (!state.active ||
            state.programs.empty() ||
            state.continuousSamples.size() < 2 ||
            state.continuousProgressMeters.size() !=
                state.continuousSamples.size())
        {
            return out;
        }

        auto continuous = sampleContinuousReference(
            state.continuousSamples,
            state.continuousProgressMeters,
            agent.positionMapMeters,
            state.currentContinuousSegment
        );
        if (!continuous.valid)
            return out;

        state.currentContinuousSegment =
            std::max(
                state.currentContinuousSegment,
                continuous.lowerSampleIndex
            );

        const double rawForwardAuthority =
            std::max(
                0.0,
                game::ship::forwardMainAccelerationLimitMps2(params)
            );
        const double rawReverseAuthority =
            std::max(
                0.0,
                game::ship::reverseMainAccelerationLimitMps2(params)
            );
        const double rawLateralAuthority =
            law == LocalFlightControlLaw::Assisted
                ? std::max(
                    0.0,
                    game::ship::
                        assistedLateralStabilizationAccelerationLimitMps2(
                            params
                        )
                  )
                : std::max(
                    0.0,
                    game::ship::manoeuvreAccelerationLimitMps2(params)
                  );
        const double rawBrakingAuthority =
            law == LocalFlightControlLaw::Assisted
                ? std::max(rawReverseAuthority, rawLateralAuthority)
                : rawForwardAuthority;
        const double feedbackReserve =
            DockingAutomaticRecoveryPolicy::linearFeedbackReserveMps2(
                rawForwardAuthority,
                rawBrakingAuthority,
                rawLateralAuthority
            );
        const double forwardAuthority =
            std::max(
                0.1,
                (rawForwardAuthority - feedbackReserve) * 0.90
            );
        const double brakingAuthority =
            std::max(
                0.1,
                (rawBrakingAuthority - feedbackReserve) * 0.90
            );

        while (state.nextCheckpointIndex <
                   state.checkpointProgressMeters.size() &&
               continuous.spatialProgressMeters + 1.0e-9 >=
                   state.checkpointProgressMeters[
                       state.nextCheckpointIndex
                   ])
        {
            recomputeRuntimeSpeedSuffix(
                    state.nominalSpeedProfileMps,
                    state.continuousProgressMeters,
                    state.currentContinuousSegment,
                    glm::length(agent.velocityMapMetersPerSecond),
                    forwardAuthority,
                    brakingAuthority,
                    state.runtimeSpeedProfileMps,
                    state.runtimeLongitudinalAccelerationMps2
                );
            ++state.speedProfileRevision;
            ++state.nextCheckpointIndex;
        }

        applyRuntimeProfile(
            state.runtimeSpeedProfileMps,
            state.runtimeLongitudinalAccelerationMps2,
            continuous
        );

        // Map the continuous segment back to storage metadata only. Pages are
        // no longer allowed to select, reset or invalidate the execution
        // reference.
        constexpr std::size_t PageStride =
            AcceptedManeuverProgram::kMaxSamples - 1;
        state.currentPage = std::min(
            state.currentContinuousSegment / PageStride,
            state.programs.size() - 1
        );
        state.currentSpatialSegment = std::min(
            state.currentContinuousSegment -
                state.currentPage * PageStride,
            static_cast<std::size_t>(
                state.programs[state.currentPage].sampleCount - 2
            )
        );
        const auto& contract = state.programs[state.currentPage];

        ManeuverTrackingController::AgentState trackingAgent;
        trackingAgent.positionMapMeters = agent.positionMapMeters;
        trackingAgent.velocityMapMetersPerSecond =
            agent.velocityMapMetersPerSecond;
        trackingAgent.forwardMap = agent.forwardMap;
        trackingAgent.rightMap = agent.rightMap;
        trackingAgent.upMap = agent.upMap;
        trackingAgent.pitchRateRadPerSec = agent.pitchRateRadPerSec;
        trackingAgent.yawRateRadPerSec = agent.yawRateRadPerSec;
        trackingAgent.rollRateRadPerSec = agent.rollRateRadPerSec;

        ManeuverTrackingController::Policy trackingPolicy;
        trackingPolicy.positionGainPerSecond2 =
            state.followerPolicy.positionGainPerSecond2;
        trackingPolicy.velocityGainPerSecond =
            state.followerPolicy.velocityGainPerSecond;
        trackingPolicy.attitudeGainPerSecond2 =
            state.followerPolicy.attitudeGainPerSecond2;
        trackingPolicy.angularVelocityGainPerSecond =
            state.followerPolicy.angularVelocityGainPerSecond;

        const auto tracking =
            ManeuverTrackingController::track(
                contract,
                continuous.reference,
                trackingAgent,
                trackingPolicy
            );

        double targetSpeed =
            glm::length(
                continuous.reference.velocityMapMetersPerSecond
            );

        const double continuousTotalProgress =
            state.continuousProgressMeters.back();
        const double routeTotalProgress =
            state.routeCurves.empty()
                ? continuousTotalProgress
                : state.routeCurves.back().endProgressMeters;
        const double routeProgressMeters =
            mapSampleProgressToRouteProgress(
                state.routeCurves,
                state.curveSampleStartProgressMeters,
                state.curveSampleEndProgressMeters,
                continuous.spatialProgressMeters
            );

        auto curveNow =
            sampleRouteCurveAtProgress(
                state.routeCurves,
                routeProgressMeters
            );

        const glm::dvec3 sampledFallbackForward =
            normalizedOr(
                continuous.reference.forwardMap,
                agent.forwardMap
            );
        const glm::dvec3 nominalForward =
            curveNow.valid
                ? curveNow.tangentMap
                : sampledFallbackForward;
        if (curveNow.valid && curveNow.maxSpeedMps > 0.0)
            targetSpeed =
                std::min(targetSpeed, curveNow.maxSpeedMps);

        const double courseResponseSeconds =
            std::max(
                deltaSeconds,
                state.pilotState.assistedCourseResponseSeconds
            );
        const double angularRateLimit =
            game::ship::maximumAngularSpeedRadPerSec(params);

        double turnSpeedCeilingMps = 0.0;
        double distanceToTurnMeters = 0.0;
        RouteCurveDiagnostic speedGuide = curveNow;

        if (curveNow.valid &&
            curveNow.curvaturePerMeter <= 1.0e-12 &&
            curveNow.curveIndex + 1 < state.routeCurves.size())
        {
            const auto& nextCurve =
                state.routeCurves[curveNow.curveIndex + 1];
            auto upcoming =
                sampleRouteCurveAtProgress(
                    state.routeCurves,
                    std::min(
                        nextCurve.endProgressMeters,
                        nextCurve.startProgressMeters + 1.0e-6
                    )
                );
            if (upcoming.valid &&
                upcoming.curvaturePerMeter > 1.0e-12)
            {
                speedGuide = upcoming;
                distanceToTurnMeters =
                    std::max(
                        0.0,
                        nextCurve.startProgressMeters -
                            routeProgressMeters
                    );
            }
        }

        if (speedGuide.valid &&
            speedGuide.curvaturePerMeter > 1.0e-12 &&
            courseResponseSeconds > 1.0e-9)
        {
            const double kappa = speedGuide.curvaturePerMeter;
            const double radius = 1.0 / kappa;

            // During one Assisted course-response time, the authored curve
            // bends away from its tangent by the sagitta below. Bound that
            // one-response deviation by the already-authoritative corridor
            // tracking tolerance. This yields a speed limit from route
            // geometry and measured course lag, not from a tuned corner-speed
            // percentage.
            const double corridorTolerance =
                std::max(
                    0.0,
                    state.trackingPositionToleranceMeters
                );
            const double cosineArgument =
                std::clamp(
                    1.0 - corridorTolerance / radius,
                    -1.0,
                    1.0
                );
            const double corridorAngle =
                std::acos(cosineArgument);
            const double speedByCourseLag =
                corridorAngle > 1.0e-9
                    ? corridorAngle /
                        (kappa * courseResponseSeconds)
                    : 0.0;
            const double speedByAngularRate =
                angularRateLimit > 1.0e-9
                    ? angularRateLimit / kappa
                    : std::numeric_limits<double>::infinity();

            turnSpeedCeilingMps =
                std::min(
                    speedByCourseLag,
                    speedByAngularRate
                );
            if (speedGuide.maxSpeedMps > 0.0)
                turnSpeedCeilingMps =
                    std::min(
                        turnSpeedCeilingMps,
                        speedGuide.maxSpeedMps
                    );

            if (curveNow.valid &&
                curveNow.curvaturePerMeter > 1.0e-12)
            {
                targetSpeed =
                    std::min(targetSpeed, turnSpeedCeilingMps);
            }
            else if (distanceToTurnMeters > 0.0 &&
                brakingAuthority > 1.0e-9)
            {
                const double longitudinalResponseGain =
                    std::max(
                        1.0e-6,
                        static_cast<double>(params.throttleAccel) > 0.0
                            ? static_cast<double>(params.throttleAccel)
                            : static_cast<double>(
                                params.fallbackThrottleResponsePerSecond
                              )
                    );
                const double brakingResponseSeconds =
                    1.0 / longitudinalResponseGain + deltaSeconds;

                // Solve:
                // d = (v^2-v_turn^2)/(2a) + t_response*(v-v_turn)
                // for the largest safe speed v at the current point.
                const double base =
                    turnSpeedCeilingMps +
                    brakingAuthority * brakingResponseSeconds;
                const double safeApproachSpeed =
                    -brakingAuthority * brakingResponseSeconds +
                    std::sqrt(
                        base * base +
                        2.0 * brakingAuthority * distanceToTurnMeters
                    );
                targetSpeed =
                    std::min(targetSpeed, safeApproachSpeed);
            }
        }

        const glm::dvec3 referencePosition =
            curveNow.valid
                ? curveNow.positionMapMeters
                : continuous.reference.positionMapMeters;
        const glm::dvec3 referenceTangent =
            nominalForward;
        const glm::dvec3 positionError =
            agent.positionMapMeters - referencePosition;
        const glm::dvec3 crossPositionError =
            positionError -
            referenceTangent *
                glm::dot(positionError, referenceTangent);
        const double crossTrackErrorMeters =
            glm::length(crossPositionError);

        // Assisted course dynamics are first-order: velocity/course follows
        // the hull nose with measured lag tau. Geometry itself remains the
        // Planner curve. For a curve with curvature kappa, the desired course
        // rotates at omega = v*kappa, so the required steady nose phase lead is
        //
        //      delta ~= tau * v * kappa.
        //
        // This is fundamentally different from aiming at a point v*tau ahead:
        // the latter moves the path; this rotates the hull around the SAME
        // local curve.
        const double actualSpeed =
            glm::length(agent.velocityMapMetersPerSecond);
        constexpr double CenteringBandFraction = 0.10;
        const double centeringDeadbandMeters =
            state.trackingPositionToleranceMeters *
            CenteringBandFraction;

        glm::dvec3 desiredCourse = referenceTangent;
        double crossTrackCorrectionAngleRad = 0.0;
        glm::dvec3 crossTrackCorrectionAxisMap {0.0};
        if (actualSpeed > std::max(
                1.0e-6,
                static_cast<double>(params.stopSpeedEpsilonMps)) &&
            crossTrackErrorMeters > 1.0e-9)
        {
            const glm::dvec3 towardCenter =
                -crossPositionError / crossTrackErrorMeters;
            const glm::dvec3 crossVelocity =
                agent.velocityMapMetersPerSecond -
                referenceTangent *
                    glm::dot(
                        agent.velocityMapMetersPerSecond,
                        referenceTangent
                    );
            const double closingSpeed =
                glm::dot(crossVelocity, towardCenter);

            // Critically damped cross-track recovery:
            //
            //   e'' + 2/tau e' + e/tau^2 = 0
            //
            // with e'=-closingSpeed. The Assisted nose/course law produces
            // lateral acceleration approximately v*sin(delta)/tau, so solve
            // directly for the small course correction angle. This uses BOTH
            // position and lateral velocity: if the craft is already closing
            // too fast, the sign reverses before centerline crossing.
            const double effectiveError =
                std::max(
                    0.0,
                    crossTrackErrorMeters - centeringDeadbandMeters
                );
            const double requiredCenterAcceleration =
                effectiveError /
                    (courseResponseSeconds * courseResponseSeconds) -
                2.0 * closingSpeed / courseResponseSeconds;

            const double correctionArgument =
                std::clamp(
                    requiredCenterAcceleration *
                        courseResponseSeconds /
                        actualSpeed,
                    -1.0,
                    1.0
                );
            crossTrackCorrectionAngleRad =
                std::asin(correctionArgument);

            crossTrackCorrectionAxisMap =
                normalizedOr(
                    glm::cross(referenceTangent, towardCenter),
                    normalizedOr(
                        state.routeUpReference,
                        glm::dvec3(0.0, 1.0, 0.0)
                    )
                );
            desiredCourse =
                rotateAroundAxis(
                    referenceTangent,
                    crossTrackCorrectionAxisMap,
                    crossTrackCorrectionAngleRad
                );
        }

        RouteCurveDiagnostic curvatureGuide = curveNow;
        double curvatureBlend = 1.0;
        double attitudeLeadDistanceMeters = 0.0;

        // Circular/Bezier curvature may start discontinuously after a line.
        // The course itself must remain on the straight until the authored
        // curve begins. Only the HULL attitude may start preparing early, and
        // that preparation distance comes from real angular acceleration.
        if (curveNow.valid &&
            curveNow.curvaturePerMeter <= 1.0e-12 &&
            curveNow.curveIndex + 1 < state.routeCurves.size())
        {
            const auto& nextCurve =
                state.routeCurves[curveNow.curveIndex + 1];
            auto upcoming =
                sampleRouteCurveAtProgress(
                    state.routeCurves,
                    std::min(
                        nextCurve.endProgressMeters,
                        nextCurve.startProgressMeters + 1.0e-6
                    )
                );

            if (upcoming.valid &&
                upcoming.curvaturePerMeter > 1.0e-12)
            {
                const double steadyCourseRate =
                    std::min(
                        actualSpeed,
                        turnSpeedCeilingMps > 0.0
                            ? turnSpeedCeilingMps
                            : actualSpeed
                    ) *
                    upcoming.curvaturePerMeter;
                const double leadArgument =
                    std::clamp(
                        courseResponseSeconds * steadyCourseRate,
                        -1.0,
                        1.0
                    );
                const double leadAngle =
                    std::asin(leadArgument);
                const double angularAuthority =
                    game::ship::
                        angularAccelerationLimitRadPerSec2(params);
                if (leadAngle > 1.0e-9 &&
                    angularAuthority > 1.0e-9)
                {
                    const double targetTurnRate =
                        steadyCourseRate;
                    const double angleCaptureSeconds =
                        2.0 *
                        std::sqrt(
                            leadAngle / angularAuthority
                        );
                    const double rateCaptureSeconds =
                        targetTurnRate / angularAuthority;
                    const double captureSeconds =
                        std::max(
                            angleCaptureSeconds,
                            rateCaptureSeconds
                        );
                    attitudeLeadDistanceMeters =
                        actualSpeed * captureSeconds;
                    const double distanceToCurve =
                        std::max(
                            0.0,
                            nextCurve.startProgressMeters -
                                routeProgressMeters
                        );
                    const double rawBlend =
                        attitudeLeadDistanceMeters > 1.0e-9
                            ? std::clamp(
                                1.0 -
                                distanceToCurve /
                                    attitudeLeadDistanceMeters,
                                0.0,
                                1.0
                              )
                            : 0.0;
                    // Smoothstep gives zero slope at both ends: no stick jerk
                    // when pre-turn preparation starts or when the arc begins.
                    curvatureBlend =
                        rawBlend * rawBlend *
                        (3.0 - 2.0 * rawBlend);
                    curvatureGuide = upcoming;
                }
                else
                {
                    curvatureBlend = 0.0;
                }
            }
            else
            {
                curvatureBlend = 0.0;
            }
        }

        double coursePhaseLeadAngleRad = 0.0;
        glm::dvec3 steeringForward = desiredCourse;
        if (curvatureGuide.valid &&
            curvatureGuide.curvaturePerMeter > 1.0e-12 &&
            glm::length(curvatureGuide.turnNormalMap) > 1.0e-12 &&
            actualSpeed > 1.0e-6)
        {
            const double guideSpeed =
                turnSpeedCeilingMps > 0.0
                    ? std::min(actualSpeed, turnSpeedCeilingMps)
                    : actualSpeed;
            const double leadArgument =
                std::clamp(
                    courseResponseSeconds *
                        guideSpeed *
                        curvatureGuide.curvaturePerMeter,
                    -1.0,
                    1.0
                );
            coursePhaseLeadAngleRad =
                std::asin(leadArgument) *
                curvatureBlend;
            steeringForward =
                rotateAroundAxis(
                    desiredCourse,
                    curvatureGuide.turnNormalMap,
                    coursePhaseLeadAngleRad
                );
        }

        // Prediction remains diagnostic only. It can tell us that current
        // velocity will miss the future curve, but it is never itself a
        // steering target.
        const double diagnosticLeadMeters =
            actualSpeed * courseResponseSeconds;
        const double diagnosticProgress =
            std::min(
                routeTotalProgress,
                routeProgressMeters + diagnosticLeadMeters
            );
        const auto diagnosticFuture =
            sampleRouteCurveAtProgress(
                state.routeCurves,
                diagnosticProgress
            );
        const glm::dvec3 predictedPosition =
            agent.positionMapMeters +
            agent.velocityMapMetersPerSecond *
                courseResponseSeconds;
        const glm::dvec3 diagnosticTangent =
            diagnosticFuture.valid
                ? diagnosticFuture.tangentMap
                : referenceTangent;
        const glm::dvec3 diagnosticPosition =
            diagnosticFuture.valid
                ? diagnosticFuture.positionMapMeters
                : referencePosition;
        const glm::dvec3 predictedError =
            predictedPosition - diagnosticPosition;
        const glm::dvec3 predictedCrossError =
            predictedError -
            diagnosticTangent *
                glm::dot(predictedError, diagnosticTangent);
        const double predictedCrossTrackMeters =
            glm::length(predictedCrossError);

        auto attitudeLead = sampleContinuousReferenceAtProgress(
            state.continuousSamples,
            state.continuousProgressMeters,
            continuous.spatialProgressMeters
        );
        if (!attitudeLead.valid)
            attitudeLead = continuous;

        glm::dvec3 desiredUpSource =
            glm::length(state.routeUpReference) > 1.0e-9
                ? state.routeUpReference
                : attitudeLead.reference.upMap;
        glm::dvec3 desiredUp =
            desiredUpSource -
            steeringForward *
                glm::dot(desiredUpSource, steeringForward);
        desiredUp =
            normalizedOr(
                desiredUp,
                attitudeLead.reference.upMap
            );

        // Dock-bottom alignment is an explicit roll task, not something that
        // should disappear inside the combined SO(3) error while pitch/yaw
        // are busy tracking the route. Measure signed roll error around the
        // current desired nose axis and publish a smooth desired roll rate.
        glm::dvec3 currentUpProjected =
            agent.upMap -
            steeringForward *
                glm::dot(agent.upMap, steeringForward);
        currentUpProjected =
            normalizedOr(currentUpProjected, desiredUp);
        const double signedRollErrorRad =
            std::atan2(
                glm::dot(
                    steeringForward,
                    glm::cross(currentUpProjected, desiredUp)
                ),
                std::clamp(
                    glm::dot(currentUpProjected, desiredUp),
                    -1.0,
                    1.0
                )
            );
        const double angularAuthority =
            game::ship::angularAccelerationLimitRadPerSec2(params);
        const double rollRateLimit =
            std::max(0.0, static_cast<double>(params.maxRollRate));
        const double rollActuatorResponseSeconds =
            angularAuthority > 1.0e-9 && rollRateLimit > 1.0e-9
                ? std::max(
                    deltaSeconds,
                    rollRateLimit / angularAuthority
                  )
                : courseResponseSeconds;
        const double desiredRollRateRadPerSec =
            rollRateLimit > 1.0e-9
                ? std::clamp(
                    signedRollErrorRad / rollActuatorResponseSeconds,
                    -rollRateLimit,
                    rollRateLimit
                  )
                : 0.0;

        const double courseLeadDistanceMeters =
            attitudeLeadDistanceMeters;

        // Terminal stop uses a conservative *measured* braking envelope.
        // Ordinary Assisted speed following has a first-order controller and a
        // slew-limited speed setpoint; BrakeToStop bypasses that setpoint and
        // owns the full reverse-main envelope. Enter BrakeToStop before the
        // ideal v^2/(2a) boundary by subtracting the controller response
        // distance from the usable route length.
        const double measuredBrakingResponse =
            state.pilotState.assistedBrakingResponseMps2;
        const double effectiveBrakingAuthority =
            measuredBrakingResponse > 1.0e-6
                ? std::min(brakingAuthority, measuredBrakingResponse)
                : brakingAuthority;
        const double longitudinalResponseGain =
            std::max(
                1.0e-6,
                static_cast<double>(params.throttleAccel) > 0.0
                    ? static_cast<double>(params.throttleAccel)
                    : static_cast<double>(
                        params.fallbackThrottleResponsePerSecond
                      )
            );
        const double controllerResponseSeconds =
            1.0 / longitudinalResponseGain + deltaSeconds;
        const double remainingRouteMeters =
            std::max(0.0, routeTotalProgress - routeProgressMeters);
        const double requiredTerminalStopDistanceMeters =
            effectiveBrakingAuthority > 1.0e-9
                ? actualSpeed * actualSpeed /
                      (2.0 * effectiveBrakingAuthority) +
                  actualSpeed * controllerResponseSeconds
                : std::numeric_limits<double>::infinity();
        const bool terminalBrakeActive =
            state.holdAtTerminal &&
            requiredTerminalStopDistanceMeters + 1.0e-9 >=
                remainingRouteMeters;

        if (state.holdAtTerminal &&
            effectiveBrakingAuthority > 1.0e-9)
        {
            const double at =
                effectiveBrakingAuthority * controllerResponseSeconds;
            const double terminalSafeSpeed =
                std::max(
                    0.0,
                    -at +
                    std::sqrt(
                        at * at +
                        2.0 *
                            effectiveBrakingAuthority *
                            remainingRouteMeters
                    )
                );
            targetSpeed = std::min(targetSpeed, terminalSafeSpeed);
        }

        const glm::dvec3 desiredVelocity =
            nominalForward * targetSpeed;

        const bool atFinalContinuousSegment =
            continuous.upperSampleIndex + 1 >=
                state.continuousSamples.size();
        const bool terminalStopAuthored =
            !state.nominalSpeedProfileMps.empty() &&
            state.nominalSpeedProfileMps.back() <=
                std::max(
                    1.0e-6,
                    static_cast<double>(params.stopSpeedEpsilonMps)
                );
        const double terminalAttitudeCaptureSpeedMps =
            std::max(
                0.50,
                static_cast<double>(params.stopSpeedEpsilonMps)
            );
        const bool terminalAttitudeHold =
            state.holdAtTerminal &&
            atFinalContinuousSegment &&
            terminalStopAuthored &&
            actualSpeed <= terminalAttitudeCaptureSpeedMps;
        const bool onFinalRoutePrimitive =
            curveNow.valid &&
            curveNow.curveIndex + 1 >= state.routeCurves.size();
        const bool brakeAttitudeLock =
            terminalStopAuthored &&
            onFinalRoutePrimitive &&
            (atFinalContinuousSegment || terminalBrakeActive) &&
            !terminalAttitudeHold;

        PredictivePilot::Request request;
        request.law = law;
        request.desiredVelocityMapMps = desiredVelocity;
        request.desiredLinearAccelerationMapMps2 =
            tracking.status !=
                    ManeuverTrackingController::Status::InvalidInput
                ? tracking.intent.idealLinearAccelerationLocalMps2
                : continuous.reference.
                    linearAccelerationFeedForwardMapMps2;

        glm::dvec3 routeAngularRateMap(0.0);
        if (curvatureGuide.valid &&
            curvatureGuide.curvaturePerMeter > 1.0e-12 &&
            glm::length(curvatureGuide.turnNormalMap) > 1.0e-12)
        {
            const double guideSpeed =
                turnSpeedCeilingMps > 0.0
                    ? std::min(actualSpeed, turnSpeedCeilingMps)
                    : actualSpeed;
            const double targetHullRate =
                guideSpeed *
                curvatureGuide.curvaturePerMeter *
                curvatureBlend;
            routeAngularRateMap =
                curvatureGuide.turnNormalMap * targetHullRate;
        }

        glm::dvec3 captureAngularRateMap(0.0);
        if (std::abs(crossTrackCorrectionAngleRad) > 1.0e-9 &&
            glm::length(crossTrackCorrectionAxisMap) > 1.0e-12)
        {
            const double maxAngularRate =
                game::ship::maximumAngularSpeedRadPerSec(params);
            const double captureRate =
                std::clamp(
                    crossTrackCorrectionAngleRad /
                        courseResponseSeconds,
                    -maxAngularRate,
                    maxAngularRate
                );
            captureAngularRateMap =
                crossTrackCorrectionAxisMap * captureRate;
        }

        const glm::dvec3 rollAngularRateMap =
            steeringForward * desiredRollRateRadPerSec;

        if (terminalAttitudeHold)
        {
            const auto& finalReference =
                state.continuousSamples.back();
            request.desiredForwardMap =
                normalizedOr(
                    finalReference.forwardMap,
                    steeringForward
                );
            request.desiredUpMap = desiredUp;
            request.desiredAngularVelocityMapRadPerSec =
                glm::dvec3(0.0);
            request.desiredAngularAccelerationMapRadPerSec2 =
                glm::dvec3(0.0);
        }
        else if (brakeAttitudeLock)
        {
            // During terminal braking translation owns the maneuver. Keep the
            // hull level in the final corridor and do not start a terminal
            // pose capture while significant forward speed remains. If we
            // crossed the nominal stop plane, continue braking straight; never
            // pitch/yaw back toward the point.
            request.desiredForwardMap = referenceTangent;
            request.desiredUpMap = desiredUp;
            request.desiredAngularVelocityMapRadPerSec =
                rollAngularRateMap;
            request.desiredAngularAccelerationMapRadPerSec2 =
                glm::dvec3(0.0);
        }
        else
        {
            request.desiredForwardMap = steeringForward;
            request.desiredUpMap = desiredUp;

            if (!state.routeCurves.empty())
            {
                // Angular reference has three explicit jobs:
                // 1) follow authored curvature,
                // 2) capture corridor center,
                // 3) align ship bottom/up with dock marking.
                request.desiredAngularVelocityMapRadPerSec =
                    routeAngularRateMap +
                    captureAngularRateMap +
                    rollAngularRateMap;
                request.desiredAngularAccelerationMapRadPerSec2 =
                    glm::dvec3(0.0);
            }
            else
            {
                request.desiredAngularVelocityMapRadPerSec =
                    attitudeLead.reference.angularVelocityMapRadPerSecond;
                request.desiredAngularAccelerationMapRadPerSec2 =
                    attitudeLead.reference.
                        angularAccelerationFeedForwardMapRadPerSec2;
            }
        }

        request.angularTrackingResponseSeconds =
            courseResponseSeconds;
        request.actualVelocityMapMps =
            agent.velocityMapMetersPerSecond;
        request.forwardMap = agent.forwardMap;
        request.rightMap = agent.rightMap;
        request.upMap = agent.upMap;
        request.pitchRateRadPerSec = agent.pitchRateRadPerSec;
        request.yawRateRadPerSec = agent.yawRateRadPerSec;
        request.rollRateRadPerSec = agent.rollRateRadPerSec;
        request.stopRequested =
            terminalStopAuthored &&
            (atFinalContinuousSegment || terminalBrakeActive);
        request.terminalAttitudeHold = terminalAttitudeHold;
        request.deltaSeconds = deltaSeconds;

        out.control =
            PredictivePilot::make(request, params, state.pilotState);

        out.valid = true;
        out.crossTrackErrorMeters = crossTrackErrorMeters;
        out.remainingDistanceMeters =
            std::max(
                0.0,
                state.continuousProgressMeters.back() -
                    continuous.spatialProgressMeters
            );
        out.targetSpeedMps = targetSpeed;
        out.targetLongitudinalAccelerationMps2 =
            glm::dot(
                continuous.reference.linearAccelerationFeedForwardMapMps2,
                referenceTangent
            );
        const glm::dvec3 actualCrossVelocity =
            agent.velocityMapMetersPerSecond -
            referenceTangent *
                glm::dot(
                    agent.velocityMapMetersPerSecond,
                    referenceTangent
                );
        out.crossTrackClosingSpeedMps =
            crossTrackErrorMeters > 1.0e-9
                ? glm::dot(
                    actualCrossVelocity,
                    -crossPositionError / crossTrackErrorMeters
                  )
                : 0.0;
        out.crossTrackCaptureSpeedMps = 0.0;
        out.forwardErrorRad =
            angleBetween(
                agent.forwardMap,
                request.desiredForwardMap
            );
        out.upErrorRad =
            angleBetween(
                agent.upMap,
                request.desiredUpMap
            );
        out.courseLeadDistanceMeters = courseLeadDistanceMeters;
        out.predictedCrossTrackMeters = predictedCrossTrackMeters;
        out.centeringDeadbandMeters = centeringDeadbandMeters;
        out.routeCurvaturePerMeter =
            curveNow.valid ? curveNow.curvaturePerMeter : 0.0;
        out.routeRadiusMeters =
            out.routeCurvaturePerMeter > 1.0e-12
                ? 1.0 / out.routeCurvaturePerMeter
                : 0.0;
        out.routeCurveIndex =
            curveNow.valid ? curveNow.curveIndex : 0;
        out.exactRemainingRouteMeters =
            std::max(
                0.0,
                routeTotalProgress - routeProgressMeters
            );
        out.desiredCourseAngularRateRadPerSec =
            glm::length(
                request.desiredAngularVelocityMapRadPerSec
            );
        out.actualAngularRateRadPerSec =
            std::sqrt(
                agent.pitchRateRadPerSec * agent.pitchRateRadPerSec +
                agent.yawRateRadPerSec * agent.yawRateRadPerSec +
                agent.rollRateRadPerSec * agent.rollRateRadPerSec
            );
        out.coursePhaseLeadAngleRad = coursePhaseLeadAngleRad;
        out.courseResponseSeconds = courseResponseSeconds;
        out.effectiveBrakingAuthorityMps2 =
            effectiveBrakingAuthority;
        out.requiredTerminalStopDistanceMeters =
            requiredTerminalStopDistanceMeters;
        out.turnSpeedCeilingMps = turnSpeedCeilingMps;
        out.distanceToTurnMeters = distanceToTurnMeters;
        out.crossTrackCorrectionAngleRad =
            crossTrackCorrectionAngleRad;
        out.desiredCaptureAngularRateRadPerSec =
            glm::length(captureAngularRateMap);
        out.signedRollErrorRad = signedRollErrorRad;
        out.desiredRollRateRadPerSec =
            desiredRollRateRadPerSec;
        out.terminalBrakeActive = terminalBrakeActive;
        out.terminalAttitudeCaptureActive = terminalAttitudeHold;
        out.brakeAttitudeLockActive = brakeAttitudeLock;
        out.pageIndex = state.currentPage;
        out.segmentIndex = state.currentSpatialSegment;
        out.checkpointIndex =
            state.nextCheckpointIndex > 0
                ? state.nextCheckpointIndex - 1
                : 0;
        out.speedProfileRevision = state.speedProfileRevision;

        const auto& finalReference =
            state.continuousSamples.back();
        const auto& terminal =
            state.programs.back().terminalTolerance;
        const double terminalPositionError =
            glm::length(
                finalReference.positionMapMeters -
                agent.positionMapMeters
            );
        const double terminalVelocityError =
            glm::length(
                finalReference.velocityMapMetersPerSecond -
                agent.velocityMapMetersPerSecond
            );
        const double terminalForwardError =
            angleBetween(
                agent.forwardMap,
                finalReference.forwardMap
            );
        const double terminalAngularRate =
            std::sqrt(
                agent.pitchRateRadPerSec * agent.pitchRateRadPerSec +
                agent.yawRateRadPerSec * agent.yawRateRadPerSec +
                agent.rollRateRadPerSec * agent.rollRateRadPerSec
            );

        if (state.holdAtTerminal &&
            atFinalContinuousSegment &&
            terminalStopAuthored)
        {
            out.terminalHold = true;
        }
        else if (atFinalContinuousSegment &&
            terminalPositionError <= terminal.positionMeters &&
            terminalVelocityError <= terminal.linearVelocityMps &&
            terminalForwardError <= terminal.forwardAngleRad &&
            terminalAngularRate <= terminal.angularVelocityRadPerSec)
        {
            out.complete = true;
            state.active = false;
        }

        return out;
    }


private:
    struct ContinuousReferenceDiagnostic
    {
        bool valid = false;
        AcceptedManeuverProgram::ReferenceSample reference {};
        std::size_t lowerSampleIndex = 0;
        std::size_t upperSampleIndex = 0;
        double interpolation01 = 0.0;
        double spatialProgressMeters = 0.0;
    };

    [[nodiscard]] static bool buildContinuousReference(
        const std::vector<AcceptedManeuverProgram>& programs,
        std::vector<AcceptedManeuverProgram::ReferenceSample>& samples,
        std::vector<double>& progress
    )
    {
        samples.clear();
        progress.clear();

        for (std::size_t pageIndex = 0;
             pageIndex < programs.size();
             ++pageIndex)
        {
            const auto& page = programs[pageIndex];
            if (!page.valid || page.sampleCount < 2)
                return false;

            const std::size_t first =
                pageIndex == 0 ? 0 : 1;
            for (std::size_t i = first;
                 i < page.sampleCount;
                 ++i)
            {
                samples.push_back(page.samples[i]);
            }
        }

        if (samples.size() < 2)
            return false;

        progress.resize(samples.size(), 0.0);
        for (std::size_t i = 1; i < samples.size(); ++i)
        {
            const double ds = glm::length(
                samples[i].positionMapMeters -
                samples[i - 1].positionMapMeters
            );
            if (!std::isfinite(ds) || ds <= 1.0e-9)
                return false;
            progress[i] = progress[i - 1] + ds;
        }
        return true;
    }

    [[nodiscard]] static ContinuousReferenceDiagnostic
    sampleContinuousReference(
        const std::vector<AcceptedManeuverProgram::ReferenceSample>& samples,
        const std::vector<double>& progress,
        const glm::dvec3& positionMapMeters,
        std::size_t minimumSegmentIndex
    ) noexcept
    {
        ContinuousReferenceDiagnostic out;
        if (samples.size() < 2 ||
            progress.size() != samples.size())
        {
            return out;
        }

        std::size_t segment = std::min(
            minimumSegmentIndex,
            samples.size() - 2
        );

        // Advance strictly along the already accepted centerline. Crossing
        // the plane beyond a sample advances to the next segment; nearest
        // global-point search is deliberately avoided so a route that passes
        // near itself cannot jump to a later branch.
        for (;;)
        {
            const glm::dvec3 a =
                samples[segment].positionMapMeters;
            const glm::dvec3 b =
                samples[segment + 1].positionMapMeters;
            const glm::dvec3 delta = b - a;
            const double length2 = glm::dot(delta, delta);
            if (!(std::isfinite(length2) && length2 > 1.0e-18))
                return out;

            const double raw =
                glm::dot(positionMapMeters - a, delta) / length2;

            if (raw > 1.0 &&
                segment + 2 < samples.size())
            {
                ++segment;
                continue;
            }

            const double u = std::clamp(raw, 0.0, 1.0);
            const auto& lower = samples[segment];
            const auto& upper = samples[segment + 1];

            out.valid = true;
            out.lowerSampleIndex = segment;
            out.upperSampleIndex = segment + 1;
            out.interpolation01 = u;
            out.spatialProgressMeters =
                progress[segment] +
                (progress[segment + 1] - progress[segment]) * u;

            auto& ref = out.reference;
            ref.timeOffsetSeconds =
                lower.timeOffsetSeconds * (1.0 - u) +
                upper.timeOffsetSeconds * u;
            ref.positionMapMeters =
                glm::mix(
                    lower.positionMapMeters,
                    upper.positionMapMeters,
                    u
                );
            ref.velocityMapMetersPerSecond =
                glm::mix(
                    lower.velocityMapMetersPerSecond,
                    upper.velocityMapMetersPerSecond,
                    u
                );
            ref.linearAccelerationFeedForwardMapMps2 =
                glm::mix(
                    lower.linearAccelerationFeedForwardMapMps2,
                    upper.linearAccelerationFeedForwardMapMps2,
                    u
                );

            ref.forwardMap = normalizedOr(
                glm::mix(lower.forwardMap, upper.forwardMap, u),
                lower.forwardMap
            );

            glm::dvec3 up = glm::mix(
                lower.upMap,
                upper.upMap,
                u
            );
            up -= ref.forwardMap * glm::dot(up, ref.forwardMap);
            ref.upMap = normalizedOr(up, lower.upMap);
            ref.rightMap = normalizedOr(
                glm::cross(ref.forwardMap, ref.upMap),
                lower.rightMap
            );
            ref.upMap = normalizedOr(
                glm::cross(ref.rightMap, ref.forwardMap),
                ref.upMap
            );

            ref.angularVelocityMapRadPerSecond =
                glm::mix(
                    lower.angularVelocityMapRadPerSecond,
                    upper.angularVelocityMapRadPerSecond,
                    u
                );
            ref.angularAccelerationFeedForwardMapRadPerSec2 =
                glm::mix(
                    lower.angularAccelerationFeedForwardMapRadPerSec2,
                    upper.angularAccelerationFeedForwardMapRadPerSec2,
                    u
                );
            return out;
        }
    }

    [[nodiscard]] static bool initializeRuntimeProfile(
        const std::vector<AcceptedManeuverProgram::ReferenceSample>& samples,
        std::vector<double>& nominalSpeeds,
        std::vector<double>& runtimeSpeeds,
        std::vector<double>& runtimeAccelerations
    )
    {
        if (samples.size() < 2)
            return false;

        nominalSpeeds.resize(samples.size(), 0.0);
        runtimeSpeeds.resize(samples.size(), 0.0);
        runtimeAccelerations.resize(samples.size(), 0.0);

        for (std::size_t i = 0; i < samples.size(); ++i)
        {
            const double speed =
                glm::length(samples[i].velocityMapMetersPerSecond);
            if (!std::isfinite(speed) || speed < 0.0)
                return false;
            nominalSpeeds[i] = speed;
            runtimeSpeeds[i] = speed;
        }

        // A speed sample is a boundary condition. Derive the acceleration
        // needed over each non-zero spatial interval from v^2 = v0^2 + 2*a*ds
        // instead of assuming that a zero-speed checkpoint means a=0.
        for (std::size_t i = 0; i + 1 < samples.size(); ++i)
        {
            const double ds = glm::length(
                samples[i + 1].positionMapMeters -
                samples[i].positionMapMeters
            );
            if (!(std::isfinite(ds) && ds > 1.0e-9))
                return false;

            runtimeAccelerations[i] =
                (runtimeSpeeds[i + 1] * runtimeSpeeds[i + 1] -
                 runtimeSpeeds[i] * runtimeSpeeds[i]) /
                (2.0 * ds);
        }
        runtimeAccelerations.back() =
            runtimeAccelerations[runtimeAccelerations.size() - 2];

        return true;
    }

    [[nodiscard]] static bool buildCheckpointProgress(
        const std::vector<planner::RouteGate>& gates,
        const std::vector<AcceptedManeuverProgram::ReferenceSample>& samples,
        const std::vector<double>& progress,
        std::vector<double>& checkpoints
    )
    {
        checkpoints.clear();
        if (gates.empty() ||
            samples.size() < 2 ||
            progress.size() != samples.size())
        {
            return false;
        }

        std::size_t minimumSegment = 0;
        for (const auto& gate : gates)
        {
            double bestDistance2 =
                std::numeric_limits<double>::infinity();
            double bestProgress = progress[minimumSegment];
            std::size_t bestSegment = minimumSegment;

            for (std::size_t segment = minimumSegment;
                 segment + 1 < samples.size();
                 ++segment)
            {
                const glm::dvec3 a =
                    samples[segment].positionMapMeters;
                const glm::dvec3 b =
                    samples[segment + 1].positionMapMeters;
                const glm::dvec3 delta = b - a;
                const double length2 = glm::dot(delta, delta);
                if (!(std::isfinite(length2) && length2 > 1.0e-18))
                    continue;

                const double u = std::clamp(
                    glm::dot(gate.positionMeters - a, delta) / length2,
                    0.0,
                    1.0
                );
                const glm::dvec3 projected = a + delta * u;
                const glm::dvec3 error =
                    gate.positionMeters - projected;
                const double distance2 = glm::dot(error, error);

                if (distance2 < bestDistance2)
                {
                    bestDistance2 = distance2;
                    bestSegment = segment;
                    bestProgress =
                        progress[segment] +
                        (progress[segment + 1] - progress[segment]) * u;
                }
            }

            if (!std::isfinite(bestDistance2) ||
                !std::isfinite(bestProgress))
            {
                return false;
            }

            if (!checkpoints.empty())
                bestProgress = std::max(checkpoints.back(), bestProgress);

            checkpoints.push_back(bestProgress);
            minimumSegment = bestSegment;
        }

        return !checkpoints.empty();
    }

    static void recomputeRuntimeSpeedSuffix(
        const std::vector<double>& nominalSpeeds,
        const std::vector<double>& progress,
        std::size_t anchorIndex,
        double actualSpeedMps,
        double acceleratingMps2,
        double brakingMps2,
        std::vector<double>& runtimeSpeeds,
        std::vector<double>& runtimeAccelerations
    ) noexcept
    {
        if (nominalSpeeds.size() < 2 ||
            progress.size() != nominalSpeeds.size() ||
            runtimeSpeeds.size() != nominalSpeeds.size() ||
            runtimeAccelerations.size() != nominalSpeeds.size())
        {
            return;
        }

        anchorIndex = std::min(anchorIndex, nominalSpeeds.size() - 1);

        for (std::size_t i = anchorIndex;
             i < nominalSpeeds.size();
             ++i)
        {
            runtimeSpeeds[i] = nominalSpeeds[i];
        }

        // Forward reachability from the measured checkpoint speed. If the
        // craft arrived slower than nominal, later targets are lowered until
        // main-engine acceleration can physically catch the nominal profile.
        double reachableFromActual =
            std::max(0.0, std::isfinite(actualSpeedMps) ? actualSpeedMps : 0.0);
        for (std::size_t i = anchorIndex + 1;
             i < runtimeSpeeds.size();
             ++i)
        {
            const double ds = progress[i] - progress[i - 1];
            if (!(std::isfinite(ds) && ds >= 0.0))
                return;

            if (acceleratingMps2 > 1.0e-9)
            {
                reachableFromActual =
                    std::sqrt(
                        std::max(
                            0.0,
                            reachableFromActual * reachableFromActual +
                            2.0 * acceleratingMps2 * ds
                        )
                    );
                runtimeSpeeds[i] =
                    std::min(runtimeSpeeds[i], reachableFromActual);
            }
        }

        // Backward braking feasibility preserves every future nominal speed
        // restriction and propagates it toward the checkpoint.
        if (brakingMps2 > 1.0e-9)
        {
            for (std::size_t i = runtimeSpeeds.size() - 1;
                 i > anchorIndex;
                 --i)
            {
                const double ds = progress[i] - progress[i - 1];
                if (!(std::isfinite(ds) && ds >= 0.0))
                    return;

                const double reachable =
                    std::sqrt(
                        std::max(
                            0.0,
                            runtimeSpeeds[i] * runtimeSpeeds[i] +
                            2.0 * brakingMps2 * ds
                        )
                    );
                runtimeSpeeds[i - 1] =
                    std::min(runtimeSpeeds[i - 1], reachable);
            }
        }

        // The measured checkpoint speed is authoritative. If it is above
        // the speed from which a future route limit is reachable, do not
        // invalidate the route: propagate the fastest physically possible
        // braking profile forward until the authored limits are caught again.
        double physicalSpeed =
            std::max(
                0.0,
                std::isfinite(actualSpeedMps) ? actualSpeedMps : 0.0
            );
        runtimeSpeeds[anchorIndex] = physicalSpeed;
        if (brakingMps2 > 1.0e-9)
        {
            for (std::size_t i = anchorIndex + 1;
                 i < runtimeSpeeds.size();
                 ++i)
            {
                const double ds = progress[i] - progress[i - 1];
                if (!(std::isfinite(ds) && ds >= 0.0))
                    return;

                const double minimumReachable =
                    std::sqrt(
                        std::max(
                            0.0,
                            physicalSpeed * physicalSpeed -
                            2.0 * brakingMps2 * ds
                        )
                    );
                runtimeSpeeds[i] =
                    std::max(runtimeSpeeds[i], minimumReachable);
                physicalSpeed = runtimeSpeeds[i];
            }
        }

        if (!nominalSpeeds.empty() &&
            nominalSpeeds.back() <= 1.0e-9)
        {
            runtimeSpeeds.back() = 0.0;
        }

        // Reconstruct longitudinal feed-forward from v^2 relation. Geometry
        // and lateral/angular references remain exactly the accepted program.
        for (std::size_t i = anchorIndex;
             i < runtimeAccelerations.size();
             ++i)
        {
            runtimeAccelerations[i] = 0.0;
        }
        for (std::size_t i = anchorIndex;
             i + 1 < runtimeSpeeds.size();
             ++i)
        {
            const double ds = progress[i + 1] - progress[i];
            if (ds > 1.0e-9)
            {
                runtimeAccelerations[i] =
                    (runtimeSpeeds[i + 1] * runtimeSpeeds[i + 1] -
                     runtimeSpeeds[i] * runtimeSpeeds[i]) /
                    (2.0 * ds);
            }
        }

    }

    static void applyRuntimeProfile(
        const std::vector<double>& runtimeSpeeds,
        const std::vector<double>& runtimeAccelerations,
        ContinuousReferenceDiagnostic& reference
    ) noexcept
    {
        if (!reference.valid ||
            reference.lowerSampleIndex >= runtimeSpeeds.size() ||
            reference.upperSampleIndex >= runtimeSpeeds.size() ||
            runtimeAccelerations.size() != runtimeSpeeds.size())
        {
            return;
        }

        const double u = reference.interpolation01;
        const double speed =
            runtimeSpeeds[reference.lowerSampleIndex] * (1.0 - u) +
            runtimeSpeeds[reference.upperSampleIndex] * u;

        const glm::dvec3 tangent =
            normalizedOr(
                reference.reference.velocityMapMetersPerSecond,
                reference.reference.forwardMap
            );
        reference.reference.velocityMapMetersPerSecond =
            tangent * std::max(0.0, speed);

        const double longitudinalAcceleration =
            runtimeAccelerations[reference.lowerSampleIndex] * (1.0 - u) +
            runtimeAccelerations[reference.upperSampleIndex] * u;
        reference.reference.linearAccelerationFeedForwardMapMps2 =
            tangent * longitudinalAcceleration;
    }

    [[nodiscard]] static glm::dvec3 rotateAroundAxis(
        const glm::dvec3& value,
        const glm::dvec3& axis,
        double angleRad
    ) noexcept
    {
        const glm::dvec3 n =
            normalizedOr(axis, glm::dvec3(0.0, 1.0, 0.0));
        const double c = std::cos(angleRad);
        const double si = std::sin(angleRad);
        return normalizedOr(
            value * c +
            glm::cross(n, value) * si +
            n * glm::dot(n, value) * (1.0 - c),
            value
        );
    }

    [[nodiscard]] static bool buildCurveSampleProgressMap(
        const std::vector<planner::RouteCurveSegment>& curves,
        const std::vector<AcceptedManeuverProgram::ReferenceSample>& samples,
        const std::vector<double>& progress,
        std::vector<double>& starts,
        std::vector<double>& ends
    )
    {
        starts.clear();
        ends.clear();

        if (curves.empty())
            return true;
        if (samples.size() < 2 || progress.size() != samples.size())
            return false;

        starts.reserve(curves.size());
        ends.reserve(curves.size());

        std::size_t searchFrom = 0;
        for (const auto& curve : curves)
        {
            auto nearestProgress =
                [&](const glm::dvec3& point, std::size_t from)
                {
                    std::size_t best = from;
                    double bestDistance =
                        std::numeric_limits<double>::infinity();
                    for (std::size_t i = from; i < samples.size(); ++i)
                    {
                        const glm::dvec3 delta =
                            samples[i].positionMapMeters - point;
                        const double d = glm::dot(delta, delta);
                        if (d < bestDistance)
                        {
                            bestDistance = d;
                            best = i;
                        }
                        if (i > best + 8 && d > bestDistance * 4.0)
                            break;
                    }
                    return best;
                };

            const std::size_t startIndex =
                nearestProgress(curve.startMeters, searchFrom);
            const std::size_t endIndex =
                nearestProgress(
                    curve.endMeters,
                    std::min(startIndex + 1, samples.size() - 1)
                );

            if (endIndex <= startIndex)
                return false;

            starts.push_back(progress[startIndex]);
            ends.push_back(progress[endIndex]);
            searchFrom = endIndex;
        }

        return starts.size() == curves.size() &&
            ends.size() == curves.size();
    }

    [[nodiscard]] static double mapSampleProgressToRouteProgress(
        const std::vector<planner::RouteCurveSegment>& curves,
        const std::vector<double>& starts,
        const std::vector<double>& ends,
        double sampleProgressMeters
    ) noexcept
    {
        if (curves.empty() ||
            starts.size() != curves.size() ||
            ends.size() != curves.size())
        {
            return std::max(0.0, sampleProgressMeters);
        }

        std::size_t index = curves.size() - 1;
        for (std::size_t i = 0; i < curves.size(); ++i)
        {
            if (sampleProgressMeters <= ends[i] + 1.0e-9)
            {
                index = i;
                break;
            }
        }

        const double ds = ends[index] - starts[index];
        const double u =
            ds > 1.0e-12
                ? std::clamp(
                    (sampleProgressMeters - starts[index]) / ds,
                    0.0,
                    1.0
                  )
                : 0.0;
        return
            curves[index].startProgressMeters * (1.0 - u) +
            curves[index].endProgressMeters * u;
    }

    [[nodiscard]] static double mapRouteProgressToSampleProgress(
        const std::vector<planner::RouteCurveSegment>& curves,
        const std::vector<double>& starts,
        const std::vector<double>& ends,
        double routeProgressMeters
    ) noexcept
    {
        if (curves.empty() ||
            starts.size() != curves.size() ||
            ends.size() != curves.size())
        {
            return std::max(0.0, routeProgressMeters);
        }

        std::size_t index = curves.size() - 1;
        for (std::size_t i = 0; i < curves.size(); ++i)
        {
            if (routeProgressMeters <=
                curves[i].endProgressMeters + 1.0e-9)
            {
                index = i;
                break;
            }
        }

        const double dr =
            curves[index].endProgressMeters -
            curves[index].startProgressMeters;
        const double u =
            dr > 1.0e-12
                ? std::clamp(
                    (routeProgressMeters -
                     curves[index].startProgressMeters) / dr,
                    0.0,
                    1.0
                  )
                : 0.0;
        return starts[index] * (1.0 - u) + ends[index] * u;
    }

    struct RouteCurveDiagnostic
    {
        bool valid = false;
        std::size_t curveIndex = 0;
        glm::dvec3 positionMapMeters {0.0};
        glm::dvec3 tangentMap {0.0, 0.0, -1.0};
        glm::dvec3 turnNormalMap {0.0};
        double curvaturePerMeter = 0.0;
        double maxSpeedMps = 0.0;
    };

    [[nodiscard]] static std::vector<planner::RouteCurveSegment>
    buildFallbackRouteCurves(
        const std::vector<planner::RouteGate>& gates
    )
    {
        std::vector<planner::RouteCurveSegment> curves;
        if (gates.size() < 2)
            return curves;

        double progress = 0.0;
        curves.reserve(gates.size() - 1);
        for (std::size_t i = 1; i < gates.size(); ++i)
        {
            const glm::dvec3 delta =
                gates[i].positionMeters -
                gates[i - 1].positionMeters;
            const double length = glm::length(delta);
            if (length <= 1.0e-9)
                continue;

            planner::RouteCurveSegment curve;
            curve.kind = planner::RouteCurveKind::Line;
            curve.startProgressMeters = progress;
            curve.endProgressMeters = progress + length;
            // Compatibility-only fallback has no independent geometric speed
            // ceiling. The accepted trajectory remains authoritative for
            // speed until Planner publishes real routeCurves.
            curve.maxSpeedMps = 0.0;
            curve.startMeters = gates[i - 1].positionMeters;
            curve.endMeters = gates[i].positionMeters;
            curve.startForward = delta / length;
            curve.endForward = curve.startForward;
            curves.push_back(curve);
            progress += length;
        }
        return curves;
    }

    [[nodiscard]] static RouteCurveDiagnostic sampleRouteCurveAtProgress(
        const std::vector<planner::RouteCurveSegment>& curves,
        double routeProgressMeters
    ) noexcept
    {
        RouteCurveDiagnostic out;
        if (curves.empty() || !std::isfinite(routeProgressMeters))
            return out;

        std::size_t index = curves.size() - 1;
        for (std::size_t i = 0; i < curves.size(); ++i)
        {
            if (routeProgressMeters <=
                curves[i].endProgressMeters + 1.0e-9)
            {
                index = i;
                break;
            }
        }

        const auto& curve = curves[index];
        const double parameter =
            curve.parameterAtProgress(routeProgressMeters);
        out.valid = true;
        out.curveIndex = index;
        out.positionMapMeters =
            curve.positionAtParameter(parameter);
        out.tangentMap =
            curve.tangentAtProgress(routeProgressMeters);
        out.curvaturePerMeter =
            curve.curvatureAtProgress(routeProgressMeters);

        if (curve.kind == planner::RouteCurveKind::CircularArc)
        {
            const double normalLength = glm::length(curve.arcNormal);
            if (normalLength > 1.0e-12 &&
                std::abs(curve.arcSweepRadians) > 1.0e-12)
            {
                out.turnNormalMap =
                    curve.arcNormal / normalLength *
                    (curve.arcSweepRadians >= 0.0 ? 1.0 : -1.0);
            }
        }
        else if (curve.kind == planner::RouteCurveKind::CubicBezier)
        {
            const glm::dvec3 d1 =
                curve.derivativeAtParameter(parameter);
            const glm::dvec3 d2 =
                curve.secondDerivativeAtParameter(parameter);
            const glm::dvec3 cross = glm::cross(d1, d2);
            const double crossLength = glm::length(cross);
            if (crossLength > 1.0e-12)
                out.turnNormalMap = cross / crossLength;
        }

        out.maxSpeedMps = curve.maxSpeedMps;
        return out;
    }

    [[nodiscard]] static ContinuousReferenceDiagnostic
    sampleContinuousReferenceAtProgress(
        const std::vector<AcceptedManeuverProgram::ReferenceSample>& samples,
        const std::vector<double>& progress,
        double spatialProgressMeters
    ) noexcept
    {
        ContinuousReferenceDiagnostic out;
        if (samples.size() < 2 ||
            progress.size() != samples.size() ||
            !std::isfinite(spatialProgressMeters))
        {
            return out;
        }

        const double clamped =
            std::clamp(
                spatialProgressMeters,
                progress.front(),
                progress.back()
            );
        auto upperIt =
            std::upper_bound(
                progress.begin(),
                progress.end(),
                clamped
            );
        std::size_t upper =
            upperIt == progress.end()
                ? progress.size() - 1
                : static_cast<std::size_t>(
                    std::distance(progress.begin(), upperIt)
                  );
        if (upper == 0)
            upper = 1;
        const std::size_t lower = upper - 1;

        const double ds = progress[upper] - progress[lower];
        const double u =
            ds > 1.0e-12
                ? std::clamp(
                    (clamped - progress[lower]) / ds,
                    0.0,
                    1.0
                  )
                : 0.0;

        out.valid = true;
        out.lowerSampleIndex = lower;
        out.upperSampleIndex = upper;
        out.interpolation01 = u;
        out.spatialProgressMeters = clamped;

        const auto& a = samples[lower];
        const auto& b = samples[upper];
        auto& ref = out.reference;
        ref.timeOffsetSeconds =
            a.timeOffsetSeconds * (1.0 - u) +
            b.timeOffsetSeconds * u;
        ref.positionMapMeters =
            glm::mix(a.positionMapMeters, b.positionMapMeters, u);
        ref.velocityMapMetersPerSecond =
            glm::mix(
                a.velocityMapMetersPerSecond,
                b.velocityMapMetersPerSecond,
                u
            );
        ref.linearAccelerationFeedForwardMapMps2 =
            glm::mix(
                a.linearAccelerationFeedForwardMapMps2,
                b.linearAccelerationFeedForwardMapMps2,
                u
            );
        ref.forwardMap =
            normalizedOr(
                glm::mix(a.forwardMap, b.forwardMap, u),
                a.forwardMap
            );
        glm::dvec3 up =
            glm::mix(a.upMap, b.upMap, u);
        up -= ref.forwardMap * glm::dot(up, ref.forwardMap);
        ref.upMap = normalizedOr(up, a.upMap);
        ref.rightMap =
            normalizedOr(
                glm::cross(ref.forwardMap, ref.upMap),
                a.rightMap
            );
        ref.upMap =
            normalizedOr(
                glm::cross(ref.rightMap, ref.forwardMap),
                ref.upMap
            );
        ref.angularVelocityMapRadPerSecond =
            glm::mix(
                a.angularVelocityMapRadPerSecond,
                b.angularVelocityMapRadPerSecond,
                u
            );
        ref.angularAccelerationFeedForwardMapRadPerSec2 =
            glm::mix(
                a.angularAccelerationFeedForwardMapRadPerSec2,
                b.angularAccelerationFeedForwardMapRadPerSec2,
                u
            );
        return out;
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
            return 3.1415926535897932384626433832795;
        }
        return std::acos(
            std::clamp(
                glm::dot(a / la, b / lb),
                -1.0,
                1.0
            )
        );
    }

    static glm::dvec3 normalizedOr(
        const glm::dvec3& value,
        const glm::dvec3& fallback
    ) noexcept
    {
        const double length = glm::length(value);
        return std::isfinite(length) && length > 1.0e-9
            ? value / length
            : fallback;
    }

    static void makeBasis(
        const glm::dvec3& requestedForward,
        glm::dvec3& forward,
        glm::dvec3& right,
        glm::dvec3& up
    ) noexcept
    {
        forward = normalizedOr(
            requestedForward,
            glm::dvec3(0.0, 0.0, -1.0)
        );
        glm::dvec3 seed =
            std::abs(forward.y) < 0.90
                ? glm::dvec3(0.0, 1.0, 0.0)
                : glm::dvec3(1.0, 0.0, 0.0);
        right = normalizedOr(
            glm::cross(forward, seed),
            glm::dvec3(1.0, 0.0, 0.0)
        );
        up = normalizedOr(
            glm::cross(right, forward),
            glm::dvec3(0.0, 1.0, 0.0)
        );
    }

    [[nodiscard]] static std::vector<AcceptedManeuverProgram>
    buildPrograms(
        const planner::RoutePlan& plan,
        const RouteFollowerAgentState& initialAgent,
        LocalFlightControlLaw law,
        const ShipParams& params,
        double acceptedAtUniverseTimeSeconds,
        std::uint64_t requestSerial,
        double trackingPositionToleranceMeters,
        const glm::dvec3& routeUpReference
    )
    {
        if (!plan.valid() ||
            plan.executionGates.size() < 2 ||
            !std::isfinite(acceptedAtUniverseTimeSeconds) ||
            requestSerial == 0)
        {
            return {};
        }

        const double forwardAuthority =
            game::ship::forwardMainAccelerationLimitMps2(params);
        const double reverseAuthority =
            game::ship::reverseMainAccelerationLimitMps2(params);
        const double lateralAuthority =
            law == LocalFlightControlLaw::Assisted
                ? game::ship::
                    assistedLateralStabilizationAccelerationLimitMps2(params)
                : game::ship::manoeuvreAccelerationLimitMps2(params);
        const double brakingAuthority =
            law == LocalFlightControlLaw::Assisted
                ? std::max(reverseAuthority, lateralAuthority)
                : forwardAuthority;
        const double feedbackReserve =
            DockingAutomaticRecoveryPolicy::linearFeedbackReserveMps2(
                forwardAuthority,
                brakingAuthority,
                lateralAuthority
            );

        world::navigation::NavigationVehicleProfile vehicle;
        vehicle.collisionRadiusMeters = 0.0;
        vehicle.preferredClearanceMeters = 0.0;
        vehicle.maxSpeedMps =
            std::max(
                0.5,
                game::ship::controlledSpeedLimitMps(params) * 0.90
            );
        vehicle.maxForwardAccelerationMps2 =
            std::max(0.1, (forwardAuthority - feedbackReserve) * 0.90);
        vehicle.maxBrakingAccelerationMps2 =
            std::max(0.1, (brakingAuthority - feedbackReserve) * 0.90);
        vehicle.maxLateralAccelerationMps2 =
            std::max(0.1, (lateralAuthority - feedbackReserve) * 0.90);
        vehicle.maxAngularVelocityRadPerSecond =
            game::ship::maximumAngularSpeedRadPerSec(params);
        vehicle.maxAngularAccelerationRadPerSecond2 =
            std::max(
                0.1,
                game::ship::angularAccelerationLimitRadPerSec2(params) *
                    0.90
            );

        world::navigation::TrajectoryGenerationRequest trajectoryRequest;
        trajectoryRequest.systemId = 0;
        trajectoryRequest.frameId = "client-docking";
        trajectoryRequest.startUniverseTimeSeconds =
            acceptedAtUniverseTimeSeconds;
        trajectoryRequest.universeTimeScale = 1.0;
        trajectoryRequest.pathGeometryAlreadyAuthored = true;
        trajectoryRequest.vehicle = vehicle;
        trajectoryRequest.initialVelocityMps =
            initialAgent.velocityMapMetersPerSecond;
        trajectoryRequest.initialAccelerationMps2 = glm::dvec3(0.0);
        trajectoryRequest.hasInitialOrientation = true;
        trajectoryRequest.initialForward = initialAgent.forwardMap;
        trajectoryRequest.initialUp = initialAgent.upMap;
        trajectoryRequest.hasInitialAngularVelocity = true;
        trajectoryRequest.initialAngularVelocityRadPerSecond =
            glm::dvec3(
                initialAgent.pitchRateRadPerSec,
                initialAgent.yawRateRadPerSec,
                initialAgent.rollRateRadPerSec
            );
        trajectoryRequest.hasRouteUpReference = true;
        trajectoryRequest.routeUpReference =
            glm::length(routeUpReference) > 1.0e-9
                ? normalizedOr(routeUpReference, initialAgent.upMap)
                : initialAgent.upMap;
        trajectoryRequest.hasTerminalVelocity = true;
        trajectoryRequest.terminalVelocityMps = glm::dvec3(0.0);

        trajectoryRequest.pathPointsMeters.reserve(
            plan.executionGates.size()
        );
        trajectoryRequest.pointSpeedConstraints.reserve(
            plan.executionGates.size()
        );

        double sourceProgressMeters = 0.0;
        for (std::size_t i = 0;
             i < plan.executionGates.size();
             ++i)
        {
            const auto& gate = plan.executionGates[i];
            trajectoryRequest.pathPointsMeters.push_back(
                gate.positionMeters
            );

            if (i > 0)
            {
                sourceProgressMeters += glm::length(
                    gate.positionMeters -
                    plan.executionGates[i - 1].positionMeters
                );
            }

            world::navigation::TrajectoryPointSpeedConstraint limit;
            limit.sourcePathProgressMeters = sourceProgressMeters;
            limit.maxSpeedMps = std::min(
                vehicle.maxSpeedMps,
                std::max(0.0, gate.speedMps)
            );
            trajectoryRequest.pointSpeedConstraints.push_back(limit);
        }

        constexpr int MaximumRefinementAttempts = 6;
        for (int attempt = 0;
             attempt < MaximumRefinementAttempts;
             ++attempt)
        {
            const auto trajectory =
                world::navigation::TrajectoryGenerator::generate(
                    trajectoryRequest
                );
            if (!trajectory.ready())
                return {};

            AcceptedManeuverProgramBuilder::Request build;
            build.trajectory = &trajectory.trajectory;
            build.shipPhysics = &params;
            build.controlLaw = law;
            build.referenceMode =
                AcceptedManeuverProgram::ReferenceMode::SpatialCorridor;
            build.objectiveRevision = requestSerial;
            build.firstProgramRevision = 1;
            build.capabilityRevision = requestSerial;
            build.mapRevision = requestSerial;
            build.mapSourceRevision = requestSerial;
            build.spaceRevision = requestSerial;
            build.spaceSourceRevision = requestSerial;
            build.minimumClearanceMeters = 0.0;
            build.hasInitialAngularVelocity = true;
            build.initialAngularVelocityMapRadPerSec =
                trajectoryRequest.initialAngularVelocityRadPerSecond;
            build.policy.trackingPositionErrorMeters =
                std::max(5.0, trackingPositionToleranceMeters);
            build.policy.trackingLinearVelocityErrorMps = 10.0;
            build.policy.trackingForwardAngleErrorRad =
                1.3962634015954636; // 80 deg: recover, do not abort.
            build.policy.trackingAngularVelocityErrorRadPerSec = 2.0;
            build.policy.alongTrackPositionDeadbandMeters = 10.0;
            build.policy.alongTrackSpeedDeadbandMps = 1.0;
            build.policy.linearFeedbackReserveMps2 = feedbackReserve;
            build.policy.angularFeedbackReserveRadPerSec2 =
                std::min(
                    0.5,
                    game::ship::angularAccelerationLimitRadPerSec2(params) *
                        0.15
                );

            auto accepted =
                AcceptedManeuverProgramBuilder::build(build);
            if (accepted.valid && !accepted.pages.empty())
                return accepted.pages;

            if (accepted.feedback.disposition !=
                AcceptedManeuverProgramBuilder::
                    ValidationDisposition::NeedsRefinement)
            {
                return {};
            }

            const double scale = std::clamp(
                accepted.feedback.recommendedScale,
                0.50,
                0.98
            );
            trajectoryRequest.vehicle.maxSpeedMps =
                std::max(
                    0.5,
                    trajectoryRequest.vehicle.maxSpeedMps * scale
                );
            for (auto& limit :
                 trajectoryRequest.pointSpeedConstraints)
            {
                limit.maxSpeedMps =
                    std::max(0.0, limit.maxSpeedMps * scale);
            }
        }

        return {};
    }
};

} // namespace game::navigation::autopilot
