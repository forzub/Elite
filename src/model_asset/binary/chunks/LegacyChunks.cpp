#include "src/model_asset/binary/ModelAssetBinaryChunkCodecs.h"
#include "src/model_asset/binary/ModelAssetBinaryMeshCodec.h"

namespace elite::model_asset::binary
{

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
        for (auto& source : g.sourceLods) r.string(source);
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
        for (auto& lod : g.lods) readMeshLod(r, lod);
    }
}

void writeNodesLegacy(Writer& w, const ModelAsset& a)
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

void readNodesLegacy(Reader& r, ModelAsset& a)
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

void writeCollisionsLegacy(Writer& w, const ModelAsset& a)
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

void readCollisionsLegacy(Reader& r, ModelAsset& a)
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

void writeSocketsLegacy(Writer& w, const ModelAsset& a)
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

void readSocketsLegacy(Reader& r, ModelAsset& a)
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

} // namespace elite::model_asset::binary
