#pragma once

#include <cstdint>
#include <string_view>

#include "src/world/generation/GenerationRandom.h"

namespace world::celestial::surface
{
/*
    Identity and seed derivation of the procedural surface generator.

    Everything is coordinate-addressed (stateless): a value depends only on
    the generator version, the body seed and the coordinates / tag it is
    asked for - never on a shared sequential RNG, on call order or on which
    patches were generated before:

        surfaceSeed = Hash(bodySeed, "planet_surface", GeneratorVersion)
        layerSeed   = Hash(surfaceSeed, "<layer>")          macro, climate ...
        featureSeed = Hash(layerSeed, class, cellX, cellY, cellZ, candidate)
        asteroid    = Hash(beltSeed, "asteroid", cellX, cellY, cellZ, candidate)

    A new layer gets a new tag and so never shifts an existing one.
    GeneratorVersion changes only when an existing layer's output changes on
    purpose (it is written to every reproducibility record).
*/
constexpr std::uint32_t GeneratorVersion = 1;

using generation::deriveSeed;
using generation::splitMix64;

inline std::uint64_t surfaceSeed(std::uint64_t bodySeed)
{
    return deriveSeed(bodySeed, "planet_surface", GeneratorVersion);
}

inline std::uint64_t layerSeed(std::uint64_t surface, std::string_view layer)
{
    return deriveSeed(surface, layer);
}

// Hash of an integer 3D lattice point under a seed.
inline std::uint64_t latticeHash(std::uint64_t seed, std::int64_t x, std::int64_t y, std::int64_t z)
{
    std::uint64_t h = splitMix64(seed ^ (static_cast<std::uint64_t>(x) * 0x9E3779B97F4A7C15ull));
    h = splitMix64(h ^ (static_cast<std::uint64_t>(y) * 0xC2B2AE3D27D4EB4Full));
    return splitMix64(h ^ (static_cast<std::uint64_t>(z) * 0x165667B19E3779F9ull));
}

// Uniform [0, 1) from a hash (53 bits).
inline double unitFromHash(std::uint64_t h)
{
    return static_cast<double>(h >> 11) * (1.0 / 9007199254740992.0);
}

// The k-th independent uniform of a hash (k = 0, 1, 2 ...).
inline double unitFromHash(std::uint64_t h, int k)
{
    return unitFromHash(splitMix64(h + 0x9E3779B97F4A7C15ull * static_cast<std::uint64_t>(k + 1)));
}

// FNV-1a 64 accumulation for checksums of generated data.
struct Checksum
{
    std::uint64_t value = 0xCBF29CE484222325ull;

    void add(const void* data, std::size_t bytes)
    {
        const auto* p = static_cast<const unsigned char*>(data);
        for (std::size_t i = 0; i < bytes; ++i)
        {
            value ^= p[i];
            value *= 0x100000001B3ull;
        }
    }
    template <typename T>
    void add(const T& v)
    {
        add(&v, sizeof(T));
    }
};
} // namespace world::celestial::surface
