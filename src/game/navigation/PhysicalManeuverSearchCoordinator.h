#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>

#include "src/game/navigation/OrdinaryPhysicalManeuverCompiler.h"

namespace game::navigation
{

// Pure bounded coordinator between an already-ranked route/terminal frontier
// and the physical maneuver compiler.  It owns neither mission intent nor
// alternative generation.  A caller can resume the returned cursor on a later
// worker slice without disabling the objective or repeating prior attempts.
class PhysicalManeuverSearchCoordinator final
{
public:
    using Compiler = OrdinaryPhysicalManeuverCompiler;

    static constexpr std::size_t kMaxAlternatives = 32;
    static constexpr std::size_t kMaxAttemptsPerAdvance = 8;

    struct AlternativeIdentity
    {
        std::uint64_t corridorAlternativeId = 0;
        std::uint64_t terminalAlternativeId = 0;
        std::uint64_t speedScheduleAlternativeId = 0;
        std::uint64_t arrivalTimeAlternativeId = 0;
    };

    struct Alternative
    {
        AlternativeIdentity identity {};
        glm::dvec3 targetPositionMapMeters {0.0};
        double targetCaptureRadiusMeters = 0.0;
        glm::dvec3 desiredVelocityMapMetersPerSecond {0.0};
        double maximumProgramSeconds = 0.0;
    };

    struct Frontier
    {
        // Mission intent and the generated alternative batch have independent
        // lifetimes.  Rebuilding a frontier must not manufacture a new goal.
        std::uint64_t objectiveRevision = 0;
        std::uint64_t frontierRevision = 0;
        std::size_t alternativeCount = 0;
        std::array<Alternative, kMaxAlternatives> alternatives {};
    };

    struct Cursor
    {
        std::uint64_t objectiveRevision = 0;
        std::uint64_t frontierRevision = 0;
        std::size_t nextAlternativeIndex = 0;
        std::uint64_t totalAttemptCount = 0;
    };

    struct Policy
    {
        std::size_t maximumAttemptsPerAdvance = 4;
    };

    struct Request
    {
        // State, capability, control law, reserves and compiler shaping policy
        // are immutable across this frontier.  Alternatives may vary only the
        // explicitly permitted terminal/speed/time fields below.
        Compiler::Query commonPhysicalQuery {};
        Frontier frontier {};
        Cursor cursor {};
        Policy policy {};
    };

    enum class Status : std::uint8_t
    {
        InvalidInput = 0,
        CandidateFound,
        SearchPending,
        FrontierExhausted,
        SharedStateBlocked
    };

    struct Attempt
    {
        std::size_t alternativeIndex = 0;
        AlternativeIdentity identity {};
        Compiler::Status compilerStatus = Compiler::Status::InvalidInput;
        Compiler::InfeasibilityWitness infeasibility {};
    };

    struct Result
    {
        Status status = Status::InvalidInput;

        // Search failure never cancels mission ownership.  Invalid API data is
        // returned to its owner for correction while the objective stays live.
        bool objectiveRemainsActive = true;

        Cursor nextCursor {};
        std::size_t attemptCount = 0;
        std::array<Attempt, kMaxAttemptsPerAdvance> attempts {};

        bool hasPhysicalCandidates = false;
        std::size_t selectedAlternativeIndex = 0;
        Alternative selectedAlternative {};
        Compiler::Result physicalCandidates {};
    };

    [[nodiscard]] static Result advance(
        const Request& request
    ) noexcept;
};

} // namespace game::navigation
