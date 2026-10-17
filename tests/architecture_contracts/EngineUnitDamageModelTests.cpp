#include "src/game/equipment/engine/EngineUnit.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace
{

using game::equipment::engine::EngineDamageInputs;
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
    d.nominalEfficiency01 = 1.0;
    d.nominalGimbalRangeRad = 0.20;
    d.nominalGimbalRateRadPerSec = 0.40;
    d.minimumOperationalEfficiency01 = 0.05;

    d.efficiencyDamage = {
        0.30, 0.45, 0.15, 0.01, 0.09
    };
    d.gimbalDamage = {
        0.15, 0.65, 0.05, 0.00, 0.15
    };
    return d;
}

void testHealthyEnginePreservesPassport()
{
    const auto d = descriptor();
    const auto s = evaluateEngineUnit(d, {});

    require(s.operational, "healthy engine became inoperative");
    require(near(s.efficiencyAuthority01, 1.0),
            "healthy efficiency authority changed");
    require(near(s.maximumThrustNewtons, 1000.0),
            "healthy nominal thrust changed");
    require(near(s.gimbalAuthority01, 1.0),
            "healthy gimbal authority changed");
    require(near(s.gimbalRangeRad, 0.20),
            "healthy gimbal range changed");
    require(near(s.gimbalRateRadPerSec, 0.40),
            "healthy gimbal rate changed");
    require(near(s.powerCostMultiplier, 1.0),
            "healthy power cost changed");
    require(near(s.heatMultiplier, 0.0),
            "healthy extra waste heat changed");
}

void testImpactIsMostCriticalMechanicalDamage()
{
    const auto d = descriptor();

    EngineDamageInputs damage;
    damage.impact01 = 1.0;
    const auto impact = evaluateEngineUnit(d, damage);

    damage = {};
    damage.laser01 = 1.0;
    const auto laser = evaluateEngineUnit(d, damage);

    require(
        impact.efficiencyAuthority01 < laser.efficiencyAuthority01,
        "impact must hurt engine efficiency more than laser"
    );
    require(
        impact.gimbalAuthority01 < laser.gimbalAuthority01,
        "impact must hurt engine gimbal more than laser"
    );

    require(near(impact.efficiencyAuthority01, 0.55),
            "impact efficiency weight changed");
    require(near(impact.gimbalAuthority01, 0.35),
            "impact gimbal weight changed");
}

void testLaserIsImportantButLessMechanicalThanImpact()
{
    const auto d = descriptor();
    EngineDamageInputs damage;
    damage.laser01 = 1.0;

    const auto s = evaluateEngineUnit(d, damage);

    require(near(s.efficiencyAuthority01, 0.70),
            "laser efficiency weight changed");
    require(near(s.gimbalAuthority01, 0.85),
            "laser gimbal weight changed");
}

void testThermalAndWearMostlyHitEfficiency()
{
    const auto d = descriptor();

    EngineDamageInputs thermal;
    thermal.thermal01 = 1.0;
    const auto t = evaluateEngineUnit(d, thermal);

    require(near(t.efficiencyAuthority01, 0.85),
            "thermal efficiency weight changed");
    require(near(t.gimbalAuthority01, 0.95),
            "thermal gimbal weight changed");

    EngineDamageInputs wear;
    wear.wear01 = 1.0;
    const auto w = evaluateEngineUnit(d, wear);

    require(near(w.efficiencyAuthority01, 0.91),
            "wear efficiency weight changed");
    require(near(w.gimbalAuthority01, 0.85),
            "wear gimbal weight changed");
}

void testRadiationIsNegligibleForEngine()
{
    const auto d = descriptor();
    EngineDamageInputs damage;
    damage.radiation01 = 1.0;

    const auto s = evaluateEngineUnit(d, damage);

    require(near(s.efficiencyAuthority01, 0.99),
            "engine radiation efficiency sensitivity changed");
    require(near(s.gimbalAuthority01, 1.0),
            "engine gimbal should ignore radiation");
}

void testEfficiencyDrivesThrustPowerAndHeatTogether()
{
    const auto d = descriptor();
    EngineDamageInputs damage;
    damage.impact01 = 1.0;

    const auto s = evaluateEngineUnit(d, damage);

    // efficiency authority = 0.55:
    // max thrust at fixed available input power becomes 550 N.
    require(near(s.maximumThrustNewtons, 550.0),
            "efficiency did not set maximum thrust");

    // Same useful thrust costs 1/0.55 input power.
    require(near(s.powerCostMultiplier, 1.0 / 0.55),
            "efficiency did not set power cost");

    const double power = requestedElectricalPowerMW(d, s, 0.50);
    require(near(power, 5.0 / 0.55),
            "requested power did not follow damaged efficiency");

    const double heat = generatedWasteHeatMW(d, s, 0.50);
    require(near(heat, power - 5.0),
            "waste heat did not follow input minus useful power");
}

void testCombinedDamageIsBoundedAndMonotonic()
{
    const auto d = descriptor();

    EngineDamageInputs a;
    a.impact01 = 0.25;

    EngineDamageInputs b = a;
    b.laser01 = 0.25;
    b.thermal01 = 0.25;

    const auto sa = evaluateEngineUnit(d, a);
    const auto sb = evaluateEngineUnit(d, b);

    require(
        sb.efficiencyAuthority01 <= sa.efficiencyAuthority01,
        "additional damage improved efficiency"
    );
    require(
        sb.gimbalAuthority01 <= sa.gimbalAuthority01,
        "additional damage improved gimbal condition"
    );

    EngineDamageInputs all;
    all.laser01 = 1.0;
    all.impact01 = 1.0;
    all.thermal01 = 1.0;
    all.radiation01 = 1.0;
    all.wear01 = 1.0;

    const auto dead = evaluateEngineUnit(d, all);
    require(!dead.operational,
            "fully accumulated engine damage remained operational");
    require(near(dead.maximumThrustNewtons, 0.0),
            "inoperative engine retained thrust");
    require(near(dead.gimbalRangeRad, 0.0),
            "inoperative engine retained gimbal range");
}

} // namespace

int main()
{
    try
    {
        testHealthyEnginePreservesPassport();
        testImpactIsMostCriticalMechanicalDamage();
        testLaserIsImportantButLessMechanicalThanImpact();
        testThermalAndWearMostlyHitEfficiency();
        testRadiationIsNegligibleForEngine();
        testEfficiencyDrivesThrustPowerAndHeatTogether();
        testCombinedDamageIsBoundedAndMonotonic();

        std::cout
            << "ENGINE UNIT DAMAGE MODEL: PASS\n"
            << " - fixed five-channel engine damage weights\n"
            << " - impact is dominant; laser is secondary\n"
            << " - thermal/wear degrade efficiency; radiation is negligible\n"
            << " - efficiency couples thrust, power demand and waste heat\n"
            << " - gimbal condition independently limits range/rate\n";
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
