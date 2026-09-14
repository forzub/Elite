#include "src/game/assets/LegacyAssemblyModelAdapter.h"

#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

namespace game::assets
{
namespace
{
using elite::model_asset::Edge;
using elite::model_asset::EdgeBoundary;
using elite::model_asset::EdgeCanonicalTopology;
using elite::model_asset::EdgeNonManifold;
using elite::model_asset::EdgePolygonBoundary;
using elite::model_asset::EdgeRenderElite;
using elite::model_asset::EdgeRenderTechnical;
using elite::model_asset::EdgeTriangulationInternal;
using elite::model_asset::MeshLod;
using elite::model_asset::ModelAsset;
using elite::model_asset::Node;
using elite::model_asset::NoIndex;
using elite::model_asset::PhysicalGeometrySpace;
using elite::model_asset::RenderGeometryDefinition;
using elite::model_asset::RenderLod;
using elite::model_asset::RenderNode;
using elite::model_asset::Triangle;
using elite::model_asset::Vertex;
using game::ship::geometry::AssemblyModule;
using game::ship::geometry::MeshData;
using game::ship::geometry::ObjectAssembly;

struct EdgeInfo
{
    std::int32_t triangleA = NoIndex;
    std::int32_t triangleB = NoIndex;
    std::int32_t faceA = -1;
    std::int32_t faceB = -1;
    bool nonManifold = false;
};

MeshLod convertMesh(const MeshData& source)
{
    MeshLod out;
    out.vertices.reserve(source.vertices.size());
    out.triangles.reserve(source.triangles.size());

    for (const auto& vertex : source.vertices)
    {
        Vertex converted;
        converted.position = vertex.position;
        converted.normal = vertex.normal;
        converted.uv = glm::vec2(0.0f);
        out.vertices.push_back(converted);
    }

    std::map<std::pair<std::uint32_t, std::uint32_t>, EdgeInfo> edgeMap;

    const auto addEdge = [&](std::uint32_t a,
                             std::uint32_t b,
                             std::int32_t triangleIndex,
                             std::int32_t faceId) {
        if (a > b)
            std::swap(a, b);

        auto& info = edgeMap[{a, b}];
        if (info.triangleA == NoIndex)
        {
            info.triangleA = triangleIndex;
            info.faceA = faceId;
        }
        else if (info.triangleB == NoIndex)
        {
            info.triangleB = triangleIndex;
            info.faceB = faceId;
        }
        else
        {
            info.nonManifold = true;
        }
    };

    for (std::size_t triangleIndex = 0;
         triangleIndex < source.triangles.size();
         ++triangleIndex)
    {
        const auto& sourceTriangle = source.triangles[triangleIndex];
        const int indices[3] = {
            sourceTriangle.v0,
            sourceTriangle.v1,
            sourceTriangle.v2,
        };

        for (const int index : indices)
        {
            if (index < 0 ||
                static_cast<std::size_t>(index) >= source.vertices.size())
            {
                throw std::runtime_error(
                    "legacy assembly mesh contains an out-of-range triangle index");
            }
        }

        Triangle converted;
        converted.a = static_cast<std::uint32_t>(sourceTriangle.v0);
        converted.b = static_cast<std::uint32_t>(sourceTriangle.v1);
        converted.c = static_cast<std::uint32_t>(sourceTriangle.v2);
        converted.sourcePolygonId = sourceTriangle.faceId;
        out.triangles.push_back(converted);

        const auto triangleId = static_cast<std::int32_t>(triangleIndex);
        addEdge(converted.a, converted.b, triangleId, sourceTriangle.faceId);
        addEdge(converted.b, converted.c, triangleId, sourceTriangle.faceId);
        addEdge(converted.c, converted.a, triangleId, sourceTriangle.faceId);
    }

    out.edges.reserve(edgeMap.size());
    for (const auto& [key, info] : edgeMap)
    {
        Edge edge;
        edge.a = key.first;
        edge.b = key.second;
        edge.triangleA = info.triangleA;
        edge.triangleB = info.triangleB;
        edge.flags = EdgeCanonicalTopology;
        edge.renderMask = EdgeRenderTechnical | EdgeRenderElite;

        if (info.triangleB == NoIndex)
        {
            edge.flags |= EdgeBoundary | EdgePolygonBoundary;
        }
        else if (info.faceA >= 0 && info.faceA == info.faceB)
        {
            edge.flags |= EdgeTriangulationInternal;
            edge.renderMask = 0;
        }
        else
        {
            edge.flags |= EdgePolygonBoundary;
        }

        if (info.nonManifold)
            edge.flags |= EdgeNonManifold;

        out.edges.push_back(edge);
    }

    out.minBounds = source.minBounds;
    out.maxBounds = source.maxBounds;
    return out;
}

std::map<std::string, std::int32_t> semanticIndexByModule(
    const ObjectAssembly& assembly)
{
    std::map<std::string, std::int32_t> result;
    for (std::size_t i = 0; i < assembly.modules.size(); ++i)
    {
        const auto& module = assembly.modules[i];
        if (module.id.empty())
            throw std::runtime_error("legacy assembly contains a module with no id");

        if (!result.emplace(module.id, static_cast<std::int32_t>(i)).second)
            throw std::runtime_error("legacy assembly contains duplicate module id: " + module.id);
    }
    return result;
}

void appendSemanticNodes(ModelAsset& asset, const ObjectAssembly& assembly)
{
    const auto indexByModule = semanticIndexByModule(assembly);
    asset.nodes.reserve(assembly.modules.size());

    for (const auto& module : assembly.modules)
    {
        Node node;
        node.id = module.id;
        node.moduleId = module.id;
        node.localPosition = module.localPosition;
        node.localRotationDeg = module.localRotationDeg;
        node.pivot = module.pivot;
        node.enabled = true;

        if (!module.parentModuleId.empty())
        {
            const auto parent = indexByModule.find(module.parentModuleId);
            if (parent == indexByModule.end())
            {
                throw std::runtime_error(
                    "legacy assembly parent module not found: " + module.parentModuleId);
            }
            node.parentIndex = parent->second;
        }

        if (module.rotates)
        {
            node.joint.type = elite::model_asset::JointType::Revolute;
            node.joint.axis = module.rotationAxis;
            node.joint.defaultRateDegPerSec = module.rotationSpeed;
        }

        asset.nodes.push_back(std::move(node));
    }
}

RenderLod buildModuleLod(
    const ObjectAssembly& assembly,
    std::uint32_t level)
{
    RenderLod lod;
    lod.level = level;
    lod.sourceKind = "legacy_obj";
    lod.relativeGeometricError = (level == 0) ? 0.0f : -1.0f;
    lod.minBounds = assembly.minBounds;
    lod.maxBounds = assembly.maxBounds;

    const auto semanticIndex = semanticIndexByModule(assembly);
    std::map<std::string, std::int32_t> renderModuleIndex;

    // Keep one transform node per legacy module. Geometry nodes hang below it,
    // which preserves the old module/part separation without imposing that
    // structure on native v4 assets.
    for (const auto& module : assembly.modules)
    {
        RenderNode moduleNode;
        moduleNode.id = "legacy/module/" + module.id;
        moduleNode.semanticNodeIndex = semanticIndex.at(module.id);
        moduleNode.localPosition = module.localPosition;
        moduleNode.localRotationDeg = module.localRotationDeg;
        moduleNode.pivot = module.pivot;
        moduleNode.enabled = true;

        const auto index = static_cast<std::int32_t>(lod.nodes.size());
        renderModuleIndex.emplace(module.id, index);
        lod.nodes.push_back(std::move(moduleNode));
    }

    for (std::size_t i = 0; i < assembly.modules.size(); ++i)
    {
        const auto& module = assembly.modules[i];
        auto& moduleNode = lod.nodes[i];
        if (!module.parentModuleId.empty())
            moduleNode.parentIndex = renderModuleIndex.at(module.parentModuleId);
    }

    for (const auto& module : assembly.modules)
    {
        const auto parentRenderIndex = renderModuleIndex.at(module.id);
        const auto semanticNodeIndex = semanticIndex.at(module.id);

        for (const auto& part : module.meshes)
        {
            const std::string stableId = module.id + "/" + part.id;

            RenderGeometryDefinition geometry;
            geometry.id = stableId;
            geometry.sourcePath = (level == 0) ? part.lod0Path : part.lod1Path;
            geometry.mesh = convertMesh((level == 0) ? part.lod0Mesh : part.lod1Mesh);

            const auto geometryIndex =
                static_cast<std::int32_t>(lod.geometries.size());
            lod.geometries.push_back(std::move(geometry));

            RenderNode partNode;
            partNode.id = "legacy/part/" + stableId;
            partNode.parentIndex = parentRenderIndex;
            partNode.geometryIndex = geometryIndex;
            partNode.semanticNodeIndex = semanticNodeIndex;
            partNode.localPosition = part.localOffset;
            partNode.enabled = true;
            lod.nodes.push_back(std::move(partNode));
        }
    }

    lod.declaredGeometryCount =
        static_cast<std::uint32_t>(lod.geometries.size());
    lod.declaredNodeCount = static_cast<std::uint32_t>(lod.nodes.size());
    return lod;
}

RenderLod buildWholeProxyLod(const ObjectAssembly& assembly)
{
    RenderLod lod;
    lod.level = 1;
    lod.sourceKind = "legacy_obj_proxy";
    lod.relativeGeometricError = -1.0f;
    lod.minBounds = assembly.wholeShipProxyMesh.minBounds;
    lod.maxBounds = assembly.wholeShipProxyMesh.maxBounds;

    RenderGeometryDefinition geometry;
    geometry.id = "legacy/whole_ship_proxy";
    geometry.sourcePath = assembly.wholeShipProxyPath;
    geometry.mesh = convertMesh(assembly.wholeShipProxyMesh);
    lod.geometries.push_back(std::move(geometry));

    RenderNode node;
    node.id = "legacy/whole_ship_proxy";
    node.geometryIndex = 0;
    node.semanticNodeIndex = NoIndex;
    lod.nodes.push_back(std::move(node));

    lod.declaredGeometryCount = 1;
    lod.declaredNodeCount = 1;
    return lod;
}

} // namespace

elite::model_asset::ModelAsset LegacyAssemblyModelAdapter::convert(
    ObjectType typeId,
    const game::ship::geometry::ObjectAssembly& assembly)
{
    ModelAsset asset;
    asset.assetId = "legacy-object-" +
        std::to_string(static_cast<std::uint16_t>(typeId));
    asset.displayName = asset.assetId;
    asset.sourceObjectType = static_cast<std::uint16_t>(typeId);
    asset.lodSwitchDistance = assembly.lodSwitchDistance;

    // AssemblyMeshLibrary has already transformed legacy OBJ data into logical
    // game coordinates and normalized it to descriptor dimensions. Treat that
    // compatibility result as meter-space and never apply another scale.
    asset.sourceBasis.preset = "legacy_assembly_canonical";
    asset.sourceBasis.canonicalized = true;
    asset.physicalSize.geometrySpace = PhysicalGeometrySpace::Meters;
    asset.minBounds = assembly.minBounds;
    asset.maxBounds = assembly.maxBounds;

    appendSemanticNodes(asset, assembly);
    asset.renderLods.push_back(buildModuleLod(assembly, 0));

    if (assembly.hasWholeShipProxy)
        asset.renderLods.push_back(buildWholeProxyLod(assembly));
    else
        asset.renderLods.push_back(buildModuleLod(assembly, 1));

    return asset;
}

} // namespace game::assets
