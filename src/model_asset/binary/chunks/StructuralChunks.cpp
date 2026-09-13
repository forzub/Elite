#include "src/model_asset/binary/ModelAssetBinaryChunkCodecs.h"

namespace elite::model_asset::binary
{

void writeStructuralLinksV4(Writer& w, const ModelAsset& a)
{
    w.pod(static_cast<std::uint32_t>(a.structuralLinks.size()));
    for (const auto& link : a.structuralLinks)
    {
        w.string(link.id); w.pod(link.nodeAIndex); w.pod(link.nodeBIndex);
        w.pod(static_cast<std::uint8_t>(link.kind));
        w.pod(static_cast<std::uint8_t>(link.loadBearing ? 1 : 0));
        w.pod(static_cast<std::uint8_t>(link.commandable ? 1 : 0));
        w.pod(link.breakForceN); w.pod(link.breakTorqueNm);
        w.pod(static_cast<std::uint8_t>(link.enabled ? 1 : 0));
        w.pod(static_cast<std::uint32_t>(link.damageProxies.size()));
        for (const auto& proxy : link.damageProxies)
        {
            w.string(proxy.id); w.pod(static_cast<std::uint8_t>(proxy.shape)); w.pod(proxy.parentNodeIndex);
            w.vec3(proxy.localPosition); w.vec3(proxy.localRotationDeg); w.vec3(proxy.halfSize);
            w.pod(proxy.radius); w.pod(proxy.halfHeight);
            w.pod(static_cast<std::uint8_t>(proxy.enabled ? 1 : 0));
        }
    }
}

void readStructuralLinksV4(Reader& r, ModelAsset& a)
{
    std::uint32_t count = 0;
    if (!r.count(count)) return;
    a.structuralLinks.resize(count);
    for (auto& link : a.structuralLinks)
    {
        std::uint8_t kind = 0, loadBearing = 0, commandable = 0, enabled = 0;
        r.string(link.id); r.pod(link.nodeAIndex); r.pod(link.nodeBIndex); r.pod(kind);
        r.pod(loadBearing); r.pod(commandable); r.pod(link.breakForceN); r.pod(link.breakTorqueNm); r.pod(enabled);
        link.kind = static_cast<StructuralLinkKind>(kind);
        link.loadBearing = loadBearing != 0;
        link.commandable = commandable != 0;
        link.enabled = enabled != 0;
        std::uint32_t proxyCount = 0;
        if (!r.count(proxyCount)) return;
        link.damageProxies.resize(proxyCount);
        for (auto& proxy : link.damageProxies)
        {
            std::uint8_t shape = 0, proxyEnabled = 0;
            r.string(proxy.id); r.pod(shape); r.pod(proxy.parentNodeIndex);
            r.vec3(proxy.localPosition); r.vec3(proxy.localRotationDeg); r.vec3(proxy.halfSize);
            r.pod(proxy.radius); r.pod(proxy.halfHeight); r.pod(proxyEnabled);
            proxy.shape = static_cast<StructuralDamageShape>(shape);
            proxy.enabled = proxyEnabled != 0;
        }
    }
}

} // namespace elite::model_asset::binary
