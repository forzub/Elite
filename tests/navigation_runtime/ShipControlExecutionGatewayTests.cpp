#include "src/game/ship/control/ShipControlExecutionGateway.h"
#include "src/game/ship/plant/ShipPhysicalControlModels.h"
#include "src/game/ship/plant/ShipPhysicalActuatorPlant.h"
#include "src/game/equipment/engine/presets/CobrettyCR1VectorMainDrive.h"
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
        "Assisted-Y feedback trim suppressed main propulsion entirely"
    );
    require(
        transform.motion.mainTrimLoad01 > 0.0,
        "Assisted-Y damage did not engage physical feedback trim"
    );
    require(
        transform.motion.mainTrimUsableScale01 > 0.99,
        "Assisted-Y feedback hid pitch bias by unnecessarily killing thrust"
    );
    require(
        std::abs(
            transform.motion.mainPropulsionAngularAccelerationBodyRadPerSec2.x
        ) <
        std::abs(rawReaction.angularAccelerationBodyRadPerSec2.x),
        "Assisted-Y physical feedback did not reduce pitch reaction"
    );
    require(
        std::abs(transform.pitchRate) > 1.0e-8f,
        "Assisted-Y feedback magically erased all residual physical reaction"
    );
}

void testRejectedAllocatorCannotMoveShipOnRequestedAcceleration()
{
    using namespace game::ship::plant;
    ShipParams physics {};
    physics.forwardMainEngineAvailable=true;
    physics.forwardMainEngineAccelerationMps2=30.0f;
    physics.maxCombatSpeed=400.0f;
    physics.maxGs=12.0f;
    physics.maxLinearGs=12.0f;
    physics.throttleAccel=5.0f;
    physics.strafeAccel=15.0f;
    physics.manoeuvreThrusterAuthorityExplicit=true;
    physics.manoeuvreThrusterAccel=0.0f;

    ShipTransform transform {};
    transform.motion.mode=game::navigation::MotionMode::HubTactical;
    transform.motion.systemId=1;
    transform.motion.localControlLaw=
        game::navigation::LocalFlightControlLaw::Assisted;
    transform.motion.travelFrame.systemId=1;
    transform.motion.travelFrame.frameId="invalid-plant";
    transform.motion.travelFrame.localToWorldBasis=glm::dmat3(1.0);
    transform.motion.travelFrame.valid=true;

    ShipControlState control;
    control.targetSpeedRate=1.0f;
    control.forwardInput=1.0f;
    // The real Assisted-I topology has four slots. Deliberately give
    // a nonexistent slot matrix: its physical solver cannot execute.
    const std::vector<EngineSlotState> missing {};
    (void)Gateway::evaluateTranslationControl(
        transform,physics,control,PhysicalControlModelId::AssistedI,
        missing,0.02f);
    require(glm::length(transform.motion.mainEngineAccelerationMps2)<
                1.0e-10 &&
            glm::length(transform.motion.assistedStabilizationAccelerationMps2)<
                1.0e-10 &&
            glm::length(transform.motion.engineAccelerationMps2)<1.0e-10,
            "invalid motor allocator leaked unexecuted Assisted acceleration");
    require(glm::length(
                transform.motion.mainPropulsionTorqueBodyNm)<1.0e-10,
            "invalid motor allocator invented rigid-body torque");
    // Requests remain observable for diagnostics, but may not enter
    // the actual fixed-step displacement after allocator rejection.
    require(glm::length(
                transform.motion.requestedMainEngineAccelerationMps2)>1.0e-6,
            "invalid motor allocator test lacked a real input request");
    require(Gateway::propagateHubTacticalTranslation(
                transform,physics,0.02),
            "rejected allocator could not propagate HubTactical state");
    require(glm::length(transform.motion.localVelocityMps)<1.0e-9 &&
            glm::length(transform.motion.localPositionMeters)<1.0e-9,
            "unexecuted motor request leaked into physical velocity/position");
}

void testExplicitEmptyPowerBudgetCannotRunInstalledMain()
{
    using namespace game::ship::plant;
    using namespace game::equipment::engine;
    using game::ship::control::MainEnginePowerGroup;

    const auto model=PhysicalControlModelId::NewtonianI;
    const auto& topology=physicalControlSlots(model);
    const auto slots=healthySlotState(topology);
    const auto passport=presets::CobrettyCR1VectorMainDrive();
    game::equipment::EquipmentDamageLayer damage {};
    EngineOperatingState operating {};
    operating.temperatureK=passport.nominalOperatingTemperatureK;
    std::vector<MountedEngine> mounted(topology.size());
    for(std::size_t i=0;i<topology.size();++i)
        if(topology[i].role==EngineSlotRole::AftMain)
            mounted[i]={&passport,&damage,&operating,topology[i].id,true};

    ShipParams physics {};
    physics.massKg=260000.0;
    physics.forwardMainEngineAvailable=true;
    physics.forwardMainEngineAccelerationMps2=40.0f;
    physics.maxCombatSpeed=400.0f;
    physics.maxCruiseSpeed=800.0f;
    physics.maxGs=12.0f;
    physics.maxLinearGs=12.0f;
    physics.throttleAccel=5.0f;

    ShipControlState control {};
    control.forwardInput=1.0f;
    control.targetSpeedRate=1.0f;

    auto makeTransform=[]() {
        ShipTransform t {};
        t.motion.mode=game::navigation::MotionMode::HubTactical;
        t.motion.systemId=1;
        t.motion.localControlLaw=
            game::navigation::LocalFlightControlLaw::Newtonian;
        t.motion.travelFrame.systemId=1;
        t.motion.travelFrame.frameId="reactor-budget";
        t.motion.travelFrame.localToWorldBasis=glm::dmat3(1.0);
        t.motion.travelFrame.valid=true;
        return t;
    };

    // A legacy diagnostic caller without any electrical budget retains
    // its independent source. This also proves the physical engine can fire.
    auto diagnostic=makeTransform();
    (void)Gateway::evaluateTranslationControl(
        diagnostic,physics,control,model,slots,0.1f,
        &mounted,nullptr,nullptr);
    require(glm::length(
                diagnostic.motion.mainEngineAccelerationMps2)>1.0e-8,
            "test motor did not deliver thrust without electrical budget");

    // A real installed ship explicitly supplying an empty reactor map must
    // instead have ZERO executed thrust; its control demand remains visible.
    auto installed=makeTransform();
    const std::vector<MainEnginePowerGroup> noReactors {};
    (void)Gateway::evaluateTranslationControl(
        installed,physics,control,model,slots,0.1f,
        &mounted,nullptr,&noReactors);
    require(glm::length(
                installed.motion.requestedMainEngineAccelerationMps2)>1.0e-8,
            "no-power test did not generate a propulsion request");
    require(glm::length(installed.motion.mainEngineAccelerationMps2)<1.0e-10 &&
            glm::length(installed.motion.engineAccelerationMps2)<1.0e-10 &&
            glm::length(installed.motion.mainPropulsionTorqueBodyNm)<1.0e-10,
            "empty installed power budget produced free main thrust");

    // Verify physical PROPAGATION, not merely the allocator's return values.
    // A nonzero request with no installed reactor is not a source of delta-v.
    require(Gateway::propagateHubTacticalTranslation(
                installed, physics, 0.1),
            "no-reactor test could not propagate HubTactical motion");
    require(glm::length(installed.motion.localVelocityMps)<1.0e-8,
            "unexecuted no-reactor request accelerated the physical ship");

    std::size_t aftIndex=topology.size();
    for(std::size_t i=0;i<topology.size();++i)
        if(topology[i].role==EngineSlotRole::AftMain)
            aftIndex=i;
    require(aftIndex<topology.size(),"Newtonian-I aft motor is missing");

    const std::vector<MainEnginePowerGroup> zeroReactor {
        {{aftIndex},0.0}
    };
    auto switchedOff=makeTransform();
    (void)Gateway::evaluateTranslationControl(
        switchedOff,physics,control,model,slots,0.1f,
        &mounted,nullptr,&zeroReactor);
    require(glm::length(
                switchedOff.motion.requestedMainEngineAccelerationMps2)>1.0e-8,
            "zero-MW reactor test did not request propulsion");
    require(glm::length(
                switchedOff.motion.mainEngineAccelerationMps2)<1.0e-8,
            "installed zero-MW reactor delivered main thrust");
    require(switchedOff.motion.engineTelemetry.valid &&
            std::abs(switchedOff.motion.engineTelemetry.electricalMW[aftIndex])<
                1.0e-8,
            "zero-MW reactor retained actual electrical consumption");
    require(Gateway::propagateHubTacticalTranslation(
                switchedOff,physics,0.1),
            "zero-MW reactor test could not propagate");
    require(glm::length(switchedOff.motion.localVelocityMps)<1.0e-8,
            "switched-off reactor moved ship from rest");

    const std::vector<MainEnginePowerGroup> unrestrictedReactor {
        {{aftIndex},1.0e9}
    };
    auto full=makeTransform();
    (void)Gateway::evaluateTranslationControl(
        full,physics,control,model,slots,0.1f,
        &mounted,nullptr,&unrestrictedReactor);
    require(full.motion.engineTelemetry.valid &&
            glm::length(full.motion.mainEngineAccelerationMps2)>1.0e-3,
            "powered installed main delivered no acceleration");
    const glm::dvec3 fullAcceleration =
        full.motion.mainEngineAccelerationMps2;
    const double fullElectricalMW =
        full.motion.engineTelemetry.electricalMW[aftIndex];
    require(fullElectricalMW>1.0e-6,
            "powered main reported zero delivered electrical demand");

    const std::vector<MainEnginePowerGroup> halfReactor {
        {{aftIndex},fullElectricalMW*0.5}
    };
    auto half=makeTransform();
    (void)Gateway::evaluateTranslationControl(
        half,physics,control,model,slots,0.1f,
        &mounted,nullptr,&halfReactor);
    require(half.motion.engineTelemetry.valid,
            "partially powered main lost delivered-motor telemetry");
    require(glm::length(half.motion.mainEngineAccelerationMps2-
                        fullAcceleration*0.5)<1.0e-4,
            "half-power reactor did not halve physically delivered main force");
    require(std::abs(half.motion.engineTelemetry.electricalMW[aftIndex]-
                     fullElectricalMW*0.5)<1.0e-4,
            "half-power reactor meter disagreed with applied throttle");
    for(ShipTransform* observed:{&full,&half})
    {
        const glm::dvec3 reported =
            observed->motion.mainEngineAccelerationMps2;
        const glm::dvec3 before =
            observed->motion.localVelocityMps;
        require(Gateway::propagateHubTacticalTranslation(
                    *observed,physics,0.1),
                "powered thrust impulse test could not propagate");
        require(glm::length(observed->motion.localVelocityMps-before-
                            reported*0.1)<1.0e-5,
                "delivered motor acceleration differs from physical delta-v");
    }

    // Regression detector for the remaining Command/Request/Executed seam:
    // no post-allocator limit may silently erase delivered physical impulse.
    // THIS CONTRACT CURRENTLY EXPOSES THE LEGACY SPEED-ENVELOPE PATH.
    auto nearLimit=makeTransform();
    nearLimit.motion.localVelocityMps=
        glm::dvec3(nearLimit.forward())*399.9;
    (void)Gateway::evaluateTranslationControl(
        nearLimit,physics,control,model,slots,0.1f,
        &mounted,nullptr,&unrestrictedReactor);
    const glm::dvec3 deliveredAtLimit =
        nearLimit.motion.mainEngineAccelerationMps2;
    require(glm::length(deliveredAtLimit)>1.0,
            "speed-boundary impulse test did not execute substantial main thrust");
    const glm::dvec3 initialNearLimit =
        nearLimit.motion.localVelocityMps;
    require(Gateway::propagateHubTacticalTranslation(
                nearLimit,physics,0.1),
            "near-speed-limit main impulse could not propagate");
    const glm::dvec3 appliedDeltaV =
        nearLimit.motion.localVelocityMps-initialNearLimit;
    const glm::dvec3 executedDeltaV = deliveredAtLimit*0.1;
    const double impulseMismatchMps =
        glm::length(appliedDeltaV-executedDeltaV);
    if(impulseMismatchMps>=1.0e-5)
        std::cerr << "[PHYSICAL_IMPULSE_MISMATCH]"
                  << " speed_before_mps=" << glm::length(initialNearLimit)
                  << " speed_after_mps="
                  << glm::length(nearLimit.motion.localVelocityMps)
                  << " executed_dv_mps=" << glm::length(executedDeltaV)
                  << " applied_dv_mps=" << glm::length(appliedDeltaV)
                  << " error_mps=" << impulseMismatchMps << "\n";
    require(impulseMismatchMps<1.0e-5,
            "post-execution speed limiter discarded executed motor impulse; "
            "limit request BEFORE allocating actual engines");
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
        testRejectedAllocatorCannotMoveShipOnRequestedAcceleration();
        testExplicitEmptyPowerBudgetCannotRunInstalledMain();

        std::cout << "SHIP CONTROL EXECUTION GATEWAY TESTS: PASS\n";
        std::cout << " - manual/TVC input retains ordinary physical-path priority\n";
        std::cout << " - precision/Assisted/Newtonian execution selection is centralized\n";
        std::cout << " - callers no longer need to interpret ShipControlState\n";
        std::cout << " - axial Newtonian-I main is centered radially, while aft-mounted TVC creates real yaw/pitch reaction\n";
        std::cout << " - damaged-main feedback physically reduces avoidable bias without synthetic cancellation\n";
        std::cout << " - installed reactor off/half/full and executed impulse are checked through physical propagation\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr << "SHIP CONTROL EXECUTION GATEWAY TESTS: FAIL: "
                  << error.what() << "\n";
        return EXIT_FAILURE;
    }
}
