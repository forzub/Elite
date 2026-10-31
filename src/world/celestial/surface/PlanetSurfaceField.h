#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

#include "src/world/celestial/surface/BodySurfaceDescriptor.h"
#include "src/world/celestial/surface/SurfaceField.h"
#include "src/world/celestial/surface/SurfaceNoise.h"

namespace world::celestial::surface
{
/*
    Solid planet / moon surface derived from the physical descriptor.

    Layers (each its own seed - adding a layer never moves another):

      continents  low-frequency exponential-gradient fBm, domain warped;
                  the land threshold is the quantile that gives the
                  descriptor's ocean coverage (or the highland fraction of a
                  dry body) - continents / basins.
      ranges      belts along zero crossings of a separate low-frequency
                  field (plate-margin-like, long and narrow), filled with a
                  ridged multifractal; strength from the tectonic regime.
      hills       derivative-damped fBm (erosion potential = damping):
                  calm plains and eroded flanks, not sandpaper.
      landmarks   impact craters (several size classes, density from the
                  descriptor), volcanoes (volcanism), lakes (climate),
                  ice caps - discrete, from 3D lattice cells: each has a
                  stable id and position.
      boulders    local rock field below ~20 m (ground LOD only).
      climate     latitude (insolation, obliquity) deformed by a warp field,
                  lapse rate, Hadley-like moisture bands, continentality and
                  rain shadow; evaluated on the MACRO height so climate
                  borders never move with LOD.
      materials   composition + biome + temperature + moisture + altitude +
                  slope + freshness + deterministic variation.

    Everything at wavelengths >= macroCutoffM is the LOD-independent macro
    field; the coastline is the sign of the macro height.
*/
class PlanetSurfaceField final : public SurfaceField
{
public:
    explicit PlanetSurfaceField(const BodySurfaceDescriptor& body);

    double referenceRadiusM() const override { return m_radius; }
    std::uint64_t identitySeed() const override { return m_surfaceSeed; }
    bool solidSurface() const override { return true; }
    SurfaceSample sample(const glm::dvec3& dir, double minWavelengthM) const override;
    SurfaceAppearance appearance(const SurfaceSample& sample, const glm::dvec3& dir, double slope) const override;
    double detailAmplitudeBoundM() const override { return m_detailBoundM; }
    void landmarks(const glm::dvec3& dir, double radiusRad, std::vector<SurfaceFeature>& out) const override;

    const BodySurfaceDescriptor& body() const { return m_body; }
    SurfaceArchetype archetype() const { return m_archetype; }
    double macroCutoffM() const override { return m_macroCutoffM; }
    double landThreshold() const { return m_landThreshold; }

private:
    struct Terrain
    {
        double macro = 0.0;
        double total = 0.0;
        double continent = 0.0;   // continent signal minus threshold
        double ranges = 0.0;      // 0..1 mountain belt strength here
        std::uint64_t rangeId = 0;
    };
    struct CraterClass
    {
        double cellM = 0.0;
        double minRadius = 0.0;   // fraction of the cell
        double maxRadius = 0.0;
        double probability = 0.0;
    };
    struct Landmark
    {
        FeatureClass featureClass = FeatureClass::None;
        std::uint64_t id = 0;
        glm::dvec3 center {0.0};
        double radiusM = 0.0;
        double depthM = 0.0;      // crater depth / volcano height / lake depth
        double levelM = 0.0;      // lake surface
        double freshness = 0.0;
    };

    Terrain terrain(const glm::dvec3& dir, double minWavelengthM) const;
    double continentSignal(const glm::dvec3& dir) const;
    void climate(const glm::dvec3& dir, double macroHeightM, double continent, double ranges, float& temperatureK,
                 float& moisture) const;
    Biome biome(const SurfaceSample& s) const;

    // Landmarks of one class near a direction (3x3x3 cells around it).
    void craterCandidates(int classIndex, const glm::dvec3& dir, std::vector<Landmark>& out) const;
    void volcanoCandidates(const glm::dvec3& dir, std::vector<Landmark>& out) const;
    void lakeCandidates(const glm::dvec3& dir, std::vector<Landmark>& out) const;
    std::optional<Landmark> lakeInCell(std::int64_t cx, std::int64_t cy, std::int64_t cz) const;
    double craterProfile(const Landmark& crater, double r) const;
    double boulders(const glm::dvec3& dir, double minWavelengthM, double rockiness) const;

    BodySurfaceDescriptor m_body;
    SurfaceArchetype m_archetype;
    double m_radius = 1.0;
    double m_reliefM = 1000.0;
    std::uint64_t m_surfaceSeed = 0;
    std::uint64_t m_seedContinents = 0, m_seedWarp = 0, m_seedBelts = 0, m_seedRanges = 0, m_seedHills = 0,
                  m_seedCraters = 0, m_seedVolcanoes = 0, m_seedLakes = 0, m_seedClimate = 0,
                  m_seedVariation = 0, m_seedBoulders = 0, m_seedUplands = 0;
    OctaveSpec m_continentSpec, m_warpSpec, m_beltSpec, m_rangeSpec, m_hillSpec;
    double m_macroCutoffM = 1000.0;
    double m_landThreshold = 0.0;
    bool m_ocean = false;            // liquid surface ocean
    double m_rangeStrength = 0.5;
    double m_hillStrength = 0.1;
    double m_damping = 1.0;
    double m_volcanoProbability = 0.0;
    double m_lakeProbability = 0.0;
    std::vector<CraterClass> m_craterClasses;
    double m_detailBoundM = 0.0;

    // Climate
    double m_equatorPoleK = 40.0;
    double m_lapseKPerM = 0.0;
    double m_capSinLatitude = 2.0;   // ice cap from |sin lat| (iceCoverage)

    // Materials: the bedrock / loose cover mixture of the composition.
    std::array<float, static_cast<std::size_t>(SurfaceMaterial::Count)> m_bedrock {};
    glm::vec3 m_regolithTint {1.0f};
    float m_ironOxide = 0.0f;
    float m_sulfur = 0.0f;
    bool m_vegetation = false;

    // Lake acceptance needs the terrain and climate at the lake centre:
    // memoised per thread and cell in lakeInCell (a pure function - the
    // cache never changes a result, only its cost).
};

// Field of any descriptor: planets, moons, gas / ice giants.
std::unique_ptr<SurfaceField> makeSurfaceField(const BodySurfaceDescriptor& body);
} // namespace world::celestial::surface
