#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
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

        const glm::dvec3 nominalForward =
            normalizedOr(
                continuous.reference.forwardMap,
                agent.forwardMap
            );
        const glm::dvec3 desiredUp =
            normalizedOr(
                continuous.reference.upMap,
                agent.upMap
            );

        // Position feedback is a servo around the already-authored program,
        // not another path planner. Derive the lateral capture velocity from
        // the ship's real lateral acceleration authority:
        //
        //     v_capture^2 = 2 * a_lateral * cross_track_distance
        //
        // This naturally commands a large correction far from centerline and
        // bleeds it to zero as the craft reaches the corridor center. No
        // fixed look-ahead distance or arbitrary response time is involved.
        const glm::dvec3 referenceTangent =
            targetSpeed > 1.0e-9
                ? continuous.reference.velocityMapMetersPerSecond /
                    targetSpeed
                : nominalForward;
        const glm::dvec3 positionError =
            agent.positionMapMeters -
            continuous.reference.positionMapMeters;
        const glm::dvec3 crossPositionError =
            positionError -
            referenceTangent *
                glm::dot(positionError, referenceTangent);
        const double crossTrackErrorMeters =
            glm::length(crossPositionError);

        const double lateralAuthority =
            law == LocalFlightControlLaw::Assisted
                ? game::ship::
                    assistedLateralStabilizationAccelerationLimitMps2(params)
                : game::ship::manoeuvreAccelerationLimitMps2(params);

        glm::dvec3 steeringForward = nominalForward;
        double crossTrackClosingSpeedMps = 0.0;
        double crossTrackCaptureSpeedMps = 0.0;
        if (crossTrackErrorMeters > 1.0e-9 &&
            lateralAuthority > 1.0e-9 &&
            targetSpeed > 1.0e-9)
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
            crossTrackClosingSpeedMps = closingSpeed;
            const double inwardSpeed =
                std::max(0.0, closingSpeed);
            const double stoppingDistance =
                inwardSpeed * inwardSpeed /
                (2.0 * lateralAuthority);
            const double captureDistance =
                std::max(
                    0.0,
                    crossTrackErrorMeters - stoppingDistance
                );
            const double captureSpeed =
                std::sqrt(
                    2.0 *
                    lateralAuthority *
                    captureDistance
                );
            crossTrackCaptureSpeedMps = captureSpeed;

            const glm::dvec3 steeringVelocity =
                nominalForward * targetSpeed +
                towardCenter * captureSpeed;
            steeringForward =
                normalizedOr(steeringVelocity, nominalForward);
        }

        // Preserve the authored scalar speed. Cross-track recovery changes
        // direction, not the planner's speed schedule.
        const glm::dvec3 desiredVelocity =
            steeringForward * targetSpeed;

        PredictivePilot::Request request;
        request.law = law;
        request.desiredVelocityMapMps = desiredVelocity;
        request.desiredLinearAccelerationMapMps2 =
            tracking.status !=
                    ManeuverTrackingController::Status::InvalidInput
                ? tracking.intent.idealLinearAccelerationLocalMps2
                : continuous.reference.
                    linearAccelerationFeedForwardMapMps2;
        request.desiredForwardMap = steeringForward;
        request.desiredUpMap = desiredUp;
        request.actualVelocityMapMps =
            agent.velocityMapMetersPerSecond;
        request.forwardMap = agent.forwardMap;
        request.rightMap = agent.rightMap;
        request.upMap = agent.upMap;
        request.pitchRateRadPerSec = agent.pitchRateRadPerSec;
        request.yawRateRadPerSec = agent.yawRateRadPerSec;
        request.rollRateRadPerSec = agent.rollRateRadPerSec;

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
        request.stopRequested =
            atFinalContinuousSegment && terminalStopAuthored;
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
        out.crossTrackClosingSpeedMps = crossTrackClosingSpeedMps;
        out.crossTrackCaptureSpeedMps = crossTrackCaptureSpeedMps;
        out.forwardErrorRad =
            angleBetween(agent.forwardMap, steeringForward);
        out.upErrorRad =
            angleBetween(agent.upMap, desiredUp);
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

        if (atFinalContinuousSegment &&
            terminalPositionError <= terminal.positionMeters &&
            terminalVelocityError <= terminal.linearVelocityMps &&
            terminalForwardError <= terminal.forwardAngleRad &&
            terminalAngularRate <= terminal.angularVelocityRadPerSec)
        {
            if (state.holdAtTerminal)
                out.terminalHold = true;
            else
            {
                out.complete = true;
                state.active = false;
            }
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
