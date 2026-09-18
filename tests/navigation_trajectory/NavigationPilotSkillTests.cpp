#include "world/navigation/control/PilotSkillExecutor.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

using Executor = world::navigation::PilotSkillExecutor;
void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void requireNear(
    double actual,
    double expected,
    double tolerance,
    const std::string& message
)
{
    if (std::abs(actual - expected) > tolerance)
        throw std::runtime_error(message);
}

Executor::PilotSkillProfile expertProfile()
{
    Executor::PilotSkillProfile profile;
    profile.execution.reactionDelaySeconds = 0.0;
    profile.execution.perceptionDecisionRateHz = 60.0;
    profile.execution.commandLatencySeconds = 0.0;
    profile.execution.responseFrequencyHz = 4.0;
    profile.execution.dampingRatio = 1.0;
    profile.execution.commandGain = 1.0;
    profile.execution.maxLinearCommandSlewMetersPerSec3 = 1000.0;
    profile.execution.maxAngularCommandSlewRadPerSec3 = 1000.0;
    return profile;
}

void testProfileValidation()
{
    Executor::PilotSkillProfile valid = expertProfile();
    require(Executor::validProfile(valid), "expert profile must validate");

    Executor::PilotSkillProfile invalid = valid;
    invalid.execution.perceptionDecisionRateHz = 0.0;
    require(!Executor::validProfile(invalid),
            "zero decision rate must fail validation");

    invalid = valid;
    invalid.policy.riskPreference01 = 1.1;
    require(!Executor::validProfile(invalid),
            "policy preference outside [0,1] must fail validation");
}

void testReactionDelayAndCommandLatency()
{
    Executor::PilotSkillProfile profile = expertProfile();
    profile.execution.reactionDelaySeconds = 0.20;
    profile.execution.perceptionDecisionRateHz = 100.0;
    profile.execution.commandLatencySeconds = 0.10;

    Executor executor(profile);
    Executor::Command initial;
    initial.revision = 1;
    require(executor.reset(0.0, initial), "reset must succeed");

    Executor::Command desired;
    desired.revision = 2;
    desired.linearAccelerationDemandMapMetersPerSec2 = {10.0, 0.0, 0.0};

    auto result = executor.step(0.01, 0.01, desired);
    require(result.reactionBlocked,
            "new intent must be held during reaction delay");
    require(result.activeTargetRevision == 1,
            "reaction delay must preserve previous target revision");

    result = executor.step(0.20, 0.19, desired);
    require(result.reactionBlocked,
            "reaction delay begins when the new revision is first observed");

    result = executor.step(0.21, 0.01, desired);
    require(!result.reactionBlocked && result.decisionSampled,
            "new revision must be sampled once reaction delay expires");
    require(result.activeTargetRevision == 1,
            "sampled command must still wait for command latency");

    result = executor.step(0.30, 0.09, desired);
    require(result.activeTargetRevision == 1,
            "target must remain old before latency deadline");

    result = executor.step(0.31, 0.01, desired);
    require(result.queuedCommandApplied,
            "latency-expired command must become active");
    require(result.activeTargetRevision == 2,
            "active target revision must advance after latency");
}

void testTargetRevisionCanAdvanceInsideSameIntent()
{
    Executor::PilotSkillProfile profile = expertProfile();
    profile.execution.reactionDelaySeconds = 0.50;
    profile.execution.perceptionDecisionRateHz = 100.0;
    profile.execution.commandLatencySeconds = 0.0;

    Executor executor(profile);

    Executor::Command initial;
    initial.revision = 7;
    initial.targetRevision = 70;
    require(executor.reset(0.0, initial),
            "target-revision reset must succeed");

    Executor::Command established;
    established.revision = 7;
    established.targetRevision = 70;

    const auto midwayResult =
        executor.step(0.25, 0.25, established);
    require(midwayResult.status == Executor::Status::Ok &&
            midwayResult.reactionBlocked,
            "initial intent reaction window was not exercised");

    const auto establishedResult =
        executor.step(0.50, 0.25, established);
    require(establishedResult.status == Executor::Status::Ok &&
            !establishedResult.reactionBlocked,
            "initial intent did not finish its configured reaction delay");

    Executor::Command desired = established;
    desired.targetRevision = 71;
    desired.linearAccelerationDemandMapMetersPerSec2 = {2.0, 0.0, 0.0};

    const auto result = executor.step(0.51, 0.01, desired);

    require(!result.reactionBlocked,
            "new target inside established intent restarted reaction delay");
    require(result.decisionSampled && result.queuedCommandApplied,
            "new target inside same intent must reach decision pipeline");
    require(result.observedIntentRevision == 7,
            "pilot changed high-level intent identity for local target update");
    require(result.activeTargetRevision == 71,
            "pilot did not publish concrete target revision");
}

void testDecisionCadenceIsSampleAndHold()
{
    Executor::PilotSkillProfile profile = expertProfile();
    profile.execution.perceptionDecisionRateHz = 10.0;

    Executor executor(profile);
    Executor::Command initial;
    initial.revision = 1;
    require(executor.reset(0.0, initial), "reset must succeed");

    Executor::Command desired;
    desired.revision = 2;
    desired.linearAccelerationDemandMapMetersPerSec2 = {1.0, 0.0, 0.0};

    auto result = executor.step(0.01, 0.01, desired);
    require(result.decisionSampled && result.queuedCommandApplied,
            "first eligible command must be sampled and applied");

    desired.linearAccelerationDemandMapMetersPerSec2 = {5.0, 0.0, 0.0};
    result = executor.step(0.05, 0.04, desired);
    require(!result.decisionSampled,
            "command changes inside one intent must wait for decision cadence");

    result = executor.step(0.10, 0.05, desired);
    require(result.decisionSampled,
            "next decision tick must sample the evolved command");
}

void testEmergencyCanShortenReactionDelay()
{
    Executor::PilotSkillProfile profile = expertProfile();
    profile.execution.reactionDelaySeconds = 0.50;
    profile.execution.perceptionDecisionRateHz = 100.0;
    profile.execution.emergencyResponseThreshold01 = 0.80;
    profile.execution.emergencyReactionDelayScale = 0.20;

    Executor executor(profile);
    Executor::Command initial;
    initial.revision = 1;
    require(executor.reset(0.0, initial), "reset must succeed");

    Executor::Command emergency;
    emergency.revision = 2;
    emergency.emergency = true;
    emergency.hazardUrgency01 = 0.90;
    emergency.linearAccelerationDemandMapMetersPerSec2 = {10.0, 0.0, 0.0};

    auto result = executor.step(0.01, 0.01, emergency);
    require(result.reactionBlocked,
            "emergency still uses the configured shortened reaction delay");

    result = executor.step(0.10, 0.09, emergency);
    require(result.reactionBlocked,
            "effective emergency delay is measured from first observation");

    result = executor.step(0.11, 0.01, emergency);
    require(!result.reactionBlocked && result.decisionSampled,
            "urgent emergency must react after scaled delay, not full delay");
}

void testDeterministicNoiseIsReplayStable()
{
    Executor::PilotSkillProfile profile = expertProfile();
    profile.execution.perceptionDecisionRateHz = 20.0;
    profile.execution.deterministicLinearNoiseAmplitudeMetersPerSec2 = 0.5;
    profile.execution.deterministicAngularNoiseAmplitudeRadPerSec2 = 0.25;
    profile.execution.deterministicSeed = 0x12345678ULL;

    Executor a(profile);
    Executor b(profile);
    Executor::Command initial;
    initial.revision = 1;
    require(a.reset(0.0, initial) && b.reset(0.0, initial),
            "replay executors must reset");

    Executor::Command desired;
    desired.revision = 2;
    desired.linearAccelerationDemandMapMetersPerSec2 = {2.0, -1.0, 0.5};
    desired.angularAccelerationDemandMapRadPerSec2 = {0.2, 0.1, -0.3};

    for (int i = 1; i <= 200; ++i)
    {
        const double t = 0.01 * static_cast<double>(i);
        const auto ra = a.step(t, 0.01, desired);
        const auto rb = b.step(t, 0.01, desired);
        require(ra.status == Executor::Status::Ok &&
                    rb.status == Executor::Status::Ok,
                "deterministic replay steps must stay valid");
        requireNear(
            ra.executedLinearAccelerationDemandMapMetersPerSec2.x,
            rb.executedLinearAccelerationDemandMapMetersPerSec2.x,
            0.0,
            "same seed/input must reproduce identical linear command"
        );
        requireNear(
            ra.executedAngularAccelerationDemandMapRadPerSec2.z,
            rb.executedAngularAccelerationDemandMapRadPerSec2.z,
            0.0,
            "same seed/input must reproduce identical angular command"
        );
    }

    Executor::PilotSkillProfile changed = profile;
    changed.execution.deterministicSeed = 0x87654321ULL;
    Executor sameSeed(profile);
    Executor differentSeed(changed);
    require(
        sameSeed.reset(0.0, initial) &&
            differentSeed.reset(0.0, initial),
        "seed comparison executors must reset"
    );

    bool diverged = false;
    for (int i = 1; i <= 100; ++i)
    {
        const double t = 0.01 * static_cast<double>(i);
        const auto same = sameSeed.step(t, 0.01, desired);
        const auto different = differentSeed.step(t, 0.01, desired);
        if (std::abs(
                same.executedLinearAccelerationDemandMapMetersPerSec2.x -
                different.executedLinearAccelerationDemandMapMetersPerSec2.x) >
            1.0e-9)
        {
            diverged = true;
            break;
        }
    }
    require(diverged, "different deterministic seeds must produce variation");
}

double runCommandStep(
    const Executor::PilotSkillProfile& profile,
    double& minimumAfterPeak
)
{
    Executor executor(profile);
    Executor::Command initial;
    initial.revision = 1;
    require(executor.reset(0.0, initial), "step-response reset must succeed");

    Executor::Command desired;
    desired.revision = 2;
    desired.linearAccelerationDemandMapMetersPerSec2 = {1.0, 0.0, 0.0};

    double maximum = -1.0e9;
    minimumAfterPeak = 1.0e9;
    bool peakSeen = false;
    for (int i = 1; i <= 400; ++i)
    {
        const double t = 0.005 * static_cast<double>(i);
        const auto result = executor.step(t, 0.005, desired);
        require(result.status == Executor::Status::Ok,
                "step-response execution must remain valid");
        const double value =
            result.executedLinearAccelerationDemandMapMetersPerSec2.x;
        if (value > maximum)
        {
            maximum = value;
        }
        else if (maximum > 1.05)
        {
            peakSeen = true;
        }
        if (peakSeen)
            minimumAfterPeak = std::min(minimumAfterPeak, value);
    }
    return maximum;
}

void testDampingControlsOvershoot()
{
    Executor::PilotSkillProfile expert = expertProfile();
    expert.execution.responseFrequencyHz = 1.0;
    expert.execution.dampingRatio = 1.0;

    double expertMinimum = 0.0;
    const double expertMaximum = runCommandStep(expert, expertMinimum);
    require(expertMaximum < 1.02,
            "critically damped expert command should not materially overshoot");

    Executor::PilotSkillProfile poor = expert;
    poor.execution.dampingRatio = 0.20;

    double poorMinimum = 0.0;
    const double poorMaximum = runCommandStep(poor, poorMinimum);
    require(poorMaximum > 1.20,
            "under-damped poor pilot must visibly overshoot a command step");
    require(poorMinimum < 1.0,
            "under-damped response must ring back after overshoot");
}

struct TrackingOutcome
{
    double position = 0.0;
    double velocity = 0.0;
    int targetCrossings = 0;
};

TrackingOutcome simulateDockingLikeTracking(
    const Executor::PilotSkillProfile& profile
)
{
    Executor executor(profile);
    Executor::Command initial;
    initial.revision = 1;
    require(executor.reset(0.0, initial), "tracking reset must succeed");

    double position = -5.0;
    double velocity = 3.0;
    double previousPosition = position;
    int crossings = 0;

    constexpr double dt = 0.005;
    constexpr double kp = 1.5;
    constexpr double kd = 2.0;

    for (int i = 1; i <= 1000; ++i)
    {
        Executor::Command desired;
        desired.revision = 2;
        desired.linearAccelerationDemandMapMetersPerSec2.x =
            kp * (-position) + kd * (-velocity);

        const double time = dt * static_cast<double>(i);
        const auto result = executor.step(time, dt, desired);
        require(result.status == Executor::Status::Ok,
                "closed-loop tracking execution must remain valid");

        const double acceleration =
            result.executedLinearAccelerationDemandMapMetersPerSec2.x;
        velocity += acceleration * dt;
        position += velocity * dt;

        if (position * previousPosition < 0.0)
            ++crossings;
        previousPosition = position;
    }

    return {position, velocity, crossings};
}

void testPoorPilotCanProduceRealDockingOscillation()
{
    Executor::PilotSkillProfile expert = expertProfile();
    const TrackingOutcome good = simulateDockingLikeTracking(expert);
    require(std::abs(good.position) < 0.10 &&
                std::abs(good.velocity) < 0.10,
            "expert execution must settle near the docking target");

    Executor::PilotSkillProfile poor = expertProfile();
    poor.execution.reactionDelaySeconds = 0.20;
    poor.execution.perceptionDecisionRateHz = 8.0;
    poor.execution.commandLatencySeconds = 0.15;
    poor.execution.responseFrequencyHz = 1.0;
    poor.execution.dampingRatio = 0.20;
    poor.execution.commandGain = 1.40;
    poor.execution.maxLinearCommandSlewMetersPerSec3 = 30.0;

    const TrackingOutcome bad = simulateDockingLikeTracking(poor);
    require(bad.targetCrossings >= 2,
            "poor delayed under-damped pilot must cross the target repeatedly");
    require(std::abs(bad.position) > 0.20 ||
                std::abs(bad.velocity) > 0.20,
            "poor pilot must remain visibly less settled at the same deadline");
}

void testPolicyPreferencesDoNotCorruptExecution()
{
    Executor::PilotSkillProfile aProfile = expertProfile();
    Executor::PilotSkillProfile bProfile = aProfile;

    aProfile.policy.anticipationSeconds = 0.0;
    aProfile.policy.riskPreference01 = 0.0;
    aProfile.policy.comfortPreference01 = 1.0;

    bProfile.policy.anticipationSeconds = 5.0;
    bProfile.policy.riskPreference01 = 1.0;
    bProfile.policy.comfortPreference01 = 0.0;

    Executor a(aProfile);
    Executor b(bProfile);
    Executor::Command initial;
    initial.revision = 1;
    require(a.reset(0.0, initial) && b.reset(0.0, initial),
            "policy-separation executors must reset");

    Executor::Command desired;
    desired.revision = 2;
    desired.linearAccelerationDemandMapMetersPerSec2 = {3.0, 2.0, 1.0};

    for (int i = 1; i <= 100; ++i)
    {
        const double t = 0.01 * static_cast<double>(i);
        const auto ra = a.step(t, 0.01, desired);
        const auto rb = b.step(t, 0.01, desired);
        requireNear(
            ra.executedLinearAccelerationDemandMapMetersPerSec2.x,
            rb.executedLinearAccelerationDemandMapMetersPerSec2.x,
            0.0,
            "risk/comfort/anticipation policy must not alter execution physics"
        );
    }
}

} // namespace

int main()
{
    try
    {
        testProfileValidation();
        testReactionDelayAndCommandLatency();
        testTargetRevisionCanAdvanceInsideSameIntent();
        testDecisionCadenceIsSampleAndHold();
        testEmergencyCanShortenReactionDelay();
        testDeterministicNoiseIsReplayStable();
        testDampingControlsOvershoot();
        testPoorPilotCanProduceRealDockingOscillation();
        testPolicyPreferencesDoNotCorruptExecution();

        std::cout << "NAVIGATION PILOT SKILL TESTS: PASS\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "NAVIGATION PILOT SKILL TESTS: FAIL: "
                  << error.what() << '\n';
        return 1;
    }
}
