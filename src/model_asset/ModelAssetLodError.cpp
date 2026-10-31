#include "src/model_asset/ModelAssetLodError.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <map>
#include <tuple>
#include <vector>

#include <glm/glm.hpp>

#include "src/model_asset/ModelAssetPresentation.h"

namespace elite::model_asset
{
namespace
{

struct Tri
{
    glm::vec3 a, b, c;
};

// Ericson, Real-Time Collision Detection 5.1.5.
glm::vec3 closestOnTriangle(const glm::vec3& p, const Tri& t)
{
    const glm::vec3 ab = t.b - t.a, ac = t.c - t.a, ap = p - t.a;
    const float d1 = glm::dot(ab, ap), d2 = glm::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return t.a;
    const glm::vec3 bp = p - t.b;
    const float d3 = glm::dot(ab, bp), d4 = glm::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return t.b;
    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) return t.a + ab * (d1 / (d1 - d3));
    const glm::vec3 cp = p - t.c;
    const float d5 = glm::dot(ab, cp), d6 = glm::dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) return t.c;
    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) return t.a + ac * (d2 / (d2 - d6));
    const float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
        return t.b + (t.c - t.b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
    const float denom = 1.0f / (va + vb + vc);
    return t.a + ab * (vb * denom) + ac * (vc * denom);
}

// Bounding-volume hierarchy over triangles; nearest-surface queries prune by
// box distance, and stop early once a triangle within `enough` is found (the
// sample cannot raise the running maximum any more).
class TriangleBvh
{
public:
    explicit TriangleBvh(const std::vector<Tri>& tris) : m_tris(tris)
    {
        m_order.resize(tris.size());
        std::vector<glm::vec3> centroid(tris.size());
        for (std::uint32_t i = 0; i < tris.size(); ++i)
        {
            m_order[i] = i;
            centroid[i] = (tris[i].a + tris[i].b + tris[i].c) / 3.0f;
        }
        if (tris.empty()) return;
        m_nodes.reserve(tris.size() / 2 + 2);
        m_nodes.resize(1);
        build(0, 0, static_cast<std::uint32_t>(tris.size()), centroid);
    }

    float nearest(const glm::vec3& p, float enough) const
    {
        if (m_nodes.empty()) return std::numeric_limits<float>::max();
        float best2 = std::numeric_limits<float>::max();
        const float enough2 = enough * enough;
        std::uint32_t stack[128];
        int top = 0;
        stack[top++] = 0;
        while (top > 0)
        {
            const Node& node = m_nodes[stack[--top]];
            if (boxDistance2(p, node) >= best2) continue;
            if (node.count > 0)
            {
                for (std::uint32_t i = node.first; i < node.first + node.count; ++i)
                {
                    const glm::vec3 d = closestOnTriangle(p, m_tris[m_order[i]]) - p;
                    best2 = std::min(best2, glm::dot(d, d));
                }
                if (best2 <= enough2) break;
                continue;
            }
            const std::uint32_t a = node.first, b = node.first + 1;
            const bool aNear = boxDistance2(p, m_nodes[a]) <= boxDistance2(p, m_nodes[b]);
            if (top + 2 > 128) continue;
            stack[top++] = aNear ? b : a; // far first, near popped next
            stack[top++] = aNear ? a : b;
        }
        return std::sqrt(best2);
    }

private:
    struct Node
    {
        glm::vec3 mn, mx;
        std::uint32_t first = 0; // leaf: first triangle in m_order; inner: left child (right = +1)
        std::uint32_t count = 0; // 0 = inner node
    };

    static float boxDistance2(const glm::vec3& p, const Node& n)
    {
        const glm::vec3 d = glm::max(glm::max(n.mn - p, p - n.mx), glm::vec3(0.0f));
        return glm::dot(d, d);
    }

    // Builds the subtree of triangles [first, first + count) into m_nodes[slot];
    // the two children of an inner node are a consecutive pair.
    void build(std::uint32_t slot, std::uint32_t first, std::uint32_t count, const std::vector<glm::vec3>& centroid)
    {
        glm::vec3 mn(std::numeric_limits<float>::max()), mx(-std::numeric_limits<float>::max());
        glm::vec3 cmn = mn, cmx = mx;
        for (std::uint32_t i = first; i < first + count; ++i)
        {
            const Tri& t = m_tris[m_order[i]];
            mn = glm::min(mn, glm::min(t.a, glm::min(t.b, t.c)));
            mx = glm::max(mx, glm::max(t.a, glm::max(t.b, t.c)));
            cmn = glm::min(cmn, centroid[m_order[i]]);
            cmx = glm::max(cmx, centroid[m_order[i]]);
        }
        m_nodes[slot].mn = mn;
        m_nodes[slot].mx = mx;
        if (count <= 4)
        {
            m_nodes[slot].first = first;
            m_nodes[slot].count = count;
            return;
        }
        const glm::vec3 extent = cmx - cmn;
        const int axis = extent.x >= extent.y && extent.x >= extent.z ? 0 : (extent.y >= extent.z ? 1 : 2);
        const std::uint32_t half = count / 2;
        std::nth_element(m_order.begin() + first, m_order.begin() + first + half, m_order.begin() + first + count,
                         [&](std::uint32_t l, std::uint32_t r) { return centroid[l][axis] < centroid[r][axis]; });
        const std::uint32_t left = static_cast<std::uint32_t>(m_nodes.size());
        m_nodes.resize(m_nodes.size() + 2);
        m_nodes[slot].first = left;
        m_nodes[slot].count = 0;
        build(left, first, half, centroid);
        build(left + 1, first + half, count - half, centroid);
    }

    const std::vector<Tri>& m_tris;
    std::vector<std::uint32_t> m_order;
    std::vector<Node> m_nodes;
};

// Triangles of a node's render nodes in one LOD, in asset space, optionally
// pre-multiplied (to compare two single-geometry parts in local space).
std::vector<Tri> gatherTriangles(const ModelAsset& asset, const presentation::Index& index, std::size_t lod,
                                 const std::vector<std::int32_t>& renderNodes, const glm::mat4& pre)
{
    std::vector<Tri> out;
    const RenderLod& rl = asset.renderLods[lod];
    for (const std::int32_t rn : renderNodes)
    {
        const RenderNode& node = rl.nodes[static_cast<std::size_t>(rn)];
        const MeshLod& mesh = rl.geometries[static_cast<std::size_t>(node.geometryIndex)].mesh;
        const glm::mat4 m = pre * index.renderNodes[lod][static_cast<std::size_t>(rn)].assetTransform;
        for (const auto& t : mesh.triangles)
        {
            if (t.a >= mesh.vertices.size() || t.b >= mesh.vertices.size() || t.c >= mesh.vertices.size()) continue;
            out.push_back(Tri {glm::vec3(m * glm::vec4(mesh.vertices[t.a].position, 1.0f)),
                               glm::vec3(m * glm::vec4(mesh.vertices[t.b].position, 1.0f)),
                               glm::vec3(m * glm::vec4(mesh.vertices[t.c].position, 1.0f))});
        }
    }
    return out;
}

// Max over samples of `from` (vertices + centroids) of the distance to `to`.
float directedDistance(const std::vector<Tri>& from, const TriangleBvh& to)
{
    float worst = 0.0f;
    for (const auto& t : from)
    {
        worst = std::max(worst, to.nearest(t.a, worst));
        worst = std::max(worst, to.nearest((t.a + t.b + t.c) / 3.0f, worst));
    }
    return worst;
}

bool identical(const std::vector<Tri>& a, const std::vector<Tri>& b)
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i].a != b[i].a || a[i].b != b[i].b || a[i].c != b[i].c) return false;
    return true;
}

float twoSided(const std::vector<Tri>& a, const std::vector<Tri>& b)
{
    if (a.empty() || b.empty()) return -1.0f;
    if (identical(a, b)) return 0.0f; // part untouched by this LOD
    const TriangleBvh gridA(a), gridB(b);
    return std::max(directedDistance(a, gridB), directedDistance(b, gridA));
}

bool nearIdentity(const glm::mat4& m)
{
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
        {
            // rotation within 1e-4, translation within 1 cm
            const float tolerance = c == 3 && r < 3 ? 1.0e-2f : 1.0e-4f;
            if (std::abs(m[c][r] - (c == r ? 1.0f : 0.0f)) > tolerance) return false;
        }
    return true;
}

} // namespace

std::string measureNodeLodErrors(ModelAsset& asset)
{
    const presentation::Index index = presentation::buildIndex(asset);
    if (index.componentLods.empty()) return "no component LODs";
    const std::size_t lod0 = static_cast<std::size_t>(index.componentLods.front());
    const std::size_t nodeCount = asset.nodes.size();

    // Shared geometry pairs (instances) are measured once.
    std::map<std::tuple<std::int32_t, std::size_t, std::int32_t>, float> cache;
    std::vector<float> previous(nodeCount, 0.0f);
    std::size_t measured = 0, reused = 0;
    char line[160];
    std::string summary;

    for (std::size_t chainPos = 1; chainPos < index.componentLods.size(); ++chainPos)
    {
        const std::size_t lod = static_cast<std::size_t>(index.componentLods[chainPos]);
        std::vector<float> errors(nodeCount, -1.0f);
        float worst = 0.0f;
        for (std::size_t n = 0; n < nodeCount; ++n)
        {
            const auto& c = index.components[n];
            const auto& base = c.renderNodes[lod0];
            const auto& here = c.renderNodes[lod];
            if (base.empty()) continue; // not in LOD0: no reference, LOD-wide value applies
            float e = -1.0f;
            if (here.empty())
            {
                e = glm::length(c.maxBounds - c.minBounds); // the whole part is omitted
            }
            else if (base.size() == 1 && here.size() == 1)
            {
                const glm::mat4 t0 = index.renderNodes[lod0][static_cast<std::size_t>(base[0])].assetTransform;
                const glm::mat4 tk = index.renderNodes[lod][static_cast<std::size_t>(here[0])].assetTransform;
                const glm::mat4 toLocal = glm::inverse(t0);
                const auto key = std::make_tuple(asset.renderLods[lod0].nodes[static_cast<std::size_t>(base[0])].geometryIndex, lod,
                                                 asset.renderLods[lod].nodes[static_cast<std::size_t>(here[0])].geometryIndex);
                const bool sameFrame = nearIdentity(toLocal * tk);
                const auto it = sameFrame ? cache.find(key) : cache.end();
                if (it != cache.end())
                {
                    e = it->second;
                    ++reused;
                }
                else
                {
                    e = twoSided(gatherTriangles(asset, index, lod0, base, toLocal),
                                 gatherTriangles(asset, index, lod, here, toLocal));
                    ++measured;
                    if (sameFrame) cache[key] = e;
                }
            }
            else
            {
                e = twoSided(gatherTriangles(asset, index, lod0, base, glm::mat4(1.0f)),
                             gatherTriangles(asset, index, lod, here, glm::mat4(1.0f)));
                ++measured;
            }
            if (e < 0.0f) continue;
            e = std::max(e, previous[n]); // coarser LOD never claims less error
            previous[n] = e;
            errors[n] = e;
            worst = std::max(worst, e);
        }
        asset.renderLods[lod].nodeGeometricErrorM = std::move(errors);
        std::snprintf(line, sizeof(line), "LOD%u worst node error %.3f m (LOD-wide %.3f m)\n",
                      asset.renderLods[lod].level, worst,
                      asset.renderLods[lod].relativeGeometricError * index.characteristicM);
        summary += line;
    }
    std::snprintf(line, sizeof(line), "measured %zu node pairs, %zu reused from instances\n", measured, reused);
    return summary + line;
}

} // namespace elite::model_asset
