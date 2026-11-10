#include "src/game/physics/UniverseThermalRadiation.h"
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
    // Pad law (e41566c6 / UniverseThermalRadiation.h propulsionPadCoolingMW): the pad is a dedicated sink of
    // 6 MW/m2 x area x intact, ramping from 0 at the nominal to full at the safe operating temperature; no coolant
    // split. Energy: generated = pad + stored. A destroyed pad stores everything and overheats the engine.
    auto d = descriptor();
    d.nominalElectricalPowerMW = 4850.0;
    d.nominalEfficiency01 = 0.991;
    d.thermalEmitter.outerDiameterM = 3.5;
    d.thermalEmitter.apertureDiameterM = 0.0;
    d.thermalEmitter.coneHeightM = 1.3;
    d.nominalOperatingTemperatureK = 1200.0;
    d.safeOperatingTemperatureK = 1300.0;
    d.thermalCapacityMJperK = 12.0;

    using game::equipment::engine::advanceEngineOperatingState;
    using game::equipment::engine::EngineOperatingState;

    const auto healthy = evaluateEngineUnit(d, damageLayer());
    const double fullHeatMW = generatedWasteHeatMW(d, healthy, 1.0);
    require(near(fullHeatMW, 4850.0 * 0.009, 1.0e-6),
            "engine heat mismatch for new independent rated input");
    const double padFullMW = game::physics::thermal::conicalEmitterAreaM2(3.5, 0.0, 1.3) *
        game::physics::thermal::PropulsionPadCoolingMWPerM2;
    require(padFullMW > fullHeatMW, "test fixture: the healthy pad must out-cool the full heat");

    const EngineOperatingState atSafe {1300.0, 0.0};
    const double dt = 10.0;
    const auto healthyRun = advanceEngineOperatingState(d, healthy, atSafe, 1.0, dt, 1050.0, 1.0);
    const double storedHealthy = d.thermalCapacityMJperK * (healthyRun.next.temperatureK - atSafe.temperatureK) / dt;
    require(near(healthyRun.coolantHeatMW, 0.0) &&
            near(healthyRun.emitterRadiatedHeatMW + storedHealthy, fullHeatMW, 1.0e-6),
            "hot ring did not unload shared coolant");
    require(healthyRun.next.temperatureK <= 1300.0 + 1.0e-9 &&
            near(healthyRun.thermalDamageIncrement01, 0.0),
            "healthy ring overheated nominal engine housing");

    const auto broken = advanceEngineOperatingState(d, healthy, atSafe, 1.0, 400.0, 1050.0, 0.0);
    const double storedMW = d.thermalCapacityMJperK * (broken.next.temperatureK - atSafe.temperatureK) / 400.0;
    require(near(broken.emitterRadiatedHeatMW, 0.0) &&
            near(broken.coolantHeatMW + storedMW, fullHeatMW, 1.0e-6),
            "damaged emitter violated engine-to-coolant energy conservation");
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

    // Pad law (975d2a5f / e41566c6): the engine's own pad (6 MW/m2 x area) is its sink, ramping from 0 at the
    // nominal to full at the safe temperature. Fixture pad = 2 x the full waste heat -> equilibrium at
    // nominal + 50 % of the band = 1250 K; small steps (explicit integration, time constant ~60 s).
    // (the engine heat capacity is floored at 1 MJ/K, so the thermal fixture runs at 1000 x the electric rating)
    d.nominalElectricalPowerMW *= 1000.0;
    const auto hot = evaluateEngineUnit(d, damageLayer());
    const double hotWaste = generatedWasteHeatMW(d, hot, 1.0);
    const double padArea = 2.0 * hotWaste / game::physics::thermal::PropulsionPadCoolingMWPerM2;
    d.thermalEmitter.outerDiameterM = 2.0 * std::sqrt(padArea / 3.14159265358979323846);
    d.thermalEmitter.apertureDiameterM = 0.0;
    d.thermalCapacityMJperK = 2.0 * hotWaste * 60.0 / 100.0;
    auto runFor = [&](double seconds, double emitterArea01) {
        EngineOperatingState s = start;
        game::equipment::engine::EngineLifeStep last;
        double wear = 0.0, heat = 0.0;
        for (int i = 0; i < static_cast<int>(seconds); ++i)
        {
            last = advanceEngineOperatingState(d, hot, s, 1.0, 1.0, 1050.0, emitterArea01);
            wear += last.wearDamageIncrement01;
            heat += last.thermalDamageIncrement01;
            s = last.next;
        }
        last.wearDamageIncrement01 = wear;
        last.thermalDamageIncrement01 = heat;
        return last;
    };
    const auto full = runFor(900.0, 1.0);
    require(near(full.next.temperatureK, 1250.0, 1.0),
            "full-thrust engine did not reach its pad-law design operating point");
    require(full.next.equivalentOperatingHours > 0.0 &&
            full.wearDamageIncrement01 > 0.0 &&
            near(full.thermalDamageIncrement01, 0.0),
            "healthy full load should age but not overheat");

    // A destroyed pad must overheat the SAME engine and accumulate thermal damage.
    const auto overheated = runFor(900.0, 0.0);
    require(overheated.next.temperatureK > 1450.0 &&
            overheated.thermalDamageIncrement01 > 0.0,
            "a destroyed pad did not cause actual accumulated thermal damage");
}

void testLiveCoolantOverheatReducesThrustAndConservesEnergy()
{
    using namespace game::equipment::engine;
    auto d=descriptor();
    d.nominalEfficiency01=0.9855;
    d.nominalElectricalPowerMW=4850.0;
    d.thermalEmitter.outerDiameterM=4.5;
    d.thermalEmitter.apertureDiameterM=2.5;
    d.thermalEmitter.materialId="elite.material.engine.hfc-c.v1";
    d.thermalEmitter.surfaceTemperatureK=2500.0;
    d.thermalConductanceMWperK=0.14;
    d.thermalCapacityMJperK=12.0;
    d.safeOperatingTemperatureK=1300.0;
    const auto effective=evaluateEngineUnit(d,damageLayer());
    EngineOperatingState operating;
    operating.temperatureK=1200.0;
    operating.coolantTemperatureK=1050.0;
    const auto baseline=safeEmitterThrustFraction01(d,effective,operating);
    operating.coolantTemperatureK=1320.0;
    // 975d2a5f (2026-10-22): the pad is the engine's dedicated sink (area-based, 6 MW/m2); the ship coolant is no
    // longer coupled to the engine governor. A hot common coolant therefore must NOT change the drive's thrust
    // ceiling; the boundary is still recorded and energy is still conserved.
    const auto unprotected=safeEmitterThrustFraction01(d,effective,operating);
    require(near(unprotected,baseline),
            "hot common coolant changed the pad-cooled engine governor");
    const auto step=advanceEngineOperatingState(
        d,effective,operating,1.0,120.0,1320.0
    );
    require(step.next.coolantTemperatureK==1320.0,
            "engine ignored actual shared coolant boundary");
    const double storedMW=d.thermalCapacityMJperK*
        (step.next.temperatureK-operating.temperatureK)/120.0;
    require(near(step.emitterRadiatedHeatMW+
                 step.coolantHeatMW+storedMW,
                 step.dissipatedHeatMW,1.0e-6),
            "thermal simulation created or destroyed energy");
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
        testLiveCoolantOverheatReducesThrustAndConservesEnergy();
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
