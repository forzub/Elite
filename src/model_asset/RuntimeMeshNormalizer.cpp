#include "src/model_asset/RuntimeMeshNormalizer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace elite::model_asset
{
namespace
{
struct QuantizedPosition
{
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t z = 0;

    bool operator==(const QuantizedPosition& other) const noexcept
    {
        return x == other.x && y == other.y && z == other.z;
    }
};

struct QuantizedPositionHash
{
    std::size_t operator()(const QuantizedPosition& value) const noexcept
    {
        std::size_t seed = 1469598103934665603ull;
        auto mix = [&](std::uint64_t v)
        {
            seed ^= static_cast<std::size_t>(v);
            seed *= 1099511628211ull;
        };
        mix(static_cast<std::uint64_t>(value.x));
        mix(static_cast<std::uint64_t>(value.y));
        mix(static_cast<std::uint64_t>(value.z));
        return seed;
    }
};

struct TriangleKey
{
    std::uint32_t a = 0;
    std::uint32_t b = 0;
    std::uint32_t c = 0;

    bool operator==(const TriangleKey& other) const noexcept
    {
        return a == other.a && b == other.b && c == other.c;
    }
};

struct TriangleKeyHash
{
    std::size_t operator()(const TriangleKey& value) const noexcept
    {
        std::size_t seed = 1469598103934665603ull;
        auto mix = [&](std::uint32_t v)
        {
            seed ^= static_cast<std::size_t>(v);
            seed *= 1099511628211ull;
        };
        mix(value.a); mix(value.b); mix(value.c);
        return seed;
    }
};

bool finitePosition(const glm::vec3& p)
{
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}

QuantizedPosition quantize(const glm::vec3& p, double epsilon)
{
    return {
        static_cast<std::int64_t>(std::llround(static_cast<double>(p.x) / epsilon)),
        static_cast<std::int64_t>(std::llround(static_cast<double>(p.y) / epsilon)),
        static_cast<std::int64_t>(std::llround(static_cast<double>(p.z) / epsilon))
    };
}

TriangleKey sortedTriangleKey(std::uint32_t a, std::uint32_t b, std::uint32_t c)
{
    std::array<std::uint32_t, 3> key {a, b, c};
    std::sort(key.begin(), key.end());
    return {key[0], key[1], key[2]};
}
} // namespace

RuntimeMeshNormalizationResult normalizeRuntimeMeshTopology(
    const std::vector<glm::vec3>& inputPositions,
    const std::vector<RuntimeMeshTriangleInput>& inputTriangles,
    double weldEpsilon)
{
    RuntimeMeshNormalizationResult out;
    if (!(weldEpsilon > 0.0) || !std::isfinite(weldEpsilon))
    {
        out.error = "invalid weld epsilon";
        return out;
    }
    if (inputPositions.empty())
    {
        out.error = "mesh has no positions";
        return out;
    }

    out.pointForInputVertex.resize(inputPositions.size(), 0);
    out.positions.reserve(inputPositions.size());
    out.representativeInputVertex.reserve(inputPositions.size());

    std::unordered_map<QuantizedPosition, std::uint32_t, QuantizedPositionHash> weldMap;
    weldMap.reserve(inputPositions.size() * 2u + 1u);

    for (std::size_t vi = 0; vi < inputPositions.size(); ++vi)
    {
        const auto& p = inputPositions[vi];
        if (!finitePosition(p))
        {
            out.error = "non-finite vertex position";
            return out;
        }

        const auto key = quantize(p, weldEpsilon);
        const auto found = weldMap.find(key);
        if (found != weldMap.end())
        {
            out.pointForInputVertex[vi] = found->second;
            continue;
        }

        if (out.positions.size() >= static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))
        {
            out.error = "normalized mesh exceeds 32-bit vertex indexing";
            return out;
        }
        const auto point = static_cast<std::uint32_t>(out.positions.size());
        weldMap.emplace(key, point);
        out.pointForInputVertex[vi] = point;
        out.positions.push_back(p);
        out.representativeInputVertex.push_back(static_cast<std::uint32_t>(vi));
    }

    out.triangles.reserve(inputTriangles.size());
    std::unordered_set<TriangleKey, TriangleKeyHash> uniqueTriangles;
    uniqueTriangles.reserve(inputTriangles.size() * 2u + 1u);

    for (std::size_t ti = 0; ti < inputTriangles.size(); ++ti)
    {
        const auto& input = inputTriangles[ti];
        if (input.a >= inputPositions.size() || input.b >= inputPositions.size() || input.c >= inputPositions.size())
        {
            out.error = "triangle index outside vertex array";
            return out;
        }

        const auto a = out.pointForInputVertex[input.a];
        const auto b = out.pointForInputVertex[input.b];
        const auto c = out.pointForInputVertex[input.c];
        if (a == b || b == c || c == a)
        {
            ++out.removedDegenerateTriangles;
            continue;
        }

        const glm::dvec3 pa(out.positions[a]);
        const glm::dvec3 pb(out.positions[b]);
        const glm::dvec3 pc(out.positions[c]);
        const auto ab = pb - pa;
        const auto ac = pc - pa;
        const auto bc = pc - pb;
        const double maxEdge2 = std::max({glm::dot(ab, ab), glm::dot(ac, ac), glm::dot(bc, bc)});
        const auto area = glm::cross(ab, ac);
        const double area2 = glm::dot(area, area);
        if (!(maxEdge2 > 0.0) || !std::isfinite(area2) || area2 <= maxEdge2 * maxEdge2 * 1.0e-14)
        {
            ++out.removedDegenerateTriangles;
            continue;
        }

        const auto duplicateKey = sortedTriangleKey(a, b, c);
        if (!uniqueTriangles.emplace(duplicateKey).second)
        {
            ++out.removedDuplicateTriangles;
            continue;
        }

        out.triangles.push_back({a, b, c, ti, input.sourceTag});
    }

    if (out.triangles.empty())
    {
        out.error = "mesh has no triangles after runtime normalization";
        return out;
    }

    out.success = true;
    return out;
}

} // namespace elite::model_asset
