#include "game/ship/controller/ManeuverDecisionController.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

using Controller = game::ship::controller::ManeuverDecisionController;
using Candidate = Controller::Candidate;
using Context = Controller::Context;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

Candidate candidate(
    std::uint64_t id,
    Controller::CandidateFamily family,
    bool collisionFree,
    bool contactExpected,
    bool progresses
)
{
    Candidate result;
    result.candidateId = id;
    result.family = family;
    result.valid = true;
    result.collisionFree = collisionFree;
    result.contactExpected = contactExpected;
    result.progressesObjective = progresses;
    result.timeToObjectiveSeconds = progresses ? 10.0 : 100.0;
    result.entrySpeedMps = 5.0;
    result.exitSpeedMps = progresses ? 20.0 : 0.0;
    result.minimumClearanceMeters = collisionFree ? 5.0 : -0.5;
    result.escapeReserve01 = progresses ? 0.5 : 1.0;
    result.criticalDamageRisk01 = 0.0;
    result.missionDamageCost01 = 0.0;
    result.expendableDamageCost01 = 0.0;
    result.threatExposure = progresses ? 5.0 : 10.0;
    result.peakClosingNormalSpeedMps = contactExpected ? 2.0 : 0.0;
    result.normalImpactEnergyProxyJ = contactExpected ? 100.0 : 0.0;
    return result;
}

void testRationalMayStopRatherThanAcceptContact()
{
    Candidate candidates[2];
    candidates[0] = candidate(
        1,
        Controller::CandidateFamily::StopOrBrake,
        true,
        false,
        false
    );
    candidates[0].stopsOrBrakes = true;

    candidates[1] = candidate(
        2,
        Controller::CandidateFamily::EmergencyContact,
        false,
        true,
        true
    );
    candidates[1].timeToObjectiveSeconds = 2.0;
    candidates[1].exitSpeedMps = 60.0;

    Context context;
    context.doctrine = Controller::Doctrine::Rational;
    context.progress = Controller::ProgressRequirement::MayStop;
    context.allowExpectedContact = false;

    const auto result = Controller::select(context, candidates, 2);
    require(result.valid && result.selectedCandidateId == 1,
            "rational MayStop must prefer a safe brake over unnecessary contact");
}

void testMustProgressDoesNotCollapseToStop()
{
    Candidate candidates[2];
    candidates[0] = candidate(
        10,
        Controller::CandidateFamily::StopOrBrake,
        true,
        false,
        false
    );
    candidates[0].stopsOrBrakes = true;

    candidates[1] = candidate(
        11,
        Controller::CandidateFamily::EmergencyContact,
        false,
        true,
        true
    );
    candidates[1].criticalDamageRisk01 = 0.05;
    candidates[1].missionDamageCost01 = 0.10;

    Context context;
    context.doctrine = Controller::Doctrine::Rational;
    context.progress = Controller::ProgressRequirement::MustProgress;
    context.allowExpectedContact = false;

    const auto result = Controller::select(context, candidates, 2);
    require(result.valid &&
            result.selectedCandidateId == 11 &&
            result.selectedContactExpected &&
            result.selectedProgressing,
            "MustProgress must retain a contact-expected progressing command when stop is the only non-contact option");
}

void testExtremeMayTradeExpendableDamageForSpeed()
{
    Candidate candidates[2];
    candidates[0] = candidate(
        20,
        Controller::CandidateFamily::LocalVisibility,
        true,
        false,
        true
    );
    candidates[0].timeToObjectiveSeconds = 12.0;
    candidates[0].exitSpeedMps = 15.0;
    candidates[0].threatExposure = 8.0;

    candidates[1] = candidate(
        21,
        Controller::CandidateFamily::EmergencyContact,
        false,
        true,
        true
    );
    candidates[1].timeToObjectiveSeconds = 3.0;
    candidates[1].exitSpeedMps = 70.0;
    candidates[1].threatExposure = 2.0;
    candidates[1].expendableDamageCost01 = 0.60;
    candidates[1].missionDamageCost01 = 0.05;
    candidates[1].criticalDamageRisk01 = 0.0;

    Context context;
    context.doctrine = Controller::Doctrine::Extreme;
    context.progress = Controller::ProgressRequirement::MustProgress;
    context.allowExpectedContact = true;
    context.allowSacrificialComponentLoss = true;

    const auto result = Controller::select(context, candidates, 2);
    require(result.valid && result.selectedCandidateId == 21,
            "Extreme may accept expendable contact damage for much faster escape/progress");
}

void testExtremeStillRejectsCatastrophicShortcut()
{
    Candidate candidates[2];
    candidates[0] = candidate(
        30,
        Controller::CandidateFamily::LocalVisibility,
        true,
        false,
        true
    );
    candidates[0].timeToObjectiveSeconds = 8.0;
    candidates[0].criticalDamageRisk01 = 0.10;

    candidates[1] = candidate(
        31,
        Controller::CandidateFamily::EmergencyContact,
        false,
        true,
        true
    );
    candidates[1].timeToObjectiveSeconds = 1.0;
    candidates[1].exitSpeedMps = 100.0;
    candidates[1].criticalDamageRisk01 = 0.90;

    Context context;
    context.doctrine = Controller::Doctrine::Extreme;
    context.progress = Controller::ProgressRequirement::MustProgress;
    context.allowExpectedContact = true;
    context.allowSacrificialComponentLoss = true;
    context.maximumPreferredCriticalDamageRisk01 = 0.20;

    const auto result = Controller::select(context, candidates, 2);
    require(result.valid && result.selectedCandidateId == 30,
            "Extreme must not trade a preferred-survival candidate for a catastrophic shortcut");
}

void testCombatEscapePrefersLowerThreatExposure()
{
    Candidate candidates[2];
    candidates[0] = candidate(
        40,
        Controller::CandidateFamily::LocalVisibility,
        true,
        false,
        true
    );
    candidates[0].timeToObjectiveSeconds = 5.0;
    candidates[0].exitSpeedMps = 50.0;
    candidates[0].threatExposure = 9.0;

    candidates[1] = candidate(
        41,
        Controller::CandidateFamily::PrecisionPassage,
        true,
        false,
        true
    );
    candidates[1].timeToObjectiveSeconds = 6.0;
    candidates[1].exitSpeedMps = 45.0;
    candidates[1].threatExposure = 2.0;

    Context context;
    context.doctrine = Controller::Doctrine::CombatEscape;
    context.progress = Controller::ProgressRequirement::MustProgress;

    const auto result = Controller::select(context, candidates, 2);
    require(result.valid && result.selectedCandidateId == 41,
            "CombatEscape must prefer the lower threat/silhouette exposure when survival risk is otherwise equal");
}

void testPrecisionRetrievalPrefersClearanceAndLowEntrySpeed()
{
    Candidate candidates[2];
    candidates[0] = candidate(
        50,
        Controller::CandidateFamily::PrecisionPassage,
        true,
        false,
        true
    );
    candidates[0].minimumClearanceMeters = 2.0;
    candidates[0].entrySpeedMps = 8.0;

    candidates[1] = candidate(
        51,
        Controller::CandidateFamily::PrecisionPassage,
        true,
        false,
        true
    );
    candidates[1].minimumClearanceMeters = 8.0;
    candidates[1].entrySpeedMps = 2.0;
    candidates[1].timeToObjectiveSeconds = 20.0;

    Context context;
    context.doctrine = Controller::Doctrine::PrecisionRetrieval;
    context.progress = Controller::ProgressRequirement::MustProgress;

    const auto result = Controller::select(context, candidates, 2);
    require(result.valid && result.selectedCandidateId == 51,
            "Precision retrieval must prefer control margin over raw transit time");
}

void testSacrificialDamageNeedsExplicitPermission()
{
    Candidate candidates[2];
    candidates[0] = candidate(
        60,
        Controller::CandidateFamily::LocalVisibility,
        true,
        false,
        true
    );
    candidates[0].timeToObjectiveSeconds = 10.0;

    candidates[1] = candidate(
        61,
        Controller::CandidateFamily::EmergencyContact,
        false,
        true,
        true
    );
    candidates[1].timeToObjectiveSeconds = 2.0;
    candidates[1].expendableDamageCost01 = 0.70;

    Context context;
    context.doctrine = Controller::Doctrine::Extreme;
    context.progress = Controller::ProgressRequirement::MustProgress;
    context.allowExpectedContact = true;
    context.allowSacrificialComponentLoss = false;

    const auto conservative = Controller::select(context, candidates, 2);
    require(conservative.valid && conservative.selectedCandidateId == 60,
            "without sacrificial permission, expendable component loss must remain a real cost");

    context.allowSacrificialComponentLoss = true;
    const auto sacrificial = Controller::select(context, candidates, 2);
    require(sacrificial.valid && sacrificial.selectedCandidateId == 61,
            "explicit sacrificial permission may let Extreme trade expendable hardware for time");
}

} // namespace

int main()
{
    try
    {
        testRationalMayStopRatherThanAcceptContact();
        testMustProgressDoesNotCollapseToStop();
        testExtremeMayTradeExpendableDamageForSpeed();
        testExtremeStillRejectsCatastrophicShortcut();
        testCombatEscapePrefersLowerThreatExposure();
        testPrecisionRetrievalPrefersClearanceAndLowEntrySpeed();
        testSacrificialDamageNeedsExplicitPermission();

        std::cout << "MANEUVER DECISION CONTROLLER TESTS: PASS\n";
        std::cout << " - Rational may brake instead of taking unnecessary contact\n";
        std::cout << " - MustProgress never collapses to stop merely because safe progress is unavailable\n";
        std::cout << " - Extreme may sacrifice expendable structure for speed\n";
        std::cout << " - catastrophic-risk envelope remains above doctrine/style\n";
        std::cout << " - CombatEscape consumes threat/silhouette exposure\n";
        std::cout << " - PrecisionRetrieval favors clearance and low entry speed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "MANEUVER DECISION CONTROLLER TESTS: FAIL: "
            << error.what() << "\n";
        return 1;
    }
}
