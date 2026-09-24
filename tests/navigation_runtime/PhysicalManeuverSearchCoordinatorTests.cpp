#include "src/game/navigation/PhysicalManeuverSearchCoordinator.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

using Coordinator =
    game::navigation::PhysicalManeuverSearchCoordinator;
using Compiler = game::navigation::OrdinaryPhysicalManeuverCompiler;
using Law = game::navigation::LocalFlightControlLaw;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

Compiler::Query basePhysicalQuery()
{
    Compiler::Query query;
    query.controlLaw = Law::Newtonian;
    query.state.positionMapMeters = {0.0, 0.0, 0.0};
    query.state.velocityMapMetersPerSecond = {18.0, 0.0, 0.0};
    query.state.forwardMap = {1.0, 0.0, 0.0};
    query.state.rightMap = {0.0, 0.0, 1.0};
    query.state.upMap = {0.0, 1.0, 0.0};
    query.state.angularVelocityMapRadPerSecond = {0.0, 0.0, 0.0};

    query.capability.maxForwardAccelerationMps2 = 73.5;
    query.capability.maxReverseAccelerationMps2 = 2.0;
    query.capability.maxLateralAccelerationMps2 = 2.0;
    query.capability.maxVerticalAccelerationMps2 = 2.0;
    query.capability.maxAngularAccelerationRadPerSec2 = 1.5;
    query.capability.maxAngularSpeedRadPerSec = 1.0;

    query.velocityResponsePerSecond = 0.75;
    query.linearFeedbackReserveMps2 = 0.5;
    query.angularFeedbackReserveRadPerSec2 = 0.1;
    query.controlResponseReserveSeconds = 0.18;
    return query;
}

Coordinator::Alternative alternative(
    std::uint64_t id,
    double horizonSeconds,
    const glm::dvec3& desiredVelocity = {0.0, 18.0, 0.0}
)
{
    Coordinator::Alternative value;
    value.identity.corridorAlternativeId = 100 + id;
    value.identity.terminalAlternativeId = 200 + id;
    value.identity.speedScheduleAlternativeId = 300 + id;
    value.identity.arrivalTimeAlternativeId = 400 + id;
    value.targetPositionMapMeters = {150.0, 100.0, 0.0};
    value.desiredVelocityMapMetersPerSecond = desiredVelocity;
    value.maximumProgramSeconds = horizonSeconds;
    return value;
}

Coordinator::Request requestWithTwoHorizons()
{
    Coordinator::Request request;
    request.commonPhysicalQuery = basePhysicalQuery();
    request.frontier.objectiveRevision = 17;
    request.frontier.frontierRevision = 91;
    request.frontier.alternativeCount = 2;
    request.frontier.alternatives[0] = alternative(1, 0.5);
    // The requested delta-v is 135 degrees from the initial forward axis.
    // This capability/policy needs about 4.60 s merely to acquire attitude;
    // 6.0 s leaves a real bounded burn window after rotation.
    request.frontier.alternatives[1] = alternative(2, 6.0);
    request.cursor.objectiveRevision = 17;
    request.cursor.frontierRevision = 91;
    request.policy.maximumAttemptsPerAdvance = 2;
    return request;
}

void testSearchContinuesFromShortHorizonToPhysicalCandidate()
{
    const auto result = Coordinator::advance(requestWithTwoHorizons());

    require(
        result.status == Coordinator::Status::CandidateFound,
        "later alternative did not clear the 135-degree rotate-before-burn bound"
    );
    require(result.objectiveRemainsActive,
            "physical retry cancelled the mission objective");
    require(result.attemptCount == 2,
            "coordinator did not consume the expected bounded attempts");
    require(
        result.attempts[0].infeasibility.reason ==
            Compiler::InfeasibilityReason::ProgramHorizonTooShort,
        "first retry lost its short-horizon witness"
    );
    require(result.hasPhysicalCandidates,
            "successful retry did not expose compiler candidates");
    require(result.selectedAlternativeIndex == 1,
            "wrong ranked alternative selected");
    require(
        result.selectedAlternative.identity.arrivalTimeAlternativeId == 402,
        "selected alternative lost its provenance identity"
    );
    require(result.nextCursor.nextAlternativeIndex == 2,
            "resume cursor did not advance past the selected alternative");
    require(result.nextCursor.totalAttemptCount == 2,
            "persistent attempt counter is incorrect");
}

void testBudgetReturnsPendingAndResumeDoesNotRepeatWork()
{
    auto firstRequest = requestWithTwoHorizons();
    firstRequest.policy.maximumAttemptsPerAdvance = 1;

    const auto first = Coordinator::advance(firstRequest);
    require(first.status == Coordinator::Status::SearchPending,
            "bounded slice did not report pending search");
    require(first.attemptCount == 1,
            "bounded slice exceeded its explicit attempt budget");
    require(first.nextCursor.nextAlternativeIndex == 1,
            "pending cursor did not preserve search progress");
    require(first.objectiveRemainsActive,
            "pending search disabled the objective");

    auto secondRequest = firstRequest;
    secondRequest.cursor = first.nextCursor;
    const auto second = Coordinator::advance(secondRequest);

    require(second.status == Coordinator::Status::CandidateFound,
            "resumed search did not reach the next physical alternative");
    require(second.attemptCount == 1,
            "resumed search repeated a previous alternative");
    require(second.attempts[0].alternativeIndex == 1,
            "resumed search restarted from the beginning");
    require(second.nextCursor.totalAttemptCount == 2,
            "total attempt count did not survive the worker boundary");
}

void testExhaustedFrontierKeepsObjectiveActiveAndWitnessesVisible()
{
    auto request = requestWithTwoHorizons();
    request.commonPhysicalQuery.capability.maxAngularAccelerationRadPerSec2 = 0.0;
    request.commonPhysicalQuery.capability.maxAngularSpeedRadPerSec = 0.0;
    request.frontier.alternatives[0] = alternative(1, 4.0);
    request.frontier.alternatives[1] =
        alternative(2, 4.0, {0.0, -18.0, 0.0});

    const auto result = Coordinator::advance(request);
    require(result.status == Coordinator::Status::FrontierExhausted,
            "all rejected alternatives did not exhaust the frontier");
    require(result.objectiveRemainsActive,
            "frontier exhaustion disabled navigation ownership");
    require(!result.hasPhysicalCandidates,
            "exhausted frontier fabricated a physical candidate");
    require(result.attemptCount == 2,
            "exhausted frontier lost rejection history");

    for (std::size_t i = 0; i < result.attemptCount; ++i)
    {
        require(
            result.attempts[i].infeasibility.reason ==
                Compiler::InfeasibilityReason::AttitudeAuthorityUnavailable,
            "frontier rejection history lost its limiting constraint"
        );
    }
}

void testSharedStateBlockerDoesNotWasteRemainingFrontier()
{
    auto request = requestWithTwoHorizons();
    request.commonPhysicalQuery.controlLaw = Law::Assisted;

    const auto result = Coordinator::advance(request);
    require(result.status == Coordinator::Status::SharedStateBlocked,
            "unsupported shared law was treated as an alternative-local miss");
    require(result.attemptCount == 1,
            "shared-state blocker consumed unrelated alternatives");
    require(result.nextCursor.nextAlternativeIndex == 0,
            "shared-state blocker destroyed the resumable frontier cursor");
    require(result.objectiveRemainsActive,
            "shared-state blocker disabled the objective");
    require(
        result.attempts[0].infeasibility.reason ==
            Compiler::InfeasibilityReason::UnsupportedControlLaw,
        "shared-state blocker lost the compiler witness"
    );
}

void testStaleCursorFailsBeforeAnyPhysicalAttempt()
{
    auto request = requestWithTwoHorizons();
    request.cursor.objectiveRevision = 16;

    const auto result = Coordinator::advance(request);
    require(result.status == Coordinator::Status::InvalidInput,
            "stale cursor crossed objective revisions");
    require(result.attemptCount == 0,
            "stale cursor reached the physical compiler");
    require(result.objectiveRemainsActive,
            "invalid search state silently cancelled the objective");
}

void testRebuiltFrontierCannotReuseOldCursor()
{
    auto request = requestWithTwoHorizons();
    request.cursor.frontierRevision = 90;

    const auto result = Coordinator::advance(request);
    require(result.status == Coordinator::Status::InvalidInput,
            "cursor from an older frontier entered the rebuilt search batch");
    require(result.attemptCount == 0,
            "stale frontier cursor reached the physical compiler");
}

void testInvalidAlternativeProvenanceFailsBeforeSearch()
{
    auto request = requestWithTwoHorizons();
    request.frontier.alternatives[1].identity.terminalAlternativeId = 0;

    const auto result = Coordinator::advance(request);
    require(result.status == Coordinator::Status::InvalidInput,
            "alternative without provenance entered physical search");
    require(result.attemptCount == 0,
            "invalid frontier partially consumed its valid prefix");
}

void testSpatialRejectionRetriesDifferentTerminalAtSameVelocity()
{
    auto request = requestWithTwoHorizons();
    request.commonPhysicalQuery.state.velocityMapMetersPerSecond =
        {0.0, 0.0, 0.0};
    request.commonPhysicalQuery.state.forwardMap = {1.0, 0.0, 0.0};
    request.commonPhysicalQuery.state.rightMap = {0.0, 0.0, 1.0};
    request.frontier.alternatives[0] = alternative(1, 3.0, {18.0, 0.0, 0.0});
    request.frontier.alternatives[0].targetPositionMapMeters =
        {-100.0, 0.0, 0.0};
    request.frontier.alternatives[1] = alternative(2, 3.0, {18.0, 0.0, 0.0});
    request.frontier.alternatives[1].targetPositionMapMeters =
        {100.0, 0.0, 0.0};

    const auto result = Coordinator::advance(request);
    require(result.status == Coordinator::Status::CandidateFound,
            "spatial rejection did not continue to the reachable terminal");
    require(result.attemptCount == 2 &&
                result.attempts[0].infeasibility.reason ==
                    Compiler::InfeasibilityReason::SpatialTargetNotApproached,
            "spatial rejection did not preserve its typed witness");
    require(result.selectedAlternativeIndex == 1,
            "coordinator selected a velocity-only result aimed away from target");
}

} // namespace

int main()
{
    try
    {
        testSearchContinuesFromShortHorizonToPhysicalCandidate();
        testBudgetReturnsPendingAndResumeDoesNotRepeatWork();
        testExhaustedFrontierKeepsObjectiveActiveAndWitnessesVisible();
        testSharedStateBlockerDoesNotWasteRemainingFrontier();
        testStaleCursorFailsBeforeAnyPhysicalAttempt();
        testRebuiltFrontierCannotReuseOldCursor();
        testInvalidAlternativeProvenanceFailsBeforeSearch();
        testSpatialRejectionRetriesDifferentTerminalAtSameVelocity();

        std::cout << "PHYSICAL MANEUVER SEARCH COORDINATOR TESTS: PASS\n";
        std::cout << " - typed rejection advances to the next ranked alternative\n";
        std::cout << " - explicit per-slice budget returns a resumable cursor\n";
        std::cout << " - frontier exhaustion keeps objective ownership active\n";
        std::cout << " - shared-state blockers preserve untried alternatives\n";
        std::cout << " - stale objective revisions fail before physical work\n";
        std::cout << " - rebuilt frontiers cannot reuse old cursors\n";
        std::cout << " - incomplete alternative provenance invalidates the frontier atomically\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "PHYSICAL MANEUVER SEARCH COORDINATOR TESTS: FAIL: "
            << error.what() << "\n";
        return 1;
    }
}
