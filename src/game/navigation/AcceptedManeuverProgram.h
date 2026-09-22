#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>

namespace game::navigation
{

// Immutable, bounded Planner -> Follower execution product.
//
// The planner/prover/decision chain must publish the SAME reference state and
// feed-forward control that passed capability + geometry proof. Execution may
// add only bounded tracking feedback around this program.
//
// Hot-path constraints:
// - fixed capacity;
// - no per-tick allocation;
// - value-owned;
// - no NavigationMap / NavigationSpace ownership.
struct AcceptedManeuverProgram
{
    static constexpr std::size_t kMaxSamples = 16;

    enum class ManeuverFamily : std::uint8_t
    {
        Undefined = 0,
        Coast,
        LeadRotateMainBurn,
        DriftPass,
        Trim,
        Brake,
        FlipAndBurn,
        FreeTransit,
        PrecisionCapture,
        PrecisionTransit,
        EmergencyRecovery
    };

    struct ReferenceSample
    {
        // Relative to acceptedAtUniverseTimeSeconds.
        double timeOffsetSeconds = 0.0;

        glm::dvec3 positionMapMeters {0.0};
        glm::dvec3 velocityMapMetersPerSecond {0.0};
        glm::dvec3 linearAccelerationFeedForwardMapMps2 {0.0};

        // Complete body basis preserves attitude/roll without introducing a
        // quaternion dependency at this API boundary.
        glm::dvec3 forwardMap {0.0, 0.0, -1.0};
        glm::dvec3 rightMap {1.0, 0.0, 0.0};
        glm::dvec3 upMap {0.0, 1.0, 0.0};

        glm::dvec3 angularVelocityMapRadPerSecond {0.0};
        glm::dvec3 angularAccelerationFeedForwardMapRadPerSec2 {0.0};
    };

    struct ActuatorSegment
    {
        // Interval from samples[i] to samples[i+1].
        double durationSeconds = 0.0;

        // Current Cobra has one aft/rear main engine. Fore-main fields remain
        // explicit so the program format does not confuse "reverse demand"
        // with hardware that is not actually installed.
        bool rearMainEnabled = false;
        double rearMainThrottleStart01 = 0.0;
        double rearMainThrottleEnd01 = 0.0;

        bool foreMainEnabled = false;
        double foreMainThrottleStart01 = 0.0;
        double foreMainThrottleEnd01 = 0.0;

        // World/NavLocal feed-forward requested from real manoeuvre/RCS
        // authority over this interval.
        glm::dvec3 manoeuvreAccelerationStartMapMps2 {0.0};
        glm::dvec3 manoeuvreAccelerationEndMapMps2 {0.0};

        // False means the sampled kinematic reference demanded more feed-forward
        // authority than the vehicle model can physically allocate. This is
        // observable during migration and will become a hard acceptance gate.
        bool propulsionFeasible = true;
    };

    struct TerminalTolerance
    {
        double positionMeters = 0.0;
        double linearVelocityMps = 0.0;
        double forwardAngleRad = 0.0;
        double angularVelocityRadPerSec = 0.0;
    };

    struct TrackingEnvelope
    {
        double positionErrorMeters = 0.0;
        double linearVelocityErrorMps = 0.0;
        double forwardAngleErrorRad = 0.0;
        double angularVelocityErrorRadPerSec = 0.0;

        // Free-transit does not need rail-like time tracking. These deadbands
        // define a longitudinal corridor around the accepted reference while
        // cross-track position/velocity errors remain fully controlled.
        double alongTrackPositionDeadbandMeters = 0.0;
        double alongTrackSpeedDeadbandMps = 0.0;

        // Authority intentionally reserved for the follower. Maneuver proof
        // must account for this reserve instead of consuming 100% authority.
        double linearFeedbackReserveMps2 = 0.0;
        double angularFeedbackReserveRadPerSec2 = 0.0;
    };

    struct CapabilitySnapshot
    {
        std::uint64_t revision = 0;
        double maxForwardAccelerationMetersPerSec2 = 0.0;
        double maxReverseAccelerationMetersPerSec2 = 0.0;
        double maxLateralAccelerationMetersPerSec2 = 0.0;
        double maxVerticalAccelerationMetersPerSec2 = 0.0;
        double maxAngularAccelerationRadPerSec2 = 0.0;
        double maxAngularSpeedRadPerSec = 0.0;
    };

    struct ProofWitness
    {
        std::uint64_t mapRevision = 0;
        std::uint64_t mapSourceRevision = 0;
        std::uint64_t spaceRevision = 0;
        std::uint64_t spaceSourceRevision = 0;

        double minimumClearanceMeters = 0.0;
        double minimumLinearAuthorityReserveMps2 = 0.0;
        double minimumAngularAuthorityReserveRadPerSec2 = 0.0;
    };

    bool valid = false;

    std::uint64_t revision = 0;
    std::uint64_t objectiveRevision = 0;

    ManeuverFamily family = ManeuverFamily::Undefined;

    // One maneuver may span multiple fixed-capacity storage pages.
    // All pages share acceptedAtUniverseTimeSeconds. Each page keeps local
    // sample times starting at zero and declares where it begins on the one
    // monotonic maneuver clock.
    double acceptedAtUniverseTimeSeconds = 0.0;
    double sequenceStartOffsetSeconds = 0.0;
    double validUntilUniverseTimeSeconds = 0.0;

    std::uint8_t sampleCount = 0;
    std::array<ReferenceSample, kMaxSamples> samples {};

    // Explicit physical command intervals owned by Planner. During migration
    // this may be zero for legacy producers; new Stage-12 programs publish
    // exactly sampleCount-1 intervals.
    std::uint8_t actuatorSegmentCount = 0;
    std::array<ActuatorSegment, kMaxSamples - 1> actuatorSegments {};
    bool actuatorProgramFeasible = true;

    TerminalTolerance terminalTolerance {};
    TrackingEnvelope tracking {};
    CapabilitySnapshot capability {};
    ProofWitness proof {};

    bool completionTriggersReplan = true;
    bool emergency = false;
    double hazardUrgency01 = 0.0;
};

} // namespace game::navigation
