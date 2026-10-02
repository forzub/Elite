#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>

#include "src/game/navigation/LocalFlightControlLaw.h"

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

    enum class TranslationMode : std::uint8_t
    {
        Undefined = 0,

        // Assisted is executed as the same game flight law used by manual
        // control: target forward speed + bounded hull attitude, with the
        // automatic velocity-to-nose stabilizer. It is NOT decomposed into
        // physical keypad/manoeuvre RCS.
        AssistedVelocity,

        // Newtonian transit owns hull rotation + longitudinal main-engine
        // burns. Ordinary route generation must not spend precision RCS as
        // fake lateral main thrust.
        NewtonianMainEngine,

        // Reserved for later close-placement/capture layers where deliberate
        // low-speed RCS/tug authority is part of the maneuver doctrine.
        PrecisionRcs
    };

    enum class ReferenceMode : std::uint8_t
    {
        // Reference position/velocity are sampled by the accepted maneuver
        // clock. Use this only when timing itself is part of the maneuver.
        TimeScheduled = 0,

        // Vehicle progress owns the clock. Follower projects the real craft
        // onto the accepted spatial program and advances only as the craft
        // physically progresses through it. This is the tunnel/canyon mode.
        //
        // STOP/START semantic:
        // - an authored reference velocity of exactly zero means STOP/capture;
        //   small measured residual velocity is finished with physical RCS;
        // - the first sample of a new moving spatial segment must therefore
        //   have non-zero reference speed, even when its feed-forward
        //   acceleration at the boundary is zero.
        SpatialCorridor
    };

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

        // Instantaneous body-axis authority (main + RCS as applicable).
        double maxForwardAccelerationMetersPerSec2 = 0.0;
        double maxReverseAccelerationMetersPerSec2 = 0.0;
        double maxLateralAccelerationMetersPerSec2 = 0.0;
        double maxVerticalAccelerationMetersPerSec2 = 0.0;

        // Physical longitudinal MAIN-engine banks kept separate from RCS so
        // execution/replan cannot mistake residual manoeuvre authority for a
        // surviving main engine after damage.
        double maxForwardMainAccelerationMetersPerSec2 = 0.0;
        double maxReverseMainAccelerationMetersPerSec2 = 0.0;

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
    ReferenceMode referenceMode = ReferenceMode::TimeScheduled;
    LocalFlightControlLaw controlLaw = defaultLocalFlightControlLaw();
    TranslationMode translationMode = TranslationMode::Undefined;

    // One maneuver may span multiple fixed-capacity storage pages.
    // All pages share acceptedAtUniverseTimeSeconds. Each page keeps local
    // sample times starting at zero and declares where it begins on the one
    // monotonic maneuver clock.
    double acceptedAtUniverseTimeSeconds = 0.0;
    double sequenceStartOffsetSeconds = 0.0;
    double validUntilUniverseTimeSeconds = 0.0;

    std::uint8_t sampleCount = 0;
    std::array<ReferenceSample, kMaxSamples> samples {};

    TerminalTolerance terminalTolerance {};
    TrackingEnvelope tracking {};
    CapabilitySnapshot capability {};
    ProofWitness proof {};

    bool completionTriggersReplan = true;
    bool emergency = false;
    double hazardUrgency01 = 0.0;
};

} // namespace game::navigation
