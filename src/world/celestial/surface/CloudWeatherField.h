#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/world/celestial/surface/BodySurfaceDescriptor.h"
#include "src/world/celestial/surface/SurfaceNoise.h"

namespace world::celestial::surface
{
/*
    Cloud layers over a stable surface identity. Weather moves and changes,
    but deterministically:

        coverage = f(bodySeed, circulation model, world time)

    - the climate tendency (ITCZ, dry subtropics, storm tracks, polar
      fronts) and the number / heights / kinds of layers depend only on the
      descriptor - stable;
    - the pattern is advected by zonal winds (trade winds westward,
      westerlies eastward) and evolves through a noise dimension driven by
      world time;
    - cyclones live for one weather epoch (6 h of world time), each from a
      lattice cell of (seed, epoch) and blended into the next epoch.

    The same world time gives the same sky after a restart or a save/load;
    nothing depends on when the map was opened.
*/
struct CloudLayer
{
    std::string kind;        // cumulus | stratus | cirrus | deck | haze
    double heightM = 2000.0; // above the reference radius
    float opacity = 0.8f;
    float coverageBias = 0.0f;
};

class CloudWeatherField
{
public:
    explicit CloudWeatherField(const BodySurfaceDescriptor& body);

    const std::vector<CloudLayer>& layers() const { return m_layers; }

    // Time-independent climate tendency of cloudiness (0..1).
    float tendency(const glm::dvec3& dir) const;

    // Cloud cover of a layer at a direction and world time (0..1).
    float coverage(std::size_t layer, const glm::dvec3& dir, double worldTimeSeconds) const;

    static constexpr double EpochSeconds = 6.0 * 3600.0;

private:
    float cyclones(const glm::dvec3& dir, std::int64_t epoch) const;

    BodySurfaceDescriptor m_body;
    std::uint64_t m_seed = 0;
    std::vector<CloudLayer> m_layers;
    double m_windRadPerSecond = 0.0;
    OctaveSpec m_pattern;
};
} // namespace world::celestial::surface
