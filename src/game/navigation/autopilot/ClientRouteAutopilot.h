#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/AcceptedManeuverProgram.h"
#include "src/game/navigation/AcceptedManeuverProgramBuilder.h"
#include "src/game/navigation/ManeuverCapabilityAdapters.h"
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
        std::size_t currentContinuousSegment = 0;

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
        std::size_t pageIndex = 0;
        std::size_t segmentIndex = 0;
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
        const glm::dvec3& routeUpReference = glm::dvec3(0.0)
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
        state.programs = std::move(programs);
        if (!buildContinuousReference(
                state.programs,
                state.continuousSamples,
                state.continuousProgressMeters
            ))
        {
            state = {};
            return false;
        }
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
        Output out;
        if (!state.active || state.programs.empty())
            return out;

        const auto selection = RouteFollower::selectPage(
            state.programs,
            universeTimeSeconds,
            agent.positionMapMeters,
            state.currentPage
        );
        if (selection.status != RouteProgramSelectionStatus::Active ||
            selection.pageIndex >= state.programs.size())
        {
            return out;
        }

        if (selection.pageIndex != state.currentPage)
        {
            state.currentPage = selection.pageIndex;
            state.currentSpatialSegment = 0;
        }

        const auto& program = state.programs[state.currentPage];
        const auto followed = RouteFollower::follow(
            program,
            universeTimeSeconds,
            agent,
            state.followerPolicy,
            state.currentSpatialSegment
        );
        if (followed.status == RouteFollowerStatus::InvalidInput)
            return out;

        if (followed.spatialReference)
        {
            state.currentSpatialSegment = std::max(
                state.currentSpatialSegment,
                followed.referenceLowerSampleIndex
            );
        }

        if (followed.status == RouteFollowerStatus::Complete)
        {
            if (state.currentPage + 1 < state.programs.size())
            {
                ++state.currentPage;
                state.currentSpatialSegment = 0;
            }
            else
            {
                out.valid = true;
                out.complete = true;
                state.active = false;
                return out;
            }
        }

        const auto reference = RouteFollower::sampleReference(
            state.programs[state.currentPage],
            universeTimeSeconds,
            agent.positionMapMeters,
            state.currentSpatialSegment
        );
        if (!reference.valid)
            return out;

        const auto continuous = sampleContinuousReference(
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

        const double targetSpeed =
            glm::length(
                continuous.reference.velocityMapMetersPerSecond
            );
        const glm::dvec3 desiredForward =
            normalizedOr(
                continuous.reference.forwardMap,
                reference.reference.forwardMap
            );
        const glm::dvec3 desiredUp =
            normalizedOr(
                continuous.reference.upMap,
                reference.reference.upMap
            );

        PredictivePilot::Request request;
        request.law = law;
        request.desiredVelocityMapMps =
            continuous.reference.velocityMapMetersPerSecond;
        request.desiredLinearAccelerationMapMps2 =
            continuous.reference.linearAccelerationFeedForwardMapMps2;
        request.desiredForwardMap = desiredForward;
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
            state.currentContinuousSegment + 1 >=
                state.continuousSamples.size();
        request.stopRequested =
            targetSpeed <= 0.05 &&
            atFinalContinuousSegment;
        request.deltaSeconds = deltaSeconds;

        out.control =
            PredictivePilot::make(request, params, state.pilotState);
        out.valid = true;
        out.crossTrackErrorMeters = followed.crossTrackErrorMeters;
        out.remainingDistanceMeters =
            std::max(
                0.0,
                state.continuousProgressMeters.back() -
                    continuous.spatialProgressMeters
            );
        out.targetSpeedMps = targetSpeed;
        out.pageIndex = state.currentPage;
        out.segmentIndex = state.currentSpatialSegment;
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
