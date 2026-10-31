#include "src/model_asset/ModelAssetBuild.h"

#include <cmath>
#include <stdexcept>

namespace elite::model_asset
{

void scaleAuthoringDistancesUniform(ModelAsset& asset, float scale)
{
    // Authoring -> meter conversion used only on a temporary BUILD copy (or the
    // inverse when a metric production package is adopted into the editor).
    // Physical values already expressed in SI units -- kg, kg*m^2, N, Nm and
    // LightProperties::rangeMeters -- are deliberately NOT multiplied again.
    if (!std::isfinite(scale) || scale <= 0.0f) throw std::runtime_error("invalid authoring-to-meter scale");
    if (std::abs(scale - 1.0f) <= 1.0e-7f) return;
    const auto scalePhysicsPosition = [&](RigidBodyProperties& p) { p.centerOfMass *= scale; };
    asset.minBounds *= scale;
    asset.maxBounds *= scale;
    for (auto& node : asset.nodes)
    {
        node.localPosition *= scale; node.pivot *= scale; node.joint.pivot *= scale;
        scalePhysicsPosition(node.physics);
    }
    for (auto& state : asset.stateVariants)
    {
        state.localPosition *= scale; state.pivot *= scale; scalePhysicsPosition(state.physics);
    }
    for (auto& c : asset.collisionVolumes)
    {
        c.localPosition *= scale; c.halfSize *= scale; c.radius *= scale; c.halfHeight *= scale;
    }
    for (auto& socket : asset.sockets)
    {
        socket.localPosition *= scale; socket.extent *= scale;
    }
    for (auto& hit : asset.hitRegions) { hit.localPosition *= scale; hit.halfSize *= scale; }
    for (auto& opening : asset.openings) { opening.localPosition *= scale; opening.halfSize *= scale; }
    for (auto& repair : asset.repairTargets) repair.localPosition *= scale;
    for (auto& link : asset.structuralLinks)
        for (auto& proxy : link.damageProxies)
        {
            proxy.localPosition *= scale; proxy.halfSize *= scale; proxy.radius *= scale; proxy.halfHeight *= scale;
        }
    for (auto& lod : asset.renderLods)
    {
        lod.minBounds *= scale; lod.maxBounds *= scale;
        for (auto& node : lod.nodes) { node.localPosition *= scale; node.pivot *= scale; }
        for (auto& geometry : lod.geometries)
        {
            geometry.mesh.minBounds *= scale; geometry.mesh.maxBounds *= scale;
            for (auto& vertex : geometry.mesh.vertices) vertex.position *= scale;
        }
    }
    for (auto& c : asset.compartments) { c.localPosition *= scale; c.halfSize *= scale; }
    for (auto& p : asset.portals) { p.localPosition *= scale; p.halfSize *= scale; }
    for (auto& node : asset.nodes) { node.joint.minTravel *= scale; node.joint.maxTravel *= scale; }
    for (auto& geometry : asset.geometries)
        for (auto& lod : geometry.lods)
        {
            lod.minBounds *= scale; lod.maxBounds *= scale;
            for (auto& vertex : lod.vertices) vertex.position *= scale;
        }
}

ModelAsset makeProductionCopy(const ModelAsset& working)
{
    if (!working.physicalSize.enabled ||
        working.physicalSize.geometrySpace != PhysicalGeometrySpace::Authoring ||
        !std::isfinite(working.physicalSize.sourceToMeters) || working.physicalSize.sourceToMeters <= 0.0f)
        throw std::runtime_error("BUILD requires an explicit authoring-space -> meter calibration");
    ModelAsset production = working;
    production.formatVersion = ModelAssetFormatVersion;
    scaleAuthoringDistancesUniform(production, production.physicalSize.sourceToMeters);
    production.physicalSize.geometrySpace = PhysicalGeometrySpace::Meters;
    production.physicalSize.autoApplyOnSourceImport = false;
    return production;
}

} // namespace elite::model_asset
