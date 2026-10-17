#include "src/game/ship/plant/ShipPhysicalActuatorPlant.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace
{

using namespace game::ship::plant;

bool near(double a, double b, double eps = 1.0e-9)
{
    return std::abs(a - b) <= eps;
}

ShipParams physics()
{
    ShipParams p {};
    p.massKg = 260000.0;
    p.centerOfMassLocalMeters = {0.0, 0.0, 0.0};
    p.pitchInertiaKgM2 = 11219866.6666667;
    p.yawInertiaKgM2 = 25324866.6666667;
    p.rollInertiaKgM2 = 15188333.3333333;
    p.turnRadius = 20.0f;
    p.physicalActuatorHalfExtentsMeters = {13.0, 2.5, 11.1};
    return p;
}

void testCenteredNewtonianIAxialBurnHasNoTorque()
{
    const auto model = PhysicalControlModelId::NewtonianI;
    const auto state = healthySlotState(physicalControlSlots(model));

    const auto out = evaluateMainPropulsionReaction(
        model,
        state,
        physics(),
        {0.0, 0.0, -60.0}
    );

    assert(out.valid);
    assert(near(glm::length(out.torqueBodyNm), 0.0, 1.0e-6));
    assert(near(
        glm::length(out.angularAccelerationBodyRadPerSec2),
        0.0,
        1.0e-12
    ));
}

void testNewtonianIGimbalCreatesYawTorque()
{
    const auto model = PhysicalControlModelId::NewtonianI;
    const auto state = healthySlotState(physicalControlSlots(model));

    const auto out = evaluateMainPropulsionReaction(
        model,
        state,
        physics(),
        {7.0, 0.0, -59.0}
    );

    assert(out.valid);
    assert(std::abs(out.torqueBodyNm.y) > 1.0);
    assert(out.angularAccelerationBodyRadPerSec2.y > 0.0);
    assert(near(out.angularAccelerationBodyRadPerSec2.x, 0.0, 1.0e-12));
    assert(near(out.angularAccelerationBodyRadPerSec2.z, 0.0, 1.0e-12));
}

void testHealthyNewtonianYAxialBankIsBalanced()
{
    const auto model = PhysicalControlModelId::NewtonianY;
    const auto state = healthySlotState(physicalControlSlots(model));

    const auto out = evaluateMainPropulsionReaction(
        model,
        state,
        physics(),
        {0.0, 0.0, -60.0}
    );

    assert(out.valid);
    assert(near(glm::length(out.torqueBodyNm), 0.0, 1.0e-6));
}

void testNewtonianYSingleMainLossCreatesPitchTorque()
{
    const auto model = PhysicalControlModelId::NewtonianY;
    auto state = healthySlotState(physicalControlSlots(model));

    // main_top
    state[0].operational = false;
    state[0].thrust01 = 0.0;
    state[0].vectoring01 = 0.0;

    const auto out = evaluateMainPropulsionReaction(
        model,
        state,
        physics(),
        {0.0, 0.0, -40.0}
    );

    assert(out.valid);
    assert(std::abs(out.torqueBodyNm.x) > 1.0);
    assert(std::abs(out.angularAccelerationBodyRadPerSec2.x) > 1.0e-3);
    assert(std::abs(out.angularAccelerationBodyRadPerSec2.x) < 0.25);
    assert(near(out.angularAccelerationBodyRadPerSec2.y, 0.0, 1.0e-12));
}

void testNewtonianXSingleCornerMainLossCreatesTwoAxisTorque()
{
    const auto model = PhysicalControlModelId::NewtonianX;
    auto state = healthySlotState(physicalControlSlots(model));

    // main_upper_left
    state[0].operational = false;
    state[0].thrust01 = 0.0;
    state[0].vectoring01 = 0.0;

    const auto out = evaluateMainPropulsionReaction(
        model,
        state,
        physics(),
        {0.0, 0.0, -45.0}
    );

    assert(out.valid);
    assert(std::abs(out.torqueBodyNm.x) > 1.0);
    assert(std::abs(out.torqueBodyNm.y) > 1.0);
    assert(std::abs(out.angularAccelerationBodyRadPerSec2.x) > 1.0e-3);
    assert(std::abs(out.angularAccelerationBodyRadPerSec2.y) > 1.0e-3);
    assert(std::abs(out.angularAccelerationBodyRadPerSec2.x) < 0.25);
    assert(std::abs(out.angularAccelerationBodyRadPerSec2.y) < 0.25);
}


void testIrregularThrustIsDeterministicAndPhysicallyBounded()
{
    const auto model = PhysicalControlModelId::AssistedI;
    auto state = healthySlotState(physicalControlSlots(model));

    // Only aft_left is unstable. The other aft engine stays steady, so the
    // whole active bank must pulse between 50% and 100% of its static output.
    state[0].thrustIrregularity01 = 1.0;

    double minScale = 1.0;
    double maxScale = 0.0;

    for (std::uint64_t tick = 0; tick < 256; tick += 4)
    {
        const double scale =
            evaluateMainPropulsionInstantaneousScale(
                model,
                state,
                {0.0, 0.0, -60.0},
                tick
            );

        assert(scale >= 0.5 - 1.0e-12);
        assert(scale <= 1.0 + 1.0e-12);

        const double repeated =
            evaluateMainPropulsionInstantaneousScale(
                model,
                state,
                {0.0, 0.0, -60.0},
                tick
            );
        assert(near(scale, repeated));

        minScale = std::min(minScale, scale);
        maxScale = std::max(maxScale, scale);
    }

    assert(maxScale - minScale > 0.20);

    const auto stable = healthySlotState(physicalControlSlots(model));
    assert(near(
        evaluateMainPropulsionInstantaneousScale(
            model,
            stable,
            {0.0, 0.0, -60.0},
            12345
        ),
        1.0
    ));
}

} // namespace

int main()
{
    testCenteredNewtonianIAxialBurnHasNoTorque();
    testNewtonianIGimbalCreatesYawTorque();
    testHealthyNewtonianYAxialBankIsBalanced();
    testNewtonianYSingleMainLossCreatesPitchTorque();
    testNewtonianXSingleCornerMainLossCreatesTwoAxisTorque();
    testIrregularThrustIsDeterministicAndPhysicallyBounded();

    std::cout
        << "SHIP PHYSICAL ACTUATOR PLANT: PASS\n"
        << " - balanced main banks produce zero parasitic torque\n"
        << " - Newtonian-I TVC produces rigid-body reaction torque\n"
        << " - Y/X single-main failures produce layout-dependent asymmetric torque\n"
        << " - severe damage produces deterministic bounded per-engine thrust hiccups\n"
        << " - Newtonian clustered mains keep failure angular acceleration bounded by their short radial lever arm\n";
    return 0;
}
