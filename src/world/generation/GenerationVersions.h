#pragma once

#include <cstdint>

namespace world::generation
{

/*
    Version of every generator layer. A layer bumps its own version when its
    output for the same inputs changes; the other layers keep theirs, so a new
    surface generator does not move the orbits of an existing system.

    The materialised catalog stores the versions it was generated with.
*/
struct UniverseGenerationVersions
{
    std::uint32_t stellarSystem = 1;   // hierarchy, disk, architecture, orbits
    std::uint32_t celestialBody = 1;   // composition, radius, thermal, atmosphere, spin
    std::uint32_t satellites = 1;      // moons, rings, small-body reservoirs
    std::uint32_t interstellar = 4;    // 2: domain cells, background medium, density lattices; 3: extraction profiles;
                                       // 4: causal cloud formation (cavities / shells), provenance classes, icy debris fields
    std::uint32_t planetSurface = 0;   // not implemented: surface materialisation
    std::uint32_t resources = 1;       // natural resources, industrial suitability, habitability, fuel
};

inline constexpr UniverseGenerationVersions CurrentGenerationVersions {};

// The one universe seed of the shipped catalog. Changing it regenerates every
// procedural part of the universe.
inline constexpr std::uint64_t DefaultUniverseSeed = 0x454C495445303031ull; // "ELITE001"

} // namespace world::generation
