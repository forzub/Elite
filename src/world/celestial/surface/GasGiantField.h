#pragma once

#include <vector>

#include "src/world/celestial/surface/BodySurfaceDescriptor.h"
#include "src/world/celestial/surface/SurfaceField.h"
#include "src/world/celestial/surface/SurfaceNoise.h"

namespace world::celestial::surface
{
/*
    Gas / ice giant cloud deck. There is no ground: the "surface" is the
    visible deck at the reference radius (height 0 everywhere) and only its
    appearance has structure. Model v2 (2026-11-11, author: the v1 bands
    looked like cartoon stripes):

      band table      per hemisphere: an equatorial zone, then belts / zones
                      of UNEQUAL widths out to ~60-70 deg, each with its own
                      brightness, hue and turbulence (belts can be "rusty"),
                      the jet count from size and spin (Rhines scale);
      shear flow      the sample point is domain-warped by noise stretched
                      along the parallels, strongest at band edges (wind
                      shear): wavy edges, festoons, plumes; belts carry a
                      folded filamentary texture, finer filaments fade in
                      with the band limit (a close look adds streaks);
      vortices        ovals on band edges, elongated E-W, the field swirled
                      inside them (spiral arms, a calm core, a dark rim);
                      a large giant may carry one great red oval with a
                      turbulent wake;
      polar regions   above the last band: mottled, darker, bluer haze with
                      small cyclones instead of bands;
      classes         ice giants (CH4): few low-contrast cyan bands, bright
                      methane streaks, an occasional dark spot; hot giants
                      (> 900 K): dark alkali / silicate red-brown.

    New noise layers use new tags (gg2_*): the rocky surfaces never move.
    The weather epoch (seconds of world time) shifts the bands by their
    differential zonal drift - deterministic from (seed, epoch).
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
    struct Band
    {
        double latLo = 0.0, latHi = 0.0;   // radians
        bool zone = true;                  // light zone / dark belt
        glm::vec3 colour {0.8f};
        double turbulence = 0.5;           // 0..1
    };
    struct Vortex
    {
        std::uint64_t id = 0;
        double lat = 0.0, lon = 0.0;       // centre (drift frame), radians
        double radiusLat = 0.02;           // semi-axis across latitude (rad)
        double aspect = 2.0;               // E-W semi-axis = radiusLat * aspect
        double spin = 1.0;                 // +1 anticyclone / -1 cyclone
        glm::vec3 colour {1.0f};
        bool great = false;                // the great oval (wake)
        bool dark = false;                 // ice-giant dark spot
    };

    glm::dvec3 drifted(const glm::dvec3& dir) const;
    const Band& bandAt(double lat, double& t, const Band*& neighbour, double& edge) const;
    void shearWarp(double lat, double lon, const glm::dvec3& dir, double& lat2, double& lon2) const;
    double bandSignal(double lat) const;
    glm::vec3 deckColour(double lat, double lon, const glm::dvec3& dir, double bandLimitM, double& turbulenceOut) const;

    BodySurfaceDescriptor m_body;
    bool m_iceGiant = false;
    bool m_hot = false;
    double m_radius = 7.0e7;
    double m_epoch = 0.0;
    std::uint64_t m_surfaceSeed = 0, m_seedBands = 0, m_seedShear = 0, m_seedTexture = 0, m_seedVortices = 0,
                  m_seedPolar = 0;
    double m_jets = 6.0;
    double m_contrast = 1.0;
    double m_polarLat = 1.15;              // where the bands end (rad)
    glm::vec3 m_zone {0.9f}, m_belt {0.6f}, m_polar {0.5f};
    std::vector<Band> m_bands;             // south -> north
    std::vector<Vortex> m_vortices;
};
} // namespace world::celestial::surface
