#include "src/model_asset/binary/ModelAssetBinaryChunkCodecs.h"

namespace elite::model_asset::binary
{

// CMPT: interior compartments + portals. Optional: readers that predate it
// skip the chunk, an asset without it has no interiors.
void writeCompartmentsV4(Writer& w, const ModelAsset& a)
{
    w.pod(static_cast<std::uint32_t>(a.compartments.size()));
    for (const auto& c : a.compartments)
    {
        w.string(c.id); w.string(c.role); w.pod(c.parentNodeIndex);
        w.vec3(c.localPosition); w.vec3(c.localRotationDeg); w.vec3(c.halfSize);
        w.pod(static_cast<std::uint32_t>(c.interiorNodeIndices.size()));
        for (const std::int32_t node : c.interiorNodeIndices) w.pod(node);
        w.pod(static_cast<std::uint8_t>(c.enabled ? 1 : 0));
    }
    w.pod(static_cast<std::uint32_t>(a.portals.size()));
    for (const auto& p : a.portals)
    {
        w.string(p.id); w.pod(p.compartmentA); w.pod(p.compartmentB); w.pod(p.parentNodeIndex);
        w.vec3(p.localPosition); w.vec3(p.localRotationDeg); w.vec3(p.halfSize);
        w.pod(p.openingIndex); w.pod(p.doorNodeIndex); w.string(p.openStateId);
        w.pod(static_cast<std::uint8_t>(p.alwaysOpen ? 1 : 0));
        w.pod(static_cast<std::uint8_t>(p.enabled ? 1 : 0));
    }
}

void readCompartmentsV4(Reader& r, ModelAsset& a)
{
    std::uint32_t count = 0;
    if (!r.count(count)) return;
    a.compartments.resize(count);
    for (auto& c : a.compartments)
    {
        r.string(c.id); r.string(c.role); r.pod(c.parentNodeIndex);
        r.vec3(c.localPosition); r.vec3(c.localRotationDeg); r.vec3(c.halfSize);
        std::uint32_t nodes = 0;
        if (!r.count(nodes)) return;
        c.interiorNodeIndices.resize(nodes);
        for (auto& node : c.interiorNodeIndices) r.pod(node);
        std::uint8_t enabled = 0; r.pod(enabled); c.enabled = enabled != 0;
    }
    if (!r.count(count)) return;
    a.portals.resize(count);
    for (auto& p : a.portals)
    {
        r.string(p.id); r.pod(p.compartmentA); r.pod(p.compartmentB); r.pod(p.parentNodeIndex);
        r.vec3(p.localPosition); r.vec3(p.localRotationDeg); r.vec3(p.halfSize);
        r.pod(p.openingIndex); r.pod(p.doorNodeIndex); r.string(p.openStateId);
        std::uint8_t alwaysOpen = 0, enabled = 0;
        r.pod(alwaysOpen); r.pod(enabled);
        p.alwaysOpen = alwaysOpen != 0; p.enabled = enabled != 0;
    }
}

// PRES: presentation metadata - RenderLod roles, prismatic joint travel,
// surface effect bindings. Must follow LODS (roles index the LOD manifest).
void writePresentationV4(Writer& w, const ModelAsset& a)
{
    w.pod(static_cast<std::uint32_t>(a.renderLods.size()));
    for (const auto& lod : a.renderLods) w.pod(static_cast<std::uint8_t>(lod.role));
    w.pod(static_cast<std::uint32_t>(a.nodes.size()));
    for (const auto& node : a.nodes) { w.pod(node.joint.minTravel); w.pod(node.joint.maxTravel); }
    w.pod(static_cast<std::uint32_t>(a.effectBindings.size()));
    for (const auto& e : a.effectBindings)
    {
        w.string(e.id); w.string(e.effect); w.string(e.channel);
        w.pod(e.materialIndex); w.pod(e.nodeIndex);
        w.vec3(e.localAxis); w.vec3(e.hotColour); w.pod(e.maxEmissive);
    }
}

void readPresentationV4(Reader& r, ModelAsset& a)
{
    std::uint32_t count = 0;
    if (!r.count(count)) return;
    for (std::uint32_t i = 0; i < count; ++i)
    {
        std::uint8_t role = 0; r.pod(role);
        if (i < a.renderLods.size() && role <= static_cast<std::uint8_t>(RenderLodRole::Proxy))
            a.renderLods[i].role = static_cast<RenderLodRole>(role);
    }
    if (!r.count(count)) return;
    for (std::uint32_t i = 0; i < count; ++i)
    {
        float minTravel = 0.0f, maxTravel = 0.0f; r.pod(minTravel); r.pod(maxTravel);
        if (i < a.nodes.size()) { a.nodes[i].joint.minTravel = minTravel; a.nodes[i].joint.maxTravel = maxTravel; }
    }
    if (!r.count(count)) return;
    a.effectBindings.resize(count);
    for (auto& e : a.effectBindings)
    {
        r.string(e.id); r.string(e.effect); r.string(e.channel);
        r.pod(e.materialIndex); r.pod(e.nodeIndex);
        r.vec3(e.localAxis); r.vec3(e.hotColour); r.pod(e.maxEmissive);
    }
}

} // namespace elite::model_asset::binary
