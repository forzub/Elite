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
    d.nominalEfficiency01 = 0.998;
    // Authored electric rating is independent of current ship velocity.
    d.nominalElectricalPowerMW = 0.625;
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
            "authored nominal electric power changed");
    require(!s.thrustFailed && !s.gimbalFailed,
            "healthy engine failed");
    require(near(s.efficiency01, 0.998),
            "healthy electric-to-field efficiency changed");
    require(near(s.maximumThrustNewtons, 1000.0),
            "healthy thrust rating changed");
    require(near(s.gimbalRangeRad, 0.20) &&
            near(s.gimbalRateRadPerSec, 0.40),
            "healthy TVC rating changed");
    require(near(requestedElectricalPowerMW(d, s, 1.0),
                 d.nominalElectricalPowerMW),
            "healthy full-thrust electrical draw changed");
    require(near(generatedWasteHeatMW(d, s, 1.0),
                 d.nominalElectricalPowerMW * (1.0 - d.nominalEfficiency01)),
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
            "50% functional damage must halve available maximum thrust");
    require(reduced.efficiency01 < healthy.efficiency01 &&
            reduced.efficiency01 > 0.99,
            "conversion loss must remain slight and independent of thrust loss");
    require(near(reduced.efficiency01, 0.998 * (1.0 - 0.002 * 0.50)),
            "relative efficiency damage curve changed");
    require(near(reduced.powerCostMultiplier, 1.0 / 0.999),
            "damaged per-newton electricity cost is wrong");

    const double halfHealthy =
        requestedElectricalPowerMW(d, healthy, 0.50);
    const double halfDamaged =
        requestedElectricalPowerMW(d, reduced, 0.50);
    require(near(halfHealthy, d.nominalElectricalPowerMW * 0.50),
            "healthy half-thrust power changed");
    require(halfDamaged > halfHealthy &&
            near(halfDamaged, halfHealthy / 0.999),
            "degraded same-thrust input cost changed");
    require(near(requestedElectricalPowerMW(d, reduced, 1.0),
                 halfDamaged),
            "electrical demand billed impossible extra damaged thrust");
    const double expectedHeat =
        halfDamaged -
        d.nominalElectricalPowerMW * 0.50 * d.nominalEfficiency01;
    require(near(generatedWasteHeatMW(d, reduced, 0.50), expectedHeat),
            "damaged waste heat must equal actual input minus useful output");
    require(reduced.heatMultiplier > 0.0,
            "BITE did not see increased waste heat");
}

void testHotEmitterFeedsBulkHeatAndDamagedSurfaceOverheats()
{
    auto d = descriptor();
    d.nominalElectricalPowerMW = 4850.0;
    d.nominalEfficiency01 = 0.9855;
    d.thermalEmitter.outerDiameterM = 4.5;
    d.thermalEmitter.apertureDiameterM = 2.5;
    d.thermalEmitter.surfaceTemperatureK = 2500.0;
    d.thermalEmitter.emissivity01 = 0.90;
    d.thermalEmitter.viewFactor01 = 0.90;

    using game::equipment::engine::ratedThermalEmitterPowerMW;
    using game::equipment::engine::advanceEngineOperatingState;
    using game::equipment::engine::EngineOperatingState;

    const auto healthy = evaluateEngineUnit(d, damageLayer());
    const double fullHeatMW = generatedWasteHeatMW(d, healthy, 1.0);
    const double ringMW = ratedThermalEmitterPowerMW(d);
    require(near(fullHeatMW, 70.325, 1.0e-6),
            "engine heat mismatch for new independent rated input");
    require(near(ringMW, 49.319051525, 1.0e-5),
            "Cobra emitter capacity mismatch");

    d.thermalConductanceMWperK =
        (fullHeatMW - ringMW) / 150.0;
    d.thermalCapacityMJperK =
        d.thermalConductanceMWperK * 15.0;
    const EngineOperatingState hotStart {1200.0, 0.0};
    const auto healthyRun = advanceEngineOperatingState(
        d, healthy, hotStart, 1.0, 400.0, 1050.0, 1.0
    );
    require(near(healthyRun.emitterRadiatedHeatMW, ringMW, 1.0e-5) &&
            near(healthyRun.coolantHeatMW, fullHeatMW - ringMW, 1.0e-5),
            "hot ring did not unload shared coolant");
    require(near(healthyRun.next.temperatureK, 1200.0, 1.0e-3) &&
            near(healthyRun.thermalDamageIncrement01, 0.0),
            "healthy ring overheated nominal engine housing");

    const auto broken = advanceEngineOperatingState(
        d, healthy, hotStart, 1.0, 400.0, 1050.0, 0.0
    );
    require(near(broken.emitterRadiatedHeatMW, 0.0) &&
            near(broken.coolantHeatMW, fullHeatMW),
            "destroyed emitter still radiates heat");
    require(broken.next.temperatureK > 1500.0 &&
            broken.thermalDamageIncrement01 > 0.0,
            "a destroyed emitter did not overheat engine bulk");
}

void testRatedLifeAndHeatAccumulateOnlyFromPhysicalOperation()
{
    auto d = descriptor();
    d.overhaulEquivalentHours = 30000.0;
    d.endOfLifeEquivalentHours = 50000.0;
    d.damageAtOverhaul01 = 0.20;
    d.damageAtEndOfLife01 = 0.95;

    using game::equipment::engine::engineWearDamageAtHours;
    using game::equipment::engine::advanceEngineOperatingState;
    using game::equipment::engine::EngineOperatingState;
    require(near(engineWearDamageAtHours(d, 0), 0.0) &&
            near(engineWearDamageAtHours(d, 15000), 0.10) &&
            near(engineWearDamageAtHours(d, 30000), 0.20) &&
            near(engineWearDamageAtHours(d, 50000), 0.95),
            "30k/50k-hour resource curves lost their explicit service endpoints");

    const auto healthy = evaluateEngineUnit(d, damageLayer());
    // Small thermal test fixture with the same operating temperature and
    // physical equilibrium as Cobra but a much shorter test-only time constant.
    const double healthyWaste =
        generatedWasteHeatMW(d, healthy, 1.0);
    d.nominalCoolantTemperatureK = 1050.0;
    d.nominalOperatingTemperatureK = 1200.0;
    d.safeOperatingTemperatureK = 1300.0;
    d.thermalConductanceMWperK = healthyWaste / 150.0;
    d.thermalCapacityMJperK =
        d.thermalConductanceMWperK * 50.0;

    const EngineOperatingState start {1050.0, 0.0};
    const auto off = advanceEngineOperatingState(
        d, healthy, start, 0.0, 600.0, 1050.0
    );
    require(near(off.next.equivalentOperatingHours, 0.0) &&
            near(off.wearDamageIncrement01, 0.0) &&
            near(off.thermalDamageIncrement01, 0.0),
            "inactive engine must not age at normal temperature");

    const auto full = advanceEngineOperatingState(
        d, healthy, start, 1.0, 600.0, 1050.0
    );
    require(near(full.next.temperatureK, 1200.0, 0.001),
            "full-thrust engine did not reach its 1200 K design operating point");
    require(full.next.equivalentOperatingHours > 0.0 &&
            full.wearDamageIncrement01 > 0.0 &&
            near(full.thermalDamageIncrement01, 0.0),
            "healthy full load should age but not overheat");

    // An external hotter coolant boundary must overheat the SAME engine
    // even while reactor/cooling remain nondamageable in this phase.
    const auto overheated = advanceEngineOperatingState(
        d, healthy, full.next, 1.0, 600.0, 1350.0
    );
    require(overheated.next.temperatureK > 1450.0 &&
            overheated.thermalDamageIncrement01 > 0.0,
            "hotter coolant did not cause actual accumulated thermal damage");
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
        testHotEmitterFeedsBulkHeatAndDamagedSurfaceOverheats();
        testRatedLifeAndHeatAccumulateOnlyFromPhysicalOperation();

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
