#include "tools/model_asset_editor/GeometryInstanceFitter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <vector>

#include <glm/gtc/matrix_inverse.hpp>

namespace elite::model_asset::editor
{
namespace
{
constexpr double Tiny = 1.0e-18;

bool sameIndexedTopology(const MeshLod& a, const MeshLod& b)
{
    if (a.vertices.size() != b.vertices.size() ||
        a.triangles.size() != b.triangles.size())
    {
        return false;
    }

    for (std::size_t i = 0; i < a.triangles.size(); ++i)
    {
        const auto& ta = a.triangles[i];
        const auto& tb = b.triangles[i];
        if (ta.a != tb.a || ta.b != tb.b || ta.c != tb.c)
            return false;
    }
    return true;
}

glm::dvec3 centroid(const MeshLod& lod)
{
    glm::dvec3 c(0.0);
    for (const auto& v : lod.vertices)
        c += glm::dvec3(v.position);
    return c / static_cast<double>(lod.vertices.size());
}

MeshLod uniquePositionCloud(const MeshLod& source)
{
    // OBJ expands one geometric point into several compiled vertices whenever
    // normals or UVs split at a seam. Those duplicates must not bias centroid
    // or PCA while recovering a baked instance transform. Within one imported
    // OBJ they originate from the same source position and therefore have the
    // exact same float coordinates; keep one copy of each position here.
    std::vector<glm::vec3> positions;
    positions.reserve(source.vertices.size());
    for (const auto& vertex : source.vertices)
        positions.push_back(vertex.position);

    std::sort(positions.begin(), positions.end(), [](const glm::vec3& a, const glm::vec3& b) {
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        return a.z < b.z;
    });

    MeshLod out;
    out.vertices.reserve(positions.size());
    bool havePrevious = false;
    glm::vec3 previous(0.0f);
    for (const glm::vec3& position : positions)
    {
        const bool duplicate = havePrevious &&
            position.x == previous.x &&
            position.y == previous.y &&
            position.z == previous.z;
        if (duplicate)
            continue;
        Vertex vertex;
        vertex.position = position;
        out.vertices.push_back(vertex);
        previous = position;
        havePrevious = true;
    }
    out.minBounds = source.minBounds;
    out.maxBounds = source.maxBounds;
    return out;
}

std::size_t farthestFrom(const MeshLod& lod, const glm::dvec3& point)
{
    std::size_t best = 0;
    double bestD2 = -1.0;
    for (std::size_t i = 0; i < lod.vertices.size(); ++i)
    {
        const glm::dvec3 d = glm::dvec3(lod.vertices[i].position) - point;
        const double d2 = glm::dot(d, d);
        if (d2 > bestD2)
        {
            bestD2 = d2;
            best = i;
        }
    }
    return best;
}

bool makeBasis(
    const glm::dvec3& a,
    const glm::dvec3& b,
    const glm::dvec3& c,
    glm::dmat3& out)
{
    const glm::dvec3 xRaw = b - a;
    const double x2 = glm::dot(xRaw, xRaw);
    if (x2 <= Tiny)
        return false;
    const glm::dvec3 x = xRaw / std::sqrt(x2);

    const glm::dvec3 yRaw = (c - a) - x * glm::dot(c - a, x);
    const double y2 = glm::dot(yRaw, yRaw);
    if (y2 <= Tiny)
        return false;
    const glm::dvec3 y = yRaw / std::sqrt(y2);
    const glm::dvec3 z = glm::normalize(glm::cross(x, y));
    out = glm::dmat3(x, y, z);
    return true;
}

bool chooseReferenceFrame(
    const MeshLod& reference,
    std::size_t& i0,
    std::size_t& i1,
    std::size_t& i2)
{
    const glm::dvec3 c = centroid(reference);
    i0 = farthestFrom(reference, c);
    const glm::dvec3 p0(reference.vertices[i0].position);
    i1 = farthestFrom(reference, p0);
    const glm::dvec3 p1(reference.vertices[i1].position);

    const glm::dvec3 line = p1 - p0;
    const double line2 = glm::dot(line, line);
    if (line2 <= Tiny)
        return false;

    double bestArea2 = -1.0;
    i2 = i0;
    for (std::size_t i = 0; i < reference.vertices.size(); ++i)
    {
        const glm::dvec3 d = glm::dvec3(reference.vertices[i].position) - p0;
        const glm::dvec3 perpendicular = d - line * (glm::dot(d, line) / line2);
        const double area2 = glm::dot(perpendicular, perpendicular);
        if (area2 > bestArea2)
        {
            bestArea2 = area2;
            i2 = i;
        }
    }
    return bestArea2 > Tiny;
}

double geometryScale(const GeometryDefinition& geometry)
{
    double scale = 0.0;
    for (const auto& lod : geometry.lods)
    {
        const glm::dvec3 d = glm::dvec3(lod.maxBounds) - glm::dvec3(lod.minBounds);
        scale = std::max(scale, glm::length(d));
    }
    return std::max(scale, 1.0);
}

struct CellKey
{
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t z = 0;

    bool operator==(const CellKey& other) const noexcept
    {
        return x == other.x && y == other.y && z == other.z;
    }
};

struct CellKeyHash
{
    std::size_t operator()(const CellKey& key) const noexcept
    {
        auto mix = [](std::uint64_t x)
        {
            x ^= x >> 33;
            x *= 0xff51afd7ed558ccdULL;
            x ^= x >> 33;
            x *= 0xc4ceb9fe1a85ec53ULL;
            x ^= x >> 33;
            return x;
        };
        const std::uint64_t hx = mix(static_cast<std::uint64_t>(key.x));
        const std::uint64_t hy = mix(static_cast<std::uint64_t>(key.y));
        const std::uint64_t hz = mix(static_cast<std::uint64_t>(key.z));
        return static_cast<std::size_t>(hx ^ (hy << 1) ^ (hz << 7));
    }
};

class VertexSpatialIndex
{
public:
    VertexSpatialIndex(const MeshLod& lod, double cellSize)
        : m_lod(lod), m_cellSize(std::max(cellSize, 1.0e-9))
    {
        m_cells.reserve(lod.vertices.size() * 2 + 1);
        for (std::size_t i = 0; i < lod.vertices.size(); ++i)
            m_cells[cellFor(glm::dvec3(lod.vertices[i].position))].push_back(i);
    }

    bool nearest(
        const glm::dvec3& point,
        double maxDistance,
        std::size_t& outIndex,
        double& outDistance) const
    {
        const CellKey center = cellFor(point);
        const int radius = std::max(1, static_cast<int>(std::ceil(maxDistance / m_cellSize)));
        const double maxD2 = maxDistance * maxDistance;
        double bestD2 = maxD2;
        bool found = false;
        std::size_t bestIndex = 0;

        for (int dz = -radius; dz <= radius; ++dz)
        {
            for (int dy = -radius; dy <= radius; ++dy)
            {
                for (int dx = -radius; dx <= radius; ++dx)
                {
                    const CellKey key {
                        center.x + dx,
                        center.y + dy,
                        center.z + dz
                    };
                    const auto it = m_cells.find(key);
                    if (it == m_cells.end())
                        continue;
                    for (const std::size_t index : it->second)
                    {
                        const glm::dvec3 d = glm::dvec3(m_lod.vertices[index].position) - point;
                        const double d2 = glm::dot(d, d);
                        if (d2 <= bestD2)
                        {
                            bestD2 = d2;
                            bestIndex = index;
                            found = true;
                        }
                    }
                }
            }
        }

        if (!found)
            return false;
        outIndex = bestIndex;
        outDistance = std::sqrt(bestD2);
        return true;
    }

private:
    CellKey cellFor(const glm::dvec3& point) const
    {
        return {
            static_cast<std::int64_t>(std::floor(point.x / m_cellSize)),
            static_cast<std::int64_t>(std::floor(point.y / m_cellSize)),
            static_cast<std::int64_t>(std::floor(point.z / m_cellSize))
        };
    }

    const MeshLod& m_lod;
    double m_cellSize = 1.0;
    std::unordered_map<CellKey, std::vector<std::size_t>, CellKeyHash> m_cells;
};

struct PrincipalFrame
{
    glm::dmat3 basis {1.0};
    glm::dvec3 eigenvalues {0.0};
    bool valid = false;
};

PrincipalFrame principalFrame(const MeshLod& lod)
{
    PrincipalFrame out;
    if (lod.vertices.size() < 3)
        return out;

    const glm::dvec3 center = centroid(lod);
    double a[3][3] = {};
    for (const auto& vertex : lod.vertices)
    {
        const glm::dvec3 d = glm::dvec3(vertex.position) - center;
        a[0][0] += d.x * d.x;
        a[0][1] += d.x * d.y;
        a[0][2] += d.x * d.z;
        a[1][1] += d.y * d.y;
        a[1][2] += d.y * d.z;
        a[2][2] += d.z * d.z;
    }
    a[1][0] = a[0][1];
    a[2][0] = a[0][2];
    a[2][1] = a[1][2];

    double v[3][3] = {
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {0.0, 0.0, 1.0}
    };

    const double trace = std::max(Tiny, a[0][0] + a[1][1] + a[2][2]);
    for (int iteration = 0; iteration < 48; ++iteration)
    {
        int p = 0;
        int q = 1;
        double largest = std::abs(a[0][1]);
        if (std::abs(a[0][2]) > largest)
        {
            p = 0;
            q = 2;
            largest = std::abs(a[0][2]);
        }
        if (std::abs(a[1][2]) > largest)
        {
            p = 1;
            q = 2;
            largest = std::abs(a[1][2]);
        }
        if (largest <= trace * 1.0e-14)
            break;

        const double phi = 0.5 * std::atan2(2.0 * a[p][q], a[q][q] - a[p][p]);
        const double c = std::cos(phi);
        const double s = std::sin(phi);

        const double app = c * c * a[p][p] - 2.0 * s * c * a[p][q] + s * s * a[q][q];
        const double aqq = s * s * a[p][p] + 2.0 * s * c * a[p][q] + c * c * a[q][q];
        for (int r = 0; r < 3; ++r)
        {
            if (r == p || r == q)
                continue;
            const double arp = c * a[r][p] - s * a[r][q];
            const double arq = s * a[r][p] + c * a[r][q];
            a[r][p] = a[p][r] = arp;
            a[r][q] = a[q][r] = arq;
        }
        a[p][p] = app;
        a[q][q] = aqq;
        a[p][q] = a[q][p] = 0.0;

        for (int r = 0; r < 3; ++r)
        {
            const double vrp = v[r][p];
            const double vrq = v[r][q];
            v[r][p] = c * vrp - s * vrq;
            v[r][q] = s * vrp + c * vrq;
        }
    }

    std::array<int, 3> order {0, 1, 2};
    std::sort(order.begin(), order.end(), [&](int lhs, int rhs) {
        return a[lhs][lhs] > a[rhs][rhs];
    });

    for (int column = 0; column < 3; ++column)
    {
        const int source = order[column];
        glm::dvec3 axis(v[0][source], v[1][source], v[2][source]);
        const double axis2 = glm::dot(axis, axis);
        if (!std::isfinite(axis2) || axis2 <= Tiny)
            return PrincipalFrame{};
        axis /= std::sqrt(axis2);
        out.basis[column] = axis;
        out.eigenvalues[column] = a[source][source];
    }

    out.valid = true;
    return out;
}

struct CandidateTransform
{
    glm::dmat3 rotation {1.0};
    glm::dvec3 translation {0.0};
    double score = std::numeric_limits<double>::infinity();
};

CandidateTransform coarsePointCloudFit(
    const MeshLod& reference,
    const MeshLod& target,
    double scale)
{
    CandidateTransform best;
    const PrincipalFrame frameA = principalFrame(reference);
    const PrincipalFrame frameB = principalFrame(target);
    if (!frameA.valid || !frameB.valid)
        return best;

    const glm::dvec3 centerA = centroid(reference);
    const glm::dvec3 centerB = centroid(target);
    const double coarseRadius = std::max(0.01, scale * 5.0e-4);
    const VertexSpatialIndex targetIndex(target, coarseRadius);

    constexpr std::array<std::array<int, 3>, 6> permutations {{
        {{0, 1, 2}},
        {{0, 2, 1}},
        {{1, 0, 2}},
        {{1, 2, 0}},
        {{2, 0, 1}},
        {{2, 1, 0}}
    }};

    const std::size_t sampleCount = std::min<std::size_t>(2048, reference.vertices.size());
    const std::size_t stride = std::max<std::size_t>(1, reference.vertices.size() / sampleCount);

    for (const auto& permutation : permutations)
    {
        for (int signBits = 0; signBits < 8; ++signBits)
        {
            glm::dmat3 remap(0.0);
            for (int column = 0; column < 3; ++column)
            {
                glm::dvec3 axis(0.0);
                axis[permutation[column]] = (signBits & (1 << column)) ? -1.0 : 1.0;
                remap[column] = axis;
            }

            const glm::dmat3 rotation = frameB.basis * remap * glm::transpose(frameA.basis);
            const double det = glm::determinant(rotation);
            if (!std::isfinite(det) || std::abs(det - 1.0) > 1.0e-5)
                continue;
            const glm::dvec3 translation = centerB - rotation * centerA;

            double sum2 = 0.0;
            std::size_t matched = 0;
            for (std::size_t i = 0; i < reference.vertices.size() && matched < sampleCount; i += stride)
            {
                const glm::dvec3 predicted = rotation * glm::dvec3(reference.vertices[i].position) + translation;
                std::size_t targetVertex = 0;
                double distance = 0.0;
                if (!targetIndex.nearest(predicted, coarseRadius, targetVertex, distance))
                {
                    sum2 = std::numeric_limits<double>::infinity();
                    break;
                }
                sum2 += distance * distance;
                ++matched;
            }

            if (!matched || !std::isfinite(sum2))
                continue;
            const double score = std::sqrt(sum2 / static_cast<double>(matched));
            if (score < best.score)
            {
                best.rotation = rotation;
                best.translation = translation;
                best.score = score;
            }
        }
    }

    return best;
}


CandidateTransform invariantLandmarkFit(
    const MeshLod& reference,
    const MeshLod& target,
    double scale)
{
    CandidateTransform best;
    if (reference.vertices.size() < 3 || target.vertices.size() < 3)
        return best;

    std::size_t s0 = 0, s1 = 0, s2 = 0;
    if (!chooseReferenceFrame(reference, s0, s1, s2))
        return best;

    const glm::dvec3 centerA = centroid(reference);
    const glm::dvec3 centerB = centroid(target);
    const glm::dvec3 p0(reference.vertices[s0].position);
    const glm::dvec3 p1(reference.vertices[s1].position);
    const glm::dvec3 p2(reference.vertices[s2].position);

    const double r0 = glm::length(p0 - centerA);
    const double r1 = glm::length(p1 - centerA);
    const double r2 = glm::length(p2 - centerA);
    const double d01 = glm::length(p1 - p0);
    const double d02 = glm::length(p2 - p0);
    const double d12 = glm::length(p2 - p1);

    // These are rotation/translation invariants. Unlike PCA eigenvectors they
    // remain well-defined when two principal moments are equal or nearly equal
    // (common for radial station sectors and symmetric ship parts).
    const double invariantTolerance = std::max(1.0e-4, scale * 5.0e-5);
    const double coarseRadius = std::max(0.002, scale * 2.0e-4);
    const VertexSpatialIndex targetIndex(target, coarseRadius);

    auto radialCandidates = [&](double wanted, std::size_t maxCount)
    {
        std::vector<std::pair<double, std::size_t>> ranked;
        ranked.reserve(64);
        for (std::size_t i = 0; i < target.vertices.size(); ++i)
        {
            const double radius = glm::length(
                glm::dvec3(target.vertices[i].position) - centerB);
            const double error = std::abs(radius - wanted);
            if (error <= invariantTolerance * 4.0)
                ranked.emplace_back(error, i);
        }
        std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });
        if (ranked.size() > maxCount)
            ranked.resize(maxCount);
        std::vector<std::size_t> out;
        out.reserve(ranked.size());
        for (const auto& item : ranked)
            out.push_back(item.second);
        return out;
    };

    const auto t0Candidates = radialCandidates(r0, 24);
    if (t0Candidates.empty())
        return best;

    glm::dmat3 basisA(1.0);
    if (!makeBasis(p0, p1, p2, basisA))
        return best;

    const std::size_t sampleCount = std::min<std::size_t>(2048, reference.vertices.size());
    const std::size_t stride = std::max<std::size_t>(1, reference.vertices.size() / sampleCount);

    for (const std::size_t t0 : t0Candidates)
    {
        const glm::dvec3 q0(target.vertices[t0].position);

        std::vector<std::pair<double, std::size_t>> rankedT1;
        rankedT1.reserve(64);
        for (std::size_t i = 0; i < target.vertices.size(); ++i)
        {
            if (i == t0)
                continue;
            const glm::dvec3 q(target.vertices[i].position);
            const double radialError = std::abs(glm::length(q - centerB) - r1);
            const double distanceError = std::abs(glm::length(q - q0) - d01);
            if (radialError > invariantTolerance * 4.0 ||
                distanceError > invariantTolerance * 4.0)
            {
                continue;
            }
            rankedT1.emplace_back(radialError + distanceError, i);
        }
        std::sort(rankedT1.begin(), rankedT1.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });
        if (rankedT1.size() > 24)
            rankedT1.resize(24);

        for (const auto& t1Candidate : rankedT1)
        {
            const std::size_t t1 = t1Candidate.second;
            const glm::dvec3 q1(target.vertices[t1].position);

            std::vector<std::pair<double, std::size_t>> rankedT2;
            rankedT2.reserve(64);
            for (std::size_t t2 = 0; t2 < target.vertices.size(); ++t2)
            {
                if (t2 == t0 || t2 == t1)
                    continue;
                const glm::dvec3 q2(target.vertices[t2].position);
                const double radialError = std::abs(glm::length(q2 - centerB) - r2);
                const double d02Error = std::abs(glm::length(q2 - q0) - d02);
                const double d12Error = std::abs(glm::length(q2 - q1) - d12);
                if (radialError > invariantTolerance * 4.0 ||
                    d02Error > invariantTolerance * 4.0 ||
                    d12Error > invariantTolerance * 4.0)
                {
                    continue;
                }
                rankedT2.emplace_back(radialError + d02Error + d12Error, t2);
            }
            std::sort(rankedT2.begin(), rankedT2.end(), [](const auto& a, const auto& b) {
                return a.first < b.first;
            });
            if (rankedT2.size() > 24)
                rankedT2.resize(24);

            for (const auto& t2Candidate : rankedT2)
            {
                const std::size_t t2 = t2Candidate.second;
                const glm::dvec3 q2(target.vertices[t2].position);
                glm::dmat3 basisB(1.0);
                if (!makeBasis(q0, q1, q2, basisB))
                    continue;
                const glm::dmat3 rotation = basisB * glm::transpose(basisA);
                const double det = glm::determinant(rotation);
                if (!std::isfinite(det) || std::abs(det - 1.0) > 1.0e-5)
                    continue;
                const glm::dvec3 translation = centerB - rotation * centerA;

                double sum2 = 0.0;
                std::size_t matched = 0;
                for (std::size_t i = 0;
                     i < reference.vertices.size() && matched < sampleCount;
                     i += stride)
                {
                    const glm::dvec3 predicted =
                        rotation * glm::dvec3(reference.vertices[i].position) + translation;
                    std::size_t nearestIndex = 0;
                    double distance = 0.0;
                    if (!targetIndex.nearest(
                            predicted, coarseRadius, nearestIndex, distance))
                    {
                        sum2 = std::numeric_limits<double>::infinity();
                        break;
                    }
                    sum2 += distance * distance;
                    ++matched;
                }

                if (matched && std::isfinite(sum2))
                {
                    const double score = std::sqrt(sum2 / static_cast<double>(matched));
                    if (score < best.score)
                    {
                        best.rotation = rotation;
                        best.translation = translation;
                        best.score = score;
                    }
                }

            }
        }
    }

    return best;
}

bool refineFromLandmarks(
    const MeshLod& reference,
    const MeshLod& target,
    const CandidateTransform& coarse,
    double scale,
    glm::dmat3& outRotation,
    glm::dvec3& outTranslation)
{
    if (!std::isfinite(coarse.score))
        return false;

    std::size_t i0 = 0, i1 = 0, i2 = 0;
    if (!chooseReferenceFrame(reference, i0, i1, i2))
        return false;

    const double radius = std::max(0.02, scale * 1.0e-3);
    const VertexSpatialIndex targetIndex(target, radius);
    const std::array<std::size_t, 3> source {{i0, i1, i2}};
    std::array<std::size_t, 3> match {{0, 0, 0}};
    for (int k = 0; k < 3; ++k)
    {
        const glm::dvec3 predicted =
            coarse.rotation * glm::dvec3(reference.vertices[source[k]].position) +
            coarse.translation;
        double distance = 0.0;
        if (!targetIndex.nearest(predicted, radius, match[k], distance))
            return false;
    }
    if (match[0] == match[1] || match[0] == match[2] || match[1] == match[2])
        return false;

    glm::dmat3 basisA(1.0), basisB(1.0);
    if (!makeBasis(
            glm::dvec3(reference.vertices[source[0]].position),
            glm::dvec3(reference.vertices[source[1]].position),
            glm::dvec3(reference.vertices[source[2]].position),
            basisA) ||
        !makeBasis(
            glm::dvec3(target.vertices[match[0]].position),
            glm::dvec3(target.vertices[match[1]].position),
            glm::dvec3(target.vertices[match[2]].position),
            basisB))
    {
        return false;
    }

    const glm::dmat3 rotation = basisB * glm::transpose(basisA);
    const double det = glm::determinant(rotation);
    if (!std::isfinite(det) || std::abs(det - 1.0) > 1.0e-5)
        return false;

    outRotation = rotation;
    outTranslation = centroid(target) - rotation * centroid(reference);
    return true;
}

double triangleArea(const MeshLod& lod, const Triangle& triangle)
{
    if (triangle.a >= lod.vertices.size() ||
        triangle.b >= lod.vertices.size() ||
        triangle.c >= lod.vertices.size())
    {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const glm::dvec3 a(lod.vertices[triangle.a].position);
    const glm::dvec3 b(lod.vertices[triangle.b].position);
    const glm::dvec3 c(lod.vertices[triangle.c].position);
    return 0.5 * glm::length(glm::cross(b - a, c - a));
}

bool compatibleMaterialAllocation(const MeshLod& reference, const MeshLod& target)
{
    // Material identity is semantic; absolute tessellated area is not. Legacy
    // station copies were exported/decimated independently, so two instances of
    // the same part can have slightly different triangle counts and total LOD
    // surface area even though every face belongs to the same material slot.
    //
    // Compare the set of material ids and each material's *fraction* of the LOD
    // surface. This still rejects a genuinely different paint/material layout,
    // but does not mistake harmless LOD decimation drift for another material.
    std::unordered_map<std::int32_t, double> areaA;
    std::unordered_map<std::int32_t, double> areaB;
    double totalA = 0.0;
    double totalB = 0.0;

    for (const auto& triangle : reference.triangles)
    {
        const double area = triangleArea(reference, triangle);
        if (!std::isfinite(area))
            return false;
        areaA[triangle.materialIndex] += area;
        totalA += area;
    }
    for (const auto& triangle : target.triangles)
    {
        const double area = triangleArea(target, triangle);
        if (!std::isfinite(area))
            return false;
        areaB[triangle.materialIndex] += area;
        totalB += area;
    }

    if (areaA.size() != areaB.size() || totalA <= Tiny || totalB <= Tiny)
        return false;

    // Half a percentage point of the complete surface is deliberately much
    // larger than float/export noise, yet small enough to catch a real change
    // in which part of a mesh uses a material. Single-material meshes therefore
    // compare as exactly 100%/100% regardless of harmless LOD area drift.
    constexpr double MaterialFractionTolerance = 5.0e-3;
    for (const auto& [material, area] : areaA)
    {
        const auto it = areaB.find(material);
        if (it == areaB.end())
            return false;
        const double fractionA = area / totalA;
        const double fractionB = it->second / totalB;
        if (std::abs(fractionA - fractionB) > MaterialFractionTolerance)
            return false;
    }
    return true;
}

struct ValidationStats
{
    bool valid = false;
    double sumPositionError2 = 0.0;
    double maxPositionError = 0.0;
    double maxNormalError = 0.0;
    double maxUvError = 0.0;
    std::size_t comparedVertices = 0;
};

ValidationStats validateMappedVertices(
    const MeshLod& reference,
    const MeshLod& target,
    const glm::dmat3& rotation,
    const glm::dvec3& translation,
    double tolerance)
{
    ValidationStats out;
    const double cellSize = std::max(tolerance * 2.0, 1.0e-5);
    const double searchRadius = std::max(tolerance * 2.5, 2.0e-5);
    const VertexSpatialIndex targetIndex(target, cellSize);

    for (const auto& vertex : reference.vertices)
    {
        const glm::dvec3 predicted = rotation * glm::dvec3(vertex.position) + translation;
        std::size_t bestIndex = 0;
        double positionError = 0.0;
        if (!targetIndex.nearest(predicted, searchRadius, bestIndex, positionError))
            return out;

        // Position is authoritative for topology-independent matching. Normals
        // and UVs are checked as appearance guards; they may have duplicated
        // vertices at the same position, so use the nearest candidate returned
        // by the spatial index rather than assuming a shared vertex number.
        const glm::dvec3 expectedNormal = rotation * glm::dvec3(vertex.normal);
        const glm::dvec3 actualNormal(target.vertices[bestIndex].normal);
        const double normalError = glm::length(expectedNormal - actualNormal);
        const glm::dvec2 uvDelta = glm::dvec2(vertex.uv) - glm::dvec2(target.vertices[bestIndex].uv);
        const double uvError = glm::length(uvDelta);

        out.sumPositionError2 += positionError * positionError;
        out.maxPositionError = std::max(out.maxPositionError, positionError);
        out.maxNormalError = std::max(out.maxNormalError, normalError);
        out.maxUvError = std::max(out.maxUvError, uvError);
        ++out.comparedVertices;
    }

    // Different OBJ export order can choose another duplicate vertex at the
    // same position. Do not reject solely on normal/UV ambiguity here; the
    // material-area guard plus two-way point-cloud validation prevents a merely
    // similar mesh from being consolidated.
    out.valid = out.maxPositionError <= tolerance;
    return out;
}

ValidationStats validateBidirectionalPointCloud(
    const MeshLod& reference,
    const MeshLod& target,
    const glm::dmat3& rotation,
    const glm::dvec3& translation,
    double tolerance)
{
    ValidationStats forward = validateMappedVertices(
        reference, target, rotation, translation, tolerance);
    if (!forward.valid)
        return forward;

    const glm::dmat3 inverseRotation = glm::transpose(rotation);
    const glm::dvec3 inverseTranslation = -inverseRotation * translation;
    ValidationStats reverse = validateMappedVertices(
        target, reference, inverseRotation, inverseTranslation, tolerance);
    if (!reverse.valid)
        return reverse;

    forward.sumPositionError2 += reverse.sumPositionError2;
    forward.maxPositionError = std::max(forward.maxPositionError, reverse.maxPositionError);
    forward.maxNormalError = std::max(forward.maxNormalError, reverse.maxNormalError);
    forward.maxUvError = std::max(forward.maxUvError, reverse.maxUvError);
    forward.comparedVertices += reverse.comparedVertices;
    forward.valid = true;
    return forward;
}

} // namespace

GeometryInstanceFit fitGeometryAsRigidInstance(
    const GeometryDefinition& reference,
    const GeometryDefinition& target)
{
    GeometryInstanceFit out;
    if (reference.lods.empty() || target.lods.empty())
    {
        out.message = "reference/target geometry has no LOD data";
        return out;
    }
    // First prove geometric identity. Material allocation is an appearance
    // contract and must not prevent us from discovering whether two legacy
    // OBJ files are the same rigid shape. Exporters may split/reorder faces
    // differently, and old station parts may also carry independently named
    // material slots. We diagnose that only after one R+T has aligned and
    // validated the complete surface.
    const MeshLod& a0 = reference.lods.front();
    const MeshLod& b0 = target.lods.front();
    if (a0.vertices.size() < 3)
    {
        out.message = "geometry has fewer than three vertices";
        return out;
    }

    glm::dmat3 rotation(1.0);
    glm::dvec3 translation(0.0);
    const bool orderedFastPath = sameIndexedTopology(a0, b0);

    if (orderedFastPath)
    {
        std::size_t i0 = 0, i1 = 0, i2 = 0;
        if (!chooseReferenceFrame(a0, i0, i1, i2))
        {
            out.message = "geometry is degenerate/collinear; rigid instance frame cannot be recovered";
            return out;
        }

        glm::dmat3 basisA(1.0), basisB(1.0);
        if (!makeBasis(
                glm::dvec3(a0.vertices[i0].position),
                glm::dvec3(a0.vertices[i1].position),
                glm::dvec3(a0.vertices[i2].position),
                basisA) ||
            !makeBasis(
                glm::dvec3(b0.vertices[i0].position),
                glm::dvec3(b0.vertices[i1].position),
                glm::dvec3(b0.vertices[i2].position),
                basisB))
        {
            out.message = "matching vertices do not define a stable rigid frame";
            return out;
        }
        rotation = basisB * glm::transpose(basisA);
        translation = centroid(b0) - rotation * centroid(a0);
    }
    else
    {
        const double scale = std::max(geometryScale(reference), geometryScale(target));
        const MeshLod referenceCloud = uniquePositionCloud(a0);
        const MeshLod targetCloud = uniquePositionCloud(b0);
        if (referenceCloud.vertices.size() < 3 || targetCloud.vertices.size() < 3)
        {
            out.message = "geometry has fewer than three unique positions";
            return out;
        }

        CandidateTransform coarse = coarsePointCloudFit(
            referenceCloud, targetCloud, scale);
        if (!std::isfinite(coarse.score))
        {
            coarse = invariantLandmarkFit(referenceCloud, targetCloud, scale);
        }
        if (!std::isfinite(coarse.score))
        {
            out.message =
                "topology/order differs and no rigid LOD0 point-cloud alignment was found";
            return out;
        }
        rotation = coarse.rotation;
        translation = coarse.translation;

        glm::dmat3 refinedRotation(1.0);
        glm::dvec3 refinedTranslation(0.0);
        if (refineFromLandmarks(
                referenceCloud,
                targetCloud,
                coarse,
                scale,
                refinedRotation,
                refinedTranslation))
        {
            rotation = refinedRotation;
            translation = refinedTranslation;
        }
    }

    const double det = glm::determinant(rotation);
    if (!std::isfinite(det) || std::abs(det - 1.0) > 1.0e-5)
    {
        out.message = "candidate transform is not a proper rotation";
        return out;
    }

    const double scale = std::max(geometryScale(reference), geometryScale(target));
    const double tolerance = std::max(0.001, scale * 2.0e-5);
    double sum2 = 0.0;
    double maxError = 0.0;
    std::size_t count = 0;

    // LODs are independent authored products. Instance identity is established
    // from canonical LOD0 only; LOD1/LOD2 may have completely different
    // decimation/topology. Once nodes share one GeometryDefinition they also
    // intentionally share the reference geometry's LOD set.
    const MeshLod referenceLod0Cloud = uniquePositionCloud(a0);
    const MeshLod targetLod0Cloud = uniquePositionCloud(b0);
    const ValidationStats stats = validateBidirectionalPointCloud(
        referenceLod0Cloud,
        targetLod0Cloud,
        rotation,
        translation,
        tolerance);
    if (!stats.valid)
    {
        out.message = orderedFastPath
            ? "LOD0 meshes have matching indexed topology but are not the same geometry under one rigid transform"
            : "LOD0 point clouds do not match under one rigid transform";
        return out;
    }
    sum2 += stats.sumPositionError2;
    maxError = std::max(maxError, stats.maxPositionError);
    count += stats.comparedVertices;

    const double rms = count ? std::sqrt(sum2 / static_cast<double>(count)) : 0.0;
    out.rotation = glm::mat3(rotation);
    out.translation = glm::vec3(translation);
    out.rmsErrorMeters = static_cast<float>(rms);
    out.maxErrorMeters = static_cast<float>(maxError);
    out.toleranceMeters = static_cast<float>(tolerance);
    out.comparedVertices = count;
    if (!std::isfinite(rms) || !std::isfinite(maxError) || maxError > tolerance)
    {
        out.message = "geometry fit exceeded rigid-instance tolerance";
        return out;
    }

    out.geometryMatched = true;
    out.materialCompatible = compatibleMaterialAllocation(a0, b0);
    if (!out.materialCompatible)
    {
        out.message =
            "LOD0 geometry matches under one rigid transform, but LOD0 material allocation differs; "
            "sharing GeometryDefinition would currently change appearance";
        return out;
    }

    out.valid = true;
    out.message = orderedFastPath
        ? "rigid instance fit accepted (indexed fast path)"
        : "rigid instance fit accepted (topology-independent point-cloud match)";
    return out;
}

} // namespace elite::model_asset::editor
