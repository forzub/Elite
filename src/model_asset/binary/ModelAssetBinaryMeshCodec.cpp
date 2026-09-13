#include "src/model_asset/binary/ModelAssetBinaryMeshCodec.h"

namespace elite::model_asset::binary
{

void writeMeshLod(Writer& w, const MeshLod& lod)
{
    w.vec3(lod.minBounds);
    w.vec3(lod.maxBounds);
    w.pod(static_cast<std::uint32_t>(lod.vertices.size()));
    for (const auto& v : lod.vertices)
    {
        w.vec3(v.position);
        w.vec3(v.normal);
        w.vec2(v.uv);
    }
    w.pod(static_cast<std::uint32_t>(lod.triangles.size()));
    for (const auto& t : lod.triangles)
    {
        w.pod(t.a); w.pod(t.b); w.pod(t.c);
        w.pod(t.sourcePolygonId); w.pod(t.materialIndex); w.pod(t.smoothingGroupId);
    }
    w.pod(static_cast<std::uint32_t>(lod.edges.size()));
    for (const auto& e : lod.edges)
    {
        w.pod(e.a); w.pod(e.b); w.pod(e.triangleA); w.pod(e.triangleB);
        w.pod(e.flags); w.pod(e.renderMask);
    }
}

void readMeshLod(Reader& r, MeshLod& lod)
{
    r.vec3(lod.minBounds);
    r.vec3(lod.maxBounds);
    std::uint32_t vertexCount = 0;
    if (!r.count(vertexCount)) return;
    lod.vertices.resize(vertexCount);
    for (auto& v : lod.vertices)
    {
        r.vec3(v.position); r.vec3(v.normal); r.vec2(v.uv);
    }
    std::uint32_t triangleCount = 0;
    if (!r.count(triangleCount)) return;
    lod.triangles.resize(triangleCount);
    for (auto& t : lod.triangles)
    {
        r.pod(t.a); r.pod(t.b); r.pod(t.c);
        r.pod(t.sourcePolygonId); r.pod(t.materialIndex); r.pod(t.smoothingGroupId);
    }
    std::uint32_t edgeCount = 0;
    if (!r.count(edgeCount)) return;
    lod.edges.resize(edgeCount);
    for (auto& e : lod.edges)
    {
        r.pod(e.a); r.pod(e.b); r.pod(e.triangleA); r.pod(e.triangleB);
        r.pod(e.flags); r.pod(e.renderMask);
    }
}

} // namespace elite::model_asset::binary
