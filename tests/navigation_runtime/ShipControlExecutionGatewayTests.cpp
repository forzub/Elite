#include "src/game/ship/control/ShipControlExecutionGateway.h"
#include "src/game/ship/plant/ShipPhysicalControlModels.h"
#include "src/game/ship/plant/ShipPhysicalActuatorPlant.h"
#include "src/world/WorldParams.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace
{

using Gateway =
    game::ship::control::ShipControlExecutionGateway;
using Path = Gateway::TranslationPath;

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void testManualInputOwnsTranslation()
{
    ShipControlState control;
    control.navigationAccelerationDemandValid = true;
    control.navigationVelocityTargetValid = true;
    control.navigationPrecisionTranslationOnly = true;
    control.targetSpeedRate = 1.0f;

    require(
        Gateway::selectTranslationPath(
            control,
            game::navigation::LocalFlightControlLaw::Assisted
        ) == Path::ManualFlight,
        "manual longitudinal input did not override navigation translation"
    );
}

void testMainGimbalOwnsOrdinaryPhysicalTranslationPath()
{
    ShipControlState control;
    control.navigationAccelerationDemandValid = true;
    control.navigationVelocityTargetValid = true;
    control.mainGimbalRightInput = 0.5f;

    require(
        Gateway::selectTranslationPath(
            control,
            game::navigation::LocalFlightControlLaw::Newtonian
        ) == Path::ManualFlight,
        "Newtonian TVC command was bypassed by legacy navigation translation"
    );
}

void testPrecisionPath()
{
    ShipControlState control;
    control.navigationAccelerationDemandValid = true;
    control.navigationVelocityTargetValid = true;
    control.navigationPrecisionTranslationOnly = true;

    require(
        Gateway::selectTranslationPath(
            control,
            game::navigation::LocalFlightControlLaw::Assisted
        ) == Path::NavigationPrecision,
        "precision navigation did not select precision translation path"
    );
}

void testAssistedPath()
{
    ShipControlState control;
    control.navigationAccelerationDemandValid = true;
    control.navigationVelocityTargetValid = true;

    require(
        Gateway::selectTranslationPath(
            control,
            game::navigation::LocalFlightControlLaw::Assisted
        ) == Path::NavigationAssisted,
        "Assisted navigation did not select Assisted translation path"
    );
}

void testNewtonianNavigationDemandPath()
{
    ShipControlState control;
    control.navigationAccelerationDemandValid = true;
    control.navigationVelocityTargetValid = true;

    require(
        Gateway::selectTranslationPath(
            control,
            game::navigation::LocalFlightControlLaw::Newtonian
        ) == Path::NavigationAccelerationDemand,
        "Newtonian navigation did not select acceleration-demand path"
    );
}

void testPhysicalOverloadAppliesMainReactionTorque()
{
    using namespace game::ship::plant;

    ShipParams nominal {};
    nominal.maxPitchRate = 2.5f;
    nominal.maxYawRate = 2.5f;
    nominal.maxRollRate = 3.0f;
    nominal.angularAccel = 3.0f;
    nominal.angularDamping = 2.5f;
    nominal.maxCombatSpeed = 500.0f;
    nominal.maxCruiseSpeed = 1000.0f;
    nominal.throttleAccel = 5.0f;
    nominal.forwardMainEngineAvailable = true;
    nominal.reverseMainEngineAvailable = false;
    nominal.forwardMainEngineAccelerationMps2 = 60.0f;
    nominal.mainGimbalPitchLimitRad = 0.12f;
    nominal.mainGimbalYawLimitRad = 0.12f;
    nominal.mainGimbalRateRadPerSec = 0.14f;
    nominal.strafeAccel = 4.0f;
    nominal.manoeuvreThrusterAccel = 4.0f;
    nominal.maxGs = 20.0f;
    nominal.maxLinearGs = 20.0f;
    nominal.turnRadius = 20.0f;
    nominal.massKg = 260000.0;
    nominal.pitchInertiaKgM2 = 11219866.6666667;
    nominal.yawInertiaKgM2 = 25324866.6666667;
    nominal.rollInertiaKgM2 = 15188333.3333333;

    const auto model = PhysicalControlModelId::NewtonianI;
    const auto slots = healthySlotState(physicalControlSlots(model));
    const auto physical = evaluatePhysicalControlModel(
        model,
        nominal,
        slots
    );
    require(physical.valid, "Newtonian-I physical projection failed");

    ShipTransform transform {};
    transform.motion.mode = game::navigation::MotionMode::HubTactical;
    transform.motion.systemId = 1;
    transform.motion.localControlLaw =
        game::navigation::LocalFlightControlLaw::Newtonian;
    transform.motion.travelFrame.systemId = 1;
    transform.motion.travelFrame.frameId = "gateway-main-reaction";
    transform.motion.travelFrame.localToWorldBasis = glm::dmat3(1.0);
    transform.motion.travelFrame.valid = true;

    ShipControlState control;
    control.targetSpeedRate = 1.0f;
    control.mainGimbalRightInput = 1.0f;

    WorldParams world {};
    Gateway::evaluateAttitudeControl(
        transform,
        physical.effectivePhysics,
        control,
        world,
        1.0f
    );

    (void)Gateway::evaluateTranslationControl(
        transform,
        physical.effectivePhysics,
        control,
        model,
        slots,
        1.0f
    );

    require(
        transform.motion.mainGimbalYawRad > 0.0,
        "physical gateway did not move the Newtonian nozzle"
    );
    require(
        std::abs(
            transform.motion.mainEngineAccelerationMps2.x
        ) > 1.0e-6,
        "physical gateway did not redirect Newtonian-I main thrust laterally"
    );
    require(
        std::abs(
            transform.motion.mainPropulsionTorqueBodyNm.y
        ) > 1.0,
        "aft-mounted Newtonian-I gimbaled main lost its real yaw reaction torque"
    );
    require(
        std::abs(transform.yawRate) > 1.0e-6f,
        "Newtonian-I gimbaled main reaction did not reach hull yaw rate"
    );
}


void testAssistedSingleMainLossUsesTrimBeforeHullRotation()
{
    using namespace game::ship::plant;

    ShipParams nominal {};
    nominal.maxPitchRate = 2.5f;
    nominal.maxYawRate = 2.5f;
    nominal.maxRollRate = 3.0f;
    nominal.angularAccel = 3.0f;
    nominal.angularDamping = 2.5f;
    nominal.maxCombatSpeed = 500.0f;
    nominal.maxCruiseSpeed = 1000.0f;
    nominal.throttleAccel = 5.0f;
    nominal.assistedMinimumTargetSpeedChangeRateMps2 = 1.0f;
    nominal.assistedTargetSpeedChangeRateFractionPerSecond = 0.10f;
    nominal.forwardMainEngineAvailable = true;
    nominal.reverseMainEngineAvailable = true;
    nominal.forwardMainEngineAccelerationMps2 = 60.0f;
    nominal.reverseMainEngineAccelerationMps2 = 60.0f;
    nominal.mainGimbalPitchLimitRad = 0.12f;
    nominal.mainGimbalYawLimitRad = 0.12f;
    nominal.mainGimbalRateRadPerSec = 0.14f;
    nominal.strafeAccel = 40.0f;
    nominal.manoeuvreThrusterAccel = 0.0f;
    nominal.maxGs = 20.0f;
    nominal.maxLinearGs = 20.0f;
    nominal.turnRadius = 20.0f;
    nominal.massKg = 260000.0;
    nominal.pitchInertiaKgM2 = 11219866.6666667;
    nominal.yawInertiaKgM2 = 25324866.6666667;
    nominal.rollInertiaKgM2 = 15188333.3333333;
    nominal.physicalActuatorHalfExtentsMeters = {13.0, 2.5, 11.1};

    const auto model = PhysicalControlModelId::AssistedY;
    auto slots = healthySlotState(physicalControlSlots(model));

    // Lose aft_top: the two surviving aft mains sit below COM, so a pure
    // forward burn has a real pitch-bias before the Assisted allocator trims it.
    slots[0].operational = false;
    slots[0].thrust01 = 0.0;
    slots[0].vectoring01 = 0.0;

    const auto physical = evaluatePhysicalControlModel(
        model,
        nominal,
        slots
    );
    require(physical.valid, "Assisted-Y damaged projection failed");

    const auto rawReaction = evaluateMainPropulsionReaction(
        model,
        slots,
        physical.effectivePhysics,
        {0.0, 0.0, -40.0}
    );
    require(
        rawReaction.valid &&
        std::abs(rawReaction.angularAccelerationBodyRadPerSec2.x) > 1.0e-3,
        "Assisted-Y damage geometry did not create the expected raw pitch bias"
    );

    ShipTransform transform {};
    transform.motion.mode = game::navigation::MotionMode::HubTactical;
    transform.motion.systemId = 1;
    transform.motion.localControlLaw =
        game::navigation::LocalFlightControlLaw::Assisted;
    transform.motion.travelFrame.systemId = 1;
    transform.motion.travelFrame.frameId = "gateway-assisted-damage-trim";
    transform.motion.travelFrame.localToWorldBasis = glm::dmat3(1.0);
    transform.motion.travelFrame.valid = true;

    ShipControlState control;
    control.targetSpeedRate = 1.0f;

    WorldParams world {};
    Gateway::evaluateAttitudeControl(
        transform,
        physical.effectivePhysics,
        control,
        world,
        0.1f
    );

    (void)Gateway::evaluateTranslationControl(
        transform,
        physical.effectivePhysics,
        control,
        model,
        slots,
        0.1f
    );

    require(
        glm::length(transform.motion.mainEngineAccelerationMps2) > 1.0e-3,
        "Assisted-Y damage trim suppressed main propulsion entirely"
    );
    require(
        transform.motion.mainTrimLoad01 > 0.0,
        "Assisted-Y damage did not register automatic trim load"
    );
    require(
        transform.motion.mainTrimUsableScale01 < 1.0,
        "Assisted-Y damage trim did not reserve main-force margin"
    );
    require(
        glm::length(transform.motion.mainEngineAccelerationMps2) <
            physical.effectivePhysics.forwardMainEngineAccelerationMps2,
        "Assisted-Y damage trim did not reserve any main-force margin"
    );
    require(
        std::abs(
            transform.motion.mainPropulsionAngularAccelerationBodyRadPerSec2.x
        ) < 1.0e-6,
        "Assisted-Y automatic trim left avoidable pitch reaction"
    );
    require(
        std::abs(transform.pitchRate) < 1.0e-5f,
        "Assisted-Y hull rotated despite sufficient automatic trim authority"
    );
}

void testNeutralManualPath()
{
    ShipControlState control;

    require(
        Gateway::selectTranslationPath(
            control,
            game::navigation::LocalFlightControlLaw::Assisted
        ) == Path::ManualFlight,
        "neutral ordinary control did not remain on manual flight path"
    );
}

} // namespace

int main()
{
    try
    {
        testManualInputOwnsTranslation();
        testMainGimbalOwnsOrdinaryPhysicalTranslationPath();
        testPrecisionPath();
        testAssistedPath();
        testNewtonianNavigationDemandPath();
        testPhysicalOverloadAppliesMainReactionTorque();
        testAssistedSingleMainLossUsesTrimBeforeHullRotation();
        testNeutralManualPath();

        std::cout << "SHIP CONTROL EXECUTION GATEWAY TESTS: PASS\n";
        std::cout << " - manual/TVC input retains ordinary physical-path priority\n";
        std::cout << " - precision/Assisted/Newtonian execution selection is centralized\n";
        std::cout << " - callers no longer need to interpret ShipControlState\n";
        std::cout << " - axial Newtonian-I main is centered radially, while aft-mounted TVC creates real yaw/pitch reaction\n";
        std::cout << " - Assisted damaged-main bias is trimmed before avoidable hull rotation\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr << "SHIP CONTROL EXECUTION GATEWAY TESTS: FAIL: "
                  << error.what() << "\n";
        return EXIT_FAILURE;
    }
}
