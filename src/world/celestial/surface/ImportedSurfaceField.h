#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "src/world/celestial/surface/BodySurfaceDescriptor.h"
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
    std::vector<glm::vec3> albedo;   // linear 0..1
    std::vector<std::uint8_t> water; // 1 = ocean (may be empty)
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

private:
    struct Bilinear
    {
        int i00 = 0, i10 = 0, i01 = 0, i11 = 0;
        double fx = 0.0, fy = 0.0;
    };
    Bilinear locate(const glm::dvec3& dir) const;

    BodySurfaceDescriptor m_body;
    std::shared_ptr<const ImportedSurfaceMaps> m_maps;
    double m_radius = 1.0;
    std::uint64_t m_seed = 0, m_seedDetail = 0, m_seedVariation = 0;
    double m_macroCutoffM = 1000.0;
    double m_detailBoundM = 0.0;
    double m_detailAmplitudeM = 100.0;
    OctaveSpec m_detailSpec;
};

// Equirect 8-bit grey heights -> metres. Ocean (water mask) texels keep
// the sea floor at -depthM; land maps 0..255 to 0..landRangeM (Earth-style
// sources) unless signed (0..255 -> -range..+range around the datum).
void importedHeightsFromGrey(const std::uint8_t* grey, int width, int height, double rangeM, bool signedRange,
                             ImportedSurfaceMaps& out);
} // namespace world::celestial::surface
