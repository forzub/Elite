#pragma once

#include <algorithm>
#include <cmath>

namespace world::interstellar
{

/*
    Extraction indices of an interstellar medium (0..1, log-scaled against a
    dense molecular clump, n ~ 1e3 cm^-3): the diffuse hot cavity gas is
    effectively worthless as a fuel source, dense cold / molecular gas has a
    real volatile potential, supernova-enriched dust carries heavy elements.
    One definition for the generator's extraction profiles and the
    navigation environment.
*/
struct ExtractionIndices
{
    double gasKgPerKm3 = 0.0;
    double dustKgPerKm3 = 0.0;
    double iceKgPerKm3 = 0.0;
    double heavyElementsKgPerKm3 = 0.0;
    double hydrogenFuel = 0.0;
    double volatiles = 0.0;
    double heavyElements = 0.0;
};

inline ExtractionIndices extractionIndices(double densityCm3, double dustToGas, double iceMantle, double enrichment)
{
    constexpr double MeanParticleMassKg = 1.4 * 1.67262192369e-27; // H + He per H atom
    const auto index = [](double kgKm3, double referenceKgKm3) {
        return std::clamp((std::log10(std::max(kgKm3, 1.0e-30)) - std::log10(referenceKgKm3) + 6.0) / 6.0, 0.0, 1.0);
    };
    ExtractionIndices e;
    e.gasKgPerKm3 = densityCm3 * 1.0e6 * MeanParticleMassKg * 1.0e9;
    e.dustKgPerKm3 = e.gasKgPerKm3 * dustToGas;
    e.iceKgPerKm3 = e.dustKgPerKm3 * iceMantle;
    e.heavyElementsKgPerKm3 = e.dustKgPerKm3 * (1.0 + 2.0 * enrichment);
    const double reference = 1.0e3 * 1.0e6 * MeanParticleMassKg * 1.0e9; // molecular clump, kg/km^3
    e.hydrogenFuel = index(e.gasKgPerKm3 * 0.71, reference * 0.71);
    e.volatiles = index(e.iceKgPerKm3, reference * 0.012 * 0.5);
    e.heavyElements = index(e.heavyElementsKgPerKm3, reference * 0.012 * 3.0);
    return e;
}

} // namespace world::interstellar
