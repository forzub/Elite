#include "src/game/equipment/engine/EngineUnit.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace
{

using game::equipment::EquipmentDamageLayer;
using game::equipment::EquipmentDamageState;
using game::equipment::engine::EngineUnitDescriptor;
using game::equipment::engine::PropulsionPrinciple;
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
    d.propulsionPrinciple = PropulsionPrinciple::FieldCoupling;
    d.nominalThrustNewtons = 1000.0;
    d.designReferenceSpeedMps = 500.0;
    d.nominalEfficiency01 = 0.80;
    d.nominalElectricalPowerMW =
        d.nominalThrustNewtons * d.designReferenceSpeedMps /
        (d.nominalEfficiency01 * 1.0e6);
    d.nominalGimbalRangeRad = 0.20;
    d.nominalGimbalRateRadPerSec = 0.40;
    d.gimbalFailure.failureAtDamage01 = 0.90;
    return d;
}

EquipmentDamageLayer damageLayer(
    const EquipmentDamageState& accumulated = {}
)
{
    EquipmentDamageLayer layer;
    layer.accumulated = accumulated;
    layer.failure.failureAtDamage01 = 0.95;
    // EngineUnit does not consult generic device sensitivity weights.
    layer.sensitivity = {0.01, 0.01, 0.01, 0.01, 0.01};
    return layer;
}

void testHealthyFieldDrivePassport()
{
    const auto d = descriptor();
    const auto s = evaluateEngineUnit(d, damageLayer());

    require(d.propulsionPrinciple == PropulsionPrinciple::FieldCoupling,
            "field drive model is not explicit");
    require(near(d.nominalElectricalPowerMW, 0.625),
            "design-point F*v/eta electric power changed");
    require(!s.thrustFailed && !s.gimbalFailed,
            "healthy engine failed");
    require(near(s.efficiency01, 0.80),
            "healthy electric-to-field efficiency changed");
    require(near(s.maximumThrustNewtons, 1000.0),
            "healthy thrust rating changed");
    require(near(s.gimbalRangeRad, 0.20) &&
            near(s.gimbalRateRadPerSec, 0.40),
            "healthy TVC rating changed");
    require(near(requestedElectricalPowerMW(d, s, 1.0), 0.625),
            "healthy full-thrust electrical draw changed");
    require(near(generatedWasteHeatMW(d, s, 1.0), 0.125),
            "healthy conversion losses changed");
}

void testFivePercentagesAddWithoutSensitivityWeights()
{
    EquipmentDamageState damage;
    damage.laser01 = 0.20;
    damage.impact01 = 0.15;
    damage.thermal01 = 0.08;
    damage.radiation01 = 0.02;
    damage.wear01 = 0.05;

    const auto s = evaluateEngineUnit(descriptor(), damageLayer(damage));
    require(near(s.efficiencyAuthority01, 0.50),
            "five direct percentages did not sum to 50% loss");
    require(near(s.gimbalAuthority01, 0.50),
            "TVC reintroduced cause-specific weights");
    require(near(s.maximumThrustNewtons, 500.0),
            "five percentages did not reduce useful thrust");

    damage = {};
    damage.laser01 = 0.30;
    const auto laser = evaluateEngineUnit(descriptor(), damageLayer(damage));

    damage = {};
    damage.impact01 = 0.30;
    const auto impact = evaluateEngineUnit(descriptor(), damageLayer(damage));

    require(near(laser.efficiencyAuthority01, 0.70) &&
            near(impact.efficiencyAuthority01, 0.70),
            "same functional damage percentage depends on its cause");
    require(near(laser.gimbalAuthority01, impact.gimbalAuthority01),
            "nozzle still depends on removed damage weights");
}

void testSaturationAndChannelAccumulation()
{
    EquipmentDamageState damage;
    require(game::equipment::addEquipmentDamage(
                damage, game::equipment::EquipmentDamageKind::Laser, 0.6),
            "laser increment rejected");
    require(game::equipment::addEquipmentDamage(
                damage, game::equipment::EquipmentDamageKind::Impact, 0.6),
            "impact increment rejected");
    require(near(damage.laser01, 0.6) &&
            near(damage.impact01, 0.6),
            "damage channels lost their independent histories");
    require(near(game::equipment::totalAccumulatedDamage01(damage), 1.0),
            "total functional damage did not saturate");

    const auto s = evaluateEngineUnit(descriptor(), damageLayer(damage));
    require(s.thrustFailed && s.gimbalFailed,
            "saturated damage did not fail the engine functions");
}

void testInstabilityAndIndependentFunctionalThresholds()
{
    const auto d = descriptor();
    EquipmentDamageState damage;
    damage.impact01 = 0.70;
    const auto steady = evaluateEngineUnit(d, damageLayer(damage));
    require(!steady.thrustFailed &&
            near(steady.thrustIrregularity01, 0.0),
            "70% damage incorrectly destabilized thrust");

    damage.impact01 = 0.80;
    const auto unstable = evaluateEngineUnit(d, damageLayer(damage));
    require(!unstable.thrustFailed &&
            near(unstable.thrustIrregularity01, 0.40),
            "80% damage should give 40% irregularity");

    damage.impact01 = 0.90;
    const auto tvcFailed = evaluateEngineUnit(d, damageLayer(damage));
    require(!tvcFailed.thrustFailed && tvcFailed.gimbalFailed,
            "90% nozzle failure incorrectly destroyed axial thrust");
    require(near(tvcFailed.maximumThrustNewtons, 100.0),
            "TVC-only failure changed surviving thrust");
    require(near(tvcFailed.gimbalRangeRad, 0.0),
            "failed TVC still has travel");

    damage.impact01 = 0.949;
    const auto almostFailed = evaluateEngineUnit(d, damageLayer(damage));
    require(!almostFailed.thrustFailed &&
            almostFailed.thrustIrregularity01 > 0.99,
            "94.9% thrust damage should be strongly intermittent");

    damage.impact01 = 0.95;
    const auto failed = evaluateEngineUnit(d, damageLayer(damage));
    require(failed.thrustFailed &&
            near(failed.maximumThrustNewtons, 0.0),
            "95% damage must disable useful thrust");
    require(near(requestedElectricalPowerMW(d, failed, 1.0), 0.0) &&
            near(generatedWasteHeatMW(d, failed, 1.0), 0.0),
            "failed thrust function must not draw power or generate heat");
}

void testDamageCostsMorePowerForSameDeliveredThrust()
{
    const auto d = descriptor();
    EquipmentDamageState damage;
    damage.thermal01 = 0.50;
    const auto reduced = evaluateEngineUnit(d, damageLayer(damage));
    const auto healthy = evaluateEngineUnit(d, damageLayer());

    require(near(reduced.maximumThrustNewtons, 500.0),
            "50% damage must halve available maximum thrust");
    require(near(reduced.efficiency01, 0.40),
            "damage did not degrade conversion efficiency");
    require(near(reduced.powerCostMultiplier, 2.0),
            "damaged engine cost per delivered newton is wrong");

    // 0.5 is the achieved fraction of HEALTHY nominal thrust, not the
    // position of the damaged-engine throttle lever.
    require(near(requestedElectricalPowerMW(d, healthy, 0.50), 0.3125),
            "healthy half-thrust power changed");
    require(near(requestedElectricalPowerMW(d, reduced, 0.50), 0.625),
            "degraded same-thrust power cost changed");
    require(near(generatedWasteHeatMW(d, reduced, 0.50), 0.375),
            "damaged waste heat did not equal input minus useful output");
}

} // namespace

int main()
{
    try
    {
        testHealthyFieldDrivePassport();
        testFivePercentagesAddWithoutSensitivityWeights();
        testSaturationAndChannelAccumulation();
        testInstabilityAndIndependentFunctionalThresholds();
        testDamageCostsMorePowerForSameDeliveredThrust();

        std::cout
            << "ENGINE UNIT DAMAGE MODEL: PASS\n"
            << " - five independent accumulated percentages, direct summed loss\n"
            << " - 70% instability start, 90% TVC failure, 95% thrust failure\n"
            << " - explicit field drive rating from F*v_ref/eta\n"
            << " - electrical demand and waste heat respond to reduced efficiency\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "ENGINE UNIT DAMAGE MODEL: FAIL: "
            << e.what() << "\n";
        return EXIT_FAILURE;
    }
}
