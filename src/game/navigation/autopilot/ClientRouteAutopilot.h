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

        double targetSpeed =
            glm::length(followed.targetVelocityMapMps);

        glm::dvec3 desiredForward =
            targetSpeed > 1.0e-6
                ? followed.targetVelocityMapMps / targetSpeed
                : reference.reference.forwardMap;
        glm::dvec3 desiredUp = reference.reference.upMap;

        // Spatial execution must look across AcceptedManeuverProgram page
        // boundaries. Each page is only fixed-capacity storage (16 samples),
        // not a semantic "wait until crossed, then discover the next frame"
        // boundary. At flight speed the pilot must already know the geometry
        // and speed restrictions several seconds ahead.
        const double actualSpeed =
            glm::length(agent.velocityMapMetersPerSecond);
        const double previewDistanceMeters = std::clamp(
            actualSpeed * 5.0,
            250.0,
            2000.0
        );

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
                : game::ship::forwardMainAccelerationLimitMps2(params);

        const auto preview = previewAcrossPages(
            state.programs,
            state.currentPage,
            state.currentSpatialSegment,
            agent.positionMapMeters,
            desiredForward,
            desiredUp,
            targetSpeed,
            previewDistanceMeters,
            brakingAuthority
        );
        if (preview.valid)
        {
            desiredForward = preview.forwardMap;
            desiredUp = preview.upMap;
            targetSpeed = std::min(targetSpeed, preview.safeSpeedMps);
        }

        PredictivePilot::Request request;
        request.law = law;
        request.desiredVelocityMapMps =
            desiredForward * targetSpeed;
        request.desiredLinearAccelerationMapMps2 =
            followed.intent.idealLinearAccelerationLocalMps2;
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
        request.stopRequested =
            targetSpeed <= 0.05 &&
            state.currentPage + 1 == state.programs.size();
        request.deltaSeconds = deltaSeconds;

        out.control =
            PredictivePilot::make(request, params, state.pilotState);
        out.valid = true;
        out.crossTrackErrorMeters = followed.crossTrackErrorMeters;
        out.remainingDistanceMeters = followed.remainingDistanceMeters;
        out.targetSpeedMps = targetSpeed;
        out.pageIndex = state.currentPage;
        out.segmentIndex = state.currentSpatialSegment;
        return out;
    }

private:
    struct SpatialPreview
    {
        bool valid = false;
        glm::dvec3 forwardMap {0.0, 0.0, -1.0};
        glm::dvec3 upMap {0.0, 1.0, 0.0};
        double safeSpeedMps = 0.0;
    };

    [[nodiscard]] static SpatialPreview previewAcrossPages(
        const std::vector<AcceptedManeuverProgram>& programs,
        std::size_t currentPage,
        std::size_t currentSegment,
        const glm::dvec3& positionMapMeters,
        const glm::dvec3& fallbackForward,
        const glm::dvec3& fallbackUp,
        double currentTargetSpeedMps,
        double previewDistanceMeters,
        double brakingAccelerationMps2
    ) noexcept
    {
        SpatialPreview out;
        if (programs.empty() ||
            currentPage >= programs.size() ||
            !(previewDistanceMeters > 0.0))
        {
            return out;
        }

        glm::dvec3 previous = positionMapMeters;
        glm::dvec3 targetPoint = positionMapMeters;
        glm::dvec3 targetUp = fallbackUp;
        double accumulated = 0.0;
        double safeSpeed = std::max(0.0, currentTargetSpeedMps);
        bool havePoint = false;

        for (std::size_t page = currentPage;
             page < programs.size() &&
             accumulated < previewDistanceMeters;
             ++page)
        {
            const auto& program = programs[page];
            if (!program.valid || program.sampleCount == 0)
                continue;

            std::size_t first = 0;
            if (page == currentPage)
            {
                first = std::min(
                    currentSegment + 1,
                    static_cast<std::size_t>(program.sampleCount - 1)
                );
            }

            for (std::size_t i = first;
                 i < program.sampleCount;
                 ++i)
            {
                const auto& sample = program.samples[i];
                const glm::dvec3 delta =
                    sample.positionMapMeters - previous;
                const double ds = glm::length(delta);
                if (!std::isfinite(ds) || ds <= 1.0e-9)
                {
                    previous = sample.positionMapMeters;
                    continue;
                }

                const double nextDistance = accumulated + ds;

                // Any lower speed already authored later in the accepted
                // route constrains today's speed by the physical braking
                // distance. This deliberately works across storage pages.
                const double futureSpeed =
                    glm::length(sample.velocityMapMetersPerSecond);
                if (std::isfinite(futureSpeed) &&
                    std::isfinite(brakingAccelerationMps2) &&
                    brakingAccelerationMps2 > 1.0e-9)
                {
                    const double reachable =
                        std::sqrt(
                            std::max(
                                0.0,
                                futureSpeed * futureSpeed +
                                2.0 *
                                    brakingAccelerationMps2 *
                                    nextDistance
                            )
                        );
                    safeSpeed = std::min(safeSpeed, reachable);
                }

                if (nextDistance >= previewDistanceMeters)
                {
                    const double u = std::clamp(
                        (previewDistanceMeters - accumulated) / ds,
                        0.0,
                        1.0
                    );
                    targetPoint =
                        previous + delta * u;
                    targetUp = sample.upMap;
                    accumulated = previewDistanceMeters;
                    havePoint = true;
                    break;
                }

                accumulated = nextDistance;
                previous = sample.positionMapMeters;
                targetPoint = sample.positionMapMeters;
                targetUp = sample.upMap;
                havePoint = true;
            }
        }

        if (!havePoint)
            return out;

        const glm::dvec3 ray = targetPoint - positionMapMeters;
        const double rayLength = glm::length(ray);
        if (!(std::isfinite(rayLength) && rayLength > 1.0e-9))
            return out;

        out.valid = true;
        out.forwardMap = ray / rayLength;
        glm::dvec3 projectedUp =
            targetUp -
            out.forwardMap * glm::dot(targetUp, out.forwardMap);
        out.upMap = normalizedOr(projectedUp, fallbackUp);
        out.safeSpeedMps =
            std::max(0.0, std::isfinite(safeSpeed) ? safeSpeed : 0.0);
        return out;
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
                std::max(0.5, gate.speedMps)
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
                    std::max(0.5, limit.maxSpeedMps * scale);
            }
        }

        return {};
    }
};

} // namespace game::navigation::autopilot
