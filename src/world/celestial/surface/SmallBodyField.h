#pragma once

#include <vector>

#include "src/world/celestial/surface/BodySurfaceDescriptor.h"
#include "src/world/celestial/surface/SurfaceField.h"
#include "src/world/celestial/surface/SurfaceNoise.h"

namespace world::celestial::surface
{
/*
    Irregular small body (asteroid, small moon) as a radius function of the
    direction - the same cube-sphere patches as a planet, so approaching it
    only subdivides; the silhouette never changes:

      macro  triaxial ellipsoid in a seeded orientation
             x low-frequency deformation
             cut by fracture planes (smooth minimum: facets, edges)
             - large impact craters (an explicit list per body)
             - medium craters from lattice cells (stable ids)
      detail ridged grooves, derivative-damped regolith relief, small
             craters and boulders below the macro cutoff.

    Composition (carbonaceous / silicate / metallic / icy) sets albedo and
    roughness of the surface material.
*/
class SmallBodyField final : public SurfaceField
{
public:
    explicit SmallBodyField(const SmallBodyDescriptor& body);

    double referenceRadiusM() const override { return m_radius; }
    std::uint64_t identitySeed() const override { return m_seed; }
    bool solidSurface() const override { return true; }
    SurfaceSample sample(const glm::dvec3& dir, double minWavelengthM) const override;
    SurfaceAppearance appearance(const SurfaceSample& sample, const glm::dvec3& dir, double slope) const override;
    double detailAmplitudeBoundM() const override { return m_detailBoundM; }
    double macroCutoffM() const override { return m_macroCutoffM; }
    void landmarks(const glm::dvec3& dir, double radiusRad, std::vector<SurfaceFeature>& out) const override;

    const SmallBodyDescriptor& body() const { return m_body; }

private:
    struct Crater
    {
        std::uint64_t id = 0;
        glm::dvec3 center {0.0};
        double radiusRad = 0.0;
        double depth = 0.0;       // fraction of the mean radius
    };
    double macroShape(const glm::dvec3& dir) const;
    void mediumCraters(const glm::dvec3& dir, double cell, std::uint64_t seed, std::vector<Crater>& out) const;

    SmallBodyDescriptor m_body;
    double m_radius = 1000.0;
    std::uint64_t m_seed = 0;
    std::uint64_t m_seedShape = 0, m_seedPlanes = 0, m_seedCraters = 0, m_seedDetail = 0, m_seedGrooves = 0;
    glm::dmat3 m_orientation {1.0};
    glm::dvec3 m_axes {1.0};
    std::vector<glm::dvec4> m_planes;   // normal, distance (fraction of radius)
    std::vector<Crater> m_craters;
    double m_macroCutoffM = 50.0;
    double m_detailBoundM = 0.0;
    OctaveSpec m_deformSpec, m_detailSpec, m_grooveSpec;
};
} // namespace world::celestial::surface
