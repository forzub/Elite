#include "PilotSkillExecutor.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace world::navigation
{
namespace
{

using Executor = PilotSkillExecutor;
using Vec3d = Executor::Vec3d;

constexpr double kTolerance = 1.0e-12;
constexpr double kTwoPi = 6.283185307179586476925286766559005768;

bool finite(double value) noexcept
{
    return std::isfinite(value);
}

bool finite(const Vec3d& value) noexcept
{
    return finite(value.x) && finite(value.y) && finite(value.z);
}

std::uint64_t effectiveTargetRevision(
    const Executor::Command& command
) noexcept
{
    return command.targetRevision != 0
        ? command.targetRevision
        : command.revision;
}

bool validCommand(const Executor::Command& command) noexcept
{
    return finite(command.linearAccelerationDemandMetersPerSec2) &&
        finite(command.angularAccelerationDemandRadPerSec2) &&
        finite(command.hazardUrgency01) &&
        command.hazardUrgency01 >= 0.0 &&
        command.hazardUrgency01 <= 1.0;
}

Vec3d add(const Vec3d& a, const Vec3d& b) noexcept
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3d subtract(const Vec3d& a, const Vec3d& b) noexcept
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3d scale(const Vec3d& value, double factor) noexcept
{
    return {value.x * factor, value.y * factor, value.z * factor};
}

double lengthSquared(const Vec3d& value) noexcept
{
    return value.x * value.x + value.y * value.y + value.z * value.z;
}

double length(const Vec3d& value) noexcept
{
    return std::sqrt(lengthSquared(value));
}

Vec3d clampMagnitude(const Vec3d& value, double maximum) noexcept
{
    const double magnitude = length(value);
    if (magnitude <= maximum || magnitude <= kTolerance)
        return value;
    return scale(value, maximum / magnitude);
}

std::uint64_t mix64(std::uint64_t value) noexcept
{
    value += 0x9E3779B97F4A7C15ull;
    value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ull;
    value = (value ^ (value >> 27)) * 0x94D049BB133111EBull;
    return value ^ (value >> 31);
}

double signedUnitNoise(
    std::uint64_t seed,
    std::uint64_t revision,
    std::uint64_t sequence,
    std::uint64_t axis
) noexcept
{
    std::uint64_t value = seed;
    value ^= mix64(revision + 0xA0761D6478BD642Full);
    value ^= mix64(sequence + 0xE7037ED1A0B428DBull);
    value ^= mix64(axis + 0x8EBC6AF09C88C6E3ull);
    value = mix64(value);

    // Keep the top 53 bits so conversion to double is deterministic and exact.
    const std::uint64_t mantissa = value >> 11;
    const double unit = static_cast<double>(mantissa) /
        static_cast<double>(std::uint64_t {1} << 53);
    return 2.0 * unit - 1.0;
}

Vec3d deterministicNoise(
    std::uint64_t seed,
    std::uint64_t revision,
    std::uint64_t sequence,
    std::uint64_t axisBase,
    double amplitude
) noexcept
{
    if (amplitude <= 0.0)
        return {};

    return {
        amplitude * signedUnitNoise(seed, revision, sequence, axisBase + 0),
        amplitude * signedUnitNoise(seed, revision, sequence, axisBase + 1),
        amplitude * signedUnitNoise(seed, revision, sequence, axisBase + 2)
    };
}

void advanceSecondOrder(
    Vec3d& value,
    Vec3d& rate,
    const Vec3d& target,
    double naturalFrequencyRadPerSec,
    double dampingRatio,
    double maximumSlew,
    double deltaSeconds,
    std::size_t substeps
) noexcept
{
    if (substeps == 0)
        return;

    const double h = deltaSeconds / static_cast<double>(substeps);
    const double omega2 =
        naturalFrequencyRadPerSec * naturalFrequencyRadPerSec;
    const double damping =
        2.0 * dampingRatio * naturalFrequencyRadPerSec;

    for (std::size_t i = 0; i < substeps; ++i)
    {
        const Vec3d acceleration = subtract(
            scale(subtract(target, value), omega2),
            scale(rate, damping)
        );
        rate = add(rate, scale(acceleration, h));
        rate = clampMagnitude(rate, maximumSlew);
        value = add(value, scale(rate, h));
    }
}

} // namespace

PilotSkillExecutor::PilotSkillExecutor() noexcept
    : PilotSkillExecutor(PilotSkillProfile {})
{
}

PilotSkillExecutor::PilotSkillExecutor(
    const PilotSkillProfile& profile
) noexcept
    : profile_(profile),
      profileValid_(validProfile(profile))
{
}

bool PilotSkillExecutor::validProfile(
    const PilotSkillProfile& profile
) noexcept
{
    const ExecutionProfile& execution = profile.execution;
    const PolicyProfile& policy = profile.policy;

    return finite(execution.reactionDelaySeconds) &&
        execution.reactionDelaySeconds >= 0.0 &&
        finite(execution.perceptionDecisionRateHz) &&
        execution.perceptionDecisionRateHz > 0.0 &&
        execution.perceptionDecisionRateHz <= 1000.0 &&
        finite(execution.commandLatencySeconds) &&
        execution.commandLatencySeconds >= 0.0 &&
        execution.commandLatencySeconds <= 10.0 &&
        execution.commandLatencySeconds * execution.perceptionDecisionRateHz <
            static_cast<double>(kMaxPendingCommands - 1) &&
        finite(execution.responseFrequencyHz) &&
        execution.responseFrequencyHz > 0.0 &&
        execution.responseFrequencyHz <= 16.0 &&
        finite(execution.dampingRatio) &&
        execution.dampingRatio >= 0.0 &&
        finite(execution.commandGain) &&
        execution.commandGain >= 0.0 &&
        finite(execution.maxLinearCommandSlewMetersPerSec3) &&
        execution.maxLinearCommandSlewMetersPerSec3 > 0.0 &&
        finite(execution.maxAngularCommandSlewRadPerSec3) &&
        execution.maxAngularCommandSlewRadPerSec3 > 0.0 &&
        finite(execution.maximumStepSeconds) &&
        execution.maximumStepSeconds > 0.0 &&
        finite(execution.integrationSubstepsPerResponsePeriod) &&
        execution.integrationSubstepsPerResponsePeriod > 0.0 &&
        execution.maximumIntegrationSubsteps > 0 &&
        execution.maximumPendingCommands > 0 &&
        execution.maximumPendingCommands <=
            kPendingCommandStorageCapacity &&
        execution.commandLatencySeconds *
                execution.perceptionDecisionRateHz <
            static_cast<double>(
                execution.maximumPendingCommands - 1
            ) &&
        finite(execution.deterministicLinearNoiseAmplitudeMetersPerSec2) &&
        execution.deterministicLinearNoiseAmplitudeMetersPerSec2 >= 0.0 &&
        finite(execution.deterministicAngularNoiseAmplitudeRadPerSec2) &&
        execution.deterministicAngularNoiseAmplitudeRadPerSec2 >= 0.0 &&
        finite(execution.emergencyResponseThreshold01) &&
        execution.emergencyResponseThreshold01 >= 0.0 &&
        execution.emergencyResponseThreshold01 <= 1.0 &&
        finite(execution.emergencyReactionDelayScale) &&
        execution.emergencyReactionDelayScale >= 0.0 &&
        execution.emergencyReactionDelayScale <= 1.0 &&
        finite(policy.anticipationSeconds) &&
        policy.anticipationSeconds >= 0.0 &&
        finite(policy.riskPreference01) &&
        policy.riskPreference01 >= 0.0 &&
        policy.riskPreference01 <= 1.0 &&
        finite(policy.comfortPreference01) &&
        policy.comfortPreference01 >= 0.0 &&
        policy.comfortPreference01 <= 1.0;
}

bool PilotSkillExecutor::reset(
    double timeSeconds,
    const Command& initialCommand
) noexcept
{
    if (!profileValid_ || !finite(timeSeconds) || !validCommand(initialCommand))
        return false;

    initialized_ = true;
    lastTimeSeconds_ = timeSeconds;
    nextDecisionTimeSeconds_ = timeSeconds;
    revisionFirstSeenTimeSeconds_ = timeSeconds;
    observedRevision_ = initialCommand.revision;
    activeTargetRevision_ =
        effectiveTargetRevision(initialCommand);
    decisionSequence_ = 0;

    activeLinearTarget_ = scale(
        initialCommand.linearAccelerationDemandMetersPerSec2,
        profile_.execution.commandGain
    );
    activeAngularTarget_ = scale(
        initialCommand.angularAccelerationDemandRadPerSec2,
        profile_.execution.commandGain
    );

    linearFilter_ = {};
    angularFilter_ = {};
    linearFilter_.value = activeLinearTarget_;
    angularFilter_.value = activeAngularTarget_;

    queueHead_ = 0;
    queueSize_ = 0;
    return true;
}

PilotSkillExecutor::StepResult PilotSkillExecutor::step(
    double timeSeconds,
    double deltaSeconds,
    const Command& desiredCommand
) noexcept
{
    StepResult result;
    result.status = Status::NotInitialized;

    if (!initialized_)
        return result;

    if (!profileValid_ ||
        !finite(timeSeconds) ||
        !finite(deltaSeconds) ||
        deltaSeconds <= 0.0 ||
        deltaSeconds > profile_.execution.maximumStepSeconds ||
        timeSeconds + kTolerance < lastTimeSeconds_ ||
        std::abs(
            (timeSeconds - lastTimeSeconds_) - deltaSeconds
        ) > std::max(1.0e-9, deltaSeconds * 1.0e-6) ||
        !validCommand(desiredCommand))
    {
        result.status = Status::InvalidInput;
        return result;
    }

    const ExecutionProfile& execution = profile_.execution;
    result.status = Status::Ok;

    if (desiredCommand.revision != observedRevision_)
    {
        observedRevision_ = desiredCommand.revision;
        revisionFirstSeenTimeSeconds_ = timeSeconds;
    }

    double reactionDelay = execution.reactionDelaySeconds;
    if (desiredCommand.emergency &&
        desiredCommand.hazardUrgency01 + kTolerance >=
            execution.emergencyResponseThreshold01)
    {
        reactionDelay *= execution.emergencyReactionDelayScale;
    }

    const double eligibleTime =
        revisionFirstSeenTimeSeconds_ + reactionDelay;
    const bool reactionBlocked =
        timeSeconds + kTolerance < eligibleTime;
    result.reactionBlocked = reactionBlocked;

    if (!reactionBlocked &&
        timeSeconds + kTolerance >= nextDecisionTimeSeconds_)
    {
        if (queueSize_ >= execution.maximumPendingCommands)
        {
            result.status = Status::QueueOverflow;
        }
        else
        {
            QueuedCommand queued;
            queued.applyTimeSeconds =
                timeSeconds + execution.commandLatencySeconds;
            queued.revision =
                effectiveTargetRevision(desiredCommand);

            const Vec3d linearNoise = deterministicNoise(
                execution.deterministicSeed,
                desiredCommand.revision,
                decisionSequence_,
                0,
                execution.deterministicLinearNoiseAmplitudeMetersPerSec2
            );
            const Vec3d angularNoise = deterministicNoise(
                execution.deterministicSeed,
                desiredCommand.revision,
                decisionSequence_,
                3,
                execution.deterministicAngularNoiseAmplitudeRadPerSec2
            );

            queued.linearTarget = add(
                scale(
                    desiredCommand.linearAccelerationDemandMetersPerSec2,
                    execution.commandGain
                ),
                linearNoise
            );
            queued.angularTarget = add(
                scale(
                    desiredCommand.angularAccelerationDemandRadPerSec2,
                    execution.commandGain
                ),
                angularNoise
            );

            const std::size_t tail =
                (queueHead_ + queueSize_) % kPendingCommandStorageCapacity;
            queue_[tail] = queued;
            ++queueSize_;
            ++decisionSequence_;
            result.decisionSampled = true;
        }

        const double decisionPeriod =
            1.0 / execution.perceptionDecisionRateHz;
        const double periodsToAdvance = std::max(
            1.0,
            std::floor(
                (timeSeconds + kTolerance - nextDecisionTimeSeconds_) /
                decisionPeriod
            ) + 1.0
        );
        nextDecisionTimeSeconds_ += periodsToAdvance * decisionPeriod;
    }

    while (queueSize_ > 0)
    {
        const QueuedCommand& queued = queue_[queueHead_];
        if (queued.applyTimeSeconds > timeSeconds + kTolerance)
            break;

        activeLinearTarget_ = queued.linearTarget;
        activeAngularTarget_ = queued.angularTarget;
        activeTargetRevision_ = queued.revision;
        queueHead_ = (queueHead_ + 1) % kPendingCommandStorageCapacity;
        --queueSize_;
        result.queuedCommandApplied = true;
    }

    const double naturalFrequency =
        kTwoPi * execution.responseFrequencyHz;
    const double requestedSubsteps =
        std::ceil(
            deltaSeconds *
            execution.responseFrequencyHz *
            execution.integrationSubstepsPerResponsePeriod
        );
    const std::size_t substeps = std::clamp<std::size_t>(
        static_cast<std::size_t>(std::max(1.0, requestedSubsteps)),
        1,
        execution.maximumIntegrationSubsteps
    );

    advanceSecondOrder(
        linearFilter_.value,
        linearFilter_.rate,
        activeLinearTarget_,
        naturalFrequency,
        execution.dampingRatio,
        execution.maxLinearCommandSlewMetersPerSec3,
        deltaSeconds,
        substeps
    );
    advanceSecondOrder(
        angularFilter_.value,
        angularFilter_.rate,
        activeAngularTarget_,
        naturalFrequency,
        execution.dampingRatio,
        execution.maxAngularCommandSlewRadPerSec3,
        deltaSeconds,
        substeps
    );

    lastTimeSeconds_ = timeSeconds;

    result.executedLinearAccelerationDemandMetersPerSec2 =
        linearFilter_.value;
    result.executedAngularAccelerationDemandRadPerSec2 =
        angularFilter_.value;
    result.observedIntentRevision = observedRevision_;
    result.activeTargetRevision = activeTargetRevision_;
    result.pendingCommandCount = queueSize_;
    return result;
}

} // namespace world::navigation
