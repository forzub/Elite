#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

namespace game::ship::controller
{

// High-level maneuver choice above Navigation.
//
// Navigation/trajectory produces physically truthful maneuver candidates.
// Damage/structural and threat systems annotate those candidates.
// This controller decides which already-described candidate best matches the
// current tactical doctrine. It never discovers geometry, invents thrust,
// mutates physics or resolves collision/damage.
class ManeuverDecisionController final
{
public:
    static constexpr std::size_t kMaxCandidates = 16;

    enum class Doctrine : std::uint8_t
    {
        Rational = 0,
        PrecisionRetrieval,
        Extreme,
        CombatEscape
    };

    enum class ProgressRequirement : std::uint8_t
    {
        MayStop = 0,
        PreferProgress,
        MustProgress
    };

    enum class CandidateFamily : std::uint8_t
    {
        StopOrBrake = 0,
        GlobalRoute,
        LocalVisibility,
        PrecisionPassage,
        EmergencyContact
    };

    struct Context
    {
        Doctrine doctrine = Doctrine::Rational;
        ProgressRequirement progress = ProgressRequirement::MayStop;

        // Contact is an accepted candidate class, never a synonym for safe.
        // MustProgress may still force comparison of contact-expected options
        // when no collision-free progressing option exists.
        bool allowExpectedContact = false;
        bool allowSacrificialComponentLoss = false;

        // Hard survival envelope. Candidates above this are considered only
        // when every valid alternative also exceeds it.
        double maximumPreferredCriticalDamageRisk01 = 0.20;
    };

    struct Candidate
    {
        std::uint64_t candidateId = 0;
        CandidateFamily family = CandidateFamily::StopOrBrake;
        bool valid = false;

        bool collisionFree = false;
        bool contactExpected = false;
        bool progressesObjective = false;
        bool stopsOrBrakes = false;

        // Navigation / trajectory annotations.
        double timeToObjectiveSeconds = std::numeric_limits<double>::infinity();
        double entrySpeedMps = 0.0;
        double exitSpeedMps = 0.0;
        double minimumClearanceMeters = 0.0;
        double escapeReserve01 = 0.0;

        // Emergency-contact physical annotations.
        double peakClosingNormalSpeedMps = 0.0;
        double normalImpactEnergyProxyJ = 0.0;

        // Damage / structural annotations. Navigation must not fabricate these.
        double criticalDamageRisk01 = 0.0;
        double missionDamageCost01 = 0.0;
        double expendableDamageCost01 = 0.0;

        // Threat assessor annotation. This may include projected silhouette,
        // line-of-fire visibility and threat intensity integrated over time.
        double threatExposure = 0.0;
    };

    struct Selection
    {
        bool valid = false;
        std::size_t selectedIndex = std::numeric_limits<std::size_t>::max();
        std::uint64_t selectedCandidateId = 0;
        std::size_t candidatesEvaluated = 0;

        bool selectedCollisionFree = false;
        bool selectedContactExpected = false;
        bool selectedProgressing = false;
    };

    [[nodiscard]] static Selection select(
        const Context& context,
        const Candidate* candidates,
        std::size_t candidateCount
    ) noexcept;
};

} // namespace game::ship::controller
