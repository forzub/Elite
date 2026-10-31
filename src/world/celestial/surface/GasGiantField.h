#pragma once

#include "src/world/celestial/surface/BodySurfaceDescriptor.h"
#include "src/world/celestial/surface/SurfaceField.h"
#include "src/world/celestial/surface/SurfaceNoise.h"

namespace world::celestial::surface
{
/*
    Gas / ice giant cloud deck. There is no ground: the "surface" is the
    visible deck at the reference radius (height 0 everywhere) and only its
    appearance has structure:

      zonal bands     jets per hemisphere from size and spin, zones / belts
                      alternate; ice giants: few, low-contrast bands;
      shear eddies    domain-warped noise stretched along latitude, strongest
                      at band edges;
      storms          oval vortices from 3D lattice cells (stable ids).

    The weather epoch (seconds of world time) shifts the bands by their
    differential zonal drift - deterministic from (seed, epoch); the Lab
    freezes the epoch, the runtime would pass world time.
*/
class GasGiantField final : public SurfaceField
{
public:
    explicit GasGiantField(const BodySurfaceDescriptor& body, double epochSeconds = 0.0);

    double referenceRadiusM() const override { return m_radius; }
    std::uint64_t identitySeed() const override { return m_surfaceSeed; }
    bool solidSurface() const override { return false; }
    SurfaceSample sample(const glm::dvec3& dir, double minWavelengthM) const override;
    SurfaceAppearance appearance(const SurfaceSample& sample, const glm::dvec3& dir, double slope) const override;
    double detailAmplitudeBoundM() const override { return 0.0; }
    double macroCutoffM() const override { return 0.0; }
    void landmarks(const glm::dvec3& dir, double radiusRad, std::vector<SurfaceFeature>& out) const override;

private:
    struct Storm
    {
        std::uint64_t id = 0;
        glm::dvec3 center {0.0};
        double radiusRad = 0.0;
        bool red = false;
    };
    void storms(const glm::dvec3& dir, std::vector<Storm>& out) const;
    glm::dvec3 drifted(const glm::dvec3& dir) const;

    BodySurfaceDescriptor m_body;
    bool m_iceGiant = false;
    double m_radius = 7.0e7;
    double m_epoch = 0.0;
    std::uint64_t m_surfaceSeed = 0, m_seedBands = 0, m_seedEddies = 0, m_seedStorms = 0;
    double m_jets = 6.0;
    double m_contrast = 1.0;
    glm::vec3 m_zone {0.9f}, m_belt {0.6f}, m_stormRed {0.7f, 0.35f, 0.25f};
};
} // namespace world::celestial::surface
