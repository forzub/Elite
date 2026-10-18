#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "src/game/equipment/engine/Engine.h"
#include "src/game/equipment/engine/presets/CobrettyCR1VectorMainDrive.h"
#include "src/game/ship/plant/ShipPhysicalActuatorPlant.h"
#include "src/game/ship/plant/ShipEngineControlMatrix.h"
#include "src/game/presentation/EngineLiveTelemetryPresentation.h"

namespace
{
void require(bool ok, const char* msg)
{
    if (!ok)
        throw std::runtime_error(msg);
}
bool near(double a, double b, double eps = 1.0e-5)
{
    return std::abs(a - b) < eps;
}
void commonEngineAndCobrettyPreset()
{
    using namespace game::equipment::engine;
    const auto passport = presets::CobrettyCR1VectorMainDrive();
    require(passport.engineTypeId == "elite.engine.cobretty.cr1-vector.v1" &&
            passport.engineDisplayName ==
                "Cobretty CR-1 Vector Main Drive" &&
            passport.manufacturerName == "Cobretty Corporation",
            "Cobretty CR-1 type identity missing");
    require(passport.nominalThrustNewtons > 9.56e6 &&
            passport.nominalThrustNewtons < 9.57e6,
            "Cobretty CR-1 rated thrust not self-contained");
    require(passport.structureMaterialId ==
                "elite.material.engine.tial-ht.v1" &&
            passport.thermalEmitter.materialId ==
                "elite.material.engine.hfc-c.v1",
            "Cobretty type does not reference universal material library");

    game::equipment::EquipmentDamageLayer damage;
    damage.failure.failureAtDamage01 = 0.95;
    EngineOperatingState state;
    state.temperatureK = 1200.0;
    const Engine e({&passport, &damage, &state, "test_CR1", true});
    auto output = e.execute({1.0, {0.0, 0.0, -1.0}, 40u});
    require(near(output.deliveredThrustNewtons,
                 passport.nominalThrustNewtons, 0.5),
            "generic engine failed to deliver own rated thrust");
    require(near(output.consumedElectricalMW, 4850.0) &&
            near(output.generatedWasteHeatMW, 70.325, 0.001),
            "generic engine did not calculate own electric/thermal budget");
    require(near(output.forceBodyNewtons.z,
                 -passport.nominalThrustNewtons, 0.5),
            "physical engine does not supply body force in newtons");
    output = e.execute({0.5, {0.0, 0.0, -1.0}, 40u});
    require(near(output.deliveredNominalThrust01, 0.5) &&
            near(output.consumedElectricalMW, 2425.0),
            "engine half-throttle output is not physically proportional");

    damage.accumulated.impact01 = 0.50;
    output = e.execute({1.0, {0.0, 0.0, -1.0}, 40u});
    require(near(output.deliveredNominalThrust01, 0.50),
            "engine ignored instance-specific damage state");
    damage.accumulated.impact01 = 0.0;
    state.emitter.surface.intactArea01 = 0.5;
    require(near(e.execute({1.0, {0.0, 0.0, -1.0}, 40u}).
                     deliveredNominalThrust01, 1.0),
            "cool damaged annulus incorrectly cut off thrust");
    state.temperatureK = 1300.0;
    require(e.execute({1.0, {0.0, 0.0, -1.0}, 40u}).
                deliveredNominalThrust01 < 0.86,
            "hot damaged annulus did not trigger main-engine governor");
}

void mountedAllocatorUsesEngineForceNotSlotFractions()
{
    using namespace game::equipment::engine;
    using namespace game::ship::plant;
    auto passport = presets::CobrettyCR1VectorMainDrive();
    const auto topology = physicalControlSlots(
        PhysicalControlModelId::AssistedI
    );
    require(topology.size() == 4, "Assisted-I topology changed");
    game::equipment::EquipmentDamageLayer damages[4];
    EngineOperatingState states[4];
    std::vector<MountedEngine> mounted(4);
    const std::string_view names[] = {
        "aft_test_left", "aft_test_right", "fore_test_left", "fore_test_right"
    };
    for (std::size_t i = 0; i < 4; ++i)
    {
        damages[i].failure.failureAtDamage01 = 0.95;
        states[i].temperatureK = 1200.0;
        mounted[i] = {&passport, &damages[i], &states[i], names[i], true};
    }

    ShipParams physics {};
    physics.massKg = 260000.0;
    physics.forwardMainEngineAvailable = true;
    physics.reverseMainEngineAvailable = true;
    physics.forwardMainEngineAccelerationMps2 = 73.549875f;
    physics.reverseMainEngineAccelerationMps2 = 73.549875f;
    physics.physicalActuatorHalfExtentsMeters = {12.0, 3.0, 11.0};
    physics.centerOfMassLocalMeters = {0.0, 0.0, 0.0};
    physics.pitchInertiaKgM2 = 11219866.66667;
    physics.yawInertiaKgM2 = 25324866.66667;
    physics.rollInertiaKgM2 = 15188333.33333;

    const glm::dvec3 desired {0.0, 0.0, -73.549875};
    std::vector<EngineSlotState> slots(4);
    auto run = [&](glm::dvec3 demand)
    {
        return evaluateMainPropulsionFeedbackTrim(
            PhysicalControlModelId::AssistedI,
            slots, physics, demand, {0.0, 0.0, 0.0},
            0.0, 0.0, 40u, &mounted
        );
    };
    const auto healthy = run(desired);
    require(healthy.valid && healthy.reaction.valid &&
            near(healthy.accelerationBodyMps2.z, -73.549875, 0.01),
            "healthy engine mount did not give Cobra full acceleration");
    require(near(healthy.deliveredThrust01[0], 1.0) &&
            near(healthy.deliveredThrust01[1], 1.0),
            "actual delivered thrust did not originate at each Engine");
    require(glm::length(healthy.reaction.torqueBodyNm) < 1.0,
            "symmetric engine bank generated false hull torque");

    // Production force is NOT synthesized from fake health stored on a slot.
    // The physical engine must still work if a diagnostic slot disagrees.
    slots[0].thrust01 = 0.0;
    const auto stillHealthy = run(desired);
    require(near(stillHealthy.accelerationBodyMps2.z,
                 healthy.accelerationBodyMps2.z, 0.01),
            "slot fraction illegally overrides physical installed engine");

    // Damage ONE motor. The balancing controller trims the opposite motor
    // to avoid a yaw moment, but it must not conjure absent thrust.
    damages[0].accumulated.impact01 = 0.5;
    const auto damaged = run({0.0, 0.0, -55.16240625});
    require(damaged.valid &&
            damaged.accelerationBodyMps2.z < -35.0 &&
            damaged.accelerationBodyMps2.z > -38.0,
            "damage did not reduce actual sum of engine forces");
    require(near(damaged.deliveredThrust01[0], 0.5, 0.001) &&
            near(damaged.deliveredThrust01[1], 0.5, 0.001),
            "damaged-engine torque balancing invented unequal forces");

    // Precisely request the reduced balanced authority rather than the raw
    // maximum. The engine plant must supply that full demand; it must not
    // multiply the weak-bank penalty a second time.
    const auto atLimit = run({0.0, 0.0, -36.7749375});
    const auto atHalf = run({0.0, 0.0, -18.38746875});
    require(near(atLimit.accelerationBodyMps2.z, -36.7749375, 0.01) &&
            near(atHalf.accelerationBodyMps2.z, -18.38746875, 0.01),
            "motor force envelope was derated twice after bank balancing");

    // A second engine TYPE can have a different rated thrust WITHOUT editing
    // the ship's physics or its slot, and that actual difference is respected.
    auto stronger = passport;
    stronger.nominalThrustNewtons *= 1.5;
    mounted[1].passport = &stronger;
    damages[0].accumulated.impact01 = 0.0;
    slots[0].thrust01 = 1.0;
    // Ask for the MIXED pair's full 2.5-engine rated force; the public
    // control law will normally cap such demand to the type-derived envelope.
    const auto mixed = run({0.0, 0.0, -91.93734375});
    require(mixed.valid &&
            near(mixed.deliveredThrust01[1], 2.0/3.0, 0.001) &&
            near(mixed.deliveredThrust01[0], 1.0, 0.001),
            "different engine presets did not balance actual Newton force");
}
void sixAxisMatrixControlsActualMotorVectors()
{
    using namespace game::equipment::engine;
    using namespace game::ship::plant;
    auto passport=presets::CobrettyCR1VectorMainDrive();
    game::equipment::EquipmentDamageLayer damage[4];
    EngineOperatingState operating[4];
    std::vector<MountedEngine> mounted(4);
    const std::string_view ids[4]={
        "aft_left","aft_right","fore_left","fore_right"
    };
    for(std::size_t i=0;i<4;++i)
    {
        damage[i].failure.failureAtDamage01=0.95;
        operating[i].temperatureK=1200.0;
        mounted[i]={&passport,&damage[i],&operating[i],ids[i],true};
    }
    ShipParams physics {};
    physics.massKg=260000.0;
    physics.physicalActuatorHalfExtentsMeters={15.0,3.0,12.5};
    physics.centerOfMassLocalMeters={0.0,0.0,0.0};
    physics.pitchInertiaKgM2=12000000.0;
    physics.yawInertiaKgM2=25000000.0;
    physics.rollInertiaKgM2=16000000.0;

    auto run=[&](glm::dvec3 translation,glm::dvec3 rotation)
    {
        EngineMatrixDemand demand;
        demand.accelerationBodyMps2=translation;
        demand.angularAccelerationBodyRadPerSec2=rotation;
        demand.elapsedSeconds=0.1;
        demand.tick=40u;
        return executeEngineControlMatrix(
            PhysicalControlModelId::AssistedI,mounted,physics,demand
        );
    };
    const auto idle=run({}, {});
    require(idle.valid && idle.count==4 &&
            glm::length(idle.forceBodyNewtons)<1.0e-4,
            "neutral controls generated spurious engine thrust");

    const auto forward=run({0.0,0.0,-10.0},{});
    require(forward.accelerationBodyMps2.z < -0.1 &&
            forward.engines[0].deliveredThrust01 > 0.0 &&
            forward.engines[1].deliveredThrust01 > 0.0 &&
            forward.engines[2].deliveredThrust01 < 1.0e-6 &&
            forward.engines[3].deliveredThrust01 < 1.0e-6,
            "axial command used the opposite main engine bank");
    const auto sideways=run({3.0,0.0,0.0},{});
    require(sideways.accelerationBodyMps2.x > 0.05 &&
            glm::length(sideways.forceBodyNewtons)>0.0,
            "sideways motion was not made from real directed engine force");
    bool tilted=false;
    for(const auto& e:sideways.engines)
    {
        if(e.deliveredThrust01>1.0e-5 &&
           (std::abs(e.nozzleYawRad)+std::abs(e.nozzlePitchRad))>1.0e-6)
            tilted=true;
        require(std::hypot(e.nozzleYawRad,e.nozzlePitchRad)
                    <= passport.nominalGimbalRateRadPerSec*0.1+1.0e-5,
                "single-tick nozzle slew exceeded the engine preset rate");
        require(std::hypot(e.nozzleYawRad,e.nozzlePitchRad)
                    <= passport.nominalGimbalRangeRad+1.0e-5,
                "engine nozzle exceeded mechanical gimbal cone");
    }
    require(tilted,"lateral acceleration had no physical nozzle deflection");
    const auto pitch=run({}, {0.25,0.0,0.0});
    const auto yaw=run({}, {0.0,0.25,0.0});
    const auto roll=run({}, {0.0,0.0,0.25});
    require(pitch.angularAccelerationBodyRadPerSec2.x>1.0e-4,
            "pitch demanded torque was not generated by the engine matrix");
    require(yaw.angularAccelerationBodyRadPerSec2.y>1.0e-4,
            "yaw demanded torque was not generated by the engine matrix");
    require(roll.angularAccelerationBodyRadPerSec2.z>1.0e-4,
            "roll demanded torque was not generated by the engine matrix");

    mounted[0].attached=false;
    const auto damaged=run({0.0,0.0,-10.0},{});
    require(damaged.engines[0].deliveredThrust01==0.0 &&
            damaged.engines[0].availableThrust01==0.0,
            "detached engine illegally produced physical force");

    game::navigation::DynamicMotionState motion;
    motion.engineTelemetry.valid=true;
    motion.engineTelemetry.count=4;
    motion.engineTelemetry.thrust01[0]=0.375;
    motion.engineTelemetry.available01[0]=0.75;
    motion.engineTelemetry.nozzleYawRad[0]=0.02;
    const auto hud=game::presentation::readEngineMotorLiveView(
        motion,0,0.9
    );
    require(hud.sampled && near(hud.thrust01,0.375) &&
            near(hud.available01,0.75) &&
            near(hud.nozzleYawRad,0.02),
            "HUD did not read physically executed engine telemetry directly");
}

} // namespace

int main()
{
    try
    {
        commonEngineAndCobrettyPreset();
        mountedAllocatorUsesEngineForceNotSlotFractions();
        sixAxisMatrixControlsActualMotorVectors();
        std::cout
            << "INSTALLED ENGINE ACTUATOR: PASS\n"
            << " - named Cobretty CR-1 preset -> common Engine instance\n"
            << " - engine supplies Newton force, electric MW and waste heat\n"
            << " - slot does not own force; varied engine ratings and failures\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& e)
    {
        std::cerr << "INSTALLED ENGINE ACTUATOR: FAIL: "
                  << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
