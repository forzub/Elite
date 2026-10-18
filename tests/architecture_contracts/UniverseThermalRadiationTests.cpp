#include "src/game/physics/UniverseThermalRadiation.h"
#include "src/game/equipment/engine/EngineUnit.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace
{

using game::physics::thermal::annulusAreaM2;
using game::physics::thermal::radiatedPowerMW;
using game::equipment::engine::ratedThermalEmitterPowerMW;
using game::equipment::engine::EngineUnitDescriptor;

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

bool near(double a, double b, double eps = 1.0e-4)
{
    return std::abs(a - b) <= eps;
}

void testSingleUniversalLawForBothSurfaceClasses()
{
    using game::physics::thermal::RadiationEffectiveness;
    require(near(RadiationEffectiveness, 2.5),
            "universal gameplay radiation multiplier must remain 2.5");

    const double shipRadiatorsMW =
        radiatedPowerMW(380.0, 1050.0, 0.90);
    require(near(shipRadiatorsMW, 58.92981091945),
            "ship panel radiation does not use universe law");

    const double ringArea = annulusAreaM2(4.5, 2.5);
    require(near(ringArea, 10.99557428756),
            "nozzle aperture erroneously counted as radiator");

    EngineUnitDescriptor d;
    d.thermalEmitter.outerDiameterM = 4.5;
    d.thermalEmitter.apertureDiameterM = 2.5;
    d.thermalEmitter.surfaceTemperatureK = 2500.0;
    d.thermalEmitter.emissivity01 = 0.90;
    d.thermalEmitter.viewFactor01 = 0.90;

    const double hot = ratedThermalEmitterPowerMW(d);
    require(near(hot, 49.31905152496),
            "hot ring does not use same universe radiation law");
    require(near(ratedThermalEmitterPowerMW(d, 0.50), hot * 0.50),
            "half exposed emitter area must radiate half as much");
    require(near(ratedThermalEmitterPowerMW(d, 0.0), 0.0),
            "fully destroyed hot surface still radiates");
    require(near(radiatedPowerMW(380.0, 1050.0, 0.9, 0.0), 0.0),
            "occluded surface must have no escaping radiation");

    const double pairFullRatedHeatMW =
        2.0 * 4850.0 * (1.0 - 0.9855);
    const double pairCapacityMW =
        shipRadiatorsMW + 2.0 * hot;
    require(near(pairFullRatedHeatMW, 140.65),
            "authored 98.55% field-drive thermal load changed");
    require(near(pairCapacityMW, 157.56791396937),
            "combined radiator and emitter design capacity changed");

    const double reserveFraction =
        1.0 - pairFullRatedHeatMW / pairCapacityMW;
    require(reserveFraction >= 0.10 && reserveFraction <= 0.12,
            "Cobra nominal 10%-class heat rejection margin lost");
}

void testGeometryAndPhysicalBounds()
{
    require(near(annulusAreaM2(4.0, 2.5), 7.657632093) &&
            near(annulusAreaM2(5.0, 2.5), 14.72621556),
            "engine rings must be sized by real outer and nozzle diameters");
    require(near(annulusAreaM2(2.0, 2.0), 0.0) &&
            near(annulusAreaM2(2.0, 3.0), 0.0),
            "invalid ring geometry must not create radiation area");

    // Emissivity is a PHYSICAL 0..1 material quantity; only the universe
    // game multiplier may exceed one. This prevents x2.5 being applied twice.
    require(near(radiatedPowerMW(3.0, 2500.0, 3.0),
                 radiatedPowerMW(3.0, 2500.0, 1.0)),
            "material emissivity above one must be clamped");
    require(near(radiatedPowerMW(20.0, 1000.0, 0.9, 1.0, 1000.0), 0.0),
            "no heat is radiated to an equally hot blackbody environment");
}

} // namespace

int main()
{
    try
    {
        testSingleUniversalLawForBothSurfaceClasses();
        testGeometryAndPhysicalBounds();
        std::cout
            << "UNIVERSE THERMAL RADIATION: PASS\n"
            << " - one game x2.5 multiplier for shared panels and engine rings\n"
            << " - 4.5m/2.5m Cobra ring, one exposed side, visibility 90%\n"
            << " - 98.55% efficiency leaves approximately 10% heat margin\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& e)
    {
        std::cerr << "UNIVERSE THERMAL RADIATION: FAIL: "
                  << e.what() << "\n";
        return EXIT_FAILURE;
    }
}
