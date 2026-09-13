#include "src/model_asset/binary/ModelAssetBinaryChunkCodecs.h"

namespace elite::model_asset::binary
{

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

} // namespace elite::model_asset::binary
