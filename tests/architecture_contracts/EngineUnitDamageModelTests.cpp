#include "src/game/equipment/engine/EngineUnit.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace
{

using game::equipment::EquipmentDamageLayer;
using game::equipment::EquipmentDamageState;
using game::equipment::EquipmentDamageSensitivity;
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
    d.gimbalDamage = {
        0.15, 0.65, 0.05, 0.00, 0.15
    };
    d.gimbalFailure.failureAtDamage01 = 0.95;
    return d;
}

EquipmentDamageLayer damageLayer(
    const EquipmentDamageState& accumulated = {}
)
{
    EquipmentDamageLayer layer;
    layer.accumulated = accumulated;
    layer.sensitivity = {
        0.30, 0.45, 0.15, 0.01, 0.09
    };
    layer.failure.failureAtDamage01 = 0.95;
    return layer;
}

void testHealthyEnginePreservesPassport()
{
    const auto d = descriptor();
    const auto s = evaluateEngineUnit(d, damageLayer());

    require(!s.thrustFailed, "healthy thrust function failed");
    require(!s.gimbalFailed, "healthy TVC function failed");
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

    EquipmentDamageState damage;
    damage.impact01 = 1.0;
    const auto impact = evaluateEngineUnit(d, damageLayer(damage));

    damage = {};
    damage.laser01 = 1.0;
    const auto laser = evaluateEngineUnit(d, damageLayer(damage));

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
    EquipmentDamageState damage;
    damage.laser01 = 1.0;

    const auto s = evaluateEngineUnit(d, damageLayer(damage));

    require(near(s.efficiencyAuthority01, 0.70),
            "laser efficiency weight changed");
    require(near(s.gimbalAuthority01, 0.85),
            "laser gimbal weight changed");
}

void testThermalAndWearMostlyHitEfficiency()
{
    const auto d = descriptor();

    EquipmentDamageState thermal;
    thermal.thermal01 = 1.0;
    const auto t = evaluateEngineUnit(d, damageLayer(thermal));

    require(near(t.efficiencyAuthority01, 0.85),
            "thermal efficiency weight changed");
    require(near(t.gimbalAuthority01, 0.95),
            "thermal gimbal weight changed");

    EquipmentDamageState wear;
    wear.wear01 = 1.0;
    const auto w = evaluateEngineUnit(d, damageLayer(wear));

    require(near(w.efficiencyAuthority01, 0.91),
            "wear efficiency weight changed");
    require(near(w.gimbalAuthority01, 0.85),
            "wear gimbal weight changed");
}

void testRadiationIsNegligibleForEngine()
{
    const auto d = descriptor();
    EquipmentDamageState damage;
    damage.radiation01 = 1.0;

    const auto s = evaluateEngineUnit(d, damageLayer(damage));

    require(near(s.efficiencyAuthority01, 0.99),
            "engine radiation efficiency sensitivity changed");
    require(near(s.gimbalAuthority01, 1.0),
            "engine gimbal should ignore radiation");
}

void testEfficiencyDrivesThrustPowerAndHeatTogether()
{
    const auto d = descriptor();
    EquipmentDamageState damage;
    damage.impact01 = 1.0;

    const auto s = evaluateEngineUnit(d, damageLayer(damage));

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

    EquipmentDamageState a;
    a.impact01 = 0.25;

    EquipmentDamageState b = a;
    b.laser01 = 0.25;
    b.thermal01 = 0.25;

    const auto sa = evaluateEngineUnit(d, damageLayer(a));
    const auto sb = evaluateEngineUnit(d, damageLayer(b));

    require(
        sb.efficiencyAuthority01 <= sa.efficiencyAuthority01,
        "additional damage improved efficiency"
    );
    require(
        sb.gimbalAuthority01 <= sa.gimbalAuthority01,
        "additional damage improved gimbal condition"
    );

    EquipmentDamageState all;
    all.laser01 = 1.0;
    all.impact01 = 1.0;
    all.thermal01 = 1.0;
    all.radiation01 = 1.0;
    all.wear01 = 1.0;

    const auto dead = evaluateEngineUnit(d, damageLayer(all));
    require(dead.thrustFailed,
            "fully accumulated engine damage did not fail thrust");
    require(dead.gimbalFailed,
            "fully accumulated engine damage did not fail TVC");
    require(near(dead.maximumThrustNewtons, 0.0),
            "failed thrust function retained thrust");
    require(near(dead.gimbalRangeRad, 0.0),
            "failed TVC function retained gimbal range");
}

void testThrustFailureThresholdZerosOnlyThrustFunction()
{
    const auto d = descriptor();

    EquipmentDamageState below;
    below.laser01 = 0.949;
    below.impact01 = 0.949;
    below.thermal01 = 0.949;
    below.radiation01 = 0.949;
    below.wear01 = 0.949;

    const auto surviving = evaluateEngineUnit(d, damageLayer(below));
    require(!surviving.thrustFailed,
            "thrust failed below the authored 95% threshold");
    require(surviving.maximumThrustNewtons > 0.0,
            "surviving thrust function produced zero thrust");
    require(requestedElectricalPowerMW(d, surviving, 1.0) > 0.0,
            "surviving thrust function consumed zero power");

    // Reach 95% primary weighted damage while keeping TVC below 95%:
    // primary = .30 + .45 + .15 + .01 + .09*(4/9) = .95
    // TVC    = .15 + .65 + .05 + .00 + .15*(4/9) < .95
    EquipmentDamageState thrustFailure;
    thrustFailure.laser01 = 1.0;
    thrustFailure.impact01 = 1.0;
    thrustFailure.thermal01 = 1.0;
    thrustFailure.radiation01 = 1.0;
    thrustFailure.wear01 = 4.0 / 9.0;

    const auto failed = evaluateEngineUnit(d, damageLayer(thrustFailure));
    require(failed.thrustFailed,
            "95% primary damage did not fail thrust function");
    require(!failed.gimbalFailed,
            "thrust-only failure incorrectly failed TVC");
    require(near(failed.efficiencyAuthority01, 0.0),
            "failed thrust function retained functional authority");
    require(near(failed.maximumThrustNewtons, 0.0),
            "failed thrust function retained generated thrust");
    require(near(failed.powerCostMultiplier, 0.0),
            "failed thrust function retained power multiplier");
    require(near(failed.heatMultiplier, 0.0),
            "failed thrust function retained heat multiplier");
    require(near(requestedElectricalPowerMW(d, failed, 1.0), 0.0),
            "failed thrust function still requested electrical power");
    require(near(generatedWasteHeatMW(d, failed, 1.0), 0.0),
            "failed thrust function still generated waste heat");
    require(failed.gimbalAuthority01 > 0.0,
            "independent TVC function lost surviving authority");
}

void testTvcFailureLeavesThrustAvailable()
{
    const auto d = descriptor();

    // TVC damage = .15 + .65 + .15 = .95.
    // Primary thrust damage = .30 + .45 + .09 = .84.
    EquipmentDamageState damage;
    damage.laser01 = 1.0;
    damage.impact01 = 1.0;
    damage.wear01 = 1.0;

    const auto s = evaluateEngineUnit(d, damageLayer(damage));

    require(!s.thrustFailed,
            "TVC-only failure incorrectly failed thrust");
    require(s.gimbalFailed,
            "95% TVC damage did not fail nozzle control");
    require(near(s.gimbalAuthority01, 0.0),
            "failed TVC retained vectoring authority");
    require(near(s.gimbalRangeRad, 0.0),
            "failed TVC retained gimbal range");
    require(near(s.gimbalRateRadPerSec, 0.0),
            "failed TVC retained gimbal slew");
    require(near(s.efficiencyAuthority01, 0.16),
            "TVC failure changed independent thrust authority");
    require(near(s.maximumThrustNewtons, 160.0),
            "TVC failure removed surviving axial thrust");
    require(requestedElectricalPowerMW(d, s, 1.0) > 0.0,
            "TVC failure incorrectly removed engine power demand");
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
        testThrustFailureThresholdZerosOnlyThrustFunction();
        testTvcFailureLeavesThrustAvailable();

        std::cout
            << "ENGINE UNIT DAMAGE MODEL: PASS\n"
            << " - fixed five-channel engine damage weights\n"
            << " - impact is dominant; laser is secondary\n"
            << " - thermal/wear degrade efficiency; radiation is negligible\n"
            << " - efficiency couples thrust, power demand and waste heat\n"
            << " - 95% primary damage zeros thrust, power and heat without deleting equipment\n"
            << " - 95% TVC damage zeros vectoring while surviving thrust remains available\n";
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
