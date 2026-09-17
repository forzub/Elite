#pragma once

#include "OrientedPassageEvaluator.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace world::navigation
{

// Bounded emergency-contact ranking for already predicted contact witnesses.
//
// This class does not discover collisions and does not replace physics CCD or
// contact response. It compares at most eight already-produced emergency
// trajectory candidates. Each candidate may carry up to four contact witnesses.
// Severity is based on relative contact-point velocity, including omega x r,
// with priority given to low closing speed along the contact normal. This makes
// a glancing/tangential contact preferable to a normal impact when contact is
// already unavoidable.
class EmergencyContactSeverityScorer final
{
public:
    using Vec3d = OrientedPassageEvaluator::Vec3d;

    static constexpr std::size_t kMaxCandidates = 8;
    static constexpr std::size_t kMaxContactsPerCandidate = 4;

    struct ContactWitness
    {
        // Unit direction is not required; the scorer normalizes this vector.
        // It must point from the obstacle/contact surface toward free space.
        Vec3d normalTowardFreeSpaceMap {1.0, 0.0, 0.0};

        Vec3d contactPointMapMeters {};
        Vec3d surfaceVelocityMapMetersPerSec {};

        Vec3d shipCenterMapMeters {};
        Vec3d shipLinearVelocityMapMetersPerSec {};
        Vec3d shipAngularVelocityMapRadPerSec {};

        // Optional effective mass supplied by a later physics-aware predictor.
        // Zero means use Candidate::shipMassKg as the coarse proxy.
        double effectiveImpactMassKg = 0.0;
    };

    struct Candidate
    {
        std::uint64_t candidateId = 0;
        double shipMassKg = 0.0;

        // Existing emergency geometry remains a lower-priority tiebreaker.
        double geometryDeficitMeters = 0.0;

        // Positive means useful progress along the selected passage/corridor.
        // It is intentionally lower priority than impact severity.
        double passageAxisProgressMetersPerSec = 0.0;

        std::array<ContactWitness, kMaxContactsPerCandidate> contacts {};
        std::size_t contactCount = 0;
    };

    struct CandidateScore
    {
        bool valid = false;
        bool contactExpected = false;
        std::size_t contactsEvaluated = 0;

        // Primary emergency metric: maximum speed directed into any predicted
        // contact surface. Zero is tangent/separating at the witness instant.
        double peakClosingNormalSpeedMetersPerSec = 0.0;

        // Coarse proxies only. Exact impulse/energy remain physics authority.
        double totalNormalImpactEnergyProxyJ = 0.0;
        double totalNormalMomentumProxyKgMetersPerSec = 0.0;

        // 0 = perfectly tangential/glancing, pi/2 = purely normal impact.
        double worstIncidenceFromTangentRad = 0.0;

        double geometryDeficitMeters = 0.0;
        double passageAxisProgressMetersPerSec = 0.0;
    };

    struct SelectionResult
    {
        bool valid = false;
        std::size_t candidatesEvaluated = 0;
        std::size_t selectedIndex = std::numeric_limits<std::size_t>::max();
        std::uint64_t selectedCandidateId = 0;
        CandidateScore selectedScore {};
    };

    [[nodiscard]] static CandidateScore evaluateCandidate(
        const Candidate& candidate
    ) noexcept;

    [[nodiscard]] static SelectionResult selectBest(
        const Candidate* candidates,
        std::size_t candidateCount
    ) noexcept;
};

} // namespace world::navigation
