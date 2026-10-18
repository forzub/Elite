#pragma once

#include <algorithm>
#include <cmath>

namespace game::physics::thermal
{

// Canonical thermal-radiation law of the ELITE game universe.
//
// The Stefan-Boltzmann constant, deep-space background and *gameplay*
// multiplier belong here, never to a ship, engine or cooling implementation.
// RADIATION_EFFECTIVENESS = 2.5 is a deliberate universal gameplay rule,
// not a material emissivity and not a real-world physical coefficient.
// Material emissivity itself is physically bounded to [0, 1].
inline constexpr double StefanBoltzmannWPerM2K4 = 5.670374419e-8;
inline constexpr double DeepSpaceTemperatureK = 2.725;
inline constexpr double RadiationEffectiveness = 2.5;
inline constexpr double Pi = 3.14159265358979323846;

// One-sided hot annulus (no fictitious solid disk over the exhaust opening).
// Geometry belongs to the installed engine type, not these universe laws.
[[nodiscard]] inline double annulusAreaM2(
    double outerDiameterM,
    double apertureDiameterM
) noexcept
{
    if (!std::isfinite(outerDiameterM) ||
        !std::isfinite(apertureDiameterM) ||
        outerDiameterM <= 0.0 ||
        apertureDiameterM < 0.0 ||
        apertureDiameterM >= outerDiameterM)
    {
        return 0.0;
    }

    return (Pi / 4.0) *
        (outerDiameterM * outerDiameterM -
         apertureDiameterM * apertureDiameterM);
}

// Unified ship-panel and engine-emitter radiation, in MW.
//
// areaM2: geometric ONE-SIDED surface area already discounted for lost
//         panels/holes where relevant.
// surfaceTemperatureK: radiating surface, not the engine's bulk temperature.
// emissivity01: material radiative emissivity (0..1).
// viewFactor01: fraction of emitted radiation escaping to the sky, 0..1;
//               occlusion/reflection into the ship must not disappear as heat.
// ambientTemperatureK: receiving blackbody environment.
//
// The universal x2.5 gameplay factor is applied exactly ONCE by this law.
[[nodiscard]] inline double radiatedPowerMW(
    double areaM2,
    double surfaceTemperatureK,
    double emissivity01,
    double viewFactor01 = 1.0,
    double ambientTemperatureK = DeepSpaceTemperatureK
) noexcept
{
    if (!std::isfinite(areaM2) ||
        !std::isfinite(surfaceTemperatureK) ||
        !std::isfinite(emissivity01) ||
        !std::isfinite(viewFactor01) ||
        !std::isfinite(ambientTemperatureK) ||
        areaM2 <= 0.0 ||
        surfaceTemperatureK <= ambientTemperatureK ||
        ambientTemperatureK < 0.0)
    {
        return 0.0;
    }

    const double area = std::max(0.0, areaM2);
    const double emissivity = std::clamp(emissivity01, 0.0, 1.0);
    const double viewFactor = std::clamp(viewFactor01, 0.0, 1.0);
    const double t4 = std::pow(surfaceTemperatureK, 4.0);
    const double background4 = std::pow(ambientTemperatureK, 4.0);

    const double watts =
        RadiationEffectiveness * emissivity * viewFactor *
        StefanBoltzmannWPerM2K4 * area *
        (t4 - background4);

    return std::isfinite(watts) && watts > 0.0
        ? watts * 1.0e-6
        : 0.0;
}

} // namespace game::physics::thermal
