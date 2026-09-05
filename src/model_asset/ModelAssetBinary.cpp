#include "src/model_asset/ModelAssetBinary.h"
#include "src/model_asset/ModelAssetMigration.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <type_traits>

namespace elite::model_asset
{
namespace
{
constexpr std::array<char, 8> ManifestMagicV4 {{'E','L','M','D','L','0','0','4'}};
constexpr std::array<char, 8> ManifestMagicV3 {{'E','L','M','D','L','0','0','3'}};
constexpr std::array<char, 8> LegacyMagicV2   {{'E','L','M','D','L','0','0','2'}};
constexpr std::array<char, 8> MeshMagicV4     {{'E','L','M','S','H','0','0','4'}};
constexpr std::array<char, 8> MeshMagicV2     {{'E','L','M','S','H','0','0','2'}};
constexpr std::uint32_t MeshPayloadFormatVersion = 4;
constexpr std::uint32_t MaxCollectionCount = 50'000'000u;
constexpr std::uint32_t MaxStringBytes = 16u * 1024u * 1024u;

struct Writer
{
    std::ostream& out;
    bool ok = true;

    template <typename T>
    void pod(const T& value)
    {
        static_assert(std::is_trivially_copyable_v<T>);
        out.write(reinterpret_cast<const char*>(&value), sizeof(T));
        ok = ok && static_cast<bool>(out);
    }

    void string(const std::string& value)
    {
        const auto size = static_cast<std::uint32_t>(value.size());
        pod(size);
        if (size)
            out.write(value.data(), static_cast<std::streamsize>(size));
        ok = ok && static_cast<bool>(out);
    }

    void vec2(const glm::vec2& v) { pod(v.x); pod(v.y); }
    void vec3(const glm::vec3& v) { pod(v.x); pod(v.y); pod(v.z); }
    void vec4(const glm::vec4& v) { pod(v.x); pod(v.y); pod(v.z); pod(v.w); }
};

struct Reader
{
    std::istream* in = nullptr;
    const std::uint8_t* cursor = nullptr;
    const std::uint8_t* end = nullptr;
    bool ok = true;

    explicit Reader(std::istream& stream) : in(&stream) {}
    Reader(const std::uint8_t* data, std::size_t size) : cursor(data), end(data + size) {}

    void bytes(void* destination, std::size_t size)
    {
        if (!ok) return;
        if (cursor)
        {
            const auto remaining = static_cast<std::size_t>(end - cursor);
            if (size > remaining)
            {
                ok = false;
                return;
            }
            if (size) std::memcpy(destination, cursor, size);
            cursor += size;
            return;
        }
        in->read(reinterpret_cast<char*>(destination), static_cast<std::streamsize>(size));
        ok = static_cast<bool>(*in);
    }

    template <typename T>
    void pod(T& value)
    {
        static_assert(std::is_trivially_copyable_v<T>);
        bytes(&value, sizeof(T));
    }

    bool count(std::uint32_t& value)
    {
        pod(value);
        if (!ok || value > MaxCollectionCount)
        {
            ok = false;
            return false;
        }
        return true;
    }

    void string(std::string& value)
    {
        std::uint32_t size = 0;
        pod(size);
        if (!ok || size > MaxStringBytes)
        {
            ok = false;
            return;
        }
        value.resize(size);
        if (size) bytes(value.data(), size);
    }

    void vec2(glm::vec2& v) { pod(v.x); pod(v.y); }
    void vec3(glm::vec3& v) { pod(v.x); pod(v.y); pod(v.z); }
    void vec4(glm::vec4& v) { pod(v.x); pod(v.y); pod(v.z); pod(v.w); }
};

void writeStrings(Writer& w, const std::vector<std::string>& values)
{
    w.pod(static_cast<std::uint32_t>(values.size()));
    for (const auto& value : values) w.string(value);
}

void readStrings(Reader& r, std::vector<std::string>& values)
{
    std::uint32_t count = 0;
    if (!r.count(count)) return;
    values.resize(count);
    for (auto& value : values) r.string(value);
}

using ChunkWriter = void (*)(Writer&, const ModelAsset&);
using ChunkReader = void (*)(Reader&, ModelAsset&);

void setError(std::string* error, const std::string& value)
{
    if (error) *error = value;
}

void writeMeta(Writer& w, const ModelAsset& a)
{
    w.string(a.assetId);
    w.string(a.displayName);
    w.pod(a.sourceObjectType);
    w.pod(a.lodSwitchDistance);
    w.string(a.sourceBasis.preset);
    w.pod(static_cast<std::int8_t>(a.sourceBasis.right));
    w.pod(static_cast<std::int8_t>(a.sourceBasis.up));
    w.pod(static_cast<std::int8_t>(a.sourceBasis.forward));
    w.pod(static_cast<std::uint8_t>(a.sourceBasis.canonicalized ? 1 : 0));
    w.vec3(a.minBounds);
    w.vec3(a.maxBounds);
}

void readMeta(Reader& r, ModelAsset& a)
{
    r.string(a.assetId);
    r.string(a.displayName);
    r.pod(a.sourceObjectType);
    r.pod(a.lodSwitchDistance);
    r.string(a.sourceBasis.preset);
    std::int8_t right = 0, up = 0, forward = 0;
    r.pod(right); r.pod(up); r.pod(forward);
    a.sourceBasis.right = static_cast<AxisDirection>(right);
    a.sourceBasis.up = static_cast<AxisDirection>(up);
    a.sourceBasis.forward = static_cast<AxisDirection>(forward);
    std::uint8_t canonicalized = 0;
    r.pod(canonicalized);
    a.sourceBasis.canonicalized = canonicalized != 0;
    r.vec3(a.minBounds);
    r.vec3(a.maxBounds);
}

void writeMaterials(Writer& w, const ModelAsset& a)
{
    w.pod(static_cast<std::uint32_t>(a.materials.size()));
    for (const auto& m : a.materials)
    {
        w.string(m.id); w.string(m.sourceName);
        w.vec4(m.baseColor); w.vec3(m.emissiveColor);
        w.pod(m.emissiveStrength); w.pod(m.metallic); w.pod(m.roughness);
        w.pod(static_cast<std::uint8_t>(m.twoSided ? 1 : 0));
        w.string(m.baseColorTexture); w.string(m.emissiveTexture);
    }
}

void readMaterials(Reader& r, ModelAsset& a)
{
    std::uint32_t count = 0;
    if (!r.count(count)) return;
    a.materials.resize(count);
    for (auto& m : a.materials)
    {
        r.string(m.id); r.string(m.sourceName);
        r.vec4(m.baseColor); r.vec3(m.emissiveColor);
        r.pod(m.emissiveStrength); r.pod(m.metallic); r.pod(m.roughness);
        std::uint8_t twoSided = 0;
        r.pod(twoSided);
        m.twoSided = twoSided != 0;
        r.string(m.baseColorTexture); r.string(m.emissiveTexture);
    }
}

// v3 GEOM is a lightweight manifest. Heavy MeshLod arrays live in one
// <asset>.lodN.elmesh payload per LOD level.
void writeGeomManifest(Writer& w, const ModelAsset& a)
{
    w.pod(static_cast<std::uint32_t>(a.geometries.size()));
    for (const auto& g : a.geometries)
    {
        w.string(g.id);
        w.pod(static_cast<std::uint8_t>(g.surfaceMode));
        w.pod(static_cast<std::uint32_t>(g.sourceLods.size()));
        for (const auto& source : g.sourceLods)
            w.string(source);
        w.pod(static_cast<std::uint32_t>(g.lods.size()));
        for (const auto& lod : g.lods)
        {
            w.vec3(lod.minBounds);
            w.vec3(lod.maxBounds);
        }
    }
}

void readGeomManifest(Reader& r, ModelAsset& a)
{
    std::uint32_t geometryCount = 0;
    if (!r.count(geometryCount)) return;
    a.geometries.resize(geometryCount);
    for (auto& g : a.geometries)
    {
        r.string(g.id);
        std::uint8_t surface = 0;
        r.pod(surface);
        g.surfaceMode = static_cast<SurfaceMode>(surface);

        std::uint32_t sourceCount = 0;
        if (!r.count(sourceCount)) return;
        g.sourceLods.resize(sourceCount);
        for (auto& source : g.sourceLods)
            r.string(source);

        std::uint32_t lodCount = 0;
        if (!r.count(lodCount)) return;
        g.lods.resize(lodCount);
        for (auto& lod : g.lods)
        {
            r.vec3(lod.minBounds);
            r.vec3(lod.maxBounds);
        }
    }
}

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

// Legacy v2 GEOM reader exists only so authored editor state can be migrated to
// the v3 split package. New saves never write this layout.
void readGeomLegacyV2(Reader& r, ModelAsset& a)
{
    std::uint32_t geometryCount = 0;
    if (!r.count(geometryCount)) return;
    a.geometries.resize(geometryCount);
    for (auto& g : a.geometries)
    {
        r.string(g.id);
        std::string sourceLod0;
        std::string sourceLod1;
        r.string(sourceLod0);
        r.string(sourceLod1);
        if (!sourceLod0.empty()) g.sourceLods.push_back(sourceLod0);
        if (!sourceLod1.empty()) g.sourceLods.push_back(sourceLod1);
        std::uint8_t surface = 0;
        r.pod(surface);
        g.surfaceMode = static_cast<SurfaceMode>(surface);
        std::uint32_t lodCount = 0;
        if (!r.count(lodCount)) return;
        g.lods.resize(lodCount);
        for (auto& lod : g.lods)
            readMeshLod(r, lod);
    }
}

void writeNodes(Writer& w, const ModelAsset& a)
{
    w.pod(static_cast<std::uint32_t>(a.nodes.size()));
    for (const auto& n : a.nodes)
    {
        w.string(n.id); w.string(n.moduleId);
        w.pod(n.parentIndex); w.pod(n.geometryIndex);
        w.vec3(n.localPosition); w.vec3(n.localRotationDeg); w.vec3(n.pivot);
        w.pod(static_cast<std::uint8_t>(n.joint.type));
        w.vec3(n.joint.pivot); w.vec3(n.joint.axis);
        w.pod(n.joint.defaultRateDegPerSec); w.pod(n.joint.minAngleDeg); w.pod(n.joint.maxAngleDeg);
        w.pod(static_cast<std::uint8_t>(n.joint.breakable ? 1 : 0));
        w.pod(n.joint.breakForceN); w.pod(n.joint.breakTorqueNm);
        w.pod(static_cast<std::uint8_t>(n.physics.mode));
        w.pod(n.physics.densityKgM3); w.pod(n.physics.massKg);
        w.vec3(n.physics.centerOfMass); w.vec3(n.physics.inertiaDiagonal); w.vec3(n.physics.inertiaProducts);
        w.pod(static_cast<std::uint8_t>(n.enabled ? 1 : 0));
    }
}

void readNodes(Reader& r, ModelAsset& a)
{
    std::uint32_t count = 0;
    if (!r.count(count)) return;
    a.nodes.resize(count);
    for (auto& n : a.nodes)
    {
        r.string(n.id); r.string(n.moduleId);
        r.pod(n.parentIndex); r.pod(n.geometryIndex);
        r.vec3(n.localPosition); r.vec3(n.localRotationDeg); r.vec3(n.pivot);
        std::uint8_t jointType = 0;
        r.pod(jointType); n.joint.type = static_cast<JointType>(jointType);
        r.vec3(n.joint.pivot); r.vec3(n.joint.axis);
        r.pod(n.joint.defaultRateDegPerSec); r.pod(n.joint.minAngleDeg); r.pod(n.joint.maxAngleDeg);
        std::uint8_t breakable = 0;
        r.pod(breakable); n.joint.breakable = breakable != 0;
        r.pod(n.joint.breakForceN); r.pod(n.joint.breakTorqueNm);
        std::uint8_t massMode = 0;
        r.pod(massMode); n.physics.mode = static_cast<MassPropertyMode>(massMode);
        r.pod(n.physics.densityKgM3); r.pod(n.physics.massKg);
        r.vec3(n.physics.centerOfMass); r.vec3(n.physics.inertiaDiagonal); r.vec3(n.physics.inertiaProducts);
        std::uint8_t enabled = 0;
        r.pod(enabled); n.enabled = enabled != 0;
    }
}

void writeCollisions(Writer& w, const ModelAsset& a)
{
    w.pod(static_cast<std::uint32_t>(a.collisionVolumes.size()));
    for (const auto& c : a.collisionVolumes)
    {
        w.string(c.id); w.string(c.moduleId); w.pod(c.parentNodeIndex);
        w.pod(static_cast<std::uint8_t>(c.shape));
        w.vec3(c.localPosition); w.vec3(c.localRotationDeg); w.vec3(c.halfSize);
        w.pod(c.radius); w.pod(c.halfHeight);
        w.pod(static_cast<std::uint8_t>(c.enabled ? 1 : 0));
    }
}

void readCollisions(Reader& r, ModelAsset& a)
{
    std::uint32_t count = 0;
    if (!r.count(count)) return;
    a.collisionVolumes.resize(count);
    for (auto& c : a.collisionVolumes)
    {
        r.string(c.id); r.string(c.moduleId); r.pod(c.parentNodeIndex);
        std::uint8_t shape = 0;
        r.pod(shape); c.shape = static_cast<CollisionShape>(shape);
        r.vec3(c.localPosition); r.vec3(c.localRotationDeg); r.vec3(c.halfSize);
        r.pod(c.radius); r.pod(c.halfHeight);
        std::uint8_t enabled = 0;
        r.pod(enabled); c.enabled = enabled != 0;
    }
}

void writeSockets(Writer& w, const ModelAsset& a)
{
    w.pod(static_cast<std::uint32_t>(a.sockets.size()));
    for (const auto& s : a.sockets)
    {
        w.string(s.id); w.string(s.kind); w.string(s.moduleId); w.pod(s.parentNodeIndex);
        w.vec3(s.localPosition); w.vec3(s.localRotationDeg); w.vec3(s.extent);
        w.pod(static_cast<std::uint8_t>(s.light.type)); w.vec3(s.light.color);
        w.pod(s.light.intensity); w.pod(s.light.rangeMeters); w.pod(s.light.outerConeDeg);
        w.pod(static_cast<std::uint8_t>(s.enabled ? 1 : 0));
    }
}

void readSockets(Reader& r, ModelAsset& a)
{
    std::uint32_t count = 0;
    if (!r.count(count)) return;
    a.sockets.resize(count);
    for (auto& s : a.sockets)
    {
        r.string(s.id); r.string(s.kind); r.string(s.moduleId); r.pod(s.parentNodeIndex);
        r.vec3(s.localPosition); r.vec3(s.localRotationDeg); r.vec3(s.extent);
        std::uint8_t lightType = 0;
        r.pod(lightType); s.light.type = static_cast<LightType>(lightType);
        r.vec3(s.light.color); r.pod(s.light.intensity); r.pod(s.light.rangeMeters); r.pod(s.light.outerConeDeg);
        std::uint8_t enabled = 0;
        r.pod(enabled); s.enabled = enabled != 0;
    }
}


void writeSemanticNodesV4(Writer& w, const ModelAsset& a)
{
    w.pod(static_cast<std::uint32_t>(a.nodes.size()));
    for (const auto& n : a.nodes)
    {
        w.string(n.id); w.string(n.moduleId); w.pod(n.parentIndex); w.string(n.defaultStateId);
        w.vec3(n.localPosition); w.vec3(n.localRotationDeg); w.vec3(n.pivot);
        w.pod(static_cast<std::uint8_t>(n.joint.type));
        w.vec3(n.joint.pivot); w.vec3(n.joint.axis);
        w.pod(n.joint.defaultRateDegPerSec); w.pod(n.joint.minAngleDeg); w.pod(n.joint.maxAngleDeg);
        w.pod(static_cast<std::uint8_t>(n.joint.breakable ? 1 : 0));
        w.pod(n.joint.breakForceN); w.pod(n.joint.breakTorqueNm);
        w.pod(static_cast<std::uint8_t>(n.physics.mode));
        w.pod(n.physics.densityKgM3); w.pod(n.physics.massKg);
        w.vec3(n.physics.centerOfMass); w.vec3(n.physics.inertiaDiagonal); w.vec3(n.physics.inertiaProducts);
        w.pod(static_cast<std::uint8_t>(n.enabled ? 1 : 0));
    }
}

void readSemanticNodesV4(Reader& r, ModelAsset& a)
{
    std::uint32_t count = 0;
    if (!r.count(count)) return;
    a.nodes.resize(count);
    for (auto& n : a.nodes)
    {
        r.string(n.id); r.string(n.moduleId); r.pod(n.parentIndex); r.string(n.defaultStateId);
        n.geometryIndex = NoIndex;
        r.vec3(n.localPosition); r.vec3(n.localRotationDeg); r.vec3(n.pivot);
        std::uint8_t jointType = 0; r.pod(jointType); n.joint.type = static_cast<JointType>(jointType);
        r.vec3(n.joint.pivot); r.vec3(n.joint.axis);
        r.pod(n.joint.defaultRateDegPerSec); r.pod(n.joint.minAngleDeg); r.pod(n.joint.maxAngleDeg);
        std::uint8_t breakable = 0; r.pod(breakable); n.joint.breakable = breakable != 0;
        r.pod(n.joint.breakForceN); r.pod(n.joint.breakTorqueNm);
        std::uint8_t massMode = 0; r.pod(massMode); n.physics.mode = static_cast<MassPropertyMode>(massMode);
        r.pod(n.physics.densityKgM3); r.pod(n.physics.massKg);
        r.vec3(n.physics.centerOfMass); r.vec3(n.physics.inertiaDiagonal); r.vec3(n.physics.inertiaProducts);
        std::uint8_t enabled = 0; r.pod(enabled); n.enabled = enabled != 0;
        if (n.defaultStateId.empty()) n.defaultStateId = "intact";
    }
}

void writeStateVariantsV4(Writer& w, const ModelAsset& a)
{
    w.pod(static_cast<std::uint32_t>(a.stateVariants.size()));
    for (const auto& s : a.stateVariants)
    {
        w.string(s.id); w.string(s.displayName); w.pod(s.nodeIndex);
        w.pod(static_cast<std::uint8_t>(s.transformOverride ? 1 : 0));
        w.vec3(s.localPosition); w.vec3(s.localRotationDeg); w.vec3(s.pivot);
        w.pod(static_cast<std::uint8_t>(s.physicsOverride ? 1 : 0));
        w.pod(static_cast<std::uint8_t>(s.physics.mode));
        w.pod(s.physics.densityKgM3); w.pod(s.physics.massKg);
        w.vec3(s.physics.centerOfMass); w.vec3(s.physics.inertiaDiagonal); w.vec3(s.physics.inertiaProducts);
        w.pod(static_cast<std::uint8_t>(s.detached ? 1 : 0));
        w.pod(static_cast<std::uint8_t>(s.enabled ? 1 : 0));
    }
}

void readStateVariantsV4(Reader& r, ModelAsset& a)
{
    std::uint32_t count = 0;
    if (!r.count(count)) return;
    a.stateVariants.resize(count);
    for (auto& s : a.stateVariants)
    {
        r.string(s.id); r.string(s.displayName); r.pod(s.nodeIndex);
        std::uint8_t transformOverride = 0; r.pod(transformOverride); s.transformOverride = transformOverride != 0;
        r.vec3(s.localPosition); r.vec3(s.localRotationDeg); r.vec3(s.pivot);
        std::uint8_t physicsOverride = 0; r.pod(physicsOverride); s.physicsOverride = physicsOverride != 0;
        std::uint8_t massMode = 0; r.pod(massMode); s.physics.mode = static_cast<MassPropertyMode>(massMode);
        r.pod(s.physics.densityKgM3); r.pod(s.physics.massKg);
        r.vec3(s.physics.centerOfMass); r.vec3(s.physics.inertiaDiagonal); r.vec3(s.physics.inertiaProducts);
        std::uint8_t detached = 0, enabled = 0;
        r.pod(detached); r.pod(enabled); s.detached = detached != 0; s.enabled = enabled != 0;
    }
}

void writeCollisionsV4(Writer& w, const ModelAsset& a)
{
    w.pod(static_cast<std::uint32_t>(a.collisionVolumes.size()));
    for (const auto& c : a.collisionVolumes)
    {
        w.string(c.id); w.string(c.moduleId); w.pod(c.parentNodeIndex);
        w.pod(static_cast<std::uint8_t>(c.shape));
        w.vec3(c.localPosition); w.vec3(c.localRotationDeg); w.vec3(c.halfSize);
        w.pod(c.radius); w.pod(c.halfHeight); writeStrings(w, c.activeStates);
        w.pod(static_cast<std::uint8_t>(c.enabled ? 1 : 0));
    }
}

void readCollisionsV4(Reader& r, ModelAsset& a)
{
    std::uint32_t count = 0;
    if (!r.count(count)) return;
    a.collisionVolumes.resize(count);
    for (auto& c : a.collisionVolumes)
    {
        r.string(c.id); r.string(c.moduleId); r.pod(c.parentNodeIndex);
        std::uint8_t shape = 0; r.pod(shape); c.shape = static_cast<CollisionShape>(shape);
        r.vec3(c.localPosition); r.vec3(c.localRotationDeg); r.vec3(c.halfSize);
        r.pod(c.radius); r.pod(c.halfHeight); readStrings(r, c.activeStates);
        std::uint8_t enabled = 0; r.pod(enabled); c.enabled = enabled != 0;
    }
}

void writeSocketsV4(Writer& w, const ModelAsset& a)
{
    w.pod(static_cast<std::uint32_t>(a.sockets.size()));
    for (const auto& s : a.sockets)
    {
        w.string(s.id); w.string(s.kind); w.string(s.moduleId); w.pod(s.parentNodeIndex);
        w.vec3(s.localPosition); w.vec3(s.localRotationDeg); w.vec3(s.extent);
        w.pod(static_cast<std::uint8_t>(s.light.type)); w.vec3(s.light.color);
        w.pod(s.light.intensity); w.pod(s.light.rangeMeters); w.pod(s.light.outerConeDeg);
        writeStrings(w, s.activeStates);
        w.pod(static_cast<std::uint8_t>(s.enabled ? 1 : 0));
    }
}

void readSocketsV4(Reader& r, ModelAsset& a)
{
    std::uint32_t count = 0;
    if (!r.count(count)) return;
    a.sockets.resize(count);
    for (auto& s : a.sockets)
    {
        r.string(s.id); r.string(s.kind); r.string(s.moduleId); r.pod(s.parentNodeIndex);
        r.vec3(s.localPosition); r.vec3(s.localRotationDeg); r.vec3(s.extent);
        std::uint8_t lightType = 0; r.pod(lightType); s.light.type = static_cast<LightType>(lightType);
        r.vec3(s.light.color); r.pod(s.light.intensity); r.pod(s.light.rangeMeters); r.pod(s.light.outerConeDeg);
        readStrings(r, s.activeStates);
        std::uint8_t enabled = 0; r.pod(enabled); s.enabled = enabled != 0;
    }
}

void writeHitRegionsV4(Writer& w, const ModelAsset& a)
{
    w.pod(static_cast<std::uint32_t>(a.hitRegions.size()));
    for (const auto& h : a.hitRegions)
    {
        w.string(h.id); w.pod(h.parentNodeIndex); writeStrings(w, h.activeStates);
        w.vec3(h.localPosition); w.vec3(h.localRotationDeg); w.vec3(h.halfSize);
        w.pod(static_cast<std::uint8_t>(h.enabled ? 1 : 0));
    }
}

void readHitRegionsV4(Reader& r, ModelAsset& a)
{
    std::uint32_t count = 0; if (!r.count(count)) return; a.hitRegions.resize(count);
    for (auto& h : a.hitRegions)
    {
        r.string(h.id); r.pod(h.parentNodeIndex); readStrings(r, h.activeStates);
        r.vec3(h.localPosition); r.vec3(h.localRotationDeg); r.vec3(h.halfSize);
        std::uint8_t enabled = 0; r.pod(enabled); h.enabled = enabled != 0;
    }
}

void writeOpeningsV4(Writer& w, const ModelAsset& a)
{
    w.pod(static_cast<std::uint32_t>(a.openings.size()));
    for (const auto& o : a.openings)
    {
        w.string(o.id); w.pod(o.parentNodeIndex); writeStrings(w, o.activeStates);
        w.vec3(o.localPosition); w.vec3(o.localRotationDeg); w.vec3(o.halfSize);
        w.pod(static_cast<std::uint8_t>(o.traversable ? 1 : 0));
        w.pod(static_cast<std::uint8_t>(o.lineOfFire ? 1 : 0));
        w.pod(static_cast<std::uint8_t>(o.enabled ? 1 : 0));
    }
}

void readOpeningsV4(Reader& r, ModelAsset& a)
{
    std::uint32_t count = 0; if (!r.count(count)) return; a.openings.resize(count);
    for (auto& o : a.openings)
    {
        r.string(o.id); r.pod(o.parentNodeIndex); readStrings(r, o.activeStates);
        r.vec3(o.localPosition); r.vec3(o.localRotationDeg); r.vec3(o.halfSize);
        std::uint8_t traversable = 0, lineOfFire = 0, enabled = 0;
        r.pod(traversable); r.pod(lineOfFire); r.pod(enabled);
        o.traversable = traversable != 0; o.lineOfFire = lineOfFire != 0; o.enabled = enabled != 0;
    }
}

void writeRepairTargetsV4(Writer& w, const ModelAsset& a)
{
    w.pod(static_cast<std::uint32_t>(a.repairTargets.size()));
    for (const auto& t : a.repairTargets)
    {
        w.string(t.id); w.string(t.kind); w.pod(t.parentNodeIndex); writeStrings(w, t.activeStates);
        w.vec3(t.localPosition); w.vec3(t.localRotationDeg); w.string(t.repairedStateId);
        w.pod(static_cast<std::uint8_t>(t.enabled ? 1 : 0));
    }
}

void readRepairTargetsV4(Reader& r, ModelAsset& a)
{
    std::uint32_t count = 0; if (!r.count(count)) return; a.repairTargets.resize(count);
    for (auto& t : a.repairTargets)
    {
        r.string(t.id); r.string(t.kind); r.pod(t.parentNodeIndex); readStrings(r, t.activeStates);
        r.vec3(t.localPosition); r.vec3(t.localRotationDeg); r.string(t.repairedStateId);
        std::uint8_t enabled = 0; r.pod(enabled); t.enabled = enabled != 0;
    }
}

void writeLodManifestV4(Writer& w, const ModelAsset& a)
{
    w.pod(static_cast<std::uint32_t>(a.renderLods.size()));
    for (const auto& lod : a.renderLods)
    {
        w.pod(lod.level); w.string(lod.sourceKind); w.pod(lod.generatedFromLod);
        w.vec3(lod.minBounds); w.vec3(lod.maxBounds);
        // Counts are diagnostics only; heavy graph content remains in .elmesh.
        // When a LOD is not resident, preserve the counts read from the manifest
        // instead of serializing the empty payload vectors as 0/0.
        const auto geometryCount = !lod.geometries.empty()
            ? static_cast<std::uint32_t>(lod.geometries.size())
            : lod.declaredGeometryCount;
        const auto nodeCount = !lod.nodes.empty()
            ? static_cast<std::uint32_t>(lod.nodes.size())
            : lod.declaredNodeCount;
        w.pod(geometryCount);
        w.pod(nodeCount);
    }
}

void readLodManifestV4(Reader& r, ModelAsset& a)
{
    std::uint32_t count = 0; if (!r.count(count)) return; a.renderLods.resize(count);
    for (auto& lod : a.renderLods)
    {
        std::uint32_t geometryCount = 0, nodeCount = 0;
        r.pod(lod.level); r.string(lod.sourceKind); r.pod(lod.generatedFromLod);
        r.vec3(lod.minBounds); r.vec3(lod.maxBounds); r.pod(geometryCount); r.pod(nodeCount);
        lod.declaredGeometryCount = geometryCount;
        lod.declaredNodeCount = nodeCount;
    }
}

// v4-compatible optional manifest extension. Keeping screen-space LOD error in
// its own chunk leaves the existing META/LODS layouts readable by older v4
// binaries; unknown chunks are skipped by the generic chunk reader.
void writeLodScreenErrorV4(Writer& w, const ModelAsset& a)
{
    w.pod(static_cast<std::uint32_t>(a.renderLods.size()));
    for (const auto& lod : a.renderLods)
    {
        w.pod(lod.level);
        const float value = lod.level == 0 ? 0.0f : lod.relativeGeometricError;
        w.pod(value);
    }
}

void readLodScreenErrorV4(Reader& r, ModelAsset& a)
{
    std::uint32_t count = 0;
    if (!r.count(count)) return;
    for (std::uint32_t i = 0; i < count; ++i)
    {
        std::uint32_t level = 0;
        float value = -1.0f;
        r.pod(level); r.pod(value);
        if (!r.ok) return;
        if (level < a.renderLods.size())
            a.renderLods[level].relativeGeometricError = level == 0 ? 0.0f : value;
    }
}

struct ChunkSpec
{
    std::array<char, 4> id;
    ChunkWriter writer;
    ChunkReader reader;
};

constexpr std::array<ChunkSpec, 11> ManifestChunksV4 {{
    {{{'M','E','T','A'}}, writeMeta, readMeta},
    {{{'M','A','T','L'}}, writeMaterials, readMaterials},
    {{{'S','E','M','N'}}, writeSemanticNodesV4, readSemanticNodesV4},
    {{{'S','T','A','T'}}, writeStateVariantsV4, readStateVariantsV4},
    {{{'C','O','L','L'}}, writeCollisionsV4, readCollisionsV4},
    {{{'S','O','C','K'}}, writeSocketsV4, readSocketsV4},
    {{{'H','I','T','R'}}, writeHitRegionsV4, readHitRegionsV4},
    {{{'O','P','E','N'}}, writeOpeningsV4, readOpeningsV4},
    {{{'R','E','P','R'}}, writeRepairTargetsV4, readRepairTargetsV4},
    {{{'L','O','D','S'}}, writeLodManifestV4, readLodManifestV4},
    {{{'L','E','R','R'}}, writeLodScreenErrorV4, readLodScreenErrorV4}
}};

constexpr std::array<ChunkSpec, 6> ManifestChunksV3 {{
    {{{'M','E','T','A'}}, writeMeta, readMeta},
    {{{'M','A','T','L'}}, writeMaterials, readMaterials},
    {{{'G','E','O','M'}}, writeGeomManifest, readGeomManifest},
    {{{'N','O','D','E'}}, writeNodes, readNodes},
    {{{'C','O','L','L'}}, writeCollisions, readCollisions},
    {{{'S','O','C','K'}}, writeSockets, readSockets}
}};

const ChunkSpec* findChunk(const std::array<char, 4>& id, bool v4)
{
    if (v4)
    {
        for (const auto& chunk : ManifestChunksV4) if (chunk.id == id) return &chunk;
    }
    else
    {
        for (const auto& chunk : ManifestChunksV3) if (chunk.id == id) return &chunk;
    }
    return nullptr;
}

ChunkReader findLegacyChunkReader(const std::array<char, 4>& id)
{
    if (id == std::array<char,4>{{'M','E','T','A'}}) return readMeta;
    if (id == std::array<char,4>{{'M','A','T','L'}}) return readMaterials;
    if (id == std::array<char,4>{{'G','E','O','M'}}) return readGeomLegacyV2;
    if (id == std::array<char,4>{{'N','O','D','E'}}) return readNodes;
    if (id == std::array<char,4>{{'C','O','L','L'}}) return readCollisions;
    if (id == std::array<char,4>{{'S','O','C','K'}}) return readSockets;
    return nullptr;
}

std::filesystem::path packageLodPayloadPath(const std::filesystem::path& manifestPath, std::size_t lodIndex)
{
    const std::string stem = manifestPath.stem().string();
    return manifestPath.parent_path() /
        (stem + ".lod" + std::to_string(lodIndex) + ".elmesh");
}

std::size_t maxLodCount(const ModelAsset& asset)
{
    if (!asset.renderLods.empty())
        return asset.renderLods.size();
    return legacyRenderLodCount(asset);
}

bool validateRenderLod(const RenderLod& lod, std::size_t semanticNodeCount, std::string* error)
{
    std::map<std::string, std::size_t> geometryIds;
    for (std::size_t geometryIndex = 0; geometryIndex < lod.geometries.size(); ++geometryIndex)
    {
        const auto& geometry = lod.geometries[geometryIndex];
        if (geometry.id.empty())
        {
            setError(error, "LOD" + std::to_string(lod.level) + " render geometry[" +
                std::to_string(geometryIndex) + "] has empty stable id");
            return false;
        }
        const auto [it, inserted] = geometryIds.emplace(geometry.id, geometryIndex);
        if (!inserted)
        {
            setError(error, "LOD" + std::to_string(lod.level) + " duplicate RenderGeometryDefinition id '" +
                geometry.id + "': geometry[" + std::to_string(it->second) + "] and geometry[" +
                std::to_string(geometryIndex) + "]");
            return false;
        }
    }
    std::map<std::string, std::size_t> nodeIds;
    for (std::size_t nodeIndex = 0; nodeIndex < lod.nodes.size(); ++nodeIndex)
    {
        const auto& node = lod.nodes[nodeIndex];
        if (node.id.empty())
        {
            setError(error, "LOD" + std::to_string(lod.level) + " RenderNode[" +
                std::to_string(nodeIndex) + "] has empty id");
            return false;
        }
        const auto [it, inserted] = nodeIds.emplace(node.id, nodeIndex);
        if (!inserted)
        {
            setError(error, "LOD" + std::to_string(lod.level) + " duplicate RenderNode id '" + node.id +
                "': node[" + std::to_string(it->second) + "] and node[" + std::to_string(nodeIndex) + "]");
            return false;
        }
        if (node.parentIndex < NoIndex ||
            node.parentIndex >= static_cast<std::int32_t>(lod.nodes.size()) ||
            node.geometryIndex < NoIndex ||
            node.geometryIndex >= static_cast<std::int32_t>(lod.geometries.size()) ||
            node.semanticNodeIndex < NoIndex ||
            node.semanticNodeIndex >= static_cast<std::int32_t>(semanticNodeCount))
        {
            setError(error, "render node index out of range in LOD" + std::to_string(lod.level) + ": " + node.id);
            return false;
        }
        if (node.parentIndex == static_cast<std::int32_t>(nodeIndex))
        {
            setError(error, "render node cannot parent itself in LOD" + std::to_string(lod.level) + ": " + node.id);
            return false;
        }
    }
    return true;
}

bool validateSemanticAsset(const ModelAsset& asset, std::string* error)
{
    std::map<std::string, std::size_t> semanticNodeIds;
    for (std::size_t i = 0; i < asset.nodes.size(); ++i)
    {
        const auto& node = asset.nodes[i];
        if (node.id.empty())
        {
            setError(error, "semantic Node[" + std::to_string(i) + "] has empty id");
            return false;
        }
        const auto [it, inserted] = semanticNodeIds.emplace(node.id, i);
        if (!inserted)
        {
            setError(error, "duplicate semantic Node id '" + node.id + "': node[" +
                std::to_string(it->second) + "] and node[" + std::to_string(i) + "]");
            return false;
        }
        if (node.parentIndex < NoIndex || node.parentIndex >= static_cast<std::int32_t>(asset.nodes.size()) ||
            node.parentIndex == static_cast<std::int32_t>(i))
        {
            setError(error, "semantic node parent index out of range: " + node.id);
            return false;
        }
    }

    std::set<std::pair<std::int32_t, std::string>> stateIds;
    for (const auto& state : asset.stateVariants)
    {
        if (state.nodeIndex < 0 || state.nodeIndex >= static_cast<std::int32_t>(asset.nodes.size()) || state.id.empty() || state.id == "intact")
        {
            setError(error, "invalid semantic state variant");
            return false;
        }
        if (!stateIds.emplace(state.nodeIndex, state.id).second)
        {
            setError(error, "duplicate semantic state variant: " + state.id);
            return false;
        }
    }
    const auto stateDeclared = [&](std::int32_t nodeIndex, const std::string& stateId) {
        if (stateId == "intact") return true;
        return stateIds.count({nodeIndex, stateId}) != 0;
    };
    for (std::size_t i = 0; i < asset.nodes.size(); ++i)
    {
        if (!stateDeclared(static_cast<std::int32_t>(i), asset.nodes[i].defaultStateId))
        {
            setError(error, "semantic node default state is not declared: " + asset.nodes[i].id + " / " + asset.nodes[i].defaultStateId);
            return false;
        }
    }

    const auto validParent = [&](std::int32_t parent) {
        return parent >= NoIndex && parent < static_cast<std::int32_t>(asset.nodes.size());
    };
    const auto validateScopedStates = [&](std::int32_t parent, const std::vector<std::string>& states, const std::string& label) {
        if (!validParent(parent)) { setError(error, label + " parent index out of range"); return false; }
        if (!states.empty() && parent == NoIndex) { setError(error, label + " has state scope but no semantic parent"); return false; }
        for (const auto& stateId : states)
        {
            if (!stateDeclared(parent, stateId)) { setError(error, label + " references undeclared state: " + stateId); return false; }
        }
        return true;
    };
    for (const auto& c : asset.collisionVolumes) if (!validateScopedStates(c.parentNodeIndex, c.activeStates, "collision " + c.id)) return false;
    for (const auto& socket : asset.sockets) if (!validateScopedStates(socket.parentNodeIndex, socket.activeStates, "socket " + socket.id)) return false;
    for (const auto& h : asset.hitRegions) if (!validateScopedStates(h.parentNodeIndex, h.activeStates, "hit region " + h.id)) return false;
    for (const auto& o : asset.openings) if (!validateScopedStates(o.parentNodeIndex, o.activeStates, "opening " + o.id)) return false;
    for (const auto& r : asset.repairTargets)
    {
        if (!validateScopedStates(r.parentNodeIndex, r.activeStates, "repair target " + r.id)) return false;
        if (!r.repairedStateId.empty() && r.parentNodeIndex >= 0 && !stateDeclared(r.parentNodeIndex, r.repairedStateId))
        {
            setError(error, "repair target references undeclared repaired state: " + r.id + " / " + r.repairedStateId);
            return false;
        }
    }

    float previousAuthoredError = 0.0f;
    bool havePreviousAuthoredError = true;
    for (const auto& lod : asset.renderLods)
    {
        if (lod.level == 0)
        {
            if (std::isfinite(lod.relativeGeometricError) && lod.relativeGeometricError > 1.0e-6f)
            {
                setError(error, "LOD0 relative geometric error must be zero/unspecified");
                return false;
            }
        }
        else if (lod.relativeGeometricError >= 0.0f)
        {
            if (!std::isfinite(lod.relativeGeometricError) || lod.relativeGeometricError <= 0.0f)
            {
                setError(error, "LOD" + std::to_string(lod.level) + " has invalid relative geometric error");
                return false;
            }
            if (havePreviousAuthoredError && lod.relativeGeometricError + 1.0e-7f < previousAuthoredError)
            {
                setError(error, "LOD relative geometric error must be non-decreasing");
                return false;
            }
            previousAuthoredError = lod.relativeGeometricError;
            havePreviousAuthoredError = true;
        }
        else
        {
            havePreviousAuthoredError = false;
        }
        if (!validateRenderLod(lod, asset.nodes.size(), error)) return false;
        for (const auto& renderNode : lod.nodes)
        {
            if (!renderNode.activeStates.empty() && renderNode.semanticNodeIndex == NoIndex)
            {
                setError(error, "state-scoped render node has no semantic binding in LOD" + std::to_string(lod.level) + ": " + renderNode.id);
                return false;
            }
            for (const auto& stateId : renderNode.activeStates)
            {
                if (!stateDeclared(renderNode.semanticNodeIndex, stateId))
                {
                    setError(error, "render node references undeclared semantic state in LOD" + std::to_string(lod.level) + ": " + renderNode.id + " / " + stateId);
                    return false;
                }
            }
        }
    }
    return true;
}

bool writeLodPayload(
    const std::filesystem::path& path,
    const ModelAsset& asset,
    std::size_t lodIndex,
    std::string* error)
{
    if (lodIndex >= asset.renderLods.size())
    {
        setError(error, "render LOD is not declared: " + std::to_string(lodIndex));
        return false;
    }
    const auto& lod = asset.renderLods[lodIndex];
    if (!validateRenderLod(lod, asset.nodes.size(), error)) return false;

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file)
    {
        setError(error, "cannot open LOD payload: " + path.string());
        return false;
    }

    file.write(MeshMagicV4.data(), static_cast<std::streamsize>(MeshMagicV4.size()));
    Writer w {file};
    w.pod(MeshPayloadFormatVersion);
    w.pod(static_cast<std::uint32_t>(lodIndex));
    w.string(lod.sourceKind); w.pod(lod.generatedFromLod);
    w.vec3(lod.minBounds); w.vec3(lod.maxBounds);

    w.pod(static_cast<std::uint32_t>(lod.geometries.size()));
    for (const auto& geometry : lod.geometries)
    {
        w.string(geometry.id); w.string(geometry.sourcePath);
        w.pod(static_cast<std::uint8_t>(geometry.surfaceMode));
        writeMeshLod(w, geometry.mesh);
    }

    w.pod(static_cast<std::uint32_t>(lod.nodes.size()));
    for (const auto& node : lod.nodes)
    {
        w.string(node.id); w.pod(node.parentIndex); w.pod(node.geometryIndex); w.pod(node.semanticNodeIndex);
        writeStrings(w, node.activeStates);
        w.vec3(node.localPosition); w.vec3(node.localRotationDeg); w.vec3(node.pivot);
        w.pod(static_cast<std::uint8_t>(node.enabled ? 1 : 0));
    }
    if (!w.ok)
    {
        setError(error, "failed writing LOD payload: " + path.string());
        return false;
    }
    return true;
}

bool readLegacyLodPayloadV3(
    Reader& r,
    ModelAsset& asset,
    std::size_t expectedLodIndex,
    std::string* error)
{
    std::uint32_t version = 0, lodIndex = 0, entryCount = 0;
    r.pod(version); r.pod(lodIndex);
    if (!r.count(entryCount) || version != 2u || lodIndex != expectedLodIndex)
    {
        setError(error, "unsupported or mismatched legacy LOD payload");
        return false;
    }
    std::map<std::string, std::size_t> geometryById;
    for (std::size_t i = 0; i < asset.geometries.size(); ++i)
        geometryById.emplace(asset.geometries[i].id, i);
    for (std::uint32_t entry = 0; entry < entryCount; ++entry)
    {
        std::string geometryId; r.string(geometryId);
        const auto it = geometryById.find(geometryId);
        if (!r.ok || it == geometryById.end())
        {
            setError(error, "legacy LOD payload contains unknown geometry id: " + geometryId);
            return false;
        }
        auto& geometry = asset.geometries[it->second];
        if (expectedLodIndex >= geometry.lods.size())
        {
            setError(error, "legacy LOD representation not declared for: " + geometryId);
            return false;
        }
        readMeshLod(r, geometry.lods[expectedLodIndex]);
        if (!r.ok) { setError(error, "corrupt legacy LOD payload"); return false; }
    }
    return true;
}

bool readLodPayload(
    const std::filesystem::path& path,
    ModelAsset& asset,
    std::size_t expectedLodIndex,
    std::string* error)
{
    // .elmesh files are the heavy path. Read the file once into contiguous
    // memory, then parse through Reader's memory cursor instead of issuing
    // millions of tiny istream::read() calls for vertices and triangles.
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
    {
        setError(error, "missing LOD payload: " + path.string());
        return false;
    }
    const auto endPosition = file.tellg();
    if (endPosition < static_cast<std::streamoff>(8))
    {
        setError(error, "invalid LOD payload header: " + path.string());
        return false;
    }
    const auto fileSize = static_cast<std::uint64_t>(endPosition);
    if (fileSize > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()) ||
        fileSize > static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max()))
    {
        setError(error, "LOD payload is too large to map into editor memory: " + path.string());
        return false;
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(fileSize));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!file)
    {
        setError(error, "failed reading LOD payload: " + path.string());
        return false;
    }

    std::array<char, 8> magic {};
    std::memcpy(magic.data(), bytes.data(), magic.size());
    Reader r(bytes.data() + magic.size(), bytes.size() - magic.size());
    if (magic == MeshMagicV2)
        return readLegacyLodPayloadV3(r, asset, expectedLodIndex, error);
    if (magic != MeshMagicV4)
    {
        setError(error, "invalid LOD payload magic: " + path.string());
        return false;
    }

    std::uint32_t version = 0, lodIndex = 0;
    r.pod(version); r.pod(lodIndex);
    if (!r.ok || version != MeshPayloadFormatVersion || lodIndex != expectedLodIndex)
    {
        setError(error, "unsupported or mismatched LOD payload: " + path.string());
        return false;
    }
    if (expectedLodIndex >= asset.renderLods.size())
    {
        setError(error, "LOD payload is not declared by v4 manifest");
        return false;
    }

    const float manifestRelativeGeometricError =
        asset.renderLods[expectedLodIndex].relativeGeometricError;
    RenderLod loaded;
    loaded.level = lodIndex;
    loaded.relativeGeometricError = manifestRelativeGeometricError;
    r.string(loaded.sourceKind); r.pod(loaded.generatedFromLod);
    r.vec3(loaded.minBounds); r.vec3(loaded.maxBounds);
    std::uint32_t geometryCount = 0; if (!r.count(geometryCount)) return false;
    loaded.geometries.resize(geometryCount);
    for (auto& geometry : loaded.geometries)
    {
        r.string(geometry.id); r.string(geometry.sourcePath);
        std::uint8_t surface = 0; r.pod(surface); geometry.surfaceMode = static_cast<SurfaceMode>(surface);
        readMeshLod(r, geometry.mesh);
    }
    std::uint32_t nodeCount = 0; if (!r.count(nodeCount)) return false;
    loaded.nodes.resize(nodeCount);
    for (auto& node : loaded.nodes)
    {
        r.string(node.id); r.pod(node.parentIndex); r.pod(node.geometryIndex); r.pod(node.semanticNodeIndex);
        readStrings(r, node.activeStates);
        r.vec3(node.localPosition); r.vec3(node.localRotationDeg); r.vec3(node.pivot);
        std::uint8_t enabled = 0; r.pod(enabled); node.enabled = enabled != 0;
    }
    if (!r.ok || !validateRenderLod(loaded, asset.nodes.size(), error))
    {
        if (r.ok && error && error->empty()) *error = "invalid render LOD graph";
        else if (!r.ok) setError(error, "corrupt LOD payload: " + path.string());
        return false;
    }
    loaded.declaredGeometryCount = static_cast<std::uint32_t>(loaded.geometries.size());
    loaded.declaredNodeCount = static_cast<std::uint32_t>(loaded.nodes.size());
    asset.renderLods[expectedLodIndex] = std::move(loaded);
    return true;
}

bool readChunks(
    std::istream& file,
    std::uint32_t chunkCount,
    ModelAsset& result,
    std::uint32_t formatVersion,
    std::string* error)
{
    for (std::uint32_t i = 0; i < chunkCount; ++i)
    {
        std::array<char, 4> id {};
        std::uint64_t size = 0;
        file.read(id.data(), 4);
        file.read(reinterpret_cast<char*>(&size), sizeof(size));
        if (!file || size > static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max()))
        {
            setError(error, "corrupt model asset chunk header");
            return false;
        }

        std::string bytes(static_cast<std::size_t>(size), '\0');
        if (size)
            file.read(bytes.data(), static_cast<std::streamsize>(size));
        if (!file)
        {
            setError(error, "truncated model asset chunk");
            return false;
        }

        ChunkReader readerFn = nullptr;
        if (formatVersion == 2u)
            readerFn = findLegacyChunkReader(id);
        else if (const ChunkSpec* spec = findChunk(id, formatVersion >= 4u))
            readerFn = spec->reader;
        if (!readerFn)
            continue;

        std::istringstream payload(bytes, std::ios::binary);
        Reader reader {payload};
        readerFn(reader, result);
        if (!reader.ok)
        {
            setError(error, "corrupt model asset chunk payload");
            return false;
        }
    }
    return true;
}

void removeStaleLodPayloadFiles(
    const std::filesystem::path& manifestPath,
    const std::set<std::filesystem::path>& keep)
{
    std::error_code ec;
    const auto directory = manifestPath.parent_path();
    if (!std::filesystem::exists(directory, ec)) return;
    const std::string prefix = manifestPath.stem().string() + ".lod";
    for (const auto& entry : std::filesystem::directory_iterator(directory, ec))
    {
        if (ec || !entry.is_regular_file()) continue;
        const auto name = entry.path().filename().string();
        if (name.rfind(prefix, 0) != 0 || entry.path().extension() != ".elmesh") continue;
        if (keep.count(entry.path()) == 0)
            std::filesystem::remove(entry.path(), ec);
    }
}
}

std::filesystem::path ModelAssetBinary::lodPayloadPath(
    const std::string& manifestPath,
    std::size_t lodIndex)
{
    return packageLodPayloadPath(std::filesystem::path(manifestPath), lodIndex);
}

bool ModelAssetBinary::validate(const ModelAsset& asset, std::string* error)
{
    if (error) error->clear();
    return validateSemanticAsset(asset, error);
}

bool ModelAssetBinary::saveManifest(
    const std::string& path,
    const ModelAsset& asset,
    std::string* error)
{
    if (error) error->clear();
    try
    {
        if (asset.renderLods.empty())
        {
            setError(error, "v4 asset has no independent render LODs");
            return false;
        }
        if (!validateSemanticAsset(asset, error)) return false;
        const std::filesystem::path manifestPath(path);
        if (manifestPath.has_parent_path())
            std::filesystem::create_directories(manifestPath.parent_path());

        std::ofstream file(manifestPath, std::ios::binary | std::ios::trunc);
        if (!file)
        {
            setError(error, "cannot open output manifest: " + path);
            return false;
        }

        file.write(ManifestMagicV4.data(), static_cast<std::streamsize>(ManifestMagicV4.size()));
        Writer header {file};
        header.pod(ModelAssetFormatVersion);
        header.pod(static_cast<std::uint32_t>(ManifestChunksV4.size()));
        if (!header.ok)
        {
            setError(error, "failed writing model asset manifest header");
            return false;
        }

        for (const auto& chunk : ManifestChunksV4)
        {
            std::ostringstream payload(std::ios::binary);
            Writer writer {payload};
            chunk.writer(writer, asset);
            if (!writer.ok)
            {
                setError(error, "failed to serialize model asset manifest chunk");
                return false;
            }
            const std::string bytes = payload.str();
            file.write(chunk.id.data(), 4);
            const std::uint64_t size = static_cast<std::uint64_t>(bytes.size());
            file.write(reinterpret_cast<const char*>(&size), sizeof(size));
            file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            if (!file)
            {
                setError(error, "failed writing model asset manifest: " + path);
                return false;
            }
        }
        return true;
    }
    catch (const std::exception& ex)
    {
        setError(error, ex.what());
        return false;
    }
}

bool ModelAssetBinary::loadManifest(
    const std::string& path,
    ModelAsset& asset,
    bool* legacyPackage,
    std::string* error)
{
    if (error) error->clear();
    if (legacyPackage) *legacyPackage = false;
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        setError(error, "cannot open model asset: " + path);
        return false;
    }

    std::array<char, 8> magic {};
    file.read(magic.data(), static_cast<std::streamsize>(magic.size()));
    if (!file || (magic != ManifestMagicV4 && magic != ManifestMagicV3 && magic != LegacyMagicV2))
    {
        setError(error, "invalid model asset magic");
        return false;
    }

    Reader header {file};
    std::uint32_t version = 0, chunkCount = 0;
    header.pod(version); header.pod(chunkCount);
    const bool isV4 = magic == ManifestMagicV4 && version == 4u;
    const bool isV3 = magic == ManifestMagicV3 && version == 3u;
    const bool isV2 = magic == LegacyMagicV2 && version == 2u;
    if (!header.ok || (!isV4 && !isV3 && !isV2) || chunkCount > 1024u)
    {
        setError(error, "unsupported model asset version");
        return false;
    }

    ModelAsset result;
    result.formatVersion = version;
    if (!readChunks(file, chunkCount, result, version, error))
        return false;
    if (result.assetId.empty())
    {
        setError(error, "model asset has no asset id");
        return false;
    }

    if (isV4)
    {
        for (std::size_t i = 0; i < result.renderLods.size(); ++i)
            result.renderLods[i].level = static_cast<std::uint32_t>(i);
    }
    if (legacyPackage) *legacyPackage = !isV4;
    asset = std::move(result);
    return true;
}

bool ModelAssetBinary::saveLod(
    const std::string& manifestPath,
    const ModelAsset& asset,
    std::size_t lodIndex,
    std::string* error)
{
    if (error) error->clear();
    try
    {
        const auto path = packageLodPayloadPath(std::filesystem::path(manifestPath), lodIndex);
        if (path.has_parent_path())
            std::filesystem::create_directories(path.parent_path());
        return writeLodPayload(path, asset, lodIndex, error);
    }
    catch (const std::exception& ex)
    {
        setError(error, ex.what());
        return false;
    }
}

bool ModelAssetBinary::loadLod(
    const std::string& manifestPath,
    ModelAsset& asset,
    std::size_t lodIndex,
    std::string* error)
{
    if (error) error->clear();
    return readLodPayload(
        packageLodPayloadPath(std::filesystem::path(manifestPath), lodIndex),
        asset,
        lodIndex,
        error);
}

bool ModelAssetBinary::pruneStaleLods(
    const std::string& manifestPath,
    const ModelAsset& asset,
    std::string* error)
{
    if (error) error->clear();
    try
    {
        const std::filesystem::path path(manifestPath);
        std::set<std::filesystem::path> keep;
        for (std::size_t lodIndex = 0; lodIndex < maxLodCount(asset); ++lodIndex)
            keep.insert(packageLodPayloadPath(path, lodIndex));
        removeStaleLodPayloadFiles(path, keep);
        return true;
    }
    catch (const std::exception& ex)
    {
        setError(error, ex.what());
        return false;
    }
}

bool ModelAssetBinary::save(const std::string& path, const ModelAsset& asset, std::string* error)
{
    if (error) error->clear();
    ModelAsset working = asset;
    if (working.renderLods.empty())
        buildIndependentRenderLodsFromLegacy(working);
    working.formatVersion = ModelAssetFormatVersion;
    const std::size_t lodCount = maxLodCount(working);
    for (std::size_t lodIndex = 0; lodIndex < lodCount; ++lodIndex)
        if (!saveLod(path, working, lodIndex, error))
            return false;

    // Manifest is committed last: it never advertises a new package until all
    // payloads have been written successfully.
    if (!saveManifest(path, working, error))
        return false;
    return pruneStaleLods(path, working, error);
}

bool ModelAssetBinary::load(const std::string& path, ModelAsset& asset, std::string* error)
{
    bool legacyPackage = false;
    ModelAsset result;
    if (!loadManifest(path, result, &legacyPackage, error))
        return false;

    if (result.formatVersion == 3u)
    {
        const std::size_t count = legacyRenderLodCount(result);
        for (std::size_t lodIndex = 0; lodIndex < count; ++lodIndex)
            if (!loadLod(path, result, lodIndex, error)) return false;
        buildIndependentRenderLodsFromLegacy(result);
        result.formatVersion = ModelAssetFormatVersion;
    }
    else if (result.formatVersion == 2u)
    {
        buildIndependentRenderLodsFromLegacy(result);
        result.formatVersion = ModelAssetFormatVersion;
    }
    else
    {
        for (std::size_t lodIndex = 0; lodIndex < result.renderLods.size(); ++lodIndex)
            if (!loadLod(path, result, lodIndex, error)) return false;
    }
    asset = std::move(result);
    return true;
}

} // namespace elite::model_asset
