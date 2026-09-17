#include "EmergencyContactSeverityScorer.h"

#include <algorithm>
#include <cmath>

namespace world::navigation
{
namespace
{

using Scorer = EmergencyContactSeverityScorer;
using Vec3d = Scorer::Vec3d;

constexpr double kEpsilon = 1.0e-12;

bool finite(double value) noexcept
{
    return std::isfinite(value);
}

bool finite(const Vec3d& value) noexcept
{
    return finite(value.x) && finite(value.y) && finite(value.z);
}

double dot(const Vec3d& a, const Vec3d& b) noexcept
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3d subtract(const Vec3d& a, const Vec3d& b) noexcept
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3d add(const Vec3d& a, const Vec3d& b) noexcept
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3d scale(const Vec3d& value, double factor) noexcept
{
    return {value.x * factor, value.y * factor, value.z * factor};
}

Vec3d cross(const Vec3d& a, const Vec3d& b) noexcept
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

double length(const Vec3d& value) noexcept
{
    return std::sqrt(dot(value, value));
}

bool normalize(const Vec3d& value, Vec3d& normalized) noexcept
{
    if (!finite(value))
        return false;
    const double magnitude = length(value);
    if (!finite(magnitude) || magnitude <= kEpsilon)
        return false;
    normalized = scale(value, 1.0 / magnitude);
    return finite(normalized);
}

bool finiteCandidateScalars(const Scorer::Candidate& candidate) noexcept
{
    return finite(candidate.shipMassKg) && candidate.shipMassKg > 0.0 &&
        finite(candidate.geometryDeficitMeters) &&
        candidate.geometryDeficitMeters >= 0.0 &&
        finite(candidate.passageAxisProgressMetersPerSec);
}

bool finiteWitness(const Scorer::ContactWitness& witness) noexcept
{
    return finite(witness.normalTowardFreeSpaceMap) &&
        finite(witness.contactPointMapMeters) &&
        finite(witness.surfaceVelocityMapMetersPerSec) &&
        finite(witness.shipCenterMapMeters) &&
        finite(witness.shipLinearVelocityMapMetersPerSec) &&
        finite(witness.shipAngularVelocityMapRadPerSec) &&
        finite(witness.effectiveImpactMassKg) &&
        witness.effectiveImpactMassKg >= 0.0;
}

bool lessMetric(double a, double b) noexcept
{
    return a < b - kEpsilon;
}

bool greaterMetric(double a, double b) noexcept
{
    return a > b + kEpsilon;
}

bool scoreBetter(
    const Scorer::CandidateScore& candidate,
    std::uint64_t candidateId,
    std::size_t candidateIndex,
    const Scorer::CandidateScore& incumbent,
    std::uint64_t incumbentId,
    std::size_t incumbentIndex
) noexcept
{
    if (candidate.contactExpected != incumbent.contactExpected)
        return !candidate.contactExpected;

    if (lessMetric(candidate.peakClosingNormalSpeedMetersPerSec,
                   incumbent.peakClosingNormalSpeedMetersPerSec))
        return true;
    if (greaterMetric(candidate.peakClosingNormalSpeedMetersPerSec,
                      incumbent.peakClosingNormalSpeedMetersPerSec))
        return false;

    if (lessMetric(candidate.totalNormalImpactEnergyProxyJ,
                   incumbent.totalNormalImpactEnergyProxyJ))
        return true;
    if (greaterMetric(candidate.totalNormalImpactEnergyProxyJ,
                      incumbent.totalNormalImpactEnergyProxyJ))
        return false;

    if (lessMetric(candidate.totalNormalMomentumProxyKgMetersPerSec,
                   incumbent.totalNormalMomentumProxyKgMetersPerSec))
        return true;
    if (greaterMetric(candidate.totalNormalMomentumProxyKgMetersPerSec,
                      incumbent.totalNormalMomentumProxyKgMetersPerSec))
        return false;

    if (lessMetric(candidate.worstIncidenceFromTangentRad,
                   incumbent.worstIncidenceFromTangentRad))
        return true;
    if (greaterMetric(candidate.worstIncidenceFromTangentRad,
                      incumbent.worstIncidenceFromTangentRad))
        return false;

    if (lessMetric(candidate.geometryDeficitMeters,
                   incumbent.geometryDeficitMeters))
        return true;
    if (greaterMetric(candidate.geometryDeficitMeters,
                      incumbent.geometryDeficitMeters))
        return false;

    if (greaterMetric(candidate.passageAxisProgressMetersPerSec,
                      incumbent.passageAxisProgressMetersPerSec))
        return true;
    if (lessMetric(candidate.passageAxisProgressMetersPerSec,
                   incumbent.passageAxisProgressMetersPerSec))
        return false;

    if (candidateId != incumbentId)
        return candidateId < incumbentId;
    return candidateIndex < incumbentIndex;
}

} // namespace

EmergencyContactSeverityScorer::CandidateScore
EmergencyContactSeverityScorer::evaluateCandidate(
    const Candidate& candidate
) noexcept
{
    CandidateScore score;
    score.geometryDeficitMeters = candidate.geometryDeficitMeters;
    score.passageAxisProgressMetersPerSec =
        candidate.passageAxisProgressMetersPerSec;

    if (!finiteCandidateScalars(candidate) ||
        candidate.contactCount > kMaxContactsPerCandidate)
    {
        return score;
    }

    score.contactExpected = candidate.contactCount > 0;

    for (std::size_t i = 0; i < candidate.contactCount; ++i)
    {
        const ContactWitness& witness = candidate.contacts[i];
        if (!finiteWitness(witness))
            return CandidateScore {};

        Vec3d normal;
        if (!normalize(witness.normalTowardFreeSpaceMap, normal))
            return CandidateScore {};

        const Vec3d leverArm = subtract(
            witness.contactPointMapMeters,
            witness.shipCenterMapMeters
        );

        // Rigid-body contact-point velocity: v_point = v_center + omega x r.
        const Vec3d shipContactPointVelocity = add(
            witness.shipLinearVelocityMapMetersPerSec,
            cross(witness.shipAngularVelocityMapRadPerSec, leverArm)
        );
        const Vec3d relativeContactVelocity = subtract(
            shipContactPointVelocity,
            witness.surfaceVelocityMapMetersPerSec
        );

        const double signedNormalSpeed = dot(relativeContactVelocity, normal);
        const double closingNormalSpeed = std::max(0.0, -signedNormalSpeed);
        const Vec3d tangentialVelocity = subtract(
            relativeContactVelocity,
            scale(normal, signedNormalSpeed)
        );
        const double tangentialSpeed = length(tangentialVelocity);

        const double effectiveMass = witness.effectiveImpactMassKg > 0.0
            ? witness.effectiveImpactMassKg
            : candidate.shipMassKg;
        const double energyProxy =
            0.5 * effectiveMass * closingNormalSpeed * closingNormalSpeed;
        const double momentumProxy = effectiveMass * closingNormalSpeed;
        const double incidenceFromTangent = std::atan2(
            closingNormalSpeed,
            tangentialSpeed
        );

        if (!finite(signedNormalSpeed) ||
            !finite(closingNormalSpeed) ||
            !finite(tangentialSpeed) ||
            !finite(effectiveMass) || effectiveMass <= 0.0 ||
            !finite(energyProxy) ||
            !finite(momentumProxy) ||
            !finite(incidenceFromTangent))
        {
            return CandidateScore {};
        }

        score.peakClosingNormalSpeedMetersPerSec = std::max(
            score.peakClosingNormalSpeedMetersPerSec,
            closingNormalSpeed
        );
        score.totalNormalImpactEnergyProxyJ += energyProxy;
        score.totalNormalMomentumProxyKgMetersPerSec += momentumProxy;
        score.worstIncidenceFromTangentRad = std::max(
            score.worstIncidenceFromTangentRad,
            incidenceFromTangent
        );
        ++score.contactsEvaluated;

        if (!finite(score.totalNormalImpactEnergyProxyJ) ||
            !finite(score.totalNormalMomentumProxyKgMetersPerSec))
        {
            return CandidateScore {};
        }
    }

    score.valid = true;
    return score;
}

EmergencyContactSeverityScorer::SelectionResult
EmergencyContactSeverityScorer::selectBest(
    const Candidate* candidates,
    std::size_t candidateCount
) noexcept
{
    SelectionResult selection;
    if (candidates == nullptr || candidateCount == 0 ||
        candidateCount > kMaxCandidates)
    {
        return selection;
    }

    bool haveBest = false;
    for (std::size_t i = 0; i < candidateCount; ++i)
    {
        ++selection.candidatesEvaluated;
        const CandidateScore score = evaluateCandidate(candidates[i]);
        if (!score.valid)
            continue;

        if (!haveBest || scoreBetter(
                score,
                candidates[i].candidateId,
                i,
                selection.selectedScore,
                selection.selectedCandidateId,
                selection.selectedIndex))
        {
            selection.selectedIndex = i;
            selection.selectedCandidateId = candidates[i].candidateId;
            selection.selectedScore = score;
            haveBest = true;
        }
    }

    selection.valid = haveBest;
    return selection;
}

} // namespace world::navigation
