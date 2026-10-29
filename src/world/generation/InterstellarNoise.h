#pragma once

#include <algorithm>
#include <cstdint>

#include <glm/glm.hpp>

#include "src/world/generation/GenerationRandom.h"

namespace world::generation::interstellar_noise
{

// Deterministic value noise of the interstellar layer (pure functions of a
// seed and a position; shared by the medium field and the structure shapes).

inline double latticeHash01(std::uint64_t seed, long long x, long long y, long long z)
{
    const std::uint64_t h = splitMix64(
        seed ^ splitMix64(static_cast<std::uint64_t>(x) * 0x9E3779B97F4A7C15ull ^
                          splitMix64(static_cast<std::uint64_t>(y) * 0xC2B2AE3D27D4EB4Full ^
                                     splitMix64(static_cast<std::uint64_t>(z)))));
    return static_cast<double>(h >> 11) * 0x1.0p-53;
}

inline double valueNoise(std::uint64_t seed, const glm::dvec3& p)
{
    const glm::dvec3 f = glm::floor(p);
    const glm::dvec3 t = p - f;
    const glm::dvec3 w = t * t * (3.0 - 2.0 * t);
    const long long x0 = static_cast<long long>(f.x), y0 = static_cast<long long>(f.y),
                    z0 = static_cast<long long>(f.z);
    double c[2][2][2];
    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2; ++j)
            for (int k = 0; k < 2; ++k)
                c[i][j][k] = latticeHash01(seed, x0 + i, y0 + j, z0 + k);
    const auto lerp = [](double a, double b, double s) { return a + (b - a) * s; };
    const double x00 = lerp(c[0][0][0], c[1][0][0], w.x), x10 = lerp(c[0][1][0], c[1][1][0], w.x);
    const double x01 = lerp(c[0][0][1], c[1][0][1], w.x), x11 = lerp(c[0][1][1], c[1][1][1], w.x);
    return lerp(lerp(x00, x10, w.y), lerp(x01, x11, w.y), w.z);
}

// Fractal value noise in [0, 1].
inline double fbm(std::uint64_t seed, const glm::dvec3& p, int octaves)
{
    double sum = 0.0, norm = 0.0, amp = 1.0, freq = 1.0;
    for (int o = 0; o < octaves; ++o)
    {
        sum += amp * valueNoise(seed + static_cast<std::uint64_t>(o) * 0x51ED27ull, p * freq);
        norm += amp;
        amp *= 0.5;
        freq *= 2.03;
    }
    return sum / norm;
}

inline double smoothstep(double e0, double e1, double x)
{
    const double t = std::clamp((x - e0) / (e1 - e0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

} // namespace world::generation::interstellar_noise
