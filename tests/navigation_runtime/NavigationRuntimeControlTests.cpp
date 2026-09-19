#include "src/game/navigation/NavigationRuntimeControlBridge.h"
#include "src/game/navigation/NpcNavigationIntentController.h"
#include "src/game/navigation/ReplicatedNavigationExecutionState.h"
#include "src/game/navigation/DynamicMotionSystem.h"
#include "src/game/shared/SharedShipPhysics.h"
#include "src/game/ship/core/ShipParams.h"
#include "src/game/ship/core/ShipTransform.h"
#include "src/world/WorldParams.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

using Bridge = game::navigation::NavigationRuntimeControlBridge;

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

Bridge::PilotSkillProfile expertProfile()
{
    Bridge::PilotSkillProfile profile;
    profile.execution.reactionDelaySeconds = 0.0;
    profile.execution.perceptionDecisionRateHz = 100.0;
    profile.execution.commandLatencySeconds = 0.0;
    profile.execution.responseFrequencyHz = 4.0;
    profile.execution.dampingRatio = 1.0;
    profile.execution.commandGain = 1.0;
    profile.execution.maxLinearCommandSlewMetersPerSec3 = 1000.0;
    profile.execution.maxAngularCommandSlewRadPerSec3 = 1000.0;
    return profile;
}

ShipParams capabilityParams()
{
    ShipParams params {};
    params.maxPitchRate = 10.0f;
    params.maxYawRate = 10.0f;
    params.maxRollRate = 10.0f;
    params.angularAccel = 2.0f;
    params.angularDamping = 0.0f;

    params.maxCombatSpeed = 1000.0f;
    params.maxCruiseSpeed = 1000.0f;
    params.throttleAccel = 1.0f;

    params.strafeAccel = 2.0f;
    params.strafeDamping = 1.0f;
    params.maxStrafeSpeed = 1000.0f;
    params.manoeuvreThrusterAccel = 2.0f;

    params.maxGs = 100.0f;
    params.maxLinearGs = 1.0f;
    params.turnRadius = 1.0f;
    return params;
}

void testBridgePublishesOneDirectDemandSample()
{
    Bridge bridge(expertProfile());

    Bridge::Intent initial;
    initial.revision = 1;
    initial.targetRevision = 11;
    require(bridge.reset(0.0, initial),
            "runtime bridge reset must succeed");

    Bridge::Intent intent;
    intent.revision = 2;
    intent.targetRevision = 22;
    intent.idealLinearAccelerationSystemMps2 = {3.0, 0.0, -6.0};
    intent.idealAngularAccelerationSystemRadPerSec2 = {1.0, 0.5, 0.25};

    const auto result = bridge.step(0.01, 0.01, intent);
    require(
        result.status ==
            world::navigation::PilotSkillExecutor::Status::Ok,
        "bridge step must execute through accepted pilot skill"
    );
    require(result.snapshot.valid,
            "successful bridge step must publish a valid execution snapshot");
    require(result.control.navigationAccelerationDemandValid,
            "bridge must use the explicit navigation acceleration channel");
    require(result.control.navigationIntentRevision == intent.revision,
            "control sample must preserve navigation intent revision");
    require(result.snapshot.intentRevision == 2,
            "execution snapshot lost high-level intent revision");
    require(result.snapshot.activeTargetRevision == 22,
            "execution snapshot lost concrete target revision");

    requireNear(
        result.control.navigationLinearAccelerationDemandSystemMps2.x,
        result.snapshot.executedLinearAccelerationDemandSystemMps2.x,
        0.0,
        "control and debug/guidance snapshot must publish the same executed linear demand"
    );
    requireNear(
        result.control.navigationAngularAccelerationDemandSystemRadPerSec2.x,
        result.snapshot.executedAngularAccelerationDemandSystemRadPerSec2.x,
        0.0,
        "control and debug/guidance snapshot must publish the same executed angular demand"
    );

    requireNear(result.control.pitchInput, 0.0, 0.0,
                "autopilot bridge must not synthesize legacy pitch keys");
    requireNear(result.control.yawInput, 0.0, 0.0,
                "autopilot bridge must not synthesize legacy yaw keys");
    requireNear(result.control.rollInput, 0.0, 0.0,
                "autopilot bridge must not synthesize legacy roll keys");
    requireNear(result.control.forwardInput, 0.0, 0.0,
                "autopilot bridge must not synthesize keypad RCS keys");
    requireNear(result.control.targetSpeedRate, 0.0, 0.0,
                "autopilot bridge must not synthesize legacy throttle trim");
}

void testLinearDemandUsesRealMainAndManoeuvreAuthority()
{
    game::navigation::DynamicMotionState motion;
    const ShipParams params = capabilityParams();

    const glm::vec3 forward(0.0f, 0.0f, -1.0f);
    const glm::dvec3 demand(10.0, 0.0, -20.0);

    game::navigation::DynamicMotionSystem::applySystemAccelerationDemand(
        motion,
        params,
        demand,
        forward
    );

    constexpr double standardGravity = 9.80665;
    requireNear(
        glm::length(motion.mainEngineAccelerationMps2),
        standardGravity,
        1.0e-6,
        "main engine demand must clamp at maxLinearGs authority"
    );
    require(
        glm::dot(
            motion.mainEngineAccelerationMps2,
            glm::dvec3(forward)
        ) > 0.0,
        "main engine must remain forward-only"
    );
    requireNear(
        glm::length(motion.manoeuvreAccelerationMps2),
        2.0,
        1.0e-9,
        "remaining vector must clamp at manoeuvre-thruster authority"
    );

    const glm::dvec3 reverseDemand(0.0, 0.0, 20.0);
    game::navigation::DynamicMotionSystem::applySystemAccelerationDemand(
        motion,
        params,
        reverseDemand,
        forward
    );

    requireNear(
        glm::length(motion.mainEngineAccelerationMps2),
        0.0,
        1.0e-12,
        "reverse demand must not invent a reverse main engine"
    );
    requireNear(
        glm::length(motion.manoeuvreAccelerationMps2),
        2.0,
        1.0e-9,
        "reverse demand may use only real manoeuvre-thruster authority"
    );
}

void testAngularDemandUsesExistingCapabilityClamp()
{
    ShipTransform transform;
    ShipParams params = capabilityParams();
    WorldParams world {};

    ShipControlState control {};
    control.navigationAccelerationDemandValid = true;
    control.navigationAngularAccelerationDemandSystemRadPerSec2 =
        glm::dvec3(100.0, 0.0, 0.0);

    SharedShipPhysics::evaluateControl(
        transform,
        params,
        control,
        world,
        0.1f
    );

    requireNear(
        transform.pitchRate,
        0.2,
        1.0e-5,
        "direct world angular demand must clamp at the existing angular acceleration envelope"
    );
    requireNear(
        transform.yawRate,
        0.0,
        1.0e-6,
        "pure world-right angular demand must not create yaw"
    );
    requireNear(
        transform.rollRate,
        0.0,
        1.0e-6,
        "pure world-right angular demand must not create roll"
    );
}

void testManualAttitudeOverridesNavigationAngularDemand()
{
    ShipTransform transform;
    ShipParams params = capabilityParams();
    WorldParams world {};

    ShipControlState control {};
    control.navigationAccelerationDemandValid = true;
    control.navigationAngularAccelerationDemandSystemRadPerSec2 =
        glm::dvec3(-100.0, 0.0, 0.0);
    control.pitchInput = 1.0f;

    SharedShipPhysics::evaluateControl(
        transform,
        params,
        control,
        world,
        0.1f
    );

    require(
        transform.pitchRate > 0.0f,
        "material manual attitude input must win over navigation angular demand"
    );
    requireNear(
        transform.pitchRate,
        0.2,
        1.0e-5,
        "manual override must still use the same angular capability clamp"
    );
}

void testNpcGoalBecomesNavigationIntentWithoutLegacyControl()
{
    game::navigation::NpcNavigationKinematicState state;
    state.relativeSystemVelocityMps = glm::dvec3(0.0);
    state.forwardSystem = glm::dvec3(0.0, 0.0, -1.0);
    state.rightSystem = glm::dvec3(1.0, 0.0, 0.0);
    state.upSystem = glm::dvec3(0.0, 1.0, 0.0);
    state.pitchRateRadPerSec = 0.5;
    state.yawRateRadPerSec = -0.25;
    state.rollRateRadPerSec = 0.10;

    NpcNavigationGoal goal;
    goal.revision = 42;
    goal.mode = NpcNavigationGoalMode::MaintainForwardCruise;
    goal.desiredForwardSpeedMps = 10.0;
    goal.velocityResponsePerSecond = 0.5;
    goal.angularDampingPerSecond = 2.0;

    const auto intent =
        game::navigation::NpcNavigationIntentController::buildIntent(
            state,
            goal
        );

    require(intent.revision == 42,
            "NPC navigation goal revision must become the runtime intent revision");
    requireNear(
        intent.idealLinearAccelerationSystemMps2.x,
        0.0,
        1.0e-12,
        "identity ship forward cruise must not create lateral X acceleration"
    );
    requireNear(
        intent.idealLinearAccelerationSystemMps2.y,
        0.0,
        1.0e-12,
        "identity ship forward cruise must not create vertical acceleration"
    );
    requireNear(
        intent.idealLinearAccelerationSystemMps2.z,
        -5.0,
        1.0e-12,
        "nominal NPC goal must become a physical forward acceleration demand"
    );

    const glm::dvec3 angularDemand(
        intent.idealAngularAccelerationSystemRadPerSec2.x,
        intent.idealAngularAccelerationSystemRadPerSec2.y,
        intent.idealAngularAccelerationSystemRadPerSec2.z
    );

    requireNear(
        glm::dot(angularDemand, state.rightSystem),
        -1.0,
        1.0e-6,
        "NPC nominal intent must oppose positive pitch rate on the ship-right axis"
    );
    requireNear(
        glm::dot(angularDemand, state.upSystem),
        0.5,
        1.0e-6,
        "NPC nominal intent must oppose negative yaw rate on the ship-up axis"
    );
    requireNear(
        glm::dot(angularDemand, state.forwardSystem),
        -0.2,
        1.0e-6,
        "NPC nominal intent must oppose positive roll rate on the ship-forward axis"
    );
}

void testNpcHoldGoalBrakesRelativeVelocity()
{
    game::navigation::NpcNavigationKinematicState state;
    state.relativeSystemVelocityMps = glm::dvec3(4.0, -2.0, 1.0);

    NpcNavigationGoal goal;
    goal.revision = 5;
    goal.mode = NpcNavigationGoalMode::Hold;
    goal.velocityResponsePerSecond = 0.25;

    const auto intent =
        game::navigation::NpcNavigationIntentController::buildIntent(
            state,
            goal
        );

    requireNear(
        intent.idealLinearAccelerationSystemMps2.x,
        -1.0,
        1.0e-12,
        "hold goal must brake actual relative X velocity"
    );
    requireNear(
        intent.idealLinearAccelerationSystemMps2.y,
        0.5,
        1.0e-12,
        "hold goal must brake actual relative Y velocity"
    );
    requireNear(
        intent.idealLinearAccelerationSystemMps2.z,
        -0.25,
        1.0e-12,
        "hold goal must brake actual relative Z velocity"
    );
}

void testReplicatedNavigationExecutionStateIsReadOnlyTruth()
{
    game::navigation::ReplicatedNavigationExecutionState state;

    game::navigation::ReplicatedNavigationExecution ignored;
    ignored.entityId = EntityId{7u};
    ignored.execution.valid = false;

    game::navigation::ReplicatedNavigationExecution accepted;
    accepted.entityId = EntityId{42u};
    accepted.shipInstanceId = 4242u;
    accepted.execution.valid = true;
    accepted.execution.intentRevision = 100u;
    accepted.execution.activeTargetRevision = 99u;
    accepted.execution.executedLinearAccelerationDemandSystemMps2 =
        glm::dvec3(1.0, 2.0, 3.0);

    state.replace({ignored, accepted});

    require(state.all().size() == 1u,
            "replicated navigation state must retain only valid server truth");
    const auto* found = state.find(EntityId{42u});
    require(found != nullptr,
            "replicated navigation state must resolve the authoritative entity");
    require(found->execution.intentRevision == 100u,
            "replicated navigation state must preserve intent revision exactly");
    requireNear(
        found->execution.executedLinearAccelerationDemandSystemMps2.y,
        2.0,
        0.0,
        "replicated navigation state must preserve executed demand exactly"
    );
    require(state.find(EntityId{7u}) == nullptr,
            "invalid replicated navigation rows must not become visible truth");

    const auto* byStableAsset = state.find(
        game::navigation::NavigationAssetRef::ship(4242u)
    );
    require(byStableAsset != nullptr,
            "replicated navigation truth must resolve a stable ship instance");
    require(byStableAsset->entityId == EntityId{42u},
            "stable ship-instance lookup must resolve the authoritative entity");

    require(
        state.find(game::navigation::NavigationAssetRef::ship(9999u)) == nullptr,
        "unknown stable ship instance must not fabricate execution truth"
    );
}

void testBridgeDemandCanReachCapabilityLayerWithoutLegacyKeys()
{
    Bridge bridge(expertProfile());

    Bridge::Intent initial;
    initial.revision = 7;
    require(bridge.reset(0.0, initial),
            "end-to-end bridge reset must succeed");

    Bridge::Intent intent;
    intent.revision = 8;
    intent.idealAngularAccelerationSystemRadPerSec2 =
        {100.0, 0.0, 0.0};

    // Let the expert executor ramp toward the request.
    Bridge::StepResult result;
    for (int i = 1; i <= 20; ++i)
    {
        result = bridge.step(
            0.01 * static_cast<double>(i),
            0.01,
            intent
        );
    }

    require(result.snapshot.valid,
            "bridge must produce a valid executed demand");

    ShipTransform transform;
    const ShipParams params = capabilityParams();
    WorldParams world {};

    SharedShipPhysics::evaluateControl(
        transform,
        params,
        result.control,
        world,
        0.1f
    );

    require(
        transform.pitchRate > 0.0f,
        "bridge direct demand must reach the actual ship angular-control path"
    );
    require(
        transform.pitchRate <= 0.20001f,
        "actual ship capability must remain authoritative downstream of pilot skill"
    );
}

} // namespace

int main()
{
    try
    {
        testBridgePublishesOneDirectDemandSample();
        testLinearDemandUsesRealMainAndManoeuvreAuthority();
        testAngularDemandUsesExistingCapabilityClamp();
        testManualAttitudeOverridesNavigationAngularDemand();
        testNpcGoalBecomesNavigationIntentWithoutLegacyControl();
        testNpcHoldGoalBrakesRelativeVelocity();
        testReplicatedNavigationExecutionStateIsReadOnlyTruth();
        testBridgeDemandCanReachCapabilityLayerWithoutLegacyKeys();

        std::cout << "NAVIGATION RUNTIME CONTROL TESTS: PASS\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "NAVIGATION RUNTIME CONTROL TESTS: FAIL: "
                  << error.what() << '\n';
        return 1;
    }
}
