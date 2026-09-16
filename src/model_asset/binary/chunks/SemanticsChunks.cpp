#include "src/model_asset/binary/ModelAssetBinaryChunkCodecs.h"

namespace elite::model_asset::binary
{

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

} // namespace elite::model_asset::binary
