#include "src/game/physics/UniverseMaterials.h"
#include "src/game/equipment/engine/EngineUnit.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace
{
void require(bool ok, const char* error)
{
    if (!ok)
        throw std::runtime_error(error);
}
bool near(double a, double b, double eps = 1.0e-6)
{
    return std::abs(a - b) <= eps;
}

game::equipment::engine::EngineUnitDescriptor cobraMotor()
{
    game::equipment::engine::EngineUnitDescriptor d;
    d.structureMaterialId = "elite.material.engine.tial-ht.v1";
    d.nominalThrustNewtons = 9561483.75;
    d.nominalElectricalPowerMW = 4850.0;
    d.nominalEfficiency01 = 0.9855;
    d.thermalEmitter.outerDiameterM = 4.5;
    d.thermalEmitter.apertureDiameterM = 2.5;
    d.thermalEmitter.materialId = "elite.material.engine.hfc-c.v1";
    d.thermalEmitter.surfaceTemperatureK = 2500.0;
    d.thermalEmitter.viewFactor01 = 0.90;
    d.nominalCoolantTemperatureK = 1050.0;
    d.nominalOperatingTemperatureK = 1200.0;
    d.safeOperatingTemperatureK = 1300.0;
    d.thermalCapacityMJperK = 12.0;
    d.thermalConductanceMWperK =
        (70.325 - game::equipment::engine::ratedThermalEmitterPowerMW(d)) /
        150.0;
    return d;
}

void materialCatalog()
{
    using namespace game::physics::materials;
    require(UniverseMaterialCatalog.size() == 3, "material catalog size");
    const auto* plates = findMaterial("elite.material.radiator.c-sic.v1");
    const auto* ring = findMaterial("elite.material.engine.hfc-c.v1");
    const auto* titanium = findMaterial("elite.material.engine.tial-ht.v1");
    require(plates && ring && titanium && plates != ring &&
            titanium != ring && titanium != plates,
            "material identities");
    require(titanium->nominalOperatingTemperatureK >= 1200.0 &&
            titanium->damageOnsetTemperatureK > 1300.0,
            "titanium engine alloy has invalid housing thermal limits");
    require(findMaterial("unknown") == nullptr, "unknown material accepted");
    require(plates->meshMaterialId != ring->meshMaterialId &&
            plates->tradeCommodityId != ring->tradeCommodityId &&
            titanium->meshMaterialId != ring->meshMaterialId &&
            titanium->tradeCommodityId != ring->tradeCommodityId,
            "mesh/economy material collision");
    for (const auto& material : UniverseMaterialCatalog)
    {
        require(material.emissivity01 > 0.0 &&
                material.emissivity01 <= 1.0 &&
                material.densityKgM3 > 0.0 &&
                material.specificHeatJPerKgK > 0.0 &&
                material.thermalConductivityWPerMK > 0.0,
                "invalid material physical passport");
        require(material.nominalOperatingTemperatureK <
                material.damageOnsetTemperatureK &&
                material.damageOnsetTemperatureK <
                material.destructionTemperatureK,
                "impossible thermal material thresholds");
    }
}

void localizedLaserAndKineticFracture()
{
    using namespace game::physics::materials;
    const auto& ring = *findMaterial("elite.material.engine.hfc-c.v1");
    const double total = game::physics::thermal::annulusAreaM2(4.5, 2.5);

    SurfaceIntegrity initial;
    const auto laser = exposeSurface(
        ring, total, initial,
        {SurfaceImpactKind::Laser, 12.0, 0.5, 2500.0, 0.0}
    );
    require(laser.intactArea01 < 1.0 &&
            laser.intactArea01 > 0.90 && laser.microcrack01 > 0.0,
            "focused laser did not ablate locally (or erased entire ring)");
    const auto kinetic = exposeSurface(
        ring, total, laser,
        {SurfaceImpactKind::Kinetic, 8.0, 0.5, 2500.0, 0.0}
    );
    require(kinetic.intactArea01 < laser.intactArea01 &&
            kinetic.microcrack01 >= laser.microcrack01,
            "kinetic impact failed to propagate cracks/lost area");

    const auto ineffective = exposeSurface(
        ring, total, kinetic,
        {SurfaceImpactKind::Laser, -1.0, 1.0, 2500.0, 0.0}
    );
    require(near(ineffective.intactArea01, kinetic.intactArea01),
            "negative hit energy damaged surface");

    const auto melted = exposeSurface(
        ring, total, initial,
        {SurfaceImpactKind::Laser, 0.0, total, 3800.0, 0.0}
    );
    require(near(melted.intactArea01, 0.0),
            "material at destruction temperature remained operational");
}

void annulusDamageBalancesPowerAndThrust()
{
    using namespace game::equipment::engine;
    auto d = cobraMotor();
    const game::equipment::EquipmentDamageLayer healthyDamage {};
    const auto effective = evaluateEngineUnit(d, healthyDamage);
    require(near(ratedThermalEmitterPowerMW(d), 49.319051525, 1.0e-5),
            "Cobra healthy emitter material changed radiation");
    require(near(generatedWasteHeatMW(d, effective, 1.0), 70.325),
            "nominal waste heat mismatch");

    EngineOperatingState run;
    run.temperatureK = 1200.0;
    require(near(safeEmitterThrustFraction01(d, effective, run), 1.0),
            "healthy material derated engine");

    run.emitter.surface.intactArea01 = 0.75;
    require(near(safeEmitterThrustFraction01(d, effective, run), 1.0),
            "25 percent emitter loss prematurely derated engine");
    run.emitter.surface.intactArea01 = 0.50;
    require(near(safeEmitterThrustFraction01(d, effective, run), 1.0),
            "cold damaged engine should initially retain full thrust");
    const auto warming = advanceEngineOperatingState(
        d, effective, run, 1.0, 60.0, 1050.0
    );
    require(warming.next.temperatureK > run.temperatureK &&
            warming.next.temperatureK < 1300.0,
            "damaged annulus should heat engine before power protection");
    run.temperatureK = d.safeOperatingTemperatureK;
    const double half = safeEmitterThrustFraction01(d, effective, run);
    require(half > 0.84 && half < 0.86,
            "50 percent emitter loss should limit thrust to ~84.85 percent");
    const auto fullUnsafe = advanceEngineOperatingState(
        d, effective, run, 1.0, 600.0, 1050.0
    );
    const auto safeRun = advanceEngineOperatingState(
        d, effective, run, half, 600.0, 1050.0
    );
    require(fullUnsafe.next.temperatureK > 1300.0 &&
            fullUnsafe.thermalDamageIncrement01 > 0.0,
            "unlimited half-destroyed emitter caused no thermal harm");
    require(safeRun.next.temperatureK <= 1300.01 &&
            near(safeRun.thermalDamageIncrement01, 0.0, 1.0e-7),
            "limiting thrust should protect housing with ideal coolant");

    run.emitter.surface.intactArea01 = 0.0;
    const double noRing = safeEmitterThrustFraction01(d, effective, run);
    require(noRing > 0.49 && noRing < 0.51,
            "zero emitter should leave ~49.8 percent safe thrust");
    require(near(ratedThermalEmitterPowerMW(d, run.emitter.surface.intactArea01),
                 0.0),
            "destroyed ring still radiated heat");

    // Damage channels operate independently: a broken emitter alone must
    // not appear as invented direct laser/impact damage to the thrust chamber.
    require(near(effective.efficiencyAuthority01, 1.0),
            "surface damage erroneously counted as five-cause engine damage");
}

void hotAnnulusAgesSeparatelyAndOnlyThreeCausesMatter()
{
    using namespace game::equipment::engine;
    auto d = cobraMotor();
    const game::equipment::EquipmentDamageLayer damage {};
    const auto effective = evaluateEngineUnit(d, damage);
    EngineOperatingState start;
    start.temperatureK = 1200.0;

    const auto age = advanceEngineOperatingState(
        d, effective, start, 1.0, 6000.0 * 3600.0, 1050.0
    );
    require(near(age.next.emitter.equivalentOperatingHours, 6000.0),
            "independent annulus hours were not counted");
    require(age.next.emitter.damage.wear01 > 0.18 &&
            age.next.emitter.damage.wear01 < 0.20,
            "emitter 6000-hour wear mismatch");
    require(near(age.next.emitter.damage.laser01, 0.0) &&
            near(age.next.emitter.damage.impact01, 0.0),
            "normal aging created fake hits");
    require(effectiveEmitterArea01(age.next) < 1.0,
            "wear did not degrade radiator surface");
    require(age.next.equivalentOperatingHours > 0.0,
            "engine lost own independent service time");

    auto passive = start;
    passive.emitter.damage.thermal01 = 1.0;
    passive.emitter.damage.radiation01 = 1.0;
    require(near(effectiveEmitterArea01(passive), 1.0),
            "inert thermal/radiation causes changed hot-annulus function");

    auto both = age.next;
    both.emitter.surface.intactArea01 = 0.6;
    require(near(effectiveEmitterArea01(both),
                 0.6 * (1.0 - both.emitter.damage.wear01)),
            "laser/impact area was subtracted twice");

    const auto exhausted = advanceEngineOperatingState(
        d, effective, start, 1.0, 12000.0 * 3600.0, 1050.0
    );
    require(near(exhausted.next.emitter.damage.wear01, 1.0) &&
            near(effectiveEmitterArea01(exhausted.next), 0.0),
            "consumable should be exhausted at 12000 full-power hours");
}

}

int main()
{
    try
    {
        materialCatalog();
        localizedLaserAndKineticFracture();
        annulusDamageBalancesPowerAndThrust();
        hotAnnulusAgesSeparatelyAndOnlyThreeCausesMatter();
        std::cout
            << "UNIVERSE MATERIAL ENGINE THERMAL: PASS\n"
            << " - material/mesh/trade stable IDs and temperature thresholds\n"
            << " - localized laser and kinetic damage to emitter material\n"
            << " - 25/50/100 percent surface-loss thermal derating\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& e)
    {
        std::cerr << "UNIVERSE MATERIAL ENGINE THERMAL: FAIL: "
                  << e.what() << "\n";
        return EXIT_FAILURE;
    }
}
