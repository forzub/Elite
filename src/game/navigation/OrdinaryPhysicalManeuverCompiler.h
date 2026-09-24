#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>

#include "src/game/navigation/LocalFlightControlLaw.h"

namespace game::navigation
{

// B5 output. This is a physically compiled maneuver candidate, NOT an accepted
// or geometry-proven program. B6 must prove this exact time history before B8
// may publish an AcceptedManeuverProgram.
struct OrdinaryPhysicalManeuverCandidate
{
    static constexpr std::size_t kMaxSamples = 16;

    enum class Family : std::uint8_t
    {
        Undefined = 0,
        Coast,
        Trim,
        LeadRotateMainBurn
    };

    struct Sample
    {
        double timeOffsetSeconds = 0.0;

        glm::dvec3 positionMapMeters {0.0};
        glm::dvec3 velocityMapMetersPerSecond {0.0};
        glm::dvec3 linearAccelerationFeedForwardMapMps2 {0.0};

        glm::dvec3 forwardMap {0.0, 0.0, -1.0};
        glm::dvec3 rightMap {1.0, 0.0, 0.0};
        glm::dvec3 upMap {0.0, 1.0, 0.0};

        glm::dvec3 angularVelocityMapRadPerSecond {0.0};
        glm::dvec3 angularAccelerationFeedForwardMapRadPerSec2 {0.0};
    };

    struct RequiredAuthority
    {
        double peakForwardAccelerationMps2 = 0.0;
        double peakReverseAccelerationMps2 = 0.0;
        double peakLateralAccelerationMps2 = 0.0;
        double peakVerticalAccelerationMps2 = 0.0;
        double peakAngularAccelerationRadPerSec2 = 0.0;
        double peakAngularSpeedRadPerSec = 0.0;
    };

    bool valid = false;
    Family family = Family::Undefined;
    LocalFlightControlLaw controlLaw = LocalFlightControlLaw::Newtonian;

    std::uint8_t sampleCount = 0;
    std::array<Sample, kMaxSamples> samples {};

    RequiredAuthority required {};

    glm::dvec3 requestedTargetPositionMapMeters {0.0};
    glm::dvec3 requestedTargetVelocityMapMetersPerSecond {0.0};

    // B5 never claims collision safety. This flag exists to make accidental
    // B5 -> B8 acceptance without B6 visibly wrong at the type boundary.
    bool requiresContinuousProof = true;
};

class OrdinaryPhysicalManeuverCompiler final
{
public:
    // A failed solve is data for the planning coordinator, not a command to
    // disable navigation.  The reason and measured lower bounds tell the
    // caller which search dimension can change on the next attempt.
    enum class InfeasibilityReason : std::uint8_t
    {
        None = 0,
        InvalidQuery,
        UnsupportedControlLaw,
        InvalidBodyFrame,
        InitialAngularStateUnsupported,
        TranslationAuthorityUnavailable,
        AttitudeAuthorityUnavailable,
        ProgramHorizonTooShort,
        SpatialTargetNotApproached,
        NumericalFailure
    };

    struct InfeasibilityWitness
    {
        InfeasibilityReason reason = InfeasibilityReason::None;

        glm::dvec3 requestedDeltaVelocityMapMetersPerSecond {0.0};
        double initialAngularSpeedRadPerSec = 0.0;
        double requiredAttitudeChangeRad = 0.0;
        double minimumAttitudeSeconds = 0.0;
        // Full-burn time needed to remove the complete requested delta-v.
        double minimumBurnSeconds = 0.0;
        // Earliest horizon that can contain attitude acquisition plus one
        // bounded receding-horizon translation primitive.
        double minimumProgramSeconds = 0.0;
        double availableProgramSeconds = 0.0;
        double initialTargetDistanceMeters = 0.0;
        double closestCandidateTargetDistanceMeters = 0.0;

        double usableForwardAccelerationMps2 = 0.0;
        double usableAngularAccelerationRadPerSec2 = 0.0;
        double usableAngularSpeedRadPerSec = 0.0;
    };

    struct Capability
    {
        // Instantaneous body-axis authority, including RCS.
        double maxForwardAccelerationMps2 = 0.0;
        double maxReverseAccelerationMps2 = 0.0;
        double maxLateralAccelerationMps2 = 0.0;
        double maxVerticalAccelerationMps2 = 0.0;

        // Explicit longitudinal MAIN-engine banks. -1 means an older caller
        // did not provide actuator separation; B5 then derives a conservative
        // main-bank hint from axis-vs-RCS authority. Zero means explicitly
        // unavailable and must never be promoted back into a main engine.
        double maxForwardMainAccelerationMps2 = -1.0;
        double maxReverseMainAccelerationMps2 = -1.0;

        double maxAngularAccelerationRadPerSec2 = 0.0;
        double maxAngularSpeedRadPerSec = 0.0;
    };

    struct State
    {
        glm::dvec3 positionMapMeters {0.0};
        glm::dvec3 velocityMapMetersPerSecond {0.0};

        glm::dvec3 forwardMap {0.0, 0.0, -1.0};
        glm::dvec3 rightMap {1.0, 0.0, 0.0};
        glm::dvec3 upMap {0.0, 1.0, 0.0};

        glm::dvec3 angularVelocityMapRadPerSecond {0.0};
    };

    struct Policy
    {
        // Maneuver-shaping values are explicit API data. The compiler may
        // contain numeric epsilons and exact polynomial constants internally,
        // but it must not own behavioral timing/ramp doctrine.
        double minimumPrimitiveSeconds = 0.05;
        double directPrimitiveSeconds = 1.0;
        double burnRampMinimumSeconds = 0.02;
        double burnRampMaximumSeconds = 0.20;
        double burnRampFractionOfRawBurn = 0.25;
    };

    struct Query
    {
        LocalFlightControlLaw controlLaw = LocalFlightControlLaw::Newtonian;
        Policy policy {};

        State state {};
        Capability capability {};

        // Center of the next spatial capture region. A short primitive need
        // not reach it, but must move closer without skipping its capture plane.
        glm::dvec3 geometricTargetPositionMapMeters {0.0};
        double targetCaptureRadiusMeters = 0.0;
        glm::dvec3 desiredVelocityMapMetersPerSecond {0.0};

        // Existing runtime target-velocity controller gain. B5 consumes the
        // requested delta-v but never assumes the resulting arbitrary vector
        // is executable.
        double velocityResponsePerSecond = 0.0;

        // Authority intentionally left unused by feed-forward so B10 can track
        // the proved reference without exceeding the physical envelope.
        double linearFeedbackReserveMps2 = 0.0;
        double angularFeedbackReserveRadPerSec2 = 0.0;

        // Pilot delay/latency reserve contributes to lead rotation timing.
        double controlResponseReserveSeconds = 0.0;

        // B5 produces a short receding-horizon primitive, not an entire route.
        double maximumProgramSeconds = 4.0;
    };

    enum class Status : std::uint8_t
    {
        InvalidInput = 0,
        Compiled,
        NoPhysicalCandidate,
        UnsupportedControlLaw
    };

    struct Result
    {
        Status status = Status::InvalidInput;
        InfeasibilityWitness infeasibility {};

        static constexpr std::size_t kMaxCandidates = 2;
        std::array<OrdinaryPhysicalManeuverCandidate, kMaxCandidates>
            candidates {};
        std::size_t candidateCount = 0;

        bool directBodyAxisFeasible = false;

        // Candidate availability is separate from selection. For a
        // main-engine-dominant Newtonian craft B5 exposes a main-engine option
        // whenever a non-zero delta-v can be compiled, even when RCS/trim is
        // also physically possible. B7 owns the final choice.
        bool mainEngineCandidateAvailable = false;

        // True only when the requested immediate acceleration cannot be
        // produced inside the current body-axis feed-forward authority.
        bool leadRotateRequired = false;
    };

    [[nodiscard]] static Result compile(
        const Query& query
    ) noexcept;
};

} // namespace game::navigation
