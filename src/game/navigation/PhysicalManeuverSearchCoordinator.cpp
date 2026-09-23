#include "PhysicalManeuverSearchCoordinator.h"

#include <algorithm>
#include <cmath>

namespace game::navigation
{
namespace
{

using Coordinator = PhysicalManeuverSearchCoordinator;

bool finite(const glm::dvec3& value) noexcept
{
    return
        std::isfinite(value.x) &&
        std::isfinite(value.y) &&
        std::isfinite(value.z);
}

bool validAlternative(
    const Coordinator::Alternative& alternative
) noexcept
{
    return
        alternative.identity.corridorAlternativeId != 0 &&
        alternative.identity.terminalAlternativeId != 0 &&
        alternative.identity.speedScheduleAlternativeId != 0 &&
        alternative.identity.arrivalTimeAlternativeId != 0 &&
        finite(alternative.targetPositionMapMeters) &&
        finite(alternative.desiredVelocityMapMetersPerSecond) &&
        std::isfinite(alternative.maximumProgramSeconds) &&
        alternative.maximumProgramSeconds > 0.0;
}

bool validRequest(const Coordinator::Request& request) noexcept
{
    if (request.frontier.objectiveRevision == 0 ||
        request.frontier.frontierRevision == 0 ||
        request.cursor.objectiveRevision !=
            request.frontier.objectiveRevision ||
        request.cursor.frontierRevision !=
            request.frontier.frontierRevision ||
        request.frontier.alternativeCount == 0 ||
        request.frontier.alternativeCount >
            Coordinator::kMaxAlternatives ||
        request.cursor.nextAlternativeIndex >
            request.frontier.alternativeCount ||
        request.policy.maximumAttemptsPerAdvance == 0 ||
        request.policy.maximumAttemptsPerAdvance >
            Coordinator::kMaxAttemptsPerAdvance)
    {
        return false;
    }

    for (std::size_t i = 0;
         i < request.frontier.alternativeCount;
        ++i)
    {
        if (!validAlternative(request.frontier.alternatives[i]) ||
            request.frontier.alternatives[i].maximumProgramSeconds <
                request.commonPhysicalQuery.policy.minimumPrimitiveSeconds)
        {
            return false;
        }
    }

    return true;
}

bool sharedStateBlocked(
    OrdinaryPhysicalManeuverCompiler::InfeasibilityReason reason
) noexcept
{
    using Reason =
        OrdinaryPhysicalManeuverCompiler::InfeasibilityReason;

    return
        reason == Reason::InvalidQuery ||
        reason == Reason::UnsupportedControlLaw ||
        reason == Reason::InvalidBodyFrame ||
        reason == Reason::InitialAngularStateUnsupported;
}

} // namespace

PhysicalManeuverSearchCoordinator::Result
PhysicalManeuverSearchCoordinator::advance(
    const Request& request
) noexcept
{
    Result result;
    result.nextCursor = request.cursor;

    if (!validRequest(request))
        return result;

    if (request.cursor.nextAlternativeIndex ==
        request.frontier.alternativeCount)
    {
        result.status = Status::FrontierExhausted;
        return result;
    }

    const std::size_t end = std::min(
        request.frontier.alternativeCount,
        request.cursor.nextAlternativeIndex +
            request.policy.maximumAttemptsPerAdvance
    );

    for (std::size_t index = request.cursor.nextAlternativeIndex;
         index < end;
         ++index)
    {
        const Alternative& alternative =
            request.frontier.alternatives[index];

        Compiler::Query physicalQuery =
            request.commonPhysicalQuery;
        physicalQuery.geometricTargetPositionMapMeters =
            alternative.targetPositionMapMeters;
        physicalQuery.desiredVelocityMapMetersPerSecond =
            alternative.desiredVelocityMapMetersPerSecond;
        physicalQuery.maximumProgramSeconds =
            alternative.maximumProgramSeconds;

        Compiler::Result physical =
            Compiler::compile(physicalQuery);

        Attempt& attempt = result.attempts[result.attemptCount++];
        attempt.alternativeIndex = index;
        attempt.identity = alternative.identity;
        attempt.compilerStatus = physical.status;
        attempt.infeasibility = physical.infeasibility;

        ++result.nextCursor.totalAttemptCount;

        if (physical.status == Compiler::Status::Compiled &&
            physical.candidateCount > 0)
        {
            result.nextCursor.nextAlternativeIndex = index + 1;
            result.status = Status::CandidateFound;
            result.hasPhysicalCandidates = true;
            result.selectedAlternativeIndex = index;
            result.selectedAlternative = alternative;
            result.physicalCandidates = physical;
            return result;
        }

        if (sharedStateBlocked(physical.infeasibility.reason))
        {
            // The remaining alternatives share this state/law/capability
            // boundary.  Do not burn the frontier budget pretending that a
            // different terminal point repairs invalid shared input.
            result.nextCursor.nextAlternativeIndex = index;
            result.status = Status::SharedStateBlocked;
            return result;
        }

        result.nextCursor.nextAlternativeIndex = index + 1;
    }

    result.status =
        result.nextCursor.nextAlternativeIndex <
                request.frontier.alternativeCount
            ? Status::SearchPending
            : Status::FrontierExhausted;
    return result;
}

} // namespace game::navigation
