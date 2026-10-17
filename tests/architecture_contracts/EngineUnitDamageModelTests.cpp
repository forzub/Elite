#include "src/game/equipment/engine/EngineUnit.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace
{

using game::equipment::engine::EngineUnitDescriptor;
using game::equipment::engine::evaluateEngineUnit;
using game::equipment::engine::generatedWasteHeatMW;
using game::equipment::engine::requestedElectricalPowerMW;

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

bool near(double a, double b, double eps = 1.0e-9)
{
    return std::abs(a - b) <= eps;
}

EngineUnitDescriptor descriptor()
{
    EngineUnitDescriptor d;
    d.nominalThrustNewtons = 1000.0;
    d.nominalElectricalPowerMW = 10.0;
    d.nominalWasteHeatMW = 4.0;
    d.nominalGimbalRangeRad = 0.20;
    d.nominalGimbalRateRadPerSec = 0.40;
    d.thrustHealthExponent = 1.0;
    d.vectoringHealthExponent = 2.0;
    d.maxPowerCostPenalty = 0.50;
    d.maxHeatPenalty = 1.00;
    return d;
}

void testHealthyEnginePreservesPassport()
{
    const auto d = descriptor();
    const auto s = evaluateEngineUnit(d, 1.0);

    require(s.operational, "healthy engine became inoperative");
    require(near(s.thrustAvailable01, 1.0),
            "healthy thrust authority changed");
    require(near(s.maximumThrustNewtons, 1000.0),
            "healthy nominal thrust changed");
    require(near(s.vectoringAvailable01, 1.0),
            "healthy vectoring authority changed");
    require(near(s.gimbalRangeRad, 0.20),
            "healthy gimbal range changed");
    require(near(s.gimbalRateRadPerSec, 0.40),
            "healthy gimbal rate changed");
    require(near(s.powerCostMultiplier, 1.0),
            "healthy power cost changed");
    require(near(s.heatMultiplier, 1.0),
            "healthy heat cost changed");
}

void testDamageDegradesFunctionsAndRaisesOperatingCost()
{
    const auto d = descriptor();
    const auto s = evaluateEngineUnit(d, 0.5);

    require(s.operational, "partially damaged engine became dead");
    require(near(s.thrustAvailable01, 0.5),
            "thrust damage curve is wrong");
    require(near(s.maximumThrustNewtons, 500.0),
            "damaged maximum thrust is wrong");

    // vectoring exponent=2 => 0.5^2 = 0.25
    require(near(s.vectoringAvailable01, 0.25),
            "vectoring damage curve is wrong");
    require(near(s.gimbalRangeRad, 0.05),
            "damaged gimbal range is wrong");
    require(near(s.gimbalRateRadPerSec, 0.10),
            "damaged gimbal rate is wrong");

    require(near(s.powerCostMultiplier, 1.25),
            "damaged power penalty is wrong");
    require(near(s.heatMultiplier, 1.50),
            "damaged heat penalty is wrong");

    require(near(requestedElectricalPowerMW(d, s, 0.5), 6.25),
            "electrical load did not use actual generated thrust and damage cost");
    require(near(generatedWasteHeatMW(d, s, 0.5), 3.0),
            "waste heat did not use actual generated thrust and damage cost");
}

void testDestroyedEngineProducesNothing()
{
    const auto d = descriptor();
    const auto s = evaluateEngineUnit(d, 0.0);

    require(!s.operational, "destroyed engine remained operational");
    require(near(s.thrustAvailable01, 0.0),
            "destroyed engine retained thrust");
    require(near(s.vectoringAvailable01, 0.0),
            "destroyed engine retained vectoring");
    require(near(requestedElectricalPowerMW(d, s, 1.0), 0.0),
            "destroyed engine requested generated-thrust power");
    require(near(generatedWasteHeatMW(d, s, 1.0), 0.0),
            "destroyed engine generated thrust heat");
}

void testDamageNeverImprovesUsefulAuthority()
{
    const auto d = descriptor();
    double previousThrust = 1.0e30;
    double previousVectoring = 1.0e30;

    for (int i = 10; i >= 0; --i)
    {
        const double health = static_cast<double>(i) / 10.0;
        const auto s = evaluateEngineUnit(d, health);

        require(
            s.thrustAvailable01 <= previousThrust + 1.0e-12,
            "lower health improved thrust authority"
        );
        require(
            s.vectoringAvailable01 <= previousVectoring + 1.0e-12,
            "lower health improved vectoring authority"
        );

        previousThrust = s.thrustAvailable01;
        previousVectoring = s.vectoringAvailable01;
    }
}

} // namespace

int main()
{
    try
    {
        testHealthyEnginePreservesPassport();
        testDamageDegradesFunctionsAndRaisesOperatingCost();
        testDestroyedEngineProducesNothing();
        testDamageNeverImprovesUsefulAuthority();

        std::cout
            << "ENGINE UNIT DAMAGE MODEL: PASS\n"
            << " - one runtime health scalar drives engine functions\n"
            << " - thrust/vectoring degradation are descriptor-authored\n"
            << " - damage raises per-thrust power/heat cost\n"
            << " - destroyed engine produces no thrust/power/heat\n"
            << " - damage never improves useful authority\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "ENGINE UNIT DAMAGE MODEL: FAIL: "
            << e.what()
            << "\n";
        return EXIT_FAILURE;
    }
}
