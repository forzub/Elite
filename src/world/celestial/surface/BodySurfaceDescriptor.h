#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

namespace world::celestial::surface
{
/*
    Visual generation descriptor of a celestial body: the single input of
    the surface generator, read from the body's GENERATED PHYSICAL
    descriptor (elite.generated_body.v1) - never an independent set of
    random visual parameters. The causal chain is

        formation -> composition -> class -> gravity -> temperature ->
        atmosphere -> hydrosphere -> terrain profile -> climate ->
        materials -> weather -> appearance

    and every visual parameter below is one of those physical facts.

    source: a body with curated / imported real data (Sol, "ImportedRealData"
    in body_visuals) keeps that provider for its maps; the procedural
    provider can still be run on the same physical descriptor (Planet Lab).
*/
enum class SurfaceSourceKind
{
    ProceduralFromDescriptor,
    ImportedRealData
};

enum class SurfaceArchetype
{
    Terrestrial,    // liquid surface water + atmosphere (continents, climate)
    AirlessRocky,   // regolith / basalt, craters (Moon, Mercury)
    DryAtmospheric, // rocky with thin / dry atmosphere (Mars, Venus-like)
    Volcanic,       // lava ocean / volcanic plains (Io)
    Icy,            // ice shell / cratered ice (Europa, Callisto)
    GasGiant,       // no solid surface: bands, storms
    IceGiant
};

struct BodySurfaceDescriptor
{
    std::string id;
    std::string name;
    std::uint64_t seed = 0;
    std::string kind;            // planet | moon | dwarf_planet
    std::string bodyClass;       // rocky | iron_rich | water_rich | gas_giant ...
    std::string origin;          // authored | generated | observed_anchor

    double radiusM = 1.0e6;
    double surfaceGravityMs2 = 9.81;
    double meanTemperatureK = 288.0;
    double minTemperatureK = 288.0;
    double maxTemperatureK = 288.0;
    double surfacePressurePa = 0.0;
    double cloudPotential = 0.0;
    double obliquityDeg = 0.0;
    double rotationPeriodHours = 24.0;

    std::string tectonicRegime;
    std::string volcanism;
    std::string hydrosphere;

    // surface_generation
    std::string terrainProfile;
    double reliefAmplitudeKm = 1.0;
    double roughness = 0.5;
    double craterDensity = 0.0;
    double resurfacingRate = 0.0;
    double erosionPotential = 0.0;
    double glaciationPotential = 0.0;
    double weatherPotential = 0.0;
    double oceanCoverage = 0.0;
    double iceCoverage = 0.0;
    std::vector<std::pair<std::string, double>> surfaceMaterials;
    std::vector<std::pair<std::string, double>> atmosphereComposition;

    // bulk composition (mass fractions)
    double ironCore = 0.0;
    double silicateMantle = 0.0;
    double waterIce = 0.0;
    double otherVolatiles = 0.0;
    double hydrogenHelium = 0.0;

    // resources.industrial_suitability
    double orbitalAccessIndex = 0.0;

    SurfaceSourceKind source = SurfaceSourceKind::ProceduralFromDescriptor;
};

// Reads a generated body JSON (elite.generated_body.v1).
bool loadGeneratedBody(const std::string& path, BodySurfaceDescriptor& out, std::string* error = nullptr);

SurfaceArchetype surfaceArchetype(const BodySurfaceDescriptor& body);
const char* archetypeName(SurfaceArchetype archetype);
bool hasSolidSurface(const BodySurfaceDescriptor& body);

/*
    Streaming / LOD policy - NOT part of the generator identity (the field
    is the same function whoever looks at it). It decides how deep a client
    asks for patches:

    - solid + reachable: orbit -> regional -> landing -> ground (decimetre
      vertex spacing);
    - solid but effectively unreachable (crushing pressure, extreme gravity
      or a molten surface): down to high-altitude detail only - no ground
      patches are requested;
    - no solid surface (gas / ice giants, sub-Neptunes): the cloud deck to
      the depth a probe reaches, never fake rocky ground.
*/
struct SurfaceDetailPolicy
{
    bool solidSurface = true;
    bool groundReachable = true;
    double finestVertexSpacingM = 0.25;
    std::string reason;
};

SurfaceDetailPolicy surfaceDetailPolicy(const BodySurfaceDescriptor& body);

// ---- small bodies (asteroids) --------------------------------------------

struct ReservoirSurfaceDescriptor
{
    std::string id;
    std::string kind;           // asteroid_belt | kuiper_belt | trojans ...
    std::uint64_t seed = 0;
    double innerAu = 0.0;
    double outerAu = 0.0;
    double sizeSlope = -3.5;
    double minBodyKm = 0.001;
    double maxBodyKm = 100.0;
    std::vector<std::pair<std::string, double>> composition; // carbonaceous_c_type ...
};

bool loadGeneratedReservoir(const std::string& path, ReservoirSurfaceDescriptor& out, std::string* error = nullptr);

enum class SmallBodyComposition
{
    Carbonaceous,
    Silicate,
    Metallic,
    Icy
};

struct SmallBodyDescriptor
{
    std::string id;             // "<reservoir>/asteroid:<cell>:<candidate>"
    std::uint64_t seed = 0;
    double meanRadiusM = 1000.0;
    SmallBodyComposition composition = SmallBodyComposition::Silicate;
};

/*
    The individual asteroids of a reservoir are never all materialised:
    a candidate is derived from (beltSeed, stable spatial cell, candidate
    index) on demand. The same cell always yields the same bodies.
*/
int asteroidCandidatesInCell(const ReservoirSurfaceDescriptor& reservoir, const glm::ivec3& cell);
SmallBodyDescriptor asteroidInReservoir(const ReservoirSurfaceDescriptor& reservoir, const glm::ivec3& cell,
                                        int candidate);
const char* compositionName(SmallBodyComposition composition);
} // namespace world::celestial::surface
