#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <iterator>
#include <type_traits>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/AcceptedManeuverProgram.h"
#include "src/game/navigation/AcceptedManeuverProgramBuilder.h"
#include "src/game/navigation/ManeuverCapabilityAdapters.h"
#include "src/game/navigation/NavigationControlCapability.h"
#include "src/game/navigation/ManeuverTrackingController.h"
#include "src/game/navigation/DockingAutomaticRecoveryPolicy.h"
#include "src/game/navigation/RouteFrameField.h"
#include "src/game/navigation/TwoPointRollGeometry.h"
#include "src/game/navigation/autopilot/CourseCaptureGuidance.h"
#include "src/game/navigation/AssistedConcurrentManeuverBudget.h"
#include "src/game/navigation/autopilot/InCorridorCenteringGuidance.h"
#include "src/game/navigation/autopilot/HullPoseGuidance.h"
#include "src/game/navigation/autopilot/HullTunnelRollGuidance.h"
#include "src/game/navigation/autopilot/AutopilotControlApi.h"
#include "src/game/navigation/autopilot/NewtonianManoeuvreProgram.h"
#include "src/game/navigation/autopilot/NewtonianSpeedTolerancePolicy.h"
#include "src/game/navigation/autopilot/RouteSpeedGuidance.h"
#include "src/game/navigation/planner/RoutePlannerApi.h"
#include "src/game/ship/core/ShipControlState.h"
#include "src/game/ship/core/ShipDynamics.h"
#include "src/game/ship/core/ShipParams.h"
#include "src/game/navigation/autopilot/NewtonianCapabilityWatch.h"
#include "src/world/navigation/TrajectoryGenerator.h"
#include "src/world/navigation/NavigationVehicleProfile.h"

namespace game::navigation::autopilot
{

struct AutopilotTrackingPolicy
{
    double positionGainPerSecond2 = 0.50;
    double velocityGainPerSecond = 1.00;
    double attitudeGainPerSecond2 = 2.00;
    double angularVelocityGainPerSecond = 3.00;
};

struct AutopilotAgentState
{
    glm::dvec3 positionMapMeters {0.0};
    glm::dvec3 velocityMapMetersPerSecond {0.0};
    glm::dvec3 forwardMap {0.0, 0.0, -1.0};
    glm::dvec3 rightMap {1.0, 0.0, 0.0};
    glm::dvec3 upMap {0.0, 1.0, 0.0};
    double pitchRateRadPerSec = 0.0;
    double yawRateRadPerSec = 0.0;
    double rollRateRadPerSec = 0.0;
};

// Client-side virtual pilot.
//
// It has no network/server/task semantics.  Its only executable product is
// ShipControlState -- the same control surface used by the human input mapper.
// Route geometry stays entirely on the client.
template <LocalFlightControlLaw Law, typename PilotController>
class DedicatedClientRouteAutopilot final
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
        // Newtonian: a speed deviation beyond the tolerance band is being
        // corrected (NewtonianSpeedTolerancePolicy hysteresis).
        bool speedCorrectionActive = false;
        bool holdAtTerminal = false;
        double trackingPositionToleranceMeters = 0.0;

        // Authoritative spatial geometry. Accepted maneuver samples still own
        // timing/speed/attitude feed-forward, but they no longer define the
        // route shape when Planner supplied parametric curves.
        std::vector<planner::RouteCurveSegment> routeCurves;
        std::vector<double> curveSampleStartProgressMeters;
        std::vector<double> curveSampleEndProgressMeters;
        glm::dvec3 routeUpReference {0.0};

        // Same canonical frame field used by tunnel presentation. The hull
        // must align to this field, never to an independently reconstructed
        // "up" convention.
        std::vector<RouteFrameField::Sample> routeFrameField;
        // Fixed-attitude stretches (Step 3), route progress.
        std::vector<planner::RouteNarrowZone> narrowZones;
        double liveTunnelRollPhaseRad = 0.0;
        double previousLiveTunnelRollPhaseRad = 0.0;
        double liveTunnelRollRateRadPerSec = 0.0;
        bool hasLiveTunnelRollPhase = false;

        AutopilotTrackingPolicy followerPolicy {};
        PilotController pilotController {};
        std::vector<AcceptedManeuverProgram> programs;
        // Newtonian: the capability the active program was accepted with.
        NewtonianCapabilityWatch capabilityWatch {};
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
        // Authoritative navigation-direction error. While moving this is
        // angle(actual velocity direction, desired route course).
        double forwardErrorRad = 0.0;
        double courseErrorRad = 0.0;

        // Hull nose is actuator state only; keep it separate from route
        // tracking so Assisted and Newtonian share one navigation metric.
        double hullForwardErrorRad = 0.0;
        double upErrorRad = 0.0;
        double courseLeadDistanceMeters = 0.0;
        double predictedCrossTrackMeters = 0.0;
        double centeringDeadbandMeters = 0.0;
        double routeCurvaturePerMeter = 0.0;
        double routeRadiusMeters = 0.0;
        double exactRemainingRouteMeters = 0.0;
        double desiredCourseAngularRateRadPerSec = 0.0;
        double actualCourseAngularRateRadPerSec = 0.0;
        double actualAngularRateRadPerSec = 0.0;
        double coursePhaseLeadAngleRad = 0.0;
        double courseResponseSeconds = 0.0;
        double effectiveBrakingAuthorityMps2 = 0.0;
        double requiredTerminalStopDistanceMeters = 0.0;
        double turnSpeedCeilingMps = 0.0;
        double distanceToTurnMeters = 0.0;
        double requiredTurnSlowdownDistanceMeters = 0.0;
        double turnSpeedSetpointSlewSeconds = 0.0;
        double crossTrackCorrectionAngleRad = 0.0;
        double desiredCaptureAngularRateRadPerSec = 0.0;
        bool courseCaptureActive = false;
        double captureMeetingRouteProgressMeters = 0.0;
        double captureMeetingJoinAngleRad = 0.0;
        double signedRollErrorRad = 0.0;
        double desiredRollRateRadPerSec = 0.0;
        double liveTunnelRollPhaseRad = 0.0;
        double liveTunnelRollRateRadPerSec = 0.0;
        bool terminalBrakeActive = false;
        bool terminalAttitudeCaptureActive = false;
        bool brakeAttitudeLockActive = false;
        std::size_t routeCurveIndex = 0;
        std::size_t pageIndex = 0;
        std::size_t segmentIndex = 0;
        std::size_t checkpointIndex = 0;
        std::uint64_t speedProfileRevision = 0;
        bool terminalHold = false;
        // Why the output is invalid when the follower itself hands the route
        // back (static text; null otherwise).
        const char* invalidReason = nullptr;
    };

    static void stop(State& state) noexcept
    {
        state = {};
    }

    static void setLiveTunnelRollPhase(
        State& state,
        double phaseRad,
        double deltaSeconds
    ) noexcept
    {
        const double phase =
            std::isfinite(phaseRad) ? phaseRad : 0.0;

        if (state.hasLiveTunnelRollPhase &&
            std::isfinite(deltaSeconds) &&
            deltaSeconds > 1.0e-9)
        {
            const double delta =
                TwoPointRollGeometry::shortestPhaseDelta(
                    state.previousLiveTunnelRollPhaseRad,
                    phase
                );
            state.liveTunnelRollRateRadPerSec =
                delta / deltaSeconds;
        }
        else
        {
            state.liveTunnelRollRateRadPerSec = 0.0;
        }

        state.previousLiveTunnelRollPhaseRad = phase;
        state.liveTunnelRollPhaseRad = phase;
        state.hasLiveTunnelRollPhase = true;
    }

    [[nodiscard]] static bool start(
        State& state,
        const planner::RoutePlan& plan,
        const AutopilotAgentState& initialAgent,
        const ShipParams& params,
        double acceptedAtUniverseTimeSeconds,
        std::uint64_t requestSerial,
        double trackingPositionToleranceMeters,
        const glm::dvec3& routeUpReference = glm::dvec3(0.0),
        bool holdAtTerminal = false,
        std::string* failureReason = nullptr
    )
    {
        constexpr LocalFlightControlLaw law = Law;

        if (failureReason)
            failureReason->clear();

        auto programs = buildPrograms(
            plan,
            initialAgent,
            law,
            params,
            acceptedAtUniverseTimeSeconds,
            requestSerial,
            trackingPositionToleranceMeters,
            routeUpReference,
            failureReason
        );
        if (programs.empty())
        {
            if (failureReason && failureReason->empty())
                *failureReason = "program-build-returned-empty";
            return false;
        }

        state = {};
        state.active = true;
        state.requestSerial = requestSerial;
        state.holdAtTerminal = holdAtTerminal;
        state.trackingPositionToleranceMeters =
            std::max(0.0, trackingPositionToleranceMeters);
        state.routeUpReference = routeUpReference;
        state.narrowZones = plan.narrowZones;
        state.routeCurves = plan.routeCurves;
        if (state.routeCurves.empty())
            state.routeCurves = buildFallbackRouteCurves(plan.executionGates);

        // WORKING CONTRACT: hull and HUD consume the same canonical route
        // orientation field. Dynamic dock roll is applied later as one phase.
        std::vector<RouteFrameField::Anchor> frameAnchors;
        frameAnchors.reserve(plan.routeFrameAnchors.size());
        for (const auto& anchor : plan.routeFrameAnchors)
        {
            RouteFrameField::Anchor converted;
            converted.progressMeters = anchor.progressMeters;
            converted.upReference = anchor.upReference;
            converted.dynamicRollPhaseWeight =
                anchor.dynamicRollPhaseWeight;
            frameAnchors.push_back(converted);
        }

        state.routeFrameField =
            RouteFrameField::build(
                state.routeCurves,
                routeUpReference,
                frameAnchors
            );

        state.programs = std::move(programs);
        if (!buildContinuousReference(
                state.programs,
                state.continuousSamples,
                state.continuousProgressMeters
            ))
        {
            if (failureReason)
                *failureReason = "continuous-reference-build-failed";
            state = {};
            return false;
        }
        if (!initializeRuntimeProfile(
                state.continuousSamples,
                state.nominalSpeedProfileMps,
                state.runtimeSpeedProfileMps,
                state.runtimeLongitudinalAccelerationMps2
            ))
        {
            if (failureReason)
                *failureReason = "runtime-speed-profile-init-failed";
            state = {};
            return false;
        }
        // A pilot that must turn its thrust axis before braking gets its
        // coast interval from the very first sample, not only after the
        // first checkpoint replans the suffix.
        const double reversalLeadSeconds =
            state.pilotController.telemetry(params).
                thrustReversalLeadSeconds;
        if (reversalLeadSeconds > 0.0)
        {
            const auto authority =
                makeManeuverExecutionAuthority(params, law);
            const double fraction =
                world::navigation::TrajectoryGenerationPolicy::
                    DefaultPlanningAuthorityFraction;
            recomputeRuntimeSpeedSuffix(
                state.nominalSpeedProfileMps,
                state.continuousProgressMeters,
                0,
                glm::length(initialAgent.velocityMapMetersPerSecond),
                authority.forwardAccelerationMps2 * fraction,
                authority.brakingAccelerationMps2 * fraction,
                state.runtimeSpeedProfileMps,
                state.runtimeLongitudinalAccelerationMps2,
                reversalLeadSeconds,
                toleratedPlateauEnd(state, 0,
                    glm::length(initialAgent.velocityMapMetersPerSecond))
            );
        }
        if (!buildCheckpointProgress(
                plan.gates,
                state.continuousSamples,
                state.continuousProgressMeters,
                state.checkpointProgressMeters
            ))
        {
            if (failureReason)
                *failureReason = "checkpoint-progress-build-failed";
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
            if (failureReason)
                *failureReason = "curve-sample-progress-map-failed";
            state = {};
            return false;
        }
        if constexpr (Law == LocalFlightControlLaw::Newtonian)
            dumpAcceptedRoute(state, plan);
        handOverManoeuvreRoute(state, params, initialAgent.forwardMap);
        if constexpr (Law == LocalFlightControlLaw::Newtonian)
            state.capabilityWatch.accept(
                makeNavigationControlCapability(params, law));

        state.nextCheckpointIndex =
            state.checkpointProgressMeters.size() > 1 ? 1 : 0;
        return true;
    }

    [[nodiscard]] static ShipControlState stabilize(
        const AutopilotAgentState& agent,
        const ShipParams& params,
        PilotController& pilotController,
        double deltaSeconds
    ) noexcept
    {
        AutopilotControlRequest request;
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
        return pilotController.update(request, params);
    }

    [[nodiscard]] static Output update(
        State& state,
        const AutopilotAgentState& agent,
        const ShipParams& params,
        double universeTimeSeconds,
        double deltaSeconds
    ) noexcept
    {
        constexpr LocalFlightControlLaw law = Law;

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

        const auto executionAuthority =
            makeManeuverExecutionAuthority(params, law);
        const double planningAuthorityFraction =
            world::navigation::TrajectoryGenerationPolicy::
                DefaultPlanningAuthorityFraction;
        const double forwardAuthority =
            executionAuthority.forwardAccelerationMps2 *
            planningAuthorityFraction;
        const double brakingAuthority =
            executionAuthority.brakingAccelerationMps2 *
            planningAuthorityFraction;
        const double hardBrakingAuthority =
            executionAuthority.brakingAccelerationMps2;

        // Capability change during the route (damaged stage). A real,
        // persisting loss reprofiles the speed ahead with the live authority
        // (curve speeds included) and recompiles the program - unless the
        // craft can no longer reach the speeds ahead, the RCS itself is the
        // plan, or no authority is left: then the route is handed back for
        // a replan from the actual state. Assisted: unchanged.
        if constexpr (Law == LocalFlightControlLaw::Newtonian)
        {
            const auto live = makeNavigationControlCapability(params, law);
            const auto change =
                state.capabilityWatch.evaluate(live, universeTimeSeconds);
            // Inside a narrow zone or on its aligned approach the crossing is
            // flown on the RCS with a fixed pose; a recompiled program there
            // would restart its phases and turn the hull between the walls.
            // A main loss is handled once the zone is left (the watch keeps
            // the change pending); an RCS loss there is the replan case.
            bool inNarrowStretch = false;
            {
                const double speed =
                    glm::length(agent.velocityMapMetersPerSecond);
                const double approach =
                    speed * NewtonianFlightPolicy::NarrowApproachSeconds;
                for (const auto& zone : state.narrowZones)
                    inNarrowStretch = inNarrowStretch ||
                        (continuous.spatialProgressMeters >=
                             zone.startProgressMeters - approach &&
                         continuous.spatialProgressMeters <=
                             zone.endProgressMeters);
            }
            if (change == CapabilityChange::MainDegraded && inNarrowStretch)
            {
                // deferred
            }
            else if (change != CapabilityChange::None)
            {
                const double speed =
                    glm::length(agent.velocityMapMetersPerSecond);
                const double lead = state.pilotController.telemetry(params).
                    thrustReversalLeadSeconds;
                const char* reason = nullptr;
                // A lost RCS/attitude direction: the route's curves are
                // planned on the main (path-normal share of the vector
                // authority), turn times come from the live pilot, so the
                // same reprofile holds - except where the RCS itself is the
                // plan: a narrow zone ahead (RCS-only crossing speed) or a
                // dead translational direction (no terminal trim).
                bool narrowAhead = false;
                for (const auto& zone : state.narrowZones)
                    narrowAhead = narrowAhead ||
                        zone.endProgressMeters > continuous.spatialProgressMeters;
                const bool rcsIsThePlan =
                    change == CapabilityChange::LateralDegraded &&
                    (narrowAhead || !(live.rcsScalarMps2 > 0.0));
                if (change == CapabilityChange::Unavailable)
                    reason = "capability-unavailable";
                else if (rcsIsThePlan)
                    reason = "capability-changed: RCS lost where the route relies on it";
                else
                {
                    // The curves are flown on the main as well: their
                    // speeds follow the new path-normal authority.
                    limitCurveSpeedsToLateral(state,
                        executionAuthority.lateralAccelerationMps2 *
                            planningAuthorityFraction);
                    recomputeRuntimeSpeedSuffix(
                        state.nominalSpeedProfileMps,
                        state.continuousProgressMeters,
                        state.currentContinuousSegment,
                        speed,
                        forwardAuthority,
                        brakingAuthority,
                        state.runtimeSpeedProfileMps,
                        state.runtimeLongitudinalAccelerationMps2,
                        lead,
                        toleratedPlateauEnd(state,
                            state.currentContinuousSegment, speed));
                    ++state.speedProfileRevision;
                    // A hull already turned against the motion brakes
                    // without the flip first.
                    const bool retrograde = speed > 1.0e-3 &&
                        glm::dot(agent.forwardMap,
                            agent.velocityMapMetersPerSecond) < 0.0;
                    if (canBrakeToSpeedsAhead(state,
                            continuous.spatialProgressMeters, speed,
                            hardBrakingAuthority, retrograde ? 0.0 : lead,
                            live.mainForwardMps2 + live.rcsScalarMps2))
                        handOverManoeuvreRoute(state, params, agent.forwardMap);
                    else
                        reason = "capability-changed: cannot brake to the speeds ahead";
                }
                traceCapabilityChange(state, universeTimeSeconds,
                    static_cast<int>(change), speed, live,
                    reason ? reason : "reprofiled");
                state.capabilityWatch.accept(live);
                if (reason)
                {
                    out.invalidReason = reason;
                    return out;
                }
            }
        }

        while (state.nextCheckpointIndex <
                   state.checkpointProgressMeters.size() &&
               continuous.spatialProgressMeters + 1.0e-9 >=
                   state.checkpointProgressMeters[
                       state.nextCheckpointIndex
                   ])
        {
            // The craft is ON its runtime profile at this checkpoint (the
            // profile it is flying, within the refresh step): the
            // recomputed profile is the same plan, and the program compiled
            // from it stays valid.
            const double plannedHere = state.runtimeSpeedProfileMps[
                std::min(state.currentContinuousSegment,
                         state.runtimeSpeedProfileMps.size() - 1)];
            const bool onProfile =
                std::abs(glm::length(agent.velocityMapMetersPerSecond) -
                         plannedHere) <=
                NewtonianSpeedTolerancePolicy::programRefreshMps(plannedHere);
            recomputeRuntimeSpeedSuffix(
                    state.nominalSpeedProfileMps,
                    state.continuousProgressMeters,
                    state.currentContinuousSegment,
                    glm::length(agent.velocityMapMetersPerSecond),
                    forwardAuthority,
                    brakingAuthority,
                    state.runtimeSpeedProfileMps,
                    state.runtimeLongitudinalAccelerationMps2,
                    state.pilotController.telemetry(params).
                        thrustReversalLeadSeconds,
                    toleratedPlateauEnd(state, state.currentContinuousSegment,
                        glm::length(agent.velocityMapMetersPerSecond))
                );
            ++state.speedProfileRevision;
            ++state.nextCheckpointIndex;
            // A checkpoint is not a reason for a new manoeuvre program: it
            // is recompiled only when the craft has left the profile it was
            // flying (a re-levelled plateau, a ramp being tracked, a real
            // deviation). Assisted: unchanged.
            if (law != LocalFlightControlLaw::Newtonian || !onProfile)
                handOverManoeuvreRoute(state, params, agent.forwardMap);
            if constexpr (Law == LocalFlightControlLaw::Newtonian)
                traceProfile(state, universeTimeSeconds,
                    glm::length(agent.velocityMapMetersPerSecond));
        }

        applyRuntimeProfile(
            state.runtimeSpeedProfileMps,
            state.runtimeLongitudinalAccelerationMps2,
            law,
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

        const double actualSpeed =
            glm::length(agent.velocityMapMetersPerSecond);

        const AutopilotControlTelemetry pilotTelemetry =
            state.pilotController.telemetry(params);

        // The damaged engine-out hull may first need to SLEW its off-nose
        // working thrust axis onto the desired course. Include actual
        // attitude capture and arrest time in the curved-course governor.
        // It is computed from present hull attitude/angular rate and the
        // physical acceleration bound, not from a fixed "damaged speed %".
        double emergencyOrientationSeconds = 0.0;
        if (law == LocalFlightControlLaw::Assisted)
        {
            const auto& body = params.assistedWorkingThrustAxisBody;
            const double length=glm::length(body);
            if (!std::isfinite(length) || length<=1.0e-9)
                emergencyOrientationSeconds =
                    std::numeric_limits<double>::infinity();
            else if (glm::dot(body/length,glm::dvec3(0.0,0.0,-1.0)) <
                     1.0-1.0e-9)
            {
                // The geometric working-axis offset, not a possibly delayed
                // damage flag, determines the extra orientation lead.
                // Healthy centered craft keep the existing speed governor.
                const glm::dvec3 worldAxis=(
                    agent.rightMap*body.x +
                    agent.upMap*body.y -
                    agent.forwardMap*body.z)/length;
                const double remainingAngle=std::acos(std::clamp(
                    glm::dot(worldAxis,
                        normalizedOr(nominalForward,agent.forwardMap)),
                    -1.0,1.0
                ));
                const double angularA =
                    game::ship::angularAccelerationLimitRadPerSec2(params);
                if (angularA>1.0e-9)
                    emergencyOrientationSeconds =
                        2.0*std::sqrt(remainingAngle/angularA) +
                        std::hypot(agent.pitchRateRadPerSec,
                                  agent.yawRateRadPerSec)/angularA;
                else
                    emergencyOrientationSeconds =
                        std::numeric_limits<double>::infinity();
            }
        }
        const double courseResponseSeconds =
            std::max(
                deltaSeconds,
                pilotTelemetry.courseResponseSeconds
            ) + emergencyOrientationSeconds;
        const double angularRateLimit =
            game::ship::maximumAngularSpeedRadPerSec(params);

        double turnSpeedCeilingMps = 0.0;
        double distanceToTurnMeters = 0.0;
        double requiredTurnSlowdownDistanceMeters = 0.0;
        double turnSpeedSetpointSlewSeconds = 0.0;
        RouteCurveDiagnostic speedGuide = curveNow;

        if (curveNow.valid &&
            curveNow.curvaturePerMeter <= 1.0e-12 &&
            curveNow.curveIndex + 1 < state.routeCurves.size())
        {
            // Preview the authored route, not merely the immediately adjacent
            // storage primitive. A straight may be followed by another
            // straight before the next real bend; waiting for that final
            // straight to become current throws away braking distance.
            //
            // For CubicBezier the curvature can be zero at the endpoint and
            // rise inside the curve, so inspect its arc-length knots as well
            // as the segment boundaries. The knots are only an s<->t map;
            // geometry and curvature still come from the parametric curve.
            bool foundUpcomingTurn = false;
            for (std::size_t curveIndex = curveNow.curveIndex + 1;
                 curveIndex < state.routeCurves.size() &&
                 !foundUpcomingTurn;
                 ++curveIndex)
            {
                const auto& candidateCurve =
                    state.routeCurves[curveIndex];

                RouteCurveDiagnostic strongestCandidate;
                double strongestProgressMeters = 0.0;
                double strongestCurvature = 0.0;

                const auto considerProgress =
                    [&](double progressMeters) noexcept
                    {
                        const double clampedProgress =
                            std::clamp(
                                progressMeters,
                                candidateCurve.startProgressMeters,
                                candidateCurve.endProgressMeters
                            );
                        const auto candidate =
                            sampleRouteCurveAtProgress(
                                state.routeCurves,
                                clampedProgress
                            );
                        if (!candidate.valid ||
                            candidate.curveIndex != curveIndex ||
                            candidate.curvaturePerMeter <=
                                strongestCurvature)
                        {
                            return;
                        }

                        strongestCandidate = candidate;
                        strongestProgressMeters = clampedProgress;
                        strongestCurvature =
                            candidate.curvaturePerMeter;
                    };

                // Circular arcs have constant curvature, so one interior
                // sample is sufficient. Bezier/spline-like primitives can
                // start with exactly zero curvature and then tighten inside;
                // never let numerical curvature at start+epsilon terminate
                // the preview before inspecting the interior.
                if (candidateCurve.kind ==
                    planner::RouteCurveKind::CircularArc)
                {
                    considerProgress(
                        0.5 *
                        (candidateCurve.startProgressMeters +
                         candidateCurve.endProgressMeters)
                    );
                }
                else
                {
                    constexpr int InteriorSamples = 16;
                    for (int sampleIndex = 1;
                         sampleIndex < InteriorSamples;
                         ++sampleIndex)
                    {
                        const double u =
                            static_cast<double>(sampleIndex) /
                            static_cast<double>(InteriorSamples);
                        considerProgress(
                            candidateCurve.startProgressMeters +
                            (candidateCurve.endProgressMeters -
                             candidateCurve.startProgressMeters) * u
                        );
                    }

                    for (const auto& knot :
                         candidateCurve.arcLengthKnots)
                    {
                        considerProgress(
                            candidateCurve.startProgressMeters +
                            knot.localProgressMeters
                        );
                    }
                }

                if (strongestCandidate.valid &&
                    strongestCurvature > 1.0e-12)
                {
                    speedGuide = strongestCandidate;
                    distanceToTurnMeters =
                        std::max(
                            0.0,
                            strongestProgressMeters -
                                routeProgressMeters
                        );
                    foundUpcomingTurn = true;
                }
            }
        }

        if (speedGuide.valid &&
            speedGuide.curvaturePerMeter > 1.0e-12 &&
            courseResponseSeconds > 1.0e-9)
        {
            const double kappa = speedGuide.curvaturePerMeter;
            const double radius = 1.0 / kappa;

            // During one closed-loop course-response time, the authored curve
            // bends away from its tangent by the sagitta below. Bound that
            // one-response deviation by the already-authoritative corridor
            // tracking tolerance. This yields a speed limit from route
            // geometry and measured controller lag, not from a law-specific
            // corner-speed percentage.
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

            if constexpr (Law == LocalFlightControlLaw::Assisted)
            {
                // Nose steering is NOT trajectory curvature authority.
                // v^2*kappa must fit the real installed lateral motor basis,
                // reserving simultaneous motor demand for braking/attitude.
                // The navigator consumes this speed ceiling BEFORE the bend.
                const double physicalCurveSpeed =
                    game::navigation::assisted_budget::
                        maximumSustainableCurveSpeedMps(
                            executionAuthority.lateralAccelerationMps2,
                            kappa
                        );
                turnSpeedCeilingMps =
                    std::min(turnSpeedCeilingMps, physicalCurveSpeed);
            }
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
                const double targetSetpointRate =
                    std::max(
                        1.0e-6,
                        pilotTelemetry.targetSpeedChangeRateMps2
                    );

                const double measuredBrakingResponse =
                    pilotTelemetry.brakingResponseMps2;
                const double effectiveTurnBraking =
                    measuredBrakingResponse > 1.0e-6
                        ? std::min(
                            brakingAuthority,
                            measuredBrakingResponse
                          )
                        : brakingAuthority;

                const double longitudinalResponseGain =
                    std::max(
                        1.0e-6,
                        pilotTelemetry.commandResponsePerSecond
                    );
                const double feedbackResponseSeconds =
                    1.0 / longitudinalResponseGain + deltaSeconds;

                RouteSpeedGuidance::TurnSlowdownRequest slowdownRequest;
                slowdownRequest.actualSpeedMps = actualSpeed;
                slowdownRequest.turnSpeedCeilingMps =
                    turnSpeedCeilingMps;
                slowdownRequest.distanceToTurnMeters =
                    distanceToTurnMeters;
                slowdownRequest.targetSetpointRateMps2 =
                    targetSetpointRate;
                // On an Assisted turn the same engines must brake,
                // generate curve-normal force and orient the hull.
                // A standalone reverse-engine rating is not three-axis
                // simultaneous braking authority.
                slowdownRequest.effectiveBrakingMps2 =
                    law == LocalFlightControlLaw::Assisted
                        ? effectiveTurnBraking *
                              game::navigation::assisted_budget::
                                  ConcurrentTurnAxisFraction
                        : effectiveTurnBraking;
                slowdownRequest.feedbackResponseSeconds =
                    feedbackResponseSeconds +
                    emergencyOrientationSeconds;

                const auto slowdown =
                    RouteSpeedGuidance::evaluateTurnSlowdown(
                        slowdownRequest
                    );

                if (slowdown.valid)
                {
                    turnSpeedSetpointSlewSeconds =
                        slowdown.setpointSlewSeconds;
                    requiredTurnSlowdownDistanceMeters =
                        slowdown.requiredPreparationDistanceMeters;

                    if (slowdown.slowdownRequiredNow)
                    {
                        targetSpeed =
                            std::min(targetSpeed, turnSpeedCeilingMps);
                    }
                }
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

        // If the craft is already using a meaningful share of the corridor,
        // do not keep full straight-line cruise merely because the next curve
        // is distant. Blend down toward the next known controllable curve
        // speed as corridor occupancy grows. This gives steering authority
        // back to the follower while it recaptures centerline.
        const double corridorToleranceMeters =
            std::max(
                state.trackingPositionToleranceMeters,
                1.0e-6
            );
        const double corridorDeadbandMeters =
            corridorToleranceMeters * 0.10;
        if (turnSpeedCeilingMps > 0.0 &&
            crossTrackErrorMeters > corridorDeadbandMeters)
        {
            const double occupancy =
                std::clamp(
                    (crossTrackErrorMeters - corridorDeadbandMeters) /
                    std::max(
                        1.0e-6,
                        corridorToleranceMeters - corridorDeadbandMeters
                    ),
                    0.0,
                    1.0
                );
            const double smoothOccupancy =
                occupancy * occupancy *
                (3.0 - 2.0 * occupancy);
            const double recoveryCeiling =
                targetSpeed * (1.0 - smoothOccupancy) +
                turnSpeedCeilingMps * smoothOccupancy;
            targetSpeed =
                std::min(targetSpeed, recoveryCeiling);
        }

        // VELOCITY-COURSE CONTRACT.
        //
        // Inside the authored corridor the navigation course is ALWAYS the
        // exact local route tangent. Do not replace it with a temporary
        // capture curve just because cross-track exceeded a small centering
        // band: that causes the old sharp correction / nose-return oscillation.
        //
        // CourseCaptureGuidance is retained only as an OUT-OF-CORRIDOR
        // recovery mechanism. Hull nose is never a navigation reference.
        constexpr double CenteringBandFraction = 0.10;
        const double centeringDeadbandMeters =
            state.trackingPositionToleranceMeters *
            CenteringBandFraction;

        const double poseProgressMeters =
            routeProgressMeters;

        auto poseGuide = curveNow;
        if (!poseGuide.valid)
        {
            poseGuide =
                sampleRouteCurveAtProgress(
                    state.routeCurves,
                    poseProgressMeters
                );
        }

        const glm::dvec3 poseTangent =
            poseGuide.valid
                ? poseGuide.tangentMap
                : referenceTangent;

        CourseCaptureGuidance::Request captureRequest;
        captureRequest.positionMapMeters =
            agent.positionMapMeters;
        captureRequest.actualVelocityMapMps =
            agent.velocityMapMetersPerSecond;
        captureRequest.routeCurves =
            &state.routeCurves;
        captureRequest.currentRouteProgressMeters =
            routeProgressMeters;
        captureRequest.centeringDeadbandMeters =
            centeringDeadbandMeters;
        captureRequest.courseResponseSeconds =
            courseResponseSeconds;

        const auto capture =
            CourseCaptureGuidance::evaluate(captureRequest);

        const bool emergencyCourseRecovery =
            capture.valid &&
            crossTrackErrorMeters >
                state.trackingPositionToleranceMeters;

        // Assisted-only gentle centerline recapture.
        //
        // HARD BOUNDARY:
        // - this does NOT replace CourseCaptureGuidance;
        // - this does NOT run for Newtonian;
        // - this does NOT alter the authored route geometry;
        // - outside the corridor, the existing emergency capture remains
        //   authoritative exactly as before.
        //
        // The old in-corridor contract used the exact tangent all the way out
        // to the full tracking tolerance. That allowed a craft to fly tens of
        // metres parallel to the centerline forever. The helper below adds
        // only a bounded lateral velocity component toward center.
        InCorridorCenteringGuidance::Result centering;
        if constexpr (Law == LocalFlightControlLaw::Assisted)
        {
            InCorridorCenteringGuidance::Request centerRequest;
            centerRequest.routeTangentMap =
                normalizedOr(poseTangent, referenceTangent);
            centerRequest.crossTrackErrorMapMeters =
                crossPositionError;
            centerRequest.actualSpeedMps = actualSpeed;
            centerRequest.deadbandMeters =
                centeringDeadbandMeters;
            centerRequest.corridorToleranceMeters =
                state.trackingPositionToleranceMeters;
            centerRequest.lateralAccelerationAuthorityMps2 =
                std::max(
                    0.0,
                    static_cast<double>(params.strafeAccel)
                );
            centerRequest.responseSeconds =
                std::max(1.0, courseResponseSeconds * 2.0);

            centering =
                InCorridorCenteringGuidance::evaluate(
                    centerRequest
                );
        }

        // Compatibility name only: this is the desired VELOCITY direction.
        // Emergency capture owns out-of-corridor recovery. Assisted gentle
        // centering owns only the interior band. Newtonian retains the exact
        // pre-change tangent semantics.
        const glm::dvec3 steeringForward =
            emergencyCourseRecovery
                ? capture.desiredCourseMap
                : (
                    centering.valid
                        ? centering.desiredCourseMap
                        : normalizedOr(
                              poseTangent,
                              referenceTangent
                          )
                  );

        const double navigationSpeedForCapture =
            glm::length(agent.velocityMapMetersPerSecond);
        const glm::dvec3 actualCourseForCapture =
            navigationSpeedForCapture > 0.5
                ? agent.velocityMapMetersPerSecond /
                    navigationSpeedForCapture
                : normalizedOr(agent.forwardMap, referenceTangent);

        double crossTrackCorrectionAngleRad =
            emergencyCourseRecovery
                ? capture.courseErrorRad
                : 0.0;
        glm::dvec3 crossTrackCorrectionAxisMap =
            glm::cross(actualCourseForCapture, steeringForward);
        const double correctionAxisLength =
            glm::length(crossTrackCorrectionAxisMap);
        if (correctionAxisLength > 1.0e-12)
            crossTrackCorrectionAxisMap /= correctionAxisLength;
        else
        {
            crossTrackCorrectionAxisMap =
                normalizedOr(
                    state.routeUpReference,
                    glm::dvec3(0.0, 1.0, 0.0)
                );
            crossTrackCorrectionAngleRad = 0.0;
        }

        // Body orientation comes from the SAME future route pose.  This is
        // the piece that was lost when course/capture/roll were split into
        // independent controllers.
        const double poseSampleProgressMeters =
            mapRouteProgressToSampleProgress(
                state.routeCurves,
                state.curveSampleStartProgressMeters,
                state.curveSampleEndProgressMeters,
                poseProgressMeters
            );
        auto attitudeLead =
            sampleContinuousReferenceAtProgress(
                state.continuousSamples,
                state.continuousProgressMeters,
                poseSampleProgressMeters
            );
        if (!attitudeLead.valid)
            attitudeLead = continuous;

        // Roll orientation belongs to the SAME authored/live tunnel frame
        // that the player sees.  Never reconstruct a separate hull-only up
        // vector: that lets ship and tunnel drift into different conventions.
        const glm::dvec3 tunnelFrameForward =
            normalizedOr(poseTangent, referenceTangent);

        const glm::dvec3 canonicalTunnelUp =
            !state.routeFrameField.empty()
                ? RouteFrameField::upAtProgress(
                    state.routeFrameField,
                    poseProgressMeters
                  )
                : attitudeLead.reference.upMap;

        RouteCurveDiagnostic curvatureGuide = poseGuide;
        double curvatureBlend = 1.0;
        const double attitudeLeadDistanceMeters = 0.0;

        double coursePhaseLeadAngleRad = 0.0;
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

        // VERIFIED WORKING CONTRACT.
        //
        // Do not reproduce this math locally. HullTunnelRollGuidance is the
        // single owner of hull<->live-tunnel roll geometry and rate tracking.
        HullTunnelRollGuidance::Request hullRollRequest;
        hullRollRequest.hullCenter = agent.positionMapMeters;
        hullRollRequest.hullForward = agent.forwardMap;
        hullRollRequest.hullUp = agent.upMap;
        hullRollRequest.tunnelCenter = referencePosition;
        hullRollRequest.tunnelForward = tunnelFrameForward;
        hullRollRequest.canonicalTunnelUp = canonicalTunnelUp;
        const double dynamicRollPhaseWeight =
            !state.routeFrameField.empty()
                ? RouteFrameField::dynamicRollPhaseWeightAtProgress(
                    state.routeFrameField,
                    poseProgressMeters
                  )
                : 1.0;

        hullRollRequest.liveTunnelRollPhaseRad =
            state.liveTunnelRollPhaseRad *
            dynamicRollPhaseWeight;
        hullRollRequest.liveTunnelRollRateRadPerSec =
            state.liveTunnelRollRateRadPerSec *
            dynamicRollPhaseWeight;
        hullRollRequest.rollResponseSeconds =
            rollActuatorResponseSeconds;
        hullRollRequest.maxRollRateRadPerSec =
            rollRateLimit;

        const auto hullRoll =
            HullTunnelRollGuidance::evaluate(hullRollRequest);
        if (!hullRoll.valid)
            return out;

        const glm::dvec3 desiredUp =
            hullRoll.desiredTunnelUp;
        const double signedRollErrorRad =
            hullRoll.signedRollErrorRad;
        const double desiredRollRateRadPerSec =
            hullRoll.desiredRollRateRadPerSec;

        const double courseLeadDistanceMeters =
            attitudeLeadDistanceMeters;

        // Terminal stop uses a conservative physical braking envelope.
        // A concrete law may have command/setpoint dynamics before full
        // braking authority is realized. Enter explicit stopping behavior
        // before the ideal v^2/(2a) boundary by subtracting the controller
        // response distance from the usable route length.
        // Braking authority is a physical vehicle capability.
        // Controller response telemetry describes dynamics/latency only; it
        // must never replace the craft's actual
        // braking envelope. Doing so can collapse a valid ~45 m/s^2 authority
        // into a tiny transient estimate (for example ~0.01 m/s^2) and make
        // terminal-speed planning throttle an otherwise valid route down to
        // walking speed.
        const double effectiveBrakingAuthority =
            brakingAuthority;

        // Keep the learned response available as a controller diagnostic, but
        // do not feed it back as physical deceleration authority.
        const double measuredBrakingResponse =
            pilotTelemetry.brakingResponseMps2;
        (void)measuredBrakingResponse;

        // Hard physical braking remains available for emergency recovery.
        const double hardRecoveryBrakingAuthority =
            hardBrakingAuthority;
        const double longitudinalResponseGain =
            std::max(
                1.0e-6,
                pilotTelemetry.commandResponsePerSecond
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

        // Navigation owns velocity, not hull attitude. The desired velocity
        // vector is the authored route tangent (except explicit out-of-corridor
        // recovery). flight-law autopilot may move the hull however the actuator
        // needs, but it may not redefine this course from nose direction.
        const glm::dvec3 desiredVelocity =
            steeringForward * targetSpeed;

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

        if constexpr (Law == LocalFlightControlLaw::Newtonian)
            if (std::FILE* file = profileTraceFile())
                std::fprintf(file, "U t=%.3f s=%.1f seg=%zu profile=%.2f "
                    "target=%.2f actual=%.2f\n", universeTimeSeconds,
                    continuous.spatialProgressMeters,
                    continuous.lowerSampleIndex,
                    glm::length(
                        continuous.reference.velocityMapMetersPerSecond),
                    targetSpeed, actualSpeed);

        AutopilotControlRequest request;
        request.desiredPositionMapMeters = referencePosition;
        request.actualPositionMapMeters = agent.positionMapMeters;
        request.positionReferenceValid = true;
        request.corridorToleranceMeters =
            state.trackingPositionToleranceMeters;
        request.desiredVelocityMapMps = desiredVelocity;
        // Newtonian receives the pure profile feed-forward: its pilot owns
        // corridor-relative correction (safe tube), so the tracker's
        // position/velocity feedback toward the centreline must not be
        // folded in as a rail.
        request.desiredLinearAccelerationMapMps2 =
            law != LocalFlightControlLaw::Newtonian &&
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
            // Required angular velocity of the ACTUAL velocity vector.
            //
            // This must use actualSpeed, never the planned/limited turn speed.
            // If the craft enters a bend too fast, the geometric requirement
            // v*kappa becomes larger, not smaller. Speed guidance brakes
            // separately; course guidance must still follow the curve.
            const double targetHullRate =
                actualSpeed *
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

        glm::dvec3 desiredCourseAngularRateMap(0.0);

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
        }
        else if (brakeAttitudeLock)
        {
            // During terminal braking the authoritative COURSE is the final
            // corridor tangent. Since moving navigation takes direction from
            // desiredVelocityMapMps, lock that vector as well as the hull
            // actuator target. No separate angular program is allowed.
            request.desiredVelocityMapMps =
                referenceTangent * targetSpeed;
            request.desiredForwardMap = referenceTangent;
            request.desiredUpMap = desiredUp;
        }
        else
        {
            request.desiredForwardMap = steeringForward;
            request.desiredUpMap = desiredUp;

            // Navigation is owned by the desired VELOCITY direction.  On a
            // smooth authored bend that direction is itself rotating, so give
            // flight-law autopilot its continuous course-rate feed-forward instead
            // of forcing the hull controller to rediscover the turn from a
            // sequence of accumulated angle errors.
            desiredCourseAngularRateMap =
                !state.routeCurves.empty()
                    ? routeAngularRateMap
                    : attitudeLead.reference.
                        angularVelocityMapRadPerSecond;

        }

        request.desiredCourseAngularRateMapRadPerSec =
            desiredCourseAngularRateMap;
        request.desiredRollRateRadPerSec =
            desiredRollRateRadPerSec;

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
        if (state.holdAtTerminal && terminalStopAuthored)
        {
            // The route's own terminal acceptance envelope (a navigation
            // terminal state: staging/waiting/rendezvous point, NOT a
            // docked state); roll uses the attitude tolerance.
            const auto& finalPose = state.continuousSamples.back();
            const auto& tolerance = state.programs.back().terminalTolerance;
            request.terminalTargetValid = true;
            request.terminalPositionMapMeters = finalPose.positionMapMeters;
            request.terminalForwardMap =
                normalizedOr(finalPose.forwardMap, referenceTangent);
            request.terminalUpMap = normalizedOr(finalPose.upMap, desiredUp);
            request.terminalPositionToleranceMeters = tolerance.positionMeters;
            request.terminalSpeedToleranceMps = tolerance.linearVelocityMps;
            request.terminalAttitudeToleranceRad = tolerance.forwardAngleRad;
            request.terminalRollToleranceRad = tolerance.forwardAngleRad;
            request.terminalAngularRateToleranceRadPerSec =
                tolerance.angularVelocityRadPerSec;
        }
        request.programProgressMeters = continuous.spatialProgressMeters;
        request.programProgressValid =
            PilotTakesManoeuvreRoute<PilotController>::value;
        request.traceRouteProgressMeters = routeProgressMeters;
        request.traceCurveIndex = curveNow.valid ? curveNow.curveIndex : 0;
        request.traceCurvaturePerMeter =
            curveNow.valid ? curveNow.curvaturePerMeter : 0.0;
        request.traceProfileSpeedMps = targetSpeed;

        if (law == LocalFlightControlLaw::Newtonian &&
            !state.continuousSamples.empty() &&
            state.continuousProgressMeters.size() ==
                state.continuousSamples.size())
        {
            // The preview must reach past the time the pilot needs to turn
            // its thrust axis before a braking burn.
            const double HorizonSeconds =
                std::max(
                    6.0,
                    pilotTelemetry.thrustReversalLeadSeconds + 4.0
                );
            const double previewSpeed =
                std::max(
                    20.0,
                    std::max(actualSpeed, targetSpeed)
                );
            const double remaining =
                std::max(
                    0.0,
                    state.continuousProgressMeters.back() -
                        routeProgressMeters
                );
            const double horizonDistance =
                std::min(
                    remaining,
                    previewSpeed * HorizonSeconds
                );

            double previousProgress = routeProgressMeters;
            double previousSpeed = std::max(actualSpeed, 1.0);
            double accumulatedTime = 0.0;

            for (std::size_t i = 0;
                 i < AutopilotPreviewCapacity &&
                 horizonDistance > 1.0e-6;
                 ++i)
            {
                const double fraction =
                    static_cast<double>(i + 1) /
                    static_cast<double>(AutopilotPreviewCapacity);
                const double previewProgress =
                    routeProgressMeters +
                    horizonDistance * fraction;

                auto previewReference =
                    sampleContinuousReferenceAtProgress(
                        state.continuousSamples,
                        state.continuousProgressMeters,
                        previewProgress
                    );
                if (!previewReference.valid)
                    break;

                applyRuntimeProfile(
                    state.runtimeSpeedProfileMps,
                    state.runtimeLongitudinalAccelerationMps2,
                    law,
                    previewReference
                );

                const double sampleSpeed =
                    glm::length(
                        previewReference.reference.
                            velocityMapMetersPerSecond
                    );
                const double ds =
                    std::max(
                        0.0,
                        previewProgress - previousProgress
                    );
                const double averageSpeed =
                    std::max(
                        1.0,
                        0.5 * (previousSpeed + sampleSpeed)
                    );
                accumulatedTime += ds / averageSpeed;

                auto& sample =
                    request.preview[request.previewCount++];
                sample.timeOffsetSeconds = accumulatedTime;
                sample.desiredPositionMapMeters =
                    previewReference.reference.positionMapMeters;
                sample.desiredVelocityMapMps =
                    previewReference.reference.
                        velocityMapMetersPerSecond;
                sample.desiredLinearAccelerationMapMps2 =
                    previewReference.reference.
                        linearAccelerationFeedForwardMapMps2;

                previousProgress = previewProgress;
                previousSpeed = std::max(sampleSpeed, 1.0);
            }
        }

        double actualCourseAngularRateRadPerSec = 0.0;
        const double previousCourseSpeed =
            glm::length(pilotTelemetry.previousVelocityMapMps);
        if (pilotTelemetry.initialized &&
            previousCourseSpeed > 0.5 &&
            actualSpeed > 0.5 &&
            deltaSeconds > 1.0e-9)
        {
            const glm::dvec3 previousCourse =
                pilotTelemetry.previousVelocityMapMps /
                previousCourseSpeed;
            const glm::dvec3 currentCourse =
                agent.velocityMapMetersPerSecond / actualSpeed;
            actualCourseAngularRateRadPerSec =
                angleBetween(previousCourse, currentCourse) /
                deltaSeconds;
        }

        out.control =
            state.pilotController.update(request, params);

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
        out.crossTrackCaptureSpeedMps =
            centering.valid
                ? centering.desiredClosingSpeedMps
                : 0.0;

        const double navigationSpeed =
            glm::length(agent.velocityMapMetersPerSecond);
        const glm::dvec3 actualCourse =
            navigationSpeed > 0.5
                ? agent.velocityMapMetersPerSecond / navigationSpeed
                : normalizedOr(agent.forwardMap, referenceTangent);
        const double desiredNavigationSpeed =
            glm::length(request.desiredVelocityMapMps);
        const glm::dvec3 desiredCourse =
            desiredNavigationSpeed > 1.0e-9
                ? request.desiredVelocityMapMps /
                    desiredNavigationSpeed
                : normalizedOr(
                    request.desiredForwardMap,
                    referenceTangent
                  );

        out.courseErrorRad =
            angleBetween(actualCourse, desiredCourse);
        out.forwardErrorRad = out.courseErrorRad;
        out.hullForwardErrorRad =
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
            glm::length(desiredCourseAngularRateMap);
        out.actualCourseAngularRateRadPerSec =
            actualCourseAngularRateRadPerSec;
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
        (void)hardRecoveryBrakingAuthority;
        out.requiredTerminalStopDistanceMeters =
            requiredTerminalStopDistanceMeters;
        out.turnSpeedCeilingMps = turnSpeedCeilingMps;
        out.distanceToTurnMeters = distanceToTurnMeters;
        out.requiredTurnSlowdownDistanceMeters =
            requiredTurnSlowdownDistanceMeters;
        out.turnSpeedSetpointSlewSeconds =
            turnSpeedSetpointSlewSeconds;
        out.crossTrackCorrectionAngleRad =
            emergencyCourseRecovery
                ? crossTrackCorrectionAngleRad
                : (
                    centering.valid
                        ? centering.correctionAngleRad
                        : 0.0
                  );
        out.desiredCaptureAngularRateRadPerSec =
            glm::length(captureAngularRateMap);
        out.courseCaptureActive =
            emergencyCourseRecovery;
        out.captureMeetingRouteProgressMeters =
            capture.valid
                ? capture.meetingRouteProgressMeters
                : routeProgressMeters;
        out.captureMeetingJoinAngleRad =
            capture.valid ? capture.meetingJoinAngleRad : 0.0;
        out.signedRollErrorRad = signedRollErrorRad;
        out.desiredRollRateRadPerSec =
            desiredRollRateRadPerSec;
        out.liveTunnelRollPhaseRad =
            state.liveTunnelRollPhaseRad;
        out.liveTunnelRollRateRadPerSec =
            state.liveTunnelRollRateRadPerSec;
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

    // Newtonian speed tolerance corridor at a checkpoint. On a speed
    // PLATEAU of the profile the craft is flying (cruise, curve, narrow-zone
    // speed: the current runtime profile there neither accelerates nor
    // brakes), a measured speed inside the tolerance band IS the plateau
    // speed: the plateau is re-levelled at the measured speed instead of
    // planning a micro brake/burn segment back to the old value, and the
    // next braking ramp (the backward pass) starts where it is feasible
    // from that speed. Beyond the band it is a real deviation: the plateau
    // is not re-levelled and the profile corrects it, until the craft is
    // back within half the band (hysteresis). On an acceleration or braking
    // ramp nothing changes: the measured speed anchors the ramp as before.
    // Returns the end (exclusive) of the plateau to re-level; 0 = none.
    // Assisted: never.
    [[nodiscard]] static std::size_t toleratedPlateauEnd(
        State& state,
        std::size_t anchorIndex,
        double actualSpeedMps
    ) noexcept
    {
        if constexpr (Law != LocalFlightControlLaw::Newtonian)
            return 0;
        const auto& profile = state.runtimeSpeedProfileMps;
        const auto& acceleration = state.runtimeLongitudinalAccelerationMps2;
        if (profile.empty() || !std::isfinite(actualSpeedMps) ||
            acceleration.size() != profile.size())
            return 0;
        if (state.nominalSpeedProfileMps.size() != profile.size())
            return 0;
        anchorIndex = std::min(anchorIndex, profile.size() - 1);
        // The band is around the NOMINAL plateau speed, so re-levelling
        // never walks away from it checkpoint by checkpoint.
        const double nominal = state.nominalSpeedProfileMps[anchorIndex];
        using Tolerance = NewtonianSpeedTolerancePolicy;
        if (state.speedCorrectionActive)
            state.speedCorrectionActive =
                !Tolerance::backInsideRelease(actualSpeedMps, nominal);
        else
            state.speedCorrectionActive =
                Tolerance::beyondBand(actualSpeedMps, nominal);
        constexpr double PlateauAccelerationMps2 =
            Tolerance::PlateauAccelerationMps2;
        const double level = profile[anchorIndex];
        std::size_t end = anchorIndex;
        while (end + 1 < profile.size() &&
               std::abs(acceleration[end]) <= PlateauAccelerationMps2 &&
               std::abs(profile[end] - level) <= 1.0e-6 &&
               std::abs(state.nominalSpeedProfileMps[end] - nominal) <= 1.0e-6)
            ++end;
        if (state.speedCorrectionActive || end == anchorIndex)
            return 0;
        return end;
    }

    // The reference's own path-normal acceleration (v^2 * curvature) at a
    // sample, and its speed.
    [[nodiscard]] static double referenceNormalAccelerationMps2(
        const AcceptedManeuverProgram::ReferenceSample& sample,
        double& speedMps) noexcept
    {
        speedMps = glm::length(sample.velocityMapMetersPerSecond);
        if (!(speedMps > 1.0e-3))
            return 0.0;
        const glm::dvec3 t = sample.velocityMapMetersPerSecond / speedMps;
        const glm::dvec3 a = sample.linearAccelerationFeedForwardMapMps2;
        return glm::length(a - t * glm::dot(a, t));
    }

    // Lower the nominal speed ahead wherever the reference's path-normal
    // acceleration exceeds the planned lateral authority:
    // v_new = v * sqrt(a_lateral / a_normal).
    static void limitCurveSpeedsToLateral(State& state, double lateralMps2) noexcept
    {
        if (!(lateralMps2 > 1.0e-9))
            return;
        for (std::size_t i = state.currentContinuousSegment;
             i < state.nominalSpeedProfileMps.size() &&
             i < state.continuousSamples.size(); ++i)
        {
            double v = 0.0;
            const double normal =
                referenceNormalAccelerationMps2(state.continuousSamples[i], v);
            if (normal > lateralMps2)
                state.nominalSpeedProfileMps[i] = std::min(
                    state.nominalSpeedProfileMps[i],
                    v * std::sqrt(lateralMps2 / normal));
        }
    }

    // Every speed restriction ahead is still physically reachable from the
    // actual speed: braking with the full authority after the hull turned
    // its thrust axis (lead) to the nominal speeds (zones, the route end),
    // and on curves at most the speed the FULL lateral authority holds on
    // the curve (the planned curve speed is a reserve, not the limit).
    [[nodiscard]] static bool canBrakeToSpeedsAhead(
        const State& state,
        double progressMeters,
        double speedMps,
        double brakingMps2,
        double leadSeconds,
        double fullLateralMps2
    ) noexcept
    {
        if (!(brakingMps2 > 1.0e-9))
            return false;
        const double lost = speedMps * std::max(0.0, leadSeconds);
        for (std::size_t i = state.currentContinuousSegment + 1;
             i < state.nominalSpeedProfileMps.size() &&
             i < state.continuousSamples.size(); ++i)
        {
            double limit = state.nominalSpeedProfileMps[i];
            double v = 0.0;
            const double normal =
                referenceNormalAccelerationMps2(state.continuousSamples[i], v);
            if (normal > 1.0e-6)
                limit = std::max(limit,
                    v * std::sqrt(std::max(0.0, fullLateralMps2) / normal));
            // Inside the speed tolerance band the craft is on its profile.
            limit += NewtonianSpeedTolerancePolicy::softBandMps(limit);
            if (speedMps <= limit)
                continue;
            const double room =
                state.continuousProgressMeters[i] - progressMeters - lost;
            if (room <= 0.0 ||
                speedMps * speedMps - 2.0 * brakingMps2 * room >
                    limit * limit + 1.0)
                return false;
        }
        return true;
    }

    static void recomputeRuntimeSpeedSuffix(
        const std::vector<double>& nominalSpeeds,
        const std::vector<double>& progress,
        std::size_t anchorIndex,
        double actualSpeedMps,
        double acceleratingMps2,
        double brakingMps2,
        std::vector<double>& runtimeSpeeds,
        std::vector<double>& runtimeAccelerations,
        double thrustReversalCoastSeconds = 0.0,
        std::size_t toleratedPlateauEndIndex = 0
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
        // Speed tolerance corridor: the plateau ahead is re-levelled at the
        // measured speed (inside the band; see toleratedPlateauEnd).
        if (std::isfinite(actualSpeedMps))
            for (std::size_t i = anchorIndex;
                 i < std::min(toleratedPlateauEndIndex, nominalSpeeds.size());
                 ++i)
                runtimeSpeeds[i] = std::max(0.0, actualSpeedMps);

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

        if (thrustReversalCoastSeconds > 0.0)
            insertThrustReversalCoast(
                progress,
                anchorIndex,
                std::isfinite(actualSpeedMps)
                    ? std::max(0.0, actualSpeedMps) : 0.0,
                acceleratingMps2,
                thrustReversalCoastSeconds,
                runtimeSpeeds
            );
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

    // DEV diagnostic only: ELITE_NEWTONIAN_PROFILE_TRACE=<path>.
    [[nodiscard]] static std::FILE* profileTraceFile() noexcept
    {
        static std::FILE* file =
            []() -> std::FILE*
            {
                const char* path =
                    std::getenv("ELITE_NEWTONIAN_PROFILE_TRACE");
                return path && *path ? std::fopen(path, "w") : nullptr;
            }();
        return file;
    }

    // DEV diagnostic only: ELITE_ROUTE_DUMP=<path> appends every accepted
    // route: curves, then the runtime speed profile every ~50 m.
    static void dumpAcceptedRoute(
        const State& state,
        const planner::RoutePlan& plan
    ) noexcept
    {
        static std::FILE* file =
            []() -> std::FILE*
            {
                const char* path = std::getenv("ELITE_ROUTE_DUMP");
                return path && *path ? std::fopen(path, "w") : nullptr;
            }();
        if (!file)
            return;
        std::fprintf(file, "ROUTE curves=%zu exec_gates=%zu samples=%zu\n",
            plan.routeCurves.size(), plan.executionGates.size(),
            state.continuousSamples.size());
        for (const auto& c : plan.routeCurves)
            std::fprintf(file, "C kind=%d s0=%.1f len=%.1f radius=%.1f "
                "vmax=%.2f\n", static_cast<int>(c.kind),
                c.startProgressMeters,
                c.endProgressMeters - c.startProgressMeters,
                c.arcRadiusMeters, c.maxSpeedMps);
        const auto& speeds = state.runtimeSpeedProfileMps;
        for (std::size_t i = 1; i + 1 < speeds.size(); ++i)
            if (speeds[i] <= 0.5 && speeds[i] <= speeds[i - 1] &&
                speeds[i] < speeds[i + 1])
                std::fprintf(file, "STOP s=%.1f v=%.2f\n",
                    state.continuousProgressMeters[i], speeds[i]);
        double nextS = 0.0;
        for (std::size_t i = 0; i < state.runtimeSpeedProfileMps.size(); ++i)
            if (state.continuousProgressMeters[i] >= nextS ||
                i + 1 == state.runtimeSpeedProfileMps.size())
            {
                std::fprintf(file, "V %.1f %.2f\n",
                    state.continuousProgressMeters[i],
                    state.runtimeSpeedProfileMps[i]);
                nextS = state.continuousProgressMeters[i] + 50.0;
            }
        std::fflush(file);
    }

    // A capability change is rare: always reported (stderr, the game log).
    static void traceCapabilityChange(
        const State& state,
        double universeTimeSeconds,
        int kind,
        double actualSpeedMps,
        const NavigationControlCapability& live,
        const char* action
    ) noexcept
    {
        const auto& before = state.capabilityWatch.accepted;
        std::fprintf(stderr,
            "[CapabilityChange] t=%.2f kind=%d speed=%.1f main=%.2f->%.2f "
            "reverse=%.2f->%.2f rcs=%.3f->%.3f angular=%.4f->%.4f action=%s\n",
            universeTimeSeconds, kind, actualSpeedMps,
            before.mainForwardMps2, live.mainForwardMps2,
            before.mainReverseMps2, live.mainReverseMps2,
            before.rcsScalarMps2, live.rcsScalarMps2,
            before.angularScalarRadPerSec2, live.angularScalarRadPerSec2,
            action);
    }

    static void traceProfile(
        const State& state,
        double universeTimeSeconds,
        double actualSpeedMps
    ) noexcept
    {
        std::FILE* file = profileTraceFile();
        if (!file)
            return;
        std::fprintf(file, "R t=%.3f rev=%llu anchor=%zu actual=%.2f\n",
            universeTimeSeconds,
            static_cast<unsigned long long>(state.speedProfileRevision),
            state.currentContinuousSegment, actualSpeedMps);
        for (std::size_t i = state.currentContinuousSegment;
             i < state.runtimeSpeedProfileMps.size(); ++i)
            std::fprintf(file, "P %zu %.1f %.2f %.2f\n", i,
                state.continuousProgressMeters[i],
                state.nominalSpeedProfileMps[i],
                state.runtimeSpeedProfileMps[i]);
        std::fflush(file);
    }

    // Acceleration -> coast -> braking. A pilot that must turn its thrust
    // axis before braking needs to reach every braking curve already
    // coasting for coastSeconds. At each sample find the highest speed from
    // which a coast of v*coastSeconds still ends on the braking-feasible
    // profile, then stop ACCELERATING once that speed is reached; the speed
    // is held (coast) until the unchanged braking curve takes over.
    static void insertThrustReversalCoast(
        const std::vector<double>& progress,
        std::size_t anchorIndex,
        double anchorSpeedMps,
        double acceleratingMps2,
        double coastSeconds,
        std::vector<double>& speeds
    ) noexcept
    {
        const std::size_t count = speeds.size();
        if (count < 2 || progress.size() != count ||
            anchorIndex + 1 >= count ||
            !(std::isfinite(coastSeconds) && coastSeconds > 0.0))
        {
            return;
        }

        const std::vector<double> braking = speeds;
        const auto brakingSpeedAt =
            [&](double s) noexcept
            {
                if (s >= progress.back())
                    return braking.back();
                const auto upper = std::upper_bound(
                    progress.begin(), progress.end(), s);
                const std::size_t i = static_cast<std::size_t>(
                    std::max<std::ptrdiff_t>(
                        1, upper - progress.begin()));
                const double span = progress[i] - progress[i - 1];
                const double u = span > 1.0e-9
                    ? std::clamp((s - progress[i - 1]) / span, 0.0, 1.0)
                    : 1.0;
                // v^2 is linear in s under constant acceleration.
                return std::sqrt(std::max(0.0,
                    braking[i - 1] * braking[i - 1] * (1.0 - u) +
                    braking[i] * braking[i] * u));
            };

        // The coast must stay under the braking profile over its WHOLE
        // length, not only at its end (a coast must never jump over a low
        // point such as a corner). Sparse-table range minimum of samples.
        std::vector<std::vector<double>> minimumTable {braking};
        for (std::size_t width = 1; 2 * width <= count; width *= 2)
        {
            const auto& previous = minimumTable.back();
            std::vector<double> level(count - 2 * width + 1);
            for (std::size_t i = 0; i < level.size(); ++i)
                level[i] = std::min(previous[i], previous[i + width]);
            minimumTable.push_back(std::move(level));
        }
        const auto sampleMinimum =
            [&](std::size_t first, std::size_t last) noexcept
            {
                std::size_t level = 0;
                while ((std::size_t(2) << level) <= last - first + 1)
                    ++level;
                return std::min(
                    minimumTable[level][first],
                    minimumTable[level][last + 1 - (std::size_t(1) << level)]
                );
            };
        const auto brakingMinimumOver =
            [&](std::size_t from, double toProgress) noexcept
            {
                double minimum = brakingSpeedAt(toProgress);
                const auto end = std::upper_bound(
                    progress.begin(), progress.end(), toProgress);
                const std::size_t last = static_cast<std::size_t>(
                    end - progress.begin());
                if (last > from)
                    minimum = std::min(minimum, sampleMinimum(from, last - 1));
                return minimum;
            };

        std::vector<double> coastReady(count, 0.0);
        for (std::size_t i = anchorIndex; i < count; ++i)
        {
            double low = 0.0;
            double high = braking[i];
            if (high <=
                brakingMinimumOver(i, progress[i] + high * coastSeconds))
            {
                coastReady[i] = high;
                continue;
            }
            for (int iteration = 0; iteration < 24; ++iteration)
            {
                const double mid = 0.5 * (low + high);
                if (mid <=
                    brakingMinimumOver(i, progress[i] + mid * coastSeconds))
                    low = mid;
                else
                    high = mid;
            }
            coastReady[i] = low;
        }

        // Start from the measured speed: the anchor sample still holds the
        // nominal value until the physical-floor pass below replaces it.
        double speed = std::min(speeds[anchorIndex], anchorSpeedMps);
        for (std::size_t i = anchorIndex + 1; i < count; ++i)
        {
            const double ds = progress[i] - progress[i - 1];
            if (!(std::isfinite(ds) && ds >= 0.0))
                return;
            // Accelerate only up to the speed from which this sample can
            // still start its coast; at or above it, hold the speed.
            double next = speed;
            if (speed < coastReady[i])
            {
                const double accelerated =
                    acceleratingMps2 > 1.0e-9
                        ? std::sqrt(speed * speed +
                              2.0 * acceleratingMps2 * ds)
                        : braking[i];
                next = std::min(accelerated, coastReady[i]);
            }
            speed = std::min(braking[i], next);
            speeds[i] = speed;
        }
    }

    static void applyRuntimeProfile(
        const std::vector<double>& runtimeSpeeds,
        const std::vector<double>& runtimeAccelerations,
        LocalFlightControlLaw law,
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

        if (law == LocalFlightControlLaw::Newtonian)
        {
            // Runtime speed replanning owns ONLY the longitudinal component.
            // Newtonian depends on the authored path-normal acceleration to
            // lead a bend with the aft-main vector. Erasing it turns a
            // curvature-following problem into a late cross-track recovery.
            const glm::dvec3 authoredAcceleration =
                reference.reference.linearAccelerationFeedForwardMapMps2;
            const glm::dvec3 authoredLateral =
                authoredAcceleration -
                tangent * glm::dot(authoredAcceleration, tangent);

            reference.reference.linearAccelerationFeedForwardMapMps2 =
                authoredLateral +
                tangent * longitudinalAcceleration;
        }
        else
        {
            // Preserve the currently proven Assisted behavior exactly.
            reference.reference.linearAccelerationFeedForwardMapMps2 =
                tangent * longitudinalAcceleration;
        }
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

    // A pilot that compiles its own manoeuvre program from the route takes
    // the route stations through setManoeuvreRoute(); others never see them.
    template <typename P, typename = void>
    struct PilotTakesManoeuvreRoute : std::false_type
    {
    };
    template <typename P>
    struct PilotTakesManoeuvreRoute<
        P,
        std::void_t<decltype(std::declval<P&>().setManoeuvreRoute(
            std::declval<const std::vector<ManoeuvreRouteSample>&>(),
            std::declval<const ShipParams&>(),
            std::declval<const glm::dvec3&>(),
            std::declval<double>()))>
    > : std::true_type
    {
    };

    // Route stations on the execution progress axis (continuous sample
    // progress): geometry from the authoritative curves, speed and
    // longitudinal acceleration from the current runtime profile.
    static void handOverManoeuvreRoute(
        State& state,
        const ShipParams& params,
        const glm::dvec3& hullForwardMap
    )
    {
        if constexpr (PilotTakesManoeuvreRoute<PilotController>::value)
        {
            const auto& progress = state.continuousProgressMeters;
            const auto& speeds = state.runtimeSpeedProfileMps;
            const auto& accelerations =
                state.runtimeLongitudinalAccelerationMps2;
            if (progress.size() < 2 ||
                progress.size() != state.continuousSamples.size() ||
                speeds.size() != progress.size() ||
                accelerations.size() != progress.size())
                return;

            // About one station per 50 ms of flight, never sparser than
            // 25 m, and every curve boundary.
            constexpr double StationSeconds = 0.05;
            constexpr double MaxStationMeters = 25.0;
            std::vector<ManoeuvreRouteSample> samples;
            samples.reserve(progress.size() / 2 + 2);
            double lastKept = -std::numeric_limits<double>::infinity();
            std::size_t lastCurve = static_cast<std::size_t>(-1);
            const std::size_t first = 0;
            for (std::size_t i = first; i < progress.size(); ++i)
            {
                const double routeProgress =
                    mapSampleProgressToRouteProgress(
                        state.routeCurves,
                        state.curveSampleStartProgressMeters,
                        state.curveSampleEndProgressMeters,
                        progress[i]
                    );
                const auto curve =
                    sampleRouteCurveAtProgress(
                        state.routeCurves, routeProgress);
                const double spacing =
                    std::clamp(
                        speeds[i] * StationSeconds, 1.0, MaxStationMeters);
                const bool boundary =
                    curve.valid && curve.curveIndex != lastCurve;
                if (!(i == first || i + 1 == progress.size() || boundary ||
                      progress[i] - lastKept >= spacing))
                    continue;
                lastKept = progress[i];
                if (curve.valid)
                    lastCurve = curve.curveIndex;

                ManoeuvreRouteSample sample;
                for (const auto& zone : state.narrowZones)
                    if (routeProgress >= zone.startProgressMeters &&
                        routeProgress <= zone.endProgressMeters)
                    {
                        sample.narrow = true;
                        sample.narrowAxisMap = zone.axisWorld;
                        sample.narrowUpMap = zone.upWorld;
                        sample.narrowToleranceMeters = std::max(
                            0.0,
                            std::min(zone.lateralToleranceRightMeters,
                                     zone.lateralToleranceUpMeters));
                    }
                sample.progressMeters = progress[i];
                sample.positionMapMeters =
                    state.continuousSamples[i].positionMapMeters;
                sample.speedMps = speeds[i];
                sample.longitudinalAccelerationMps2 = accelerations[i];
                sample.tangentMap =
                    curve.valid
                        ? curve.tangentMap
                        : normalizedOr(
                              state.continuousSamples[i].forwardMap,
                              glm::dvec3(0.0, 0.0, -1.0));
                if (curve.valid && curve.curvaturePerMeter > 1.0e-12)
                {
                    // turnNormalMap is the turn AXIS; the in-plane normal
                    // toward the centre of curvature is axis x tangent.
                    const glm::dvec3 inward =
                        glm::cross(curve.turnNormalMap, curve.tangentMap);
                    if (glm::length(inward) > 1.0e-9)
                    {
                        sample.normalMap = glm::normalize(inward);
                        sample.curvaturePerMeter = curve.curvaturePerMeter;
                    }
                }
                samples.push_back(sample);
            }
            // The planned hull starts where the craft is: the route behind
            // it is history (the profile itself stays whole).
            const double currentProgress =
                progress[std::min(
                    state.currentContinuousSegment, progress.size() - 1)];
            state.pilotController.setManoeuvreRoute(
                samples, params, hullForwardMap, currentProgress);
        }
        else
        {
            (void)state;
            (void)params;
            (void)hullForwardMap;
        }
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
        const AutopilotAgentState& initialAgent,
        LocalFlightControlLaw law,
        const ShipParams& params,
        double acceptedAtUniverseTimeSeconds,
        std::uint64_t requestSerial,
        double trackingPositionToleranceMeters,
        const glm::dvec3& routeUpReference,
        std::string* failureReason
    )
    {
        const auto failPrograms =
            [&](const std::string& reason)
                -> std::vector<AcceptedManeuverProgram>
            {
                if (failureReason)
                    *failureReason = reason;
                return {};
            };

        if (!plan.valid())
            return failPrograms("planner-route-invalid-at-follower-boundary");
        if (plan.executionGates.size() < 2)
            return failPrograms("planner-route-has-too-few-execution-gates");
        if (!std::isfinite(acceptedAtUniverseTimeSeconds))
            return failPrograms("accepted-time-non-finite");
        if (requestSerial == 0)
            return failPrograms("request-serial-zero");

        const auto executionAuthority =
            makeManeuverExecutionAuthority(params, law);
        const double feedbackReserve =
            executionAuthority.feedbackReserveMps2;

        // Factual capability, zero is zero: no invented minimum speed or
        // angular authority; without them there is no executable program.
        const auto capability =
            makeNavigationControlCapability(params, law);
        if (!(capability.controlledSpeedLimitMps > 0.0) ||
            !(capability.angularScalarRadPerSec2 > 0.0) ||
            !(capability.maxAngularSpeedRadPerSec > 0.0))
            return failPrograms("capability-unavailable: no speed or attitude authority");

        world::navigation::NavigationVehicleProfile vehicle;
        vehicle.collisionRadiusMeters = 0.0;
        vehicle.preferredClearanceMeters = 0.0;
        vehicle.maxSpeedMps = capability.controlledSpeedLimitMps;
        vehicle.maxForwardAccelerationMps2 =
            executionAuthority.forwardAccelerationMps2;
        vehicle.maxBrakingAccelerationMps2 =
            executionAuthority.brakingAccelerationMps2;
        vehicle.maxLateralAccelerationMps2 =
            executionAuthority.lateralAccelerationMps2;
        vehicle.maxAngularVelocityRadPerSecond =
            capability.maxAngularSpeedRadPerSec;
        vehicle.maxAngularAccelerationRadPerSecond2 =
            capability.angularScalarRadPerSec2 * 0.90;

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
            plan.executionGates.size() +
            plan.routeCurves.size()
        );
        std::vector<double> executionGateProgressMeters;
        executionGateProgressMeters.reserve(
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

            executionGateProgressMeters.push_back(
                sourceProgressMeters
            );

            world::navigation::TrajectoryPointSpeedConstraint limit;
            limit.sourcePathProgressMeters = sourceProgressMeters;
            limit.maxSpeedMps = std::min(
                vehicle.maxSpeedMps,
                std::max(0.0, gate.speedMps)
            );
            trajectoryRequest.pointSpeedConstraints.push_back(limit);
        }

        if (law == LocalFlightControlLaw::Newtonian &&
            plan.routeCurves.size() >= 2 &&
            executionGateProgressMeters.size() ==
                plan.executionGates.size())
        {
            // The planner rounds Newtonian corners where a fillet fits; a
            // corner it could not round stays a real tangent discontinuity,
            // which no finite lateral capability flies through, so the
            // profile arrives stopped there. Joins whose direction differs
            // by less than half a degree (numerical arc/line seams, merged
            // near-collinear points) are flown through.
            constexpr double FlyThroughJoinRad =
                0.5 * 3.14159265358979323846 / 180.0;
            const double TangentEqualityEpsilon2 =
                2.0 - 2.0 * std::cos(FlyThroughJoinRad);

            for (std::size_t curveIndex = 0;
                 curveIndex + 1 < plan.routeCurves.size();
                 ++curveIndex)
            {
                const auto& before = plan.routeCurves[curveIndex];
                const auto& after = plan.routeCurves[curveIndex + 1];

                const glm::dvec3 beforeTangent =
                    normalizedOr(
                        before.tangentAtProgress(
                            before.endProgressMeters
                        ),
                        before.endForward
                    );
                const glm::dvec3 afterTangent =
                    normalizedOr(
                        after.tangentAtProgress(
                            after.startProgressMeters
                        ),
                        after.startForward
                    );

                if (glm::length2(
                        beforeTangent - afterTangent
                    ) <= TangentEqualityEpsilon2)
                {
                    continue;
                }

                const glm::dvec3 joinPosition =
                    0.5 * (
                        before.endMeters +
                        after.startMeters
                    );

                std::size_t nearestGate = 0;
                double nearestDistance2 =
                    std::numeric_limits<double>::infinity();

                for (std::size_t gateIndex = 0;
                     gateIndex < plan.executionGates.size();
                     ++gateIndex)
                {
                    const glm::dvec3 delta =
                        plan.executionGates[gateIndex].
                            positionMeters -
                        joinPosition;
                    const double distance2 =
                        glm::dot(delta, delta);
                    if (distance2 < nearestDistance2)
                    {
                        nearestDistance2 = distance2;
                        nearestGate = gateIndex;
                    }
                }

                if (nearestGate == 0 ||
                    nearestGate + 1 >=
                        plan.executionGates.size())
                {
                    continue;
                }

                world::navigation::
                    TrajectoryPointSpeedConstraint stop;
                stop.sourcePathProgressMeters =
                    executionGateProgressMeters[nearestGate];
                stop.maxSpeedMps = 0.0;
                trajectoryRequest.pointSpeedConstraints.
                    push_back(stop);
            }
        }

        // Newtonian speed tolerance corridor: an initial speed inside the
        // tolerance band of the route's speed limit at the start IS that
        // controlled speed for the strategic profile (400.5 m/s on a 400 m/s
        // route is not a braking manoeuvre). Beyond the band the measured
        // speed stays authoritative and the generator brakes it.
        if (law == LocalFlightControlLaw::Newtonian &&
            !trajectoryRequest.pointSpeedConstraints.empty())
        {
            const double startLimit =
                trajectoryRequest.pointSpeedConstraints.front().maxSpeedMps;
            const double initialSpeed =
                glm::length(trajectoryRequest.initialVelocityMps);
            if (initialSpeed > startLimit && startLimit > 1.0e-6 &&
                !NewtonianSpeedTolerancePolicy::beyondBand(
                    initialSpeed, startLimit))
                trajectoryRequest.initialVelocityMps *=
                    startLimit / initialSpeed;
        }

        constexpr int MaximumRefinementAttempts = 6;
        std::string lastRefinementDiagnostic;
        for (int attempt = 0;
             attempt < MaximumRefinementAttempts;
             ++attempt)
        {
            const auto trajectory =
                world::navigation::TrajectoryGenerator::generate(
                    trajectoryRequest
                );
            if (!trajectory.ready())
            {
                return failPrograms(
                    "trajectory-generation-failed attempt=" +
                    std::to_string(attempt) +
                    " status=" +
                    std::to_string(
                        static_cast<int>(trajectory.trajectory.status)
                    ) +
                    " message=\"" +
                    trajectory.trajectory.message +
                    "\"" +
                    " guide_points=" +
                    std::to_string(
                        trajectory.diagnostics.executionGuidePoints
                    ) +
                    " max_curvature=" +
                    std::to_string(
                        trajectory.diagnostics.maxCurvaturePerMeter
                    ) +
                    " initial_along_mps=" +
                    std::to_string(
                        trajectory.diagnostics.initialAlongPathSpeedMps
                    ) +
                    " initial_cross_mps=" +
                    std::to_string(
                        trajectory.diagnostics.initialCrossTrackSpeedMps
                    )
                );
            }

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
                return failPrograms(
                    "accepted-program-rejected: " +
                    (!accepted.feedback.message.empty()
                        ? accepted.feedback.message
                        : accepted.failureReason) +
                    " page=" +
                    std::to_string(accepted.feedback.pageIndex) +
                    " sample=" +
                    std::to_string(accepted.feedback.sampleIndex) +
                    " required=" +
                    std::to_string(accepted.feedback.requiredValue) +
                    " available=" +
                    std::to_string(accepted.feedback.availableValue)
                );
            }

            const double scale = std::clamp(
                accepted.feedback.recommendedScale,
                0.50,
                0.98
            );

            lastRefinementDiagnostic =
                " attempt=" + std::to_string(attempt) +
                " refinement=" +
                    std::to_string(
                        static_cast<int>(accepted.feedback.refinement)
                    ) +
                " message=\"" +
                    (!accepted.feedback.message.empty()
                        ? accepted.feedback.message
                        : accepted.failureReason) +
                    "\"" +
                " page=" +
                    std::to_string(accepted.feedback.pageIndex) +
                " sample=" +
                    std::to_string(accepted.feedback.sampleIndex) +
                " required=" +
                    std::to_string(accepted.feedback.requiredValue) +
                " available=" +
                    std::to_string(accepted.feedback.availableValue) +
                " recommended_scale=" +
                    std::to_string(accepted.feedback.recommendedScale) +
                " applied_scale=" +
                    std::to_string(scale) +
                " vehicle_max_speed=" +
                    std::to_string(trajectoryRequest.vehicle.maxSpeedMps);
            trajectoryRequest.vehicle.maxSpeedMps =
                std::max(
                    0.5,
                    trajectoryRequest.vehicle.maxSpeedMps * scale
                );
            // The first route station is a measured initial state,
            // not a future planning constraint. Refinement must never lower
            // it below the velocity with which the craft actually enters the
            // accepted route; doing so creates an artificial acceleration
            // discontinuity at sample zero and can make "brake earlier"
            // refinement self-defeating.
            //
            // Only future speed constraints are refinable. The generator
            // already treats initialVelocityMps as authoritative and will
            // brake an initial overspeed over physically available distance.
            for (std::size_t limitIndex = 1;
                 limitIndex <
                     trajectoryRequest.pointSpeedConstraints.size();
                 ++limitIndex)
            {
                auto& limit =
                    trajectoryRequest.pointSpeedConstraints[limitIndex];
                limit.maxSpeedMps =
                    std::max(0.0, limit.maxSpeedMps * scale);
            }
        }

        return failPrograms(
            "accepted-program-refinement-exhausted" +
            lastRefinementDiagnostic
        );
    }
};

} // namespace game::navigation::autopilot
