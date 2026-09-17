#include "world/navigation/trajectory/EmergencyContactSeverityScorer.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

using Scorer = world::navigation::EmergencyContactSeverityScorer;

constexpr double kPi = 3.141592653589793238462643383279502884;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void requireNear(double actual, double expected, double tolerance, const std::string& message)
{
    if (std::abs(actual - expected) > tolerance)
        throw std::runtime_error(message);
}

Scorer::Candidate baseCandidate(std::uint64_t id)
{
    Scorer::Candidate candidate;
    candidate.candidateId = id;
    candidate.shipMassKg = 1000.0;
    candidate.geometryDeficitMeters = 0.25;
    candidate.passageAxisProgressMetersPerSec = 10.0;
    return candidate;
}

Scorer::ContactWitness witness(
    Scorer::Vec3d shipVelocity,
    Scorer::Vec3d normal = {1.0, 0.0, 0.0}
)
{
    Scorer::ContactWitness value;
    value.normalTowardFreeSpaceMap = normal;
    value.shipLinearVelocityMapMetersPerSec = shipVelocity;
    return value;
}

void testGlancingContactBeatsMoreNormalImpact()
{
    Scorer::Candidate normalImpact = baseCandidate(10);
    normalImpact.contactCount = 1;
    normalImpact.contacts[0] = witness({-10.0, 0.0, 0.0});

    Scorer::Candidate glancingImpact = baseCandidate(20);
    glancingImpact.contactCount = 1;
    glancingImpact.contacts[0] = witness({-2.0, 20.0, 0.0});

    const Scorer::Candidate candidates[] {normalImpact, glancingImpact};
    const Scorer::SelectionResult selected = Scorer::selectBest(candidates, 2);

    require(selected.valid, "glancing comparison must produce a valid selection");
    require(selected.selectedCandidateId == 20,
            "lower normal contact speed must beat a slower-total-speed normal hit");
    requireNear(selected.selectedScore.peakClosingNormalSpeedMetersPerSec,
                2.0, 1.0e-9,
                "glancing fixture must expose two meters per second normal speed");
    require(selected.selectedScore.worstIncidenceFromTangentRad < 0.2,
            "glancing fixture must retain a shallow incidence angle");
}

void testAngularVelocityContributesOmegaCrossRAtContactPoint()
{
    Scorer::Candidate candidate = baseCandidate(1);
    candidate.contactCount = 1;
    candidate.contacts[0] = witness({0.0, 0.0, 0.0});
    candidate.contacts[0].shipCenterMapMeters = {0.0, 0.0, 0.0};
    candidate.contacts[0].contactPointMapMeters = {0.0, 1.0, 0.0};
    candidate.contacts[0].shipAngularVelocityMapRadPerSec = {0.0, 0.0, 1.0};

    const Scorer::CandidateScore score = Scorer::evaluateCandidate(candidate);

    require(score.valid, "rotating contact witness must be valid");
    requireNear(score.peakClosingNormalSpeedMetersPerSec, 1.0, 1.0e-9,
                "omega cross r must add the rotating point velocity");
    requireNear(score.totalNormalImpactEnergyProxyJ, 500.0, 1.0e-9,
                "1000 kg at one meter per second normal speed must yield 500 J proxy");
}

void testMovingSurfaceUsesRelativeContactVelocity()
{
    Scorer::Candidate staticSurface = baseCandidate(1);
    staticSurface.contactCount = 1;
    staticSurface.contacts[0] = witness({-5.0, 0.0, 0.0});

    Scorer::Candidate movingSurface = baseCandidate(2);
    movingSurface.contactCount = 1;
    movingSurface.contacts[0] = witness({-5.0, 0.0, 0.0});
    movingSurface.contacts[0].surfaceVelocityMapMetersPerSec = {-4.0, 0.0, 0.0};

    const Scorer::Candidate candidates[] {staticSurface, movingSurface};
    const Scorer::SelectionResult selected = Scorer::selectBest(candidates, 2);

    require(selected.valid, "moving-surface comparison must be valid");
    require(selected.selectedCandidateId == 2,
            "surface motion reducing relative normal speed must lower severity");
    requireNear(selected.selectedScore.peakClosingNormalSpeedMetersPerSec,
                1.0, 1.0e-9,
                "relative contact speed must subtract surface velocity");
}

void testEffectiveImpactMassBreaksEqualNormalSpeedTie()
{
    Scorer::Candidate heavy = baseCandidate(1);
    heavy.contactCount = 1;
    heavy.contacts[0] = witness({-2.0, 0.0, 0.0});
    heavy.contacts[0].effectiveImpactMassKg = 1000.0;

    Scorer::Candidate light = baseCandidate(2);
    light.contactCount = 1;
    light.contacts[0] = witness({-2.0, 0.0, 0.0});
    light.contacts[0].effectiveImpactMassKg = 100.0;

    const Scorer::Candidate candidates[] {heavy, light};
    const Scorer::SelectionResult selected = Scorer::selectBest(candidates, 2);

    require(selected.valid, "effective-mass comparison must be valid");
    require(selected.selectedCandidateId == 2,
            "lower normal energy proxy must break equal-normal-speed tie");
    requireNear(selected.selectedScore.totalNormalImpactEnergyProxyJ,
                200.0, 1.0e-9,
                "100 kg at two meters per second normal speed must yield 200 J proxy");
}

void testNoContactCandidateAlwaysBeatsEmergencyContact()
{
    Scorer::Candidate noContact = baseCandidate(50);
    noContact.geometryDeficitMeters = 10.0;
    noContact.passageAxisProgressMetersPerSec = 1.0;

    Scorer::Candidate contact = baseCandidate(1);
    contact.contactCount = 1;
    contact.contacts[0] = witness({-0.01, 100.0, 0.0});
    contact.geometryDeficitMeters = 0.0;
    contact.passageAxisProgressMetersPerSec = 100.0;

    const Scorer::Candidate candidates[] {contact, noContact};
    const Scorer::SelectionResult selected = Scorer::selectBest(candidates, 2);

    require(selected.valid, "safe/no-contact fixture must produce a selection");
    require(selected.selectedCandidateId == 50,
            "no predicted contact must beat any emergency contact candidate");
    require(!selected.selectedScore.contactExpected,
            "selected no-contact candidate must remain explicitly contact-free");
}

void testNormalImpactReportsNinetyDegreeIncidence()
{
    Scorer::Candidate candidate = baseCandidate(1);
    candidate.contactCount = 1;
    candidate.contacts[0] = witness({-3.0, 0.0, 0.0});

    const Scorer::CandidateScore score = Scorer::evaluateCandidate(candidate);
    require(score.valid, "normal-impact fixture must be valid");
    requireNear(score.worstIncidenceFromTangentRad, 0.5 * kPi, 1.0e-9,
                "pure normal impact must be ninety degrees from tangent");
}

void testInvalidWitnessFailsClosedAndBoundIsHard()
{
    Scorer::Candidate invalid = baseCandidate(1);
    invalid.contactCount = 1;
    invalid.contacts[0] = witness({-1.0, 0.0, 0.0}, {0.0, 0.0, 0.0});

    require(!Scorer::evaluateCandidate(invalid).valid,
            "zero contact normal must fail closed");

    Scorer::Candidate candidates[Scorer::kMaxCandidates + 1] {};
    for (std::size_t i = 0; i < Scorer::kMaxCandidates + 1; ++i)
        candidates[i] = baseCandidate(static_cast<std::uint64_t>(i));

    require(!Scorer::selectBest(candidates, Scorer::kMaxCandidates + 1).valid,
            "candidate count above hard emergency bound must fail closed");
}

} // namespace

int main()
{
    try
    {
        testGlancingContactBeatsMoreNormalImpact();
        testAngularVelocityContributesOmegaCrossRAtContactPoint();
        testMovingSurfaceUsesRelativeContactVelocity();
        testEffectiveImpactMassBreaksEqualNormalSpeedTie();
        testNoContactCandidateAlwaysBeatsEmergencyContact();
        testNormalImpactReportsNinetyDegreeIncidence();
        testInvalidWitnessFailsClosedAndBoundIsHard();

        std::cout << "NAVIGATION TRAJECTORY EMERGENCY CONTACT SEVERITY TESTS: PASS\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "NAVIGATION TRAJECTORY EMERGENCY CONTACT SEVERITY TESTS: FAIL: "
                  << error.what() << '\n';
        return 1;
    }
}
