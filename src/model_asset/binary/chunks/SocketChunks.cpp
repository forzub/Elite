#include "src/model_asset/binary/ModelAssetBinaryChunkCodecs.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace elite::model_asset::binary
{

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

void writeSocketMetadataV4(Writer& w, const ModelAsset& a)
{
    std::uint32_t count = 0;
    for (const auto& socket : a.sockets)
        if (!socket.interfaceProfile.empty() || std::abs(socket.previewFovDeg - 75.0f) > 1.0e-6f) ++count;
    w.pod(count);
    for (const auto& socket : a.sockets)
    {
        if (socket.interfaceProfile.empty() && std::abs(socket.previewFovDeg - 75.0f) <= 1.0e-6f) continue;
        w.string(socket.id);
        w.string(socket.interfaceProfile);
        w.pod(socket.previewFovDeg);
    }
}

void readSocketMetadataV4(Reader& r, ModelAsset& a)
{
    std::uint32_t count = 0;
    if (!r.count(count)) return;
    for (std::uint32_t i = 0; i < count; ++i)
    {
        std::string id, profile;
        float fov = 75.0f;
        r.string(id); r.string(profile); r.pod(fov);
        if (!r.ok) return;
        const auto it = std::find_if(a.sockets.begin(), a.sockets.end(), [&](const Socket& s) { return s.id == id; });
        if (it != a.sockets.end())
        {
            it->interfaceProfile = std::move(profile);
            it->previewFovDeg = fov;
        }
    }
}

} // namespace elite::model_asset::binary
