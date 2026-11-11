#pragma once

#include <array>
#include <cmath>

#include <cstdint>
#include <memory>
#include <vector>

#include "src/world/celestial/surface/BodySurfaceDescriptor.h"
#include "src/world/celestial/surface/CraterLayer.h"
#include "src/world/celestial/surface/SurfaceField.h"
#include "src/world/celestial/surface/SurfaceNoise.h"

namespace world::celestial::surface
{
/*
    Curated real data (Sol bodies, "ImportedRealData") as a SurfaceField, so
    the runtime renderer sees ONE kind of continuous surface function:
        macro   = the imported equirect maps (albedo, height, water mask) -
                  the real geography, bilinear in planet-fixed coordinates;
        detail  = procedural octaves below the source resolution, seeded by
                  the body identity, band-limited like every other field.
    The macro cutoff is the source texel size: a closer look never moves a
    coastline of the curated map, it only adds terrain inside its texels.

    Map convention (same as the map shaders and the baker): row 0 = north,
    column u = 0.5 + atan2(z, x) / 2 pi of the body-fixed direction
    (x = prime meridian, y = north, z = east).
*/
struct ImportedSurfaceMaps
{
    int width = 0;
    int height = 0;
    std::vector<float> heightM;      // above the reference radius (may be empty)
    // The source's 8-bit sRGB-ish colour, 3 bytes per texel: decoded to linear
    // on read through a 256-entry table - bit-identical to the former
    // glm::vec3 (12 B / texel) per-texel pow(c / 255, 2.2), at a quarter of
    // the memory (docs/CELESTIAL_RESOURCE_AUDIT.md, finding 2).
    std::vector<std::uint8_t> albedoRgb;
    std::vector<std::uint8_t> water; // 1 = ocean (may be empty)

    bool hasAlbedo() const { return !albedoRgb.empty(); }
    std::size_t texelCount() const { return albedoRgb.size() / 3; }
    static const float* albedoDecodeTable()
    {
        static const std::array<float, 256> table = [] {
            std::array<float, 256> t {};
            for (int c = 0; c < 256; ++c)
                t[static_cast<std::size_t>(c)] = std::pow(static_cast<float>(c) / 255.0f, 2.2f);
            return t;
        }();
        return table.data();
    }
    glm::vec3 albedoAt(std::size_t i) const
    {
        const float* t = albedoDecodeTable();
        return {t[albedoRgb[i * 3]], t[albedoRgb[i * 3 + 1]], t[albedoRgb[i * 3 + 2]]};
    }
};

class ImportedSurfaceField final : public SurfaceField
{
public:
    ImportedSurfaceField(const BodySurfaceDescriptor& body, std::shared_ptr<const ImportedSurfaceMaps> maps);

    double referenceRadiusM() const override { return m_radius; }
    std::uint64_t identitySeed() const override { return m_seed; }
    bool solidSurface() const override { return true; }
    SurfaceSample sample(const glm::dvec3& dir, double minWavelengthM) const override;
    SurfaceAppearance appearance(const SurfaceSample& sample, const glm::dvec3& dir, double slope) const override;
    double detailAmplitudeBoundM() const override { return m_detailBoundM; }
    double macroCutoffM() const override { return m_macroCutoffM; }
    void landmarks(const glm::dvec3&, double, std::vector<SurfaceFeature>&) const override {}
    std::size_t memoryBytes() const override
    {
        return m_maps ? m_maps->heightM.capacity() * sizeof(float) + m_maps->albedoRgb.capacity() +
                            m_maps->water.capacity()
                      : 0;
    }

private:
    struct Bilinear
    {
        int i00 = 0, i10 = 0, i01 = 0, i11 = 0;
        double fx = 0.0, fy = 0.0;
    };
    Bilinear locate(const glm::dvec3& dir) const { return locate(dir, m_maps->width, m_maps->height); }
    static Bilinear locate(const glm::dvec3& dir, int w, int h);

    BodySurfaceDescriptor m_body;
    std::shared_ptr<const ImportedSurfaceMaps> m_maps;
    double m_radius = 1.0;
    std::uint64_t m_seed = 0, m_seedDetail = 0, m_seedVariation = 0;
    double m_macroCutoffM = 1000.0;
    double m_detailBoundM = 0.0;
    double m_detailAmplitudeM = 100.0;
    OctaveSpec m_detailSpec;
    // airless / icy bodies (the Moon): below the map's texel a cratered
    // regolith landscape instead of the terrestrial hills-and-ridges detail
    bool m_cratered = false;
    CraterLayer m_craters;
    // a cratered body: the map's height low-passed to its basins / maria (the
    // macro shape); the craters of every size above it are the crater layer -
    // a smooth sphere covered with craters, not the map's 8-bit texel roughness
    std::vector<float> m_smoothHeightM; // quarter resolution, m_smoothW x m_smoothH
    int m_smoothW = 0, m_smoothH = 0;
    double m_maxCraterRadiusM = 0.0;
    double m_meanAlbedoLum = 0.0;
    double craterDensityAt(const glm::dvec3& dir) const; // maria (dark albedo) hold fewer craters
};

// Equirect 8-bit grey heights -> metres. Ocean (water mask) texels keep
// the sea floor at -depthM; land maps 0..255 to 0..landRangeM (Earth-style
// sources) unless signed (0..255 -> -range..+range around the datum).
void importedHeightsFromGrey(const std::uint8_t* grey, int width, int height, double rangeM, bool signedRange,
                             ImportedSurfaceMaps& out);
} // namespace world::celestial::surface
