#include "tools/model_asset_editor/ModelAssetEditorWire.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

namespace elite::model_asset::editor::wire
{
namespace
{
constexpr std::array<std::uint8_t, 8> Magic {{'E','L','W','I','R','0','0','1'}};
constexpr std::uint32_t MessageLodGeometry = 1u;

std::uint32_t checkedU32(std::size_t value, const char* what)
{
    if (value > std::numeric_limits<std::uint32_t>::max())
        throw std::overflow_error(std::string("editor wire payload too large: ") + what);
    return static_cast<std::uint32_t>(value);
}

struct Writer
{
    explicit Writer(std::size_t estimatedBytes)
        : data(estimatedBytes), offset(0)
    {
    }

    std::vector<std::uint8_t> data;
    std::size_t offset = 0;

    void ensure(std::size_t size)
    {
        if (size > std::numeric_limits<std::size_t>::max() - offset)
            throw std::overflow_error("editor wire payload size overflow");
        const auto required = offset + size;
        if (required <= data.size()) return;
        const auto doubled = data.size() > std::numeric_limits<std::size_t>::max() / 2u
            ? required : data.size() * 2u;
        data.resize(std::max(required, doubled));
    }

    void bytes(const void* source, std::size_t size)
    {
        ensure(size);
        if (size) std::memcpy(data.data() + offset, source, size);
        offset += size;
    }

    void u8(std::uint8_t value)
    {
        ensure(1u);
        data[offset++] = value;
    }

    void u32(std::uint32_t value)
    {
        ensure(4u);
        auto* dst = data.data() + offset;
        dst[0] = static_cast<std::uint8_t>(value & 0xffu);
        dst[1] = static_cast<std::uint8_t>((value >> 8u) & 0xffu);
        dst[2] = static_cast<std::uint8_t>((value >> 16u) & 0xffu);
        dst[3] = static_cast<std::uint8_t>((value >> 24u) & 0xffu);
        offset += 4u;
    }

    void i32(std::int32_t value)
    {
        u32(static_cast<std::uint32_t>(value));
    }

    void f32(float value)
    {
        static_assert(sizeof(float) == sizeof(std::uint32_t));
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        u32(bits);
    }

    void align4()
    {
        while ((offset & 3u) != 0u) u8(0u);
    }

    void string(const std::string& value)
    {
        u32(checkedU32(value.size(), "string"));
        if (!value.empty()) bytes(value.data(), value.size());
        align4();
    }

    std::vector<std::uint8_t> finish()
    {
        data.resize(offset);
        return std::move(data);
    }
};

void writeMeshArrays(Writer& w, const MeshLod& mesh)
{
    w.u32(checkedU32(mesh.vertices.size(), "vertices"));
    for (const auto& vertex : mesh.vertices)
    {
        w.f32(vertex.position.x); w.f32(vertex.position.y); w.f32(vertex.position.z);
    }
    for (const auto& vertex : mesh.vertices)
    {
        w.f32(vertex.normal.x); w.f32(vertex.normal.y); w.f32(vertex.normal.z);
    }

    w.u32(checkedU32(mesh.triangles.size(), "triangles"));
    for (const auto& triangle : mesh.triangles)
    {
        w.u32(triangle.a); w.u32(triangle.b); w.u32(triangle.c);
    }
    for (const auto& triangle : mesh.triangles) w.i32(triangle.materialIndex);
    for (const auto& triangle : mesh.triangles) w.u32(triangle.smoothingGroupId);

    w.u32(checkedU32(mesh.edges.size(), "edges"));
    for (const auto& edge : mesh.edges)
    {
        w.u32(edge.a); w.u32(edge.b);
        w.i32(edge.triangleA); w.i32(edge.triangleB);
        w.u32(edge.flags); w.u8(edge.renderMask);
    }
}

void writeRawMeshArrays(Writer& w, const MeshLod& mesh)
{
    w.u32(checkedU32(mesh.vertices.size(), "raw vertices"));
    for (const auto& vertex : mesh.vertices)
    {
        w.f32(vertex.position.x); w.f32(vertex.position.y); w.f32(vertex.position.z);
    }
    for (const auto& vertex : mesh.vertices)
    {
        w.f32(vertex.normal.x); w.f32(vertex.normal.y); w.f32(vertex.normal.z);
    }
    w.u32(checkedU32(mesh.triangles.size(), "raw triangles"));
    for (const auto& triangle : mesh.triangles)
    {
        w.u32(triangle.a); w.u32(triangle.b); w.u32(triangle.c);
    }
}

std::size_t estimatedPayloadBytes(
    const RenderLod& lod,
    const std::map<std::string, MeshLod>* rawSnapshots)
{
    std::size_t total = 28u;
    const auto add = [&total](std::size_t value)
    {
        if (value > std::numeric_limits<std::size_t>::max() - total)
            throw std::overflow_error("editor wire payload size overflow");
        total += value;
    };
    for (const auto& geometry : lod.geometries)
    {
        add(8u + ((geometry.id.size() + 3u) & ~std::size_t(3u)));
        add(4u + geometry.mesh.vertices.size() * 24u);
        add(4u + geometry.mesh.triangles.size() * 20u);
        add(4u + geometry.mesh.edges.size() * 21u);
        add(4u); // raw-present byte plus alignment padding
        if (rawSnapshots)
        {
            const auto rawIt = rawSnapshots->find(geometry.id);
            if (rawIt != rawSnapshots->end())
            {
                add(4u + rawIt->second.vertices.size() * 24u);
                add(4u + rawIt->second.triangles.size() * 12u);
            }
        }
    }
    return total;
}

} // namespace

std::vector<std::uint8_t> encodeLodGeometryPayload(
    std::uint32_t transferId,
    std::uint32_t lodIndex,
    const RenderLod& lod,
    const std::map<std::string, MeshLod>* rawSnapshots)
{
    Writer w(estimatedPayloadBytes(lod, rawSnapshots));
    w.bytes(Magic.data(), Magic.size());
    w.u32(WireVersion);
    w.u32(MessageLodGeometry);
    w.u32(transferId);
    w.u32(lodIndex);
    w.u32(checkedU32(lod.geometries.size(), "geometries"));

    for (std::size_t geometryIndex = 0; geometryIndex < lod.geometries.size(); ++geometryIndex)
    {
        const auto& geometry = lod.geometries[geometryIndex];
        w.u32(checkedU32(geometryIndex, "geometry index"));
        w.string(geometry.id);
        writeMeshArrays(w, geometry.mesh);

        const MeshLod* raw = nullptr;
        if (rawSnapshots)
        {
            const auto it = rawSnapshots->find(geometry.id);
            if (it != rawSnapshots->end()) raw = &it->second;
        }
        w.u8(raw ? 1u : 0u);
        w.align4();
        if (raw) writeRawMeshArrays(w, *raw);
    }
    return w.finish();
}

} // namespace elite::model_asset::editor::wire
