#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include "src/world/interstellar/DensityLattice.h"

namespace world::interstellar
{

/*
    Physical navigation environment of interstellar space, for the future
    interstellar route planner. It reports what is THERE - gas, dust, solid
    bodies, ionisation, radiation, extinction, turbulence, resources - as
    physical quantities or documented normalised indices. It never decides
    a route cost or "dangerous = true": the hazard of a crossing depends on
    the ship and its speed, and that judgement belongs to the planner.

    Sources (one truth each): the continuous background medium is the
    generator's field (world::generation::backgroundMediumAt, a pure function
    of the universe seed); distinct structures are the materialised density
    lattices; icy debris fields are their statistical solid populations.
*/

// Solid-body size classes (lower bound of each class).
enum class SolidSizeClass : int
{
    Dust100Um = 0, // 100 um - 1 cm
    Pebble1Cm,     // 1 cm - 10 m
    Boulder10M,    // 10 m - 1 km
    Body1Km,       // above 1 km
    Count
};

struct NavigationEnvironmentSample
{
    double gasDensityCm3 = 0.0;           // hydrogen nuclei per cm^3
    double gasMassDensityKgM3 = 0.0;
    double dustMassDensityKgM3 = 0.0;     // interstellar grains + debris-field dust
    double temperatureK = 0.0;
    double ionizationFraction = 0.0;
    double electronDensityCm3 = 0.0;      // plasma level
    double molecularFraction = 0.0;
    double turbulenceKms = 0.0;
    // Number density of solid bodies per m^3 by size class.
    std::array<double, static_cast<std::size_t>(SolidSizeClass::Count)> solidNumberDensityM3 {};
    double extinctionAvPerLy = 0.0;       // optical / sensor extinction (mag per ly)
    // Ionising-radiation environment, relative (1 = the hot Local Bubble
    // interior: soft X-ray / EUV bath; ~0.1 inside shielded cold gas).
    double ionizingRadiationIndex = 0.0;
    // Extraction indices 0..1 (log scale against a dense molecular clump),
    // same definition as the generator's extraction profiles.
    double hydrogenFuelIndex = 0.0;
    double volatileIndex = 0.0;
    double heavyElementIndex = 0.0;
    char phase = 'h';
    std::string structureId;              // distinct structure dominating the point, if any
    std::string debrisFieldId;            // icy debris field the point lies in, if any
};

struct NavigationEnvironmentSegment
{
    double lengthLy = 0.0;
    double gasColumnCm2 = 0.0;            // integral n dl
    double dustColumnKgM2 = 0.0;          // integral rho_dust dl
    // Expected hits per m^2 of frontal cross-section, by size class.
    std::array<double, static_cast<std::size_t>(SolidSizeClass::Count)> solidColumnM2 {};
    double extinctionAv = 0.0;            // total along the segment
    double maxGasDensityCm3 = 0.0;
    double maxElectronDensityCm3 = 0.0;
    double maxIonizingRadiationIndex = 0.0;
    double maxTurbulenceKms = 0.0;
    double maxHydrogenFuelIndex = 0.0;
    double maxVolatileIndex = 0.0;
    std::vector<std::string> structuresCrossed;
    std::vector<std::string> debrisFieldsCrossed;
};

// One deterministic body of an icy debris field.
struct IcyBody
{
    glm::dvec3 positionLy {0.0};
    double diameterM = 0.0;
    double massKg = 0.0;
    std::uint64_t seed = 0;
};

class InterstellarNavigationEnvironment
{
public:
    // A materialised interstellar object as the catalog stores it.
    struct ObjectView
    {
        std::string id;
        std::string objectType;
        glm::dvec3 positionLy {0.0};
        const nlohmann::json* properties = nullptr;
    };

    InterstellarNavigationEnvironment(std::uint64_t universeSeed, const std::vector<ObjectView>& objects);

    NavigationEnvironmentSample sample(const glm::dvec3& positionLy) const;

    // Integral along a straight segment; stepLy <= 0 picks ~0.5 ly steps.
    NavigationEnvironmentSegment segment(const glm::dvec3& fromLy, const glm::dvec3& toLy, double stepLy = 0.0) const;

    std::size_t structureCount() const { return m_structures.size(); }
    std::size_t debrisFieldCount() const { return m_fields.size(); }

private:
    struct Structure
    {
        std::string id;
        char phase = 'p';
        DensityLattice lattice;
        glm::dvec3 lo {0.0}, hi {0.0};
        double temperatureK = 0.0, ionization = 0.0, molecular = 0.0, dustToGas = 0.0, ice = 0.0,
               enrichment = 0.0, turbulenceKms = 0.0;
    };
    struct DebrisField
    {
        std::string id;
        glm::dvec3 centreLy {0.0};
        glm::dmat3 toLocal {1.0};
        glm::dvec3 radiiLy {1.0};
        double perAu3[4] {};      // above 100 um, 1 cm, 10 m, 1 km
        double dustKgM3 = 0.0;
        double volatileIndex = 0.0;
    };

    std::uint64_t m_seed = 0;
    std::vector<Structure> m_structures;
    std::vector<DebrisField> m_fields;
};

// Deterministic bodies of an icy debris field inside a sphere: the field is
// cut into 1000-AU cubes, each cube draws its bodies from its own seed
// (Poisson with the local number density above minDiameterM, sizes from the
// field's power law), so a body exists identically whoever asks. At most
// maxBodies are returned (largest first).
std::vector<IcyBody> materializeIcyBodies(
    const nlohmann::json& fieldProperties,
    const glm::dvec3& fieldCentreLy,
    std::uint64_t fieldSeed,
    const glm::dvec3& queryCentreLy,
    double queryRadiusLy,
    double minDiameterM,
    std::size_t maxBodies = 1000);

} // namespace world::interstellar
