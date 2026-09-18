#include "src/game/navigation/NavigationRuntimeControlBridge.h"
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
    require(bridge.reset(0.0, initial),
            "runtime bridge reset must succeed");

    Bridge::Intent intent;
    intent.revision = 2;
    intent.idealLinearAccelerationDemandMapMps2 = {3.0, 0.0, -6.0};
    intent.idealAngularAccelerationDemandMapRadPerSec2 = {1.0, 0.5, 0.25};

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

    requireNear(
        result.control.navigationLinearAccelerationDemandMapMps2.x,
        result.snapshot.executedLinearAccelerationDemandMapMps2.x,
        0.0,
        "control and debug/guidance snapshot must publish the same executed linear demand"
    );
    requireNear(
        result.control.navigationAngularAccelerationDemandMapRadPerSec2.x,
        result.snapshot.executedAngularAccelerationDemandMapRadPerSec2.x,
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

    game::navigation::DynamicMotionSystem::applyWorldAccelerationDemand(
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
    game::navigation::DynamicMotionSystem::applyWorldAccelerationDemand(
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
    control.navigationAngularAccelerationDemandMapRadPerSec2 =
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
    control.navigationAngularAccelerationDemandMapRadPerSec2 =
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

void testBridgeDemandCanReachCapabilityLayerWithoutLegacyKeys()
{
    Bridge bridge(expertProfile());

    Bridge::Intent initial;
    initial.revision = 7;
    require(bridge.reset(0.0, initial),
            "end-to-end bridge reset must succeed");

    Bridge::Intent intent;
    intent.revision = 8;
    intent.idealAngularAccelerationDemandMapRadPerSec2 =
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
