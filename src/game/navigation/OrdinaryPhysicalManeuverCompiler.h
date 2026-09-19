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
    struct Capability
    {
        double maxForwardAccelerationMps2 = 0.0;
        double maxReverseAccelerationMps2 = 0.0;
        double maxLateralAccelerationMps2 = 0.0;
        double maxVerticalAccelerationMps2 = 0.0;

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

    struct Query
    {
        LocalFlightControlLaw controlLaw = LocalFlightControlLaw::Newtonian;

        State state {};
        Capability capability {};

        glm::dvec3 geometricTargetPositionMapMeters {0.0};
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

        static constexpr std::size_t kMaxCandidates = 2;
        std::array<OrdinaryPhysicalManeuverCandidate, kMaxCandidates>
            candidates {};
        std::size_t candidateCount = 0;

        bool directBodyAxisFeasible = false;
        bool leadRotateRequired = false;
    };

    [[nodiscard]] static Result compile(
        const Query& query
    ) noexcept;
};

} // namespace game::navigation
