#pragma once

#include <cmath>
#include <cstdint>
#include <functional>

#include <glm/glm.hpp>

namespace world::celestial::surface
{
/*
    Cube-sphere addressing: 6 faces, a quadtree per face, equal-angle
    (tangent) warp so cells are near-uniform on the sphere.

        PatchKey {face, level, x, y}: level 0 = the whole face,
        x, y in [0, 2^level).

    A patch's vertices are the directions of a regular (u, v) grid inside
    its face square; neighbouring patches share their edge directions
    exactly (same formula, same arguments), across faces up to rounding.
*/
struct PatchKey
{
    int face = 0;
    int level = 0;
    int x = 0;
    int y = 0;

    bool operator==(const PatchKey& o) const
    {
        return face == o.face && level == o.level && x == o.x && y == o.y;
    }

    PatchKey parent() const
    {
        return PatchKey {face, level - 1, x >> 1, y >> 1};
    }

    PatchKey child(int i) const
    {
        return PatchKey {face, level + 1, x * 2 + (i & 1), y * 2 + (i >> 1)};
    }
};

struct PatchKeyHash
{
    std::size_t operator()(const PatchKey& k) const
    {
        return std::hash<std::uint64_t>()((static_cast<std::uint64_t>(k.face) << 58) ^
                                          (static_cast<std::uint64_t>(k.level) << 52) ^
                                          (static_cast<std::uint64_t>(k.x) << 26) ^ static_cast<std::uint64_t>(k.y));
    }
};

// Face frames: normal, u axis, v axis (right-handed, outward normal).
inline void cubeFaceFrame(int face, glm::dvec3& n, glm::dvec3& u, glm::dvec3& v)
{
    switch (face)
    {
        case 0: n = {1, 0, 0}; u = {0, 0, -1}; v = {0, 1, 0}; break;
        case 1: n = {-1, 0, 0}; u = {0, 0, 1}; v = {0, 1, 0}; break;
        case 2: n = {0, 1, 0}; u = {1, 0, 0}; v = {0, 0, -1}; break;
        case 3: n = {0, -1, 0}; u = {1, 0, 0}; v = {0, 0, 1}; break;
        case 4: n = {0, 0, 1}; u = {1, 0, 0}; v = {0, 1, 0}; break;
        default: n = {0, 0, -1}; u = {-1, 0, 0}; v = {0, 1, 0}; break;
    }
}

// (u, v) in [-1, 1] on a face -> unit direction (equal-angle warp).
inline glm::dvec3 cubeFaceDirection(int face, double u, double v)
{
    glm::dvec3 n, a, b;
    cubeFaceFrame(face, n, a, b);
    constexpr double QuarterPi = 0.78539816339744831;
    return glm::normalize(n + a * std::tan(u * QuarterPi) + b * std::tan(v * QuarterPi));
}

// Face-square coordinate of grid index i of n along a patch axis.
inline double patchCoordinate(int level, int cell, int i, int n)
{
    const double size = 2.0 / static_cast<double>(1 << level);
    return -1.0 + size * (static_cast<double>(cell) + static_cast<double>(i) / static_cast<double>(n));
}

inline glm::dvec3 patchDirection(const PatchKey& key, int i, int j, int n)
{
    return cubeFaceDirection(key.face, patchCoordinate(key.level, key.x, i, n),
                             patchCoordinate(key.level, key.y, j, n));
}

// Approximate edge length of a patch on a sphere of radius r.
inline double patchEdgeLengthM(int level, double radiusM)
{
    return radiusM * 1.5707963267948966 / static_cast<double>(1 << level);
}
} // namespace world::celestial::surface
