#include "src/model_asset/ModelAssetBinary.h"

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <type_traits>

namespace elite::model_asset
{
namespace
{
constexpr std::array<char, 8> Magic {{'E','L','M','D','L','0','0','2'}};
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
    std::istream& in;
    bool ok = true;

    template <typename T>
    void pod(T& value)
    {
        static_assert(std::is_trivially_copyable_v<T>);
        in.read(reinterpret_cast<char*>(&value), sizeof(T));
        ok = ok && static_cast<bool>(in);
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
        if (size)
            in.read(value.data(), static_cast<std::streamsize>(size));
        ok = ok && static_cast<bool>(in);
    }

    void vec2(glm::vec2& v) { pod(v.x); pod(v.y); }
    void vec3(glm::vec3& v) { pod(v.x); pod(v.y); pod(v.z); }
    void vec4(glm::vec4& v) { pod(v.x); pod(v.y); pod(v.z); pod(v.w); }
};

using ChunkWriter = void (*)(Writer&, const ModelAsset&);
using ChunkReader = void (*)(Reader&, ModelAsset&);

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
    std::uint8_t canonicalized = 0; r.pod(canonicalized); a.sourceBasis.canonicalized = canonicalized != 0;
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
        std::uint8_t twoSided = 0; r.pod(twoSided); m.twoSided = twoSided != 0;
        r.string(m.baseColorTexture); r.string(m.emissiveTexture);
    }
}

void writeGeom(Writer& w, const ModelAsset& a)
{
    w.pod(static_cast<std::uint32_t>(a.geometries.size()));
    for (const auto& g : a.geometries)
    {
        w.string(g.id);
        w.string(g.sourceLod0);
        w.string(g.sourceLod1);
        w.pod(static_cast<std::uint8_t>(g.surfaceMode));
        w.pod(static_cast<std::uint32_t>(g.lods.size()));
        for (const auto& lod : g.lods)
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
                w.pod(t.a); w.pod(t.b); w.pod(t.c); w.pod(t.sourcePolygonId); w.pod(t.materialIndex); w.pod(t.smoothingGroupId);
            }
            w.pod(static_cast<std::uint32_t>(lod.edges.size()));
            for (const auto& e : lod.edges)
            {
                w.pod(e.a); w.pod(e.b); w.pod(e.triangleA); w.pod(e.triangleB);
                w.pod(e.flags); w.pod(e.renderMask);
            }
        }
    }
}

void readGeom(Reader& r, ModelAsset& a)
{
    std::uint32_t geometryCount = 0;
    if (!r.count(geometryCount)) return;
    a.geometries.resize(geometryCount);
    for (auto& g : a.geometries)
    {
        r.string(g.id);
        r.string(g.sourceLod0);
        r.string(g.sourceLod1);
        std::uint8_t surface = 0;
        r.pod(surface);
        g.surfaceMode = static_cast<SurfaceMode>(surface);
        std::uint32_t lodCount = 0;
        if (!r.count(lodCount)) return;
        g.lods.resize(lodCount);
        for (auto& lod : g.lods)
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
                r.pod(t.a); r.pod(t.b); r.pod(t.c); r.pod(t.sourcePolygonId); r.pod(t.materialIndex); r.pod(t.smoothingGroupId);
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
        std::uint8_t lightType = 0; r.pod(lightType); s.light.type = static_cast<LightType>(lightType);
        r.vec3(s.light.color); r.pod(s.light.intensity); r.pod(s.light.rangeMeters); r.pod(s.light.outerConeDeg);
        std::uint8_t enabled = 0;
        r.pod(enabled); s.enabled = enabled != 0;
    }
}

struct ChunkSpec
{
    std::array<char, 4> id;
    ChunkWriter writer;
    ChunkReader reader;
};

constexpr std::array<ChunkSpec, 6> Chunks {{
    {{{'M','E','T','A'}}, writeMeta, readMeta},
    {{{'M','A','T','L'}}, writeMaterials, readMaterials},
    {{{'G','E','O','M'}}, writeGeom, readGeom},
    {{{'N','O','D','E'}}, writeNodes, readNodes},
    {{{'C','O','L','L'}}, writeCollisions, readCollisions},
    {{{'S','O','C','K'}}, writeSockets, readSockets}
}};

const ChunkSpec* findChunk(const std::array<char, 4>& id)
{
    for (const auto& chunk : Chunks)
        if (chunk.id == id)
            return &chunk;
    return nullptr;
}

void setError(std::string* error, const std::string& value)
{
    if (error) *error = value;
}
}

bool ModelAssetBinary::save(const std::string& path, const ModelAsset& asset, std::string* error)
{
    if (error) error->clear();
    try
    {
        const std::filesystem::path filePath(path);
        if (filePath.has_parent_path())
            std::filesystem::create_directories(filePath.parent_path());

        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file)
        {
            setError(error, "cannot open output file: " + path);
            return false;
        }

        file.write(Magic.data(), static_cast<std::streamsize>(Magic.size()));
        Writer header {file};
        header.pod(ModelAssetFormatVersion);
        header.pod(static_cast<std::uint32_t>(Chunks.size()));
        if (!header.ok) return false;

        for (const auto& chunk : Chunks)
        {
            std::ostringstream payload(std::ios::binary);
            Writer writer {payload};
            chunk.writer(writer, asset);
            if (!writer.ok)
            {
                setError(error, "failed to serialize model asset chunk");
                return false;
            }
            const std::string bytes = payload.str();
            file.write(chunk.id.data(), 4);
            const std::uint64_t size = static_cast<std::uint64_t>(bytes.size());
            file.write(reinterpret_cast<const char*>(&size), sizeof(size));
            file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            if (!file)
            {
                setError(error, "failed writing model asset: " + path);
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

bool ModelAssetBinary::load(const std::string& path, ModelAsset& asset, std::string* error)
{
    if (error) error->clear();
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        setError(error, "cannot open model asset: " + path);
        return false;
    }

    std::array<char, 8> magic {};
    file.read(magic.data(), static_cast<std::streamsize>(magic.size()));
    if (!file || magic != Magic)
    {
        setError(error, "invalid model asset magic");
        return false;
    }

    Reader header {file};
    std::uint32_t version = 0;
    std::uint32_t chunkCount = 0;
    header.pod(version);
    header.pod(chunkCount);
    if (!header.ok || version != ModelAssetFormatVersion || chunkCount > 1024u)
    {
        setError(error, "unsupported model asset version");
        return false;
    }

    ModelAsset result;
    result.formatVersion = version;

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

        const ChunkSpec* spec = findChunk(id);
        if (!spec)
            continue;

        std::istringstream payload(bytes, std::ios::binary);
        Reader reader {payload};
        spec->reader(reader, result);
        if (!reader.ok)
        {
            setError(error, "corrupt model asset chunk payload");
            return false;
        }
    }

    if (result.assetId.empty())
    {
        setError(error, "model asset has no asset id");
        return false;
    }

    asset = std::move(result);
    return true;
}

} // namespace elite::model_asset
