#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

namespace world::celestial::surface
{
/*
    A reusable impact-crater population for airless (and icy) bodies, from
    kilometres down to metres - the lunar landscape below a curated map's
    texel or a procedural field's smallest crater class.

    Classes of cells shrinking by 2x; one candidate crater per cell with a
    fixed probability, so the count per area grows as D^-2 (the cumulative
    size law of a cratered surface near equilibrium: few big craters, very
    many small ones, small ones on top of big ones). Each crater has an age:
    fresh ones are deep bowls (depth / diameter 0.2) with a sharp raised rim,
    an ejecta blanket (rim / r^3 out to 3 radii), bright ejecta and blocks on
    the rim; old ones are shallow, soft saucers. Band-limited: a class adds
    relief only where it is resolved (smooth fade), so every LOD is the same
    surface. Deterministic from the seed (identical on server and client).
*/
struct CraterSample
{
    double heightM = 0.0;       // relief added to the surface
    float freshness = 0.0f;     // fresh ejecta nearby (bright, rocky), 0..1
    float rockiness = 0.0f;     // blocks on fresh rims / walls, 0..1
    bool insideCrater = false;  // inside the bowl of the largest resolved crater here
    double craterRadiusM = 0.0;
    std::uint64_t craterId = 0;
};

class CraterLayer
{
public:
    // density 0..1: 1 = saturated lunar highlands (descriptor crater_density)
    // boulders: add the 0.5 - 6 m blocks (a field that has its own leaves them off)
    void configure(std::uint64_t seed, double bodyRadiusM, double maxCraterRadiusM, double minCraterRadiusM, double density,
                   bool boulders = true);
    bool empty() const { return m_classes.empty(); }

    CraterSample sample(const glm::dvec3& dir, double minWavelengthM) const;

private:
    struct Class
    {
        double cellM = 0.0;
        double probability = 0.0;
        std::uint64_t seed = 0;
    };
    double boulders(const glm::dvec3& dir, double minWavelengthM, double rockiness) const;

    std::vector<Class> m_classes;
    std::uint64_t m_seed = 0;
    std::uint64_t m_bouldersSeed = 0;
    double m_radius = 1.0;
    bool m_withBoulders = true;
};
} // namespace world::celestial::surface
