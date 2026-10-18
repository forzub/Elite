#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "src/game/equipment/engine/Engine.h"
#include "src/game/equipment/engine/presets/CobrettyCR1VectorMainDrive.h"
#include "src/game/ship/plant/ShipPhysicalActuatorPlant.h"

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

    // A second engine TYPE can have a different rated thrust WITHOUT editing
    // the ship's physics or its slot, and that actual difference is respected.
    auto stronger = passport;
    stronger.nominalThrustNewtons *= 1.5;
    mounted[1].passport = &stronger;
    damages[0].accumulated.impact01 = 0.0;
    slots[0].thrust01 = 1.0;
    const auto mixed = run(desired);
    require(mixed.valid &&
            near(mixed.deliveredThrust01[1], 2.0/3.0, 0.001) &&
            near(mixed.deliveredThrust01[0], 1.0, 0.001),
            "different engine presets did not balance actual Newton force");
}
} // namespace

int main()
{
    try
    {
        commonEngineAndCobrettyPreset();
        mountedAllocatorUsesEngineForceNotSlotFractions();
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
