#include "src/model_asset/binary/ModelAssetBinaryChunkCodecs.h"

namespace elite::model_asset::binary
{

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

} // namespace elite::model_asset::binary
