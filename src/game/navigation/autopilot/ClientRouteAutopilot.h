#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/AcceptedManeuverProgram.h"
#include "src/game/navigation/ManeuverCapabilityAdapters.h"
#include "src/game/navigation/DockingAutomaticRecoveryPolicy.h"
#include "src/game/navigation/autopilot/PredictivePilot.h"
#include "src/game/navigation/autopilot/RouteFollowerApi.h"
#include "src/game/navigation/planner/RoutePlannerApi.h"
#include "src/game/ship/core/ShipControlState.h"
#include "src/game/ship/core/ShipDynamics.h"
#include "src/game/ship/core/ShipParams.h"

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
        LocalFlightControlLaw law,
        const ShipParams& params,
        double acceptedAtUniverseTimeSeconds,
        std::uint64_t requestSerial,
        double trackingPositionToleranceMeters
    )
    {
        auto programs = buildPrograms(
            plan,
            law,
            params,
            acceptedAtUniverseTimeSeconds,
            trackingPositionToleranceMeters
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

        const double targetSpeed =
            glm::length(followed.targetVelocityMapMps);

        PredictivePilot::Request request;
        request.law = law;
        request.desiredVelocityMapMps =
            followed.targetVelocityMapMps;
        request.desiredLinearAccelerationMapMps2 =
            followed.intent.idealLinearAccelerationLocalMps2;
        request.desiredForwardMap =
            targetSpeed > 1.0e-6
                ? followed.targetVelocityMapMps / targetSpeed
                : reference.reference.forwardMap;
        request.desiredUpMap = reference.reference.upMap;
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
        out.pageIndex = state.currentPage;
        out.segmentIndex = state.currentSpatialSegment;
        return out;
    }

private:
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
        LocalFlightControlLaw law,
        const ShipParams& params,
        double acceptedAtUniverseTimeSeconds,
        double trackingPositionToleranceMeters
    )
    {
        std::vector<AcceptedManeuverProgram> out;
        if (!plan.valid() ||
            plan.executionGates.size() < 2 ||
            !std::isfinite(acceptedAtUniverseTimeSeconds))
        {
            return out;
        }

        const auto& gates = plan.executionGates;
        const std::size_t maxSamples =
            AcceptedManeuverProgram::kMaxSamples;
        std::size_t first = 0;
        std::uint64_t revision = 1;
        double sequenceOffsetSeconds = 0.0;

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
        const double positionTolerance =
            std::max(5.0, trackingPositionToleranceMeters);

        while (first + 1 < gates.size())
        {
            const std::size_t last =
                std::min(gates.size() - 1, first + maxSamples - 1);
            const std::size_t count = last - first + 1;

            AcceptedManeuverProgram page;
            page.valid = true;
            page.revision = revision++;
            page.objectiveRevision = 1;
            page.family =
                last + 1 == gates.size()
                    ? AcceptedManeuverProgram::ManeuverFamily::PrecisionTransit
                    : AcceptedManeuverProgram::ManeuverFamily::FreeTransit;
            page.referenceMode =
                AcceptedManeuverProgram::ReferenceMode::SpatialCorridor;
            page.controlLaw = law;
            page.translationMode =
                law == LocalFlightControlLaw::Assisted
                    ? AcceptedManeuverProgram::TranslationMode::AssistedVelocity
                    : AcceptedManeuverProgram::TranslationMode::NewtonianMainEngine;
            page.acceptedAtUniverseTimeSeconds =
                acceptedAtUniverseTimeSeconds;
            page.sequenceStartOffsetSeconds =
                sequenceOffsetSeconds;
            page.sampleCount = static_cast<std::uint8_t>(count);

            double localTime = 0.0;
            for (std::size_t i = 0; i < count; ++i)
            {
                const std::size_t gateIndex = first + i;
                const auto& gate = gates[gateIndex];
                auto& sample = page.samples[i];

                if (i > 0)
                {
                    const auto& previous = gates[gateIndex - 1];
                    const double distance =
                        glm::length(gate.positionMeters -
                                    previous.positionMeters);
                    const double averageSpeed =
                        std::max(
                            0.5,
                            0.5 * (
                                std::max(0.0, previous.speedMps) +
                                std::max(0.0, gate.speedMps)
                            )
                        );
                    localTime += distance / averageSpeed;
                }

                sample.timeOffsetSeconds = localTime;
                sample.positionMapMeters = gate.positionMeters;

                glm::dvec3 tangent = gate.forward;
                if (gateIndex + 1 < gates.size())
                {
                    tangent = gates[gateIndex + 1].positionMeters -
                        gate.positionMeters;
                }
                else if (gateIndex > 0)
                {
                    tangent = gate.positionMeters -
                        gates[gateIndex - 1].positionMeters;
                }

                glm::dvec3 forward;
                glm::dvec3 right;
                glm::dvec3 up;
                makeBasis(
                    normalizedOr(gate.forward, tangent),
                    forward,
                    right,
                    up
                );

                sample.velocityMapMetersPerSecond =
                    normalizedOr(tangent, forward) *
                    std::max(0.0, gate.speedMps);
                sample.linearAccelerationFeedForwardMapMps2 =
                    glm::dvec3(0.0);
                sample.forwardMap = forward;
                sample.rightMap = right;
                sample.upMap = up;
                sample.angularVelocityMapRadPerSecond =
                    glm::dvec3(0.0);
                sample.angularAccelerationFeedForwardMapRadPerSec2 =
                    glm::dvec3(0.0);
            }

            if (!(localTime > 0.0))
                return {};

            page.validUntilUniverseTimeSeconds =
                acceptedAtUniverseTimeSeconds +
                sequenceOffsetSeconds +
                localTime +
                3600.0;

            page.terminalTolerance.positionMeters =
                std::max(3.0, positionTolerance * 0.25);
            page.terminalTolerance.linearVelocityMps = 1.0;
            page.terminalTolerance.forwardAngleRad =
                0.17453292519943295;
            page.terminalTolerance.angularVelocityRadPerSec = 0.10;

            page.tracking.positionErrorMeters = positionTolerance;
            page.tracking.linearVelocityErrorMps = 10.0;
            page.tracking.forwardAngleErrorRad =
                1.3962634015954636; // 80 deg: recover, don't abort.
            page.tracking.angularVelocityErrorRadPerSec = 2.0;
            page.tracking.alongTrackPositionDeadbandMeters = 10.0;
            page.tracking.alongTrackSpeedDeadbandMps = 1.0;
            page.tracking.linearFeedbackReserveMps2 = feedbackReserve;
            page.tracking.angularFeedbackReserveRadPerSec2 =
                std::min(
                    0.5,
                    game::ship::angularAccelerationLimitRadPerSec2(params) *
                        0.15
                );
            page.capability =
                makeManeuverCapabilitySnapshot(params, revision);

            out.push_back(page);
            sequenceOffsetSeconds += localTime;

            if (last + 1 >= gates.size())
                break;

            // Keep one shared boundary sample so spatial page selection has a
            // continuous centerline with no geometric gap.
            first = last;
        }

        return out;
    }
};

} // namespace game::navigation::autopilot
