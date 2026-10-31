#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace world::celestial::surface
{
/*
    The one runtime model of a body surface: a pure function of a unit
    direction (planet-local frame, +Y = spin axis) and an LOD band limit.

        SurfaceField::sample(dir, minWavelengthM)

    - macroHeightM / water / climate / biome / featureId do NOT depend on the
      band limit: coastlines, lakes, ice caps, landmarks and climate borders
      are the same at every LOD;
    - heightM is the displayed height: the macro field plus the octaves and
      small features down to minWavelengthM. A finer LOD only adds detail.

    Planets (PlanetSurfaceField), gas giants (GasGiantField) and small
    irregular bodies (SmallBodyField) and curated real data
    (ImportedSurfaceField) implement it; the runtime renderer (far globe bake, camera-centred local terrain), the
    reproducibility checks and the baker see only this.
*/
enum class SurfaceMaterial : std::uint8_t
{
    Rock,
    Basalt,
    Regolith,
    Sand,
    Soil,
    Vegetation,
    Ice,
    Snow,
    Salt,
    Lava,
    Count
};

const char* materialName(SurfaceMaterial material);

enum class Biome : std::uint8_t
{
    Ocean,
    Lake,
    IceSheet,
    Snow,
    Tundra,
    BorealForest,
    TemperateForest,
    Grassland,
    Savanna,
    Desert,
    Rainforest,
    Barren,      // airless / dry rock and regolith
    Volcanic,
    IcePlain,    // icy moons
    GasDeck      // no surface
};

const char* biomeName(Biome biome);

enum class FeatureClass : std::uint8_t
{
    None,
    Continent,
    MountainRange,
    ImpactCrater,
    Volcano,
    Lake,
    IceCap,
    Storm
};

const char* featureClassName(FeatureClass featureClass);

// "impact_crater:3f2a..." - stable from seed and coordinates only.
std::string featureLabel(FeatureClass featureClass, std::uint64_t id);

struct SurfaceFeature
{
    FeatureClass featureClass = FeatureClass::None;
    std::uint64_t id = 0;
    glm::dvec3 centerDir {0.0, 1.0, 0.0};
    double radiusM = 0.0;
};

struct SurfaceSample
{
    double heightM = 0.0;       // displayed (band-limited) height above the reference radius
    double macroHeightM = 0.0;  // LOD-independent
    double bandLimitM = 0.0;    // the minWavelengthM this sample was taken at
    bool water = false;         // ocean or lake surface (LOD-independent)
    bool lake = false;
    double waterLevelM = 0.0;   // surface of that water
    // Signed coast value (LOD-independent): macro height above the water
    // level that applies here, > 0 land, < 0 water. Its zero set is the
    // coastline (oceans, lakes).
    double coastM = 1.0e4;
    float temperatureK = 0.0f;  // climate (from the macro height)
    float moisture = 0.0f;
    float iceMask = 0.0f;       // permanent ice / ice cap
    float freshness = 0.0f;     // fresh crater ejecta (bright)
    Biome biome = Biome::Barren;
    FeatureClass featureClass = FeatureClass::None;
    std::uint64_t featureId = 0;
};

struct SurfaceAppearance
{
    glm::vec3 albedo {0.5f};
    float emission = 0.0f;
    // Material weights (sum 1) - drive the near-camera material detail.
    std::array<float, static_cast<std::size_t>(SurfaceMaterial::Count)> weights {};
};

class SurfaceField
{
public:
    virtual ~SurfaceField() = default;

    virtual double referenceRadiusM() const = 0;
    virtual std::uint64_t identitySeed() const = 0;
    virtual bool solidSurface() const = 0;

    virtual SurfaceSample sample(const glm::dvec3& dir, double minWavelengthM) const = 0;

    // slope: 1 - dot(surface normal, up), 0 = flat.
    virtual SurfaceAppearance appearance(const SurfaceSample& sample, const glm::dvec3& dir,
                                         double slope) const = 0;

    // Upper bound of |height - macro height| once the band limit is below
    // the macro cutoff: what LOD detail can add.
    virtual double detailAmplitudeBoundM() const = 0;
    // Wavelength above which everything is LOD-independent macro field.
    virtual double macroCutoffM() const = 0;

    // Landmarks whose influence reaches the cap (dir, angular radius).
    virtual void landmarks(const glm::dvec3& dir, double radiusRad, std::vector<SurfaceFeature>& out) const = 0;
};
} // namespace world::celestial::surface
