#pragma once

#include <string>
#include <vector>

#include "src/world/generation/GenerationRandom.h"
#include "src/world/generation/UniverseDescriptors.h"

namespace world::generation
{

/*
    Stellar layer of the generator: galactic context, present-day stellar
    state, multiplicity hierarchy and its dynamical stability.

    Physical relations (with the confidence of each):
    - ICRS -> Galactic rotation (IAU, PhysicalLaw); Sol at R0 = 8.18 kpc,
      z0 = +20.8 pc (GRAVITY 2019 / Bennett & Bovy 2019).
    - Population densities: Juric et al. 2008 thin/thick disk + halo
      (PopulationModel). Metallicity per population: Bensby 2014,
      Casagrande 2011 (EmpiricalFit).
    - Main sequence: piecewise mass-luminosity / mass-radius relations,
      lifetime 10 Gyr M^-2.5 (EmpiricalFit); MS brightening fitted to the Sun.
    - White dwarfs: Kalirai 2008 initial-final mass relation, Mestel cooling.
    - Brown dwarfs: Burrows 1997-like cooling power law.
    - Planet zones in binaries: Holman & Wiegert 1999 (EmpiricalFit).
    - Hierarchical triples: Mardling & Aarseth 2001 (EmpiricalFit).
    - Binary orbits: Moe & Di Stefano 2017, Raghavan 2010 (PopulationModel).
*/

GalacticContextDescriptor buildGalacticContext(
    const glm::dvec3& positionLyIcrs,
    const std::vector<StarInput>& stars,
    std::uint64_t systemSeed);

// Present-day state of one star. Observed values in `input` win; missing ones
// are derived from the stellar model or generated from the population.
StarDescriptor buildStar(
    const StarInput& input,
    const GalacticContextDescriptor& context,
    double systemAgeGyr,
    double systemFeH,
    std::uint64_t starSeed,
    std::vector<std::string>& warnings);

// Age and metallicity shared by all stars of a system (coeval birth).
void sampleSystemAgeMetallicity(
    const std::vector<StarInput>& stars,
    const GalacticContextDescriptor& context,
    std::uint64_t systemSeed,
    double& ageGyr,
    double& feh,
    ProvenanceMap& provenance);

double mainSequenceLifetimeGyr(double massSolar, double feh);
double zeroAgeLuminositySolar(double massSolar);

// Builds the barycentric tree: binary relative orbits (observed or sampled),
// each star's and barycenter's orbit about its parent, planet stability
// zones. Appends stability checks (Mardling-Aarseth) to `checks`.
std::vector<BarycenterDescriptor> buildStellarHierarchy(
    const HierarchyNodeInput& root,
    std::vector<StarDescriptor>& stars,
    int systemId,
    std::uint64_t systemSeed,
    std::vector<StabilityCheck>& checks,
    std::vector<std::string>& warnings);

// Holman & Wiegert 1999 critical semi-major axes (fraction of a_binary).
double holmanWiegertSTypeCritical(double mu, double e);
double holmanWiegertPTypeCritical(double mu, double e);

// Mardling & Aarseth 2001: a_out / a_in critical ratio.
double mardlingAarsethCriticalRatio(double massRatioOuter, double eOuter, double mutualInclinationDeg);

// Kepler's third law, AU / years / solar masses.
double periodYearsFromAu(double aAu, double totalMassSolar);
double semiMajorAxisAuFromPeriod(double periodYears, double totalMassSolar);

} // namespace world::generation
