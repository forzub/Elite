#include "ManeuverDecisionController.h"

#include <algorithm>
#include <cmath>

namespace game::ship::controller
{
namespace
{

using Controller = ManeuverDecisionController;
using Candidate = Controller::Candidate;
using Context = Controller::Context;

constexpr double kEpsilon = 1.0e-12;

bool finiteOrPositiveInfinity(double value) noexcept
{
    return std::isfinite(value) ||
        value == std::numeric_limits<double>::infinity();
}

bool unitInterval(double value) noexcept
{
    return std::isfinite(value) && value >= 0.0 && value <= 1.0;
}

bool validContext(const Context& context) noexcept
{
    return unitInterval(context.maximumPreferredCriticalDamageRisk01);
}

bool validCandidate(const Candidate& candidate) noexcept
{
    return candidate.valid &&
        finiteOrPositiveInfinity(candidate.timeToObjectiveSeconds) &&
        candidate.timeToObjectiveSeconds >= 0.0 &&
        std::isfinite(candidate.entrySpeedMps) &&
        candidate.entrySpeedMps >= 0.0 &&
        std::isfinite(candidate.exitSpeedMps) &&
        candidate.exitSpeedMps >= 0.0 &&
        std::isfinite(candidate.minimumClearanceMeters) &&
        unitInterval(candidate.escapeReserve01) &&
        std::isfinite(candidate.peakClosingNormalSpeedMps) &&
        candidate.peakClosingNormalSpeedMps >= 0.0 &&
        std::isfinite(candidate.normalImpactEnergyProxyJ) &&
        candidate.normalImpactEnergyProxyJ >= 0.0 &&
        unitInterval(candidate.criticalDamageRisk01) &&
        unitInterval(candidate.missionDamageCost01) &&
        unitInterval(candidate.expendableDamageCost01) &&
        std::isfinite(candidate.threatExposure) &&
        candidate.threatExposure >= 0.0;
}

bool less(double a, double b) noexcept
{
    return a < b - kEpsilon;
}

bool greater(double a, double b) noexcept
{
    return a > b + kEpsilon;
}

struct PopulationFacts
{
    bool haveProgress = false;
    bool haveNonContact = false;
    bool haveNonContactProgress = false;
    bool havePreferredCriticalRisk = false;
};

PopulationFacts inspectPopulation(
    const Context& context,
    const Candidate* candidates,
    std::size_t candidateCount
) noexcept
{
    PopulationFacts facts;
    for (std::size_t i = 0; i < candidateCount; ++i)
    {
        const Candidate& candidate = candidates[i];
        if (!validCandidate(candidate))
            continue;

        facts.haveProgress =
            facts.haveProgress || candidate.progressesObjective;
        facts.haveNonContact =
            facts.haveNonContact || !candidate.contactExpected;
        facts.haveNonContactProgress =
            facts.haveNonContactProgress ||
            (candidate.progressesObjective && !candidate.contactExpected);
        facts.havePreferredCriticalRisk =
            facts.havePreferredCriticalRisk ||
            candidate.criticalDamageRisk01 <=
                context.maximumPreferredCriticalDamageRisk01 + kEpsilon;
    }
    return facts;
}

bool progressionPreferred(
    const Context& context,
    const PopulationFacts& facts,
    const Candidate& candidate
) noexcept
{
    if (!facts.haveProgress)
        return true;

    switch (context.progress)
    {
        case Controller::ProgressRequirement::MustProgress:
            return candidate.progressesObjective;
        case Controller::ProgressRequirement::PreferProgress:
            return candidate.progressesObjective;
        case Controller::ProgressRequirement::MayStop:
        default:
            return true;
    }
}

bool contactPolicyPreferred(
    const Context& context,
    const PopulationFacts& facts,
    const Candidate& candidate
) noexcept
{
    if (context.allowExpectedContact || !candidate.contactExpected)
        return true;

    if (context.progress == Controller::ProgressRequirement::MustProgress &&
        facts.haveProgress)
    {
        // If a collision-free progressing candidate exists, use it. If all
        // progressing candidates expect contact, keep them eligible: MustProgress
        // is allowed to escalate rather than collapsing to no command.
        return !facts.haveNonContactProgress;
    }

    // In non-mandatory progress modes, a non-contact candidate (including a
    // physically safe stop/brake) keeps contact-expected choices below the
    // preferred set. If none exists, emergency candidates remain selectable.
    return !facts.haveNonContact;
}

bool commonPreferred(
    const Context& context,
    const PopulationFacts& facts,
    const Candidate& candidate
) noexcept
{
    if (!progressionPreferred(context, facts, candidate))
        return false;

    if (facts.havePreferredCriticalRisk &&
        candidate.criticalDamageRisk01 >
            context.maximumPreferredCriticalDamageRisk01 + kEpsilon)
    {
        return false;
    }

    return contactPolicyPreferred(context, facts, candidate);
}

bool commonBetter(
    const Context& context,
    const PopulationFacts& facts,
    const Candidate& a,
    const Candidate& b
) noexcept
{
    const bool aPreferred = commonPreferred(context, facts, a);
    const bool bPreferred = commonPreferred(context, facts, b);
    if (aPreferred != bPreferred)
        return aPreferred;

    // Catastrophic/critical-system risk is never traded casually for style,
    // comfort or a small time gain. If every candidate exceeds the preferred
    // band this still chooses the least bad survivor.
    if (less(a.criticalDamageRisk01, b.criticalDamageRisk01))
        return true;
    if (greater(a.criticalDamageRisk01, b.criticalDamageRisk01))
        return false;

    if (context.progress == Controller::ProgressRequirement::PreferProgress &&
        a.progressesObjective != b.progressesObjective)
    {
        return a.progressesObjective;
    }

    return false;
}

bool rationalBetter(const Candidate& a, const Candidate& b) noexcept
{
    if (a.collisionFree != b.collisionFree)
        return a.collisionFree;

    if (less(a.missionDamageCost01, b.missionDamageCost01))
        return true;
    if (greater(a.missionDamageCost01, b.missionDamageCost01))
        return false;

    if (less(a.peakClosingNormalSpeedMps, b.peakClosingNormalSpeedMps))
        return true;
    if (greater(a.peakClosingNormalSpeedMps, b.peakClosingNormalSpeedMps))
        return false;

    if (greater(a.escapeReserve01, b.escapeReserve01))
        return true;
    if (less(a.escapeReserve01, b.escapeReserve01))
        return false;

    if (less(a.threatExposure, b.threatExposure))
        return true;
    if (greater(a.threatExposure, b.threatExposure))
        return false;

    if (less(a.expendableDamageCost01, b.expendableDamageCost01))
        return true;
    if (greater(a.expendableDamageCost01, b.expendableDamageCost01))
        return false;

    if (less(a.timeToObjectiveSeconds, b.timeToObjectiveSeconds))
        return true;
    if (greater(a.timeToObjectiveSeconds, b.timeToObjectiveSeconds))
        return false;

    return false;
}

bool precisionBetter(const Candidate& a, const Candidate& b) noexcept
{
    if (a.collisionFree != b.collisionFree)
        return a.collisionFree;

    if (less(a.missionDamageCost01, b.missionDamageCost01))
        return true;
    if (greater(a.missionDamageCost01, b.missionDamageCost01))
        return false;

    if (less(a.peakClosingNormalSpeedMps, b.peakClosingNormalSpeedMps))
        return true;
    if (greater(a.peakClosingNormalSpeedMps, b.peakClosingNormalSpeedMps))
        return false;

    if (greater(a.minimumClearanceMeters, b.minimumClearanceMeters))
        return true;
    if (less(a.minimumClearanceMeters, b.minimumClearanceMeters))
        return false;

    if (less(a.entrySpeedMps, b.entrySpeedMps))
        return true;
    if (greater(a.entrySpeedMps, b.entrySpeedMps))
        return false;

    if (greater(a.escapeReserve01, b.escapeReserve01))
        return true;
    if (less(a.escapeReserve01, b.escapeReserve01))
        return false;

    return false;
}

bool extremeBetter(const Candidate& a, const Candidate& b) noexcept
{
    if (a.progressesObjective != b.progressesObjective)
        return a.progressesObjective;

    if (less(a.timeToObjectiveSeconds, b.timeToObjectiveSeconds))
        return true;
    if (greater(a.timeToObjectiveSeconds, b.timeToObjectiveSeconds))
        return false;

    if (less(a.threatExposure, b.threatExposure))
        return true;
    if (greater(a.threatExposure, b.threatExposure))
        return false;

    if (greater(a.exitSpeedMps, b.exitSpeedMps))
        return true;
    if (less(a.exitSpeedMps, b.exitSpeedMps))
        return false;

    if (less(a.missionDamageCost01, b.missionDamageCost01))
        return true;
    if (greater(a.missionDamageCost01, b.missionDamageCost01))
        return false;

    if (less(a.peakClosingNormalSpeedMps, b.peakClosingNormalSpeedMps))
        return true;
    if (greater(a.peakClosingNormalSpeedMps, b.peakClosingNormalSpeedMps))
        return false;

    if (less(a.normalImpactEnergyProxyJ, b.normalImpactEnergyProxyJ))
        return true;
    if (greater(a.normalImpactEnergyProxyJ, b.normalImpactEnergyProxyJ))
        return false;

    if (greater(a.escapeReserve01, b.escapeReserve01))
        return true;
    if (less(a.escapeReserve01, b.escapeReserve01))
        return false;

    if (less(a.expendableDamageCost01, b.expendableDamageCost01))
        return true;
    if (greater(a.expendableDamageCost01, b.expendableDamageCost01))
        return false;

    return false;
}

bool combatEscapeBetter(const Candidate& a, const Candidate& b) noexcept
{
    if (a.progressesObjective != b.progressesObjective)
        return a.progressesObjective;

    if (less(a.threatExposure, b.threatExposure))
        return true;
    if (greater(a.threatExposure, b.threatExposure))
        return false;

    if (less(a.timeToObjectiveSeconds, b.timeToObjectiveSeconds))
        return true;
    if (greater(a.timeToObjectiveSeconds, b.timeToObjectiveSeconds))
        return false;

    if (greater(a.exitSpeedMps, b.exitSpeedMps))
        return true;
    if (less(a.exitSpeedMps, b.exitSpeedMps))
        return false;

    if (less(a.missionDamageCost01, b.missionDamageCost01))
        return true;
    if (greater(a.missionDamageCost01, b.missionDamageCost01))
        return false;

    if (less(a.peakClosingNormalSpeedMps, b.peakClosingNormalSpeedMps))
        return true;
    if (greater(a.peakClosingNormalSpeedMps, b.peakClosingNormalSpeedMps))
        return false;

    return false;
}

bool doctrineBetter(
    Controller::Doctrine doctrine,
    const Candidate& a,
    const Candidate& b
) noexcept
{
    switch (doctrine)
    {
        case Controller::Doctrine::PrecisionRetrieval:
            return precisionBetter(a, b);
        case Controller::Doctrine::Extreme:
            return extremeBetter(a, b);
        case Controller::Doctrine::CombatEscape:
            return combatEscapeBetter(a, b);
        case Controller::Doctrine::Rational:
        default:
            return rationalBetter(a, b);
    }
}

} // namespace

ManeuverDecisionController::Selection ManeuverDecisionController::select(
    const Context& context,
    const Candidate* candidates,
    std::size_t candidateCount
) noexcept
{
    Selection result;
    if (!validContext(context) ||
        candidates == nullptr ||
        candidateCount == 0 ||
        candidateCount > kMaxCandidates)
    {
        return result;
    }

    const PopulationFacts facts =
        inspectPopulation(context, candidates, candidateCount);

    bool haveBest = false;
    std::size_t bestIndex = 0;

    for (std::size_t i = 0; i < candidateCount; ++i)
    {
        const Candidate& candidate = candidates[i];
        if (!validCandidate(candidate))
            continue;

        ++result.candidatesEvaluated;

        if (!haveBest)
        {
            bestIndex = i;
            haveBest = true;
            continue;
        }

        const Candidate& incumbent = candidates[bestIndex];

        const bool candidateCommon =
            commonBetter(context, facts, candidate, incumbent);
        const bool incumbentCommon =
            commonBetter(context, facts, incumbent, candidate);

        if (candidateCommon != incumbentCommon)
        {
            if (candidateCommon)
                bestIndex = i;
            continue;
        }

        const bool candidateDoctrine =
            doctrineBetter(context.doctrine, candidate, incumbent);
        const bool incumbentDoctrine =
            doctrineBetter(context.doctrine, incumbent, candidate);

        if (candidateDoctrine != incumbentDoctrine)
        {
            if (candidateDoctrine)
                bestIndex = i;
            continue;
        }

        if (candidate.candidateId < incumbent.candidateId)
            bestIndex = i;
    }

    if (!haveBest)
        return result;

    const Candidate& selected = candidates[bestIndex];
    result.valid = true;
    result.selectedIndex = bestIndex;
    result.selectedCandidateId = selected.candidateId;
    result.selectedCollisionFree = selected.collisionFree;
    result.selectedContactExpected = selected.contactExpected;
    result.selectedProgressing = selected.progressesObjective;
    return result;
}

} // namespace game::ship::controller
