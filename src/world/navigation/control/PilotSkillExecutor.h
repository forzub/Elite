#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace world::navigation
{

// Deterministic per-pilot command execution model.
//
// Navigation/control supplies an ideal acceleration demand. Pilot skill changes
// when and how faithfully that demand reaches flight control. It never changes
// world geometry, hull size, vehicle capability or collision equations.
class PilotSkillExecutor final
{
public:
    struct Vec3d
    {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
    };

    struct ExecutionProfile
    {
        double reactionDelaySeconds = 0.0;
        double perceptionDecisionRateHz = 60.0;
        double commandLatencySeconds = 0.0;

        // Second-order command tracking response.
        double responseFrequencyHz = 4.0;
        double dampingRatio = 1.0;
        double commandGain = 1.0;

        // Slew is the maximum rate of change of acceleration demand.
        double maxLinearCommandSlewMetersPerSec3 = 1.0e6;
        double maxAngularCommandSlewRadPerSec3 = 1.0e6;

        // Deterministic per-decision precision error. This is command-space
        // error, not direct position/velocity corruption.
        double deterministicLinearNoiseAmplitudeMetersPerSec2 = 0.0;
        double deterministicAngularNoiseAmplitudeRadPerSec2 = 0.0;
        std::uint64_t deterministicSeed = 0;

        // Emergency intent can shorten reaction delay once urgency reaches
        // this threshold. A scale of 0 means immediate reaction after threshold.
        double emergencyResponseThreshold01 = 1.0;
        double emergencyReactionDelayScale = 1.0;
    };

    struct PolicyProfile
    {
        // These are intentionally not consumed by PilotSkillExecutor.
        // Upstream maneuver selection/live integration owns them.
        double anticipationSeconds = 0.0;
        double riskPreference01 = 0.5;
        double comfortPreference01 = 0.5;
    };

    struct PilotSkillProfile
    {
        ExecutionProfile execution {};
        PolicyProfile policy {};
    };

    struct Command
    {
        // Revision identifies a new maneuver/intent. Reaction delay restarts
        // only when this revision changes; a command may evolve continuously
        // inside the same intent at the decision cadence.
        std::uint64_t revision = 0;

        // Concrete sampled target revision inside the same maneuver.
        // Zero means "same as revision" for backwards-compatible callers.
        std::uint64_t targetRevision = 0;

        Vec3d linearAccelerationDemandMetersPerSec2 {};
        Vec3d angularAccelerationDemandRadPerSec2 {};

        bool emergency = false;
        double hazardUrgency01 = 0.0;
    };

    enum class Status : std::uint8_t
    {
        Ok = 0,
        NotInitialized,
        InvalidInput,
        QueueOverflow
    };

    struct StepResult
    {
        Status status = Status::NotInitialized;

        Vec3d executedLinearAccelerationDemandMetersPerSec2 {};
        Vec3d executedAngularAccelerationDemandRadPerSec2 {};

        std::uint64_t observedIntentRevision = 0;
        std::uint64_t activeTargetRevision = 0;

        bool reactionBlocked = false;
        bool decisionSampled = false;
        bool queuedCommandApplied = false;
        std::size_t pendingCommandCount = 0;
    };

    static constexpr std::size_t kMaxPendingCommands = 256;
    static constexpr std::size_t kMaxIntegrationSubsteps = 64;
    static constexpr double kMaximumStepSeconds = 0.25;

    PilotSkillExecutor() noexcept;

    explicit PilotSkillExecutor(
        const PilotSkillProfile& profile
    ) noexcept;

    [[nodiscard]] static bool validProfile(
        const PilotSkillProfile& profile
    ) noexcept;

    [[nodiscard]] bool reset(
        double timeSeconds,
        const Command& initialCommand
    ) noexcept;

    [[nodiscard]] StepResult step(
        double timeSeconds,
        double deltaSeconds,
        const Command& desiredCommand
    ) noexcept;

    [[nodiscard]] const PilotSkillProfile& profile() const noexcept
    {
        return profile_;
    }

private:
    struct FilterState
    {
        Vec3d value {};
        Vec3d rate {};
    };

    struct QueuedCommand
    {
        double applyTimeSeconds = 0.0;
        std::uint64_t revision = 0;
        Vec3d linearTarget {};
        Vec3d angularTarget {};
    };

    PilotSkillProfile profile_ {};
    bool profileValid_ = false;
    bool initialized_ = false;

    double lastTimeSeconds_ = 0.0;
    double nextDecisionTimeSeconds_ = 0.0;
    double revisionFirstSeenTimeSeconds_ = 0.0;

    std::uint64_t observedRevision_ = 0;
    std::uint64_t activeTargetRevision_ = 0;
    std::uint64_t decisionSequence_ = 0;

    Vec3d activeLinearTarget_ {};
    Vec3d activeAngularTarget_ {};
    FilterState linearFilter_ {};
    FilterState angularFilter_ {};

    std::array<QueuedCommand, kMaxPendingCommands> queue_ {};
    std::size_t queueHead_ = 0;
    std::size_t queueSize_ = 0;
};

} // namespace world::navigation
