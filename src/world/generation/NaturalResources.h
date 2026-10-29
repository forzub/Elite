#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace world::generation
{

struct SystemDescriptor;
struct BodyDescriptor;
struct ReservoirDescriptor;

/*
    Natural endowment and physical accessibility of a body or small-body
    population - the industrial-facing layer of the physical universe.

    Three layers, kept apart:
    1. NaturalResourceProfile (immutable, deterministic): what exists. Bulk
       mass fractions per resource family derived only from the body's
       physical history - composition (core / mantle / ices / envelope),
       host [Fe/H] and [alpha/Fe], radiogenic decay over the host age,
       formation distance against the snow line - and the geological
       concentration that brings material near the surface:
       differentiation (siderophiles sink into the core), impacts
       (excavation, delivered asteroidal metal), volcanism (chalcophiles,
       ilmenite basalts), tectonics (hydrothermal ores, granitic actinides),
       hydrosphere (sediments), atmosphere (accessible gases), regolith
       (implanted solar-wind volatiles). No abundance is an independent
       random number; fieldSeed is the seed the future surface generator
       uses to place deposits of exactly these potentials.
    2. IndustrialSuitabilityProfile: how hard extraction and operation are
       (gravity well, atmosphere, radiation, temperature, terrain, hazards,
       energy, resource depth and grade).
    3. HabitabilityProfile + FuelPotential: graded settlement suitability
       (never one bool) and fuel sources with throughput and access cost.

    No prices: the economy layer (future, dynamic) values this endowment.
    Systems are not equalised - comparative advantage is intended.
*/

enum class ResourceFamily
{
    WaterIce,
    Hydrogen,
    CarbonOrganics,
    OxygenMinerals,
    Silicates,
    Iron,
    Nickel,
    Aluminium,
    Magnesium,
    Titanium,
    CopperGroup,       // chalcophile industrial metals: Cu, Zn, Pb, Sn
    RefractoryMetals,  // W, Mo, Nb, Ta
    PlatinumGroup,     // Pt, Pd, Ir, Os, Rh, Ru (+ Au)
    Actinides,         // U, Th
    NitrogenAmmonia,
    CarbonGases,       // CH4, CO, CO2
    NobleGases,        // He, Ne, Ar
    Helium3,
};

inline constexpr int ResourceFamilyCount = 18;

const char* resourceFamilyName(ResourceFamily family);

struct ResourceDeposit
{
    ResourceFamily family = ResourceFamily::WaterIce;
    double bulkMassFraction = 0.0;        // of the whole body / population
    double accessibleMassFraction = 0.0;  // in reach: atmosphere, surface, upper crust, small bodies
    double concentrationFactor = 1.0;     // local ore enrichment over the accessible mean
    double oreGrade = 0.0;                // 0..1, log-scaled concentration of the best deposits
    std::string depthClass;               // atmosphere | surface | shallow_crust | deep_crust | mantle | core | population
    std::vector<std::string> mechanisms;  // differentiation_depleted, impact_excavated, volcanic, hydrothermal, ...
};

struct NaturalResourceProfile
{
    bool valid = false;
    std::uint64_t fieldSeed = 0;          // seed of the surface deposit field
    double differentiation = 0.0;         // 0 primitive .. 1 fully differentiated
    bool solidSurface = true;
    std::vector<ResourceDeposit> deposits;
};

struct IndustrialSuitabilityProfile
{
    bool valid = false;
    double surfaceGravityG = 0.0;
    double deltaVToLowOrbitKms = 0.0;     // incl. gravity and drag losses
    double escapeVelocityKms = 0.0;
    double surfacePressureBar = 0.0;
    double minTemperatureK = 0.0;
    double maxTemperatureK = 0.0;
    double surfaceRadiationIndex = 0.0;   // 0 shielded .. 1 lethal unshielded
    double terrainRoughness = 0.0;        // 0..1
    double solarPowerWm2 = 0.0;           // at the surface (or the cloud deck)
    double geothermalPowerWm2 = 0.0;
    double tidalVolcanicHazard = 0.0;     // 0..1
    double orbitalAccessIndex = 0.0;      // 0..1, 1 = trivially reachable
    double operatingCostIndex = 0.0;      // 0..1, environment burden on machinery
    // Transport geometry: the body's (or its parent planet's) orbit around
    // the host and the Hohmann delta-v to the host's habitable-zone orbit -
    // the natural hub of a system's population (not counting surface launch).
    double heliocentricDistanceAu = 0.0;
    double deltaVToHabitableZoneKms = 0.0;
    // Relative extraction cost per family (1 = easy accessible grade on a
    // small airless body; larger = harder). Families without accessible
    // material are absent.
    std::vector<std::pair<ResourceFamily, double>> extractionCost;
};

struct HabitabilityProfile
{
    bool valid = false;
    double exteriorSurvivability = 0.0;   // 0..1: unprotected human outside
    double pressureSuitability = 0.0;     // 0..1
    double oxygenPartialPressureBar = 0.0;
    std::string oxygenRequirement;        // none | supplemental | full_supply
    double temperatureBurden = 0.0;       // 0 comfortable .. 1 extreme
    double waterAccessibility = 0.0;      // 0..1
    double agriculturePotential = 0.0;    // 0..1 (open field needs air + water + light)
    double radiationProtectionRequirement = 0.0; // 0..1
    double gravityComfort = 0.0;          // 0..1
    double atmosphericToxicity = 0.0;     // 0..1 (incl. corrosion)
    double climateStability = 0.0;        // 0..1
    double constructionDifficulty = 0.0;  // 0..1
    double lifeSupportCost = 0.0;         // 0..1
    double industrialOperatingCost = 0.0; // 0..1
    std::string settlementClass;          // open_air | pressure_suit | sealed_habitat | orbital_only
};

struct FuelSource
{
    std::string kind;                     // water_ice | hydrogen | methane | ammonia | carbon_dioxide | helium3 | ring_ice
    std::string mode;                     // surface_mining | ice_mining | atmospheric_extraction | atmospheric_skimming | ring_harvesting | population_mining
    double abundanceIndex = 0.0;          // 0..1
    double throughputIndex = 0.0;         // 0..1: extraction rate per installation
    double accessCostIndex = 0.0;         // 0..1: gravity well / environment
    double netIndex = 0.0;                // abundance x throughput x (1 - access cost)
};

struct FuelPotential
{
    bool valid = false;
    std::vector<FuelSource> sources;
    std::string bestSource;
    double bestNetIndex = 0.0;
};

struct ResourceProfiles
{
    NaturalResourceProfile natural;
    IndustrialSuitabilityProfile industrial;
    HabitabilityProfile habitability;
    FuelPotential fuel;
};

// Final generation layer: fills body.resources and reservoir.resources of
// every body and population of the system from their physical descriptors.
void deriveResourceProfiles(SystemDescriptor& system);

} // namespace world::generation
