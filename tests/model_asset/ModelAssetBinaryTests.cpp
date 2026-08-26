#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "src/model_asset/ModelAsset.h"
#include "src/model_asset/ModelAssetBinary.h"
#include "tools/model_asset_editor/NativeObjImporter.h"
#include "tools/model_asset_editor/GeometryInstanceFitter.h"

#include <glm/gtc/quaternion.hpp>

using namespace elite::model_asset;

namespace
{
void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

bool near(float a, float b, float eps = 1.0e-6f)
{
    return std::abs(a - b) <= eps;
}


void testRigidInstanceFitRecoversBakedTransform()
{
    GeometryDefinition reference;
    reference.id = "segment_reference";
    MeshLod lod;
    lod.vertices = {
        {{-2.0f, 0.0f, -1.0f},{0,1,0},{0,0}},
        {{ 3.0f, 0.0f, -1.0f},{0,1,0},{1,0}},
        {{ 3.0f, 1.0f,  2.0f},{0,1,0},{1,1}},
        {{-2.0f, 2.0f,  2.0f},{0,1,0},{0,1}}
    };
    lod.triangles = {
        {0,1,2,0,NoIndex,0},
        {0,2,3,0,NoIndex,0}
    };
    lod.minBounds = {-2.0f, 0.0f, -1.0f};
    lod.maxBounds = { 3.0f, 2.0f,  2.0f};
    reference.lods.push_back(lod);

    GeometryDefinition target = reference;
    target.id = "segment_baked_120";
    const glm::mat3 expectedRotation = glm::mat3_cast(glm::angleAxis(
        glm::radians(120.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
    const glm::vec3 expectedTranslation(17.0f, -3.0f, 8.0f);
    glm::vec3 minB(std::numeric_limits<float>::max());
    glm::vec3 maxB(std::numeric_limits<float>::lowest());
    for (auto& vertex : target.lods[0].vertices)
    {
        vertex.position = expectedRotation * vertex.position + expectedTranslation;
        vertex.normal = expectedRotation * vertex.normal;
        minB = glm::min(minB, vertex.position);
        maxB = glm::max(maxB, vertex.position);
    }
    target.lods[0].minBounds = minB;
    target.lods[0].maxBounds = maxB;

    const auto fit = elite::model_asset::editor::fitGeometryAsRigidInstance(
        reference, target);
    require(fit.valid, fit.message.c_str());
    require(fit.maxErrorMeters < 1.0e-4f,
        "rigid instance fit did not recover baked mesh transform");
    require(glm::length(fit.translation - expectedTranslation) < 1.0e-4f,
        "rigid instance fit recovered wrong translation");
    require(glm::length(fit.rotation * reference.lods[0].vertices[1].position +
                        fit.translation - target.lods[0].vertices[1].position) < 1.0e-4f,
        "rigid instance fit recovered wrong rotation");
}

void testRigidInstanceFitIgnoresLegacyObjIndexOrder()
{
    GeometryDefinition reference;
    reference.id = "segment_reference";
    MeshLod lod;
    lod.vertices = {
        {{-4.0f, 0.0f, -1.0f},{0,1,0},{0,0}},
        {{ 3.0f, 0.2f, -2.0f},{0,1,0},{1,0}},
        {{ 4.0f, 2.0f,  3.0f},{0,1,0},{1,1}},
        {{-2.0f, 3.0f,  2.0f},{0,1,0},{0,1}},
        {{ 0.5f, 5.0f, -0.5f},{0,1,0},{0.5f,0.5f}}
    };
    lod.triangles = {
        {0,1,4,0,NoIndex,0},
        {1,2,4,1,NoIndex,0},
        {2,3,4,2,NoIndex,0},
        {3,0,4,3,NoIndex,0}
    };
    lod.minBounds = {-4.0f, 0.0f, -2.0f};
    lod.maxBounds = { 4.0f, 5.0f,  3.0f};
    reference.lods.push_back(lod);

    const glm::mat3 expectedRotation = glm::mat3_cast(glm::angleAxis(
        glm::radians(120.0f), glm::normalize(glm::vec3(0.2f, 1.0f, 0.1f))));
    const glm::vec3 expectedTranslation(21.0f, -7.0f, 13.0f);

    GeometryDefinition target = reference;
    target.id = "segment_reindexed";
    const std::array<std::uint32_t, 5> order {{2, 4, 0, 3, 1}};
    std::array<std::uint32_t, 5> inverse {{0,0,0,0,0}};
    std::vector<Vertex> reordered;
    reordered.reserve(order.size());
    glm::vec3 minB(std::numeric_limits<float>::max());
    glm::vec3 maxB(std::numeric_limits<float>::lowest());
    for (std::uint32_t newIndex = 0; newIndex < order.size(); ++newIndex)
    {
        inverse[order[newIndex]] = newIndex;
        Vertex vertex = reference.lods[0].vertices[order[newIndex]];
        vertex.position = expectedRotation * vertex.position + expectedTranslation;
        vertex.normal = expectedRotation * vertex.normal;
        minB = glm::min(minB, vertex.position);
        maxB = glm::max(maxB, vertex.position);
        reordered.push_back(vertex);
    }
    target.lods[0].vertices = std::move(reordered);
    target.lods[0].triangles.clear();
    // Reorder both triangle list and the vertex numbering inside each face.
    for (auto it = reference.lods[0].triangles.rbegin();
         it != reference.lods[0].triangles.rend(); ++it)
    {
        Triangle t = *it;
        t.a = inverse[it->b];
        t.b = inverse[it->c];
        t.c = inverse[it->a];
        target.lods[0].triangles.push_back(t);
    }
    target.lods[0].minBounds = minB;
    target.lods[0].maxBounds = maxB;

    const auto fit = elite::model_asset::editor::fitGeometryAsRigidInstance(
        reference, target);
    require(fit.valid, fit.message.c_str());
    require(fit.maxErrorMeters < 1.0e-4f,
        "topology-independent fit did not recover reindexed duplicate");
    require(glm::length(fit.translation - expectedTranslation) < 1.0e-3f,
        "topology-independent fit recovered wrong translation");

    // A genuinely different surface must still be rejected even though its
    // vertex/triangle counts are unchanged.
    target.lods[0].vertices[0].position += glm::vec3(0.75f, 0.0f, 0.0f);
    const auto mismatch = elite::model_asset::editor::fitGeometryAsRigidInstance(
        reference, target);
    require(!mismatch.valid,
        "topology-independent fit accepted a deformed mesh");
}


void testRigidInstanceFitIgnoresDuplicatedSeamVertices()
{
    GeometryDefinition reference;
    reference.id = "segment_reference_seams";
    MeshLod lod;
    lod.vertices = {
        {{-2.0f, 0.0f, -1.0f},{0,1,0},{0,0}},
        {{ 2.0f, 0.0f, -1.0f},{0,1,0},{1,0}},
        {{ 2.0f, 0.0f,  1.0f},{0,1,0},{1,1}},
        {{-2.0f, 0.0f,  1.0f},{0,1,0},{0,1}}
    };
    lod.triangles = {
        {0,1,2,0,NoIndex,0},
        {0,2,3,0,NoIndex,0}
    };
    lod.minBounds = {-2.0f, 0.0f, -1.0f};
    lod.maxBounds = { 2.0f, 0.0f,  1.0f};
    reference.lods.push_back(lod);

    GeometryDefinition target = reference;
    target.id = "segment_baked_with_seams";
    // Same surface, but the second triangle uses duplicated OBJ corners as it
    // would after an exporter splits a normal/UV seam. Raw vertex counts differ
    // even though the rendered surface is identical.
    target.lods[0].vertices.push_back(target.lods[0].vertices[0]);
    target.lods[0].vertices.push_back(target.lods[0].vertices[2]);
    target.lods[0].vertices.push_back(target.lods[0].vertices[3]);
    target.lods[0].triangles[1].a = 4;
    target.lods[0].triangles[1].b = 5;
    target.lods[0].triangles[1].c = 6;

    const glm::mat3 expectedRotation = glm::mat3_cast(glm::angleAxis(
        glm::radians(120.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
    const glm::vec3 expectedTranslation(12.0f, 3.0f, -7.0f);
    glm::vec3 minB(std::numeric_limits<float>::max());
    glm::vec3 maxB(std::numeric_limits<float>::lowest());
    for (auto& vertex : target.lods[0].vertices)
    {
        vertex.position = expectedRotation * vertex.position + expectedTranslation;
        vertex.normal = expectedRotation * vertex.normal;
        minB = glm::min(minB, vertex.position);
        maxB = glm::max(maxB, vertex.position);
    }
    target.lods[0].minBounds = minB;
    target.lods[0].maxBounds = maxB;

    const auto fit = elite::model_asset::editor::fitGeometryAsRigidInstance(
        reference, target);
    require(fit.valid, fit.message.c_str());
    require(fit.maxErrorMeters < 1.0e-4f,
        "rigid instance fit rejected duplicated seam vertices");
}


void testRigidInstanceFitReportsMaterialDifferenceAfterGeometryMatch()
{
    GeometryDefinition reference;
    reference.id = "material_reference";
    MeshLod lod;
    lod.vertices = {
        {{0.0f, 0.0f, 0.0f},{0,1,0},{0,0}},
        {{2.0f, 0.0f, 0.0f},{0,1,0},{1,0}},
        {{0.0f, 1.0f, 1.0f},{0,1,0},{0,1}}
    };
    lod.triangles = {{0,1,2,0,3,0}};
    lod.minBounds = {0.0f, 0.0f, 0.0f};
    lod.maxBounds = {2.0f, 1.0f, 1.0f};
    reference.lods.push_back(lod);

    GeometryDefinition target = reference;
    target.id = "material_target";
    target.lods[0].triangles[0].materialIndex = 7;
    const glm::mat3 rotation = glm::mat3_cast(glm::angleAxis(
        glm::radians(120.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
    const glm::vec3 translation(9.0f, -2.0f, 4.0f);
    glm::vec3 minB(std::numeric_limits<float>::max());
    glm::vec3 maxB(std::numeric_limits<float>::lowest());
    for (auto& vertex : target.lods[0].vertices)
    {
        vertex.position = rotation * vertex.position + translation;
        vertex.normal = rotation * vertex.normal;
        minB = glm::min(minB, vertex.position);
        maxB = glm::max(maxB, vertex.position);
    }
    target.lods[0].minBounds = minB;
    target.lods[0].maxBounds = maxB;

    const auto fit = elite::model_asset::editor::fitGeometryAsRigidInstance(
        reference, target);
    require(!fit.valid,
        "material-mismatched instances were consolidated without an override contract");
    require(fit.geometryMatched,
        "material mismatch prevented geometric rigid fit diagnostics");
    require(!fit.materialCompatible,
        "material mismatch was not reported separately from geometry");
    require(fit.maxErrorMeters < 1.0e-4f,
        "material mismatch obscured the recovered geometric transform");
}


void testRigidInstanceFitAcceptsSingleMaterialLodAreaDrift()
{
    GeometryDefinition reference;
    reference.id = "lod_area_reference";

    MeshLod lod0;
    lod0.vertices = {
        {{0.0f, 0.0f, 0.0f},{0,1,0},{0,0}},
        {{2.0f, 0.0f, 0.0f},{0,1,0},{1,0}},
        {{0.0f, 2.0f, 0.0f},{0,1,0},{0,1}},
        {{0.0f, 0.0f, 1.0f},{0,1,0},{1,1}}
    };
    lod0.triangles = {
        {0,1,2,0,3,0},
        {0,3,1,1,3,0},
        {0,2,3,2,3,0}
    };
    lod0.minBounds = {0.0f, 0.0f, 0.0f};
    lod0.maxBounds = {2.0f, 2.0f, 1.0f};
    reference.lods.push_back(lod0);

    // LOD1 deliberately contains an extra overlapping triangle in the target.
    // Its absolute tessellated area therefore differs, but both meshes are
    // still semantically a single-material representation of the same points.
    MeshLod lod1 = lod0;
    reference.lods.push_back(lod1);

    GeometryDefinition target = reference;
    target.id = "lod_area_target";
    target.lods[1].triangles.push_back(target.lods[1].triangles.front());

    const glm::mat3 rotation = glm::mat3_cast(glm::angleAxis(
        glm::radians(120.0f), glm::vec3(1.0f, 0.0f, 0.0f)));
    const glm::vec3 translation(0.0f, 4.0f, -3.0f);
    for (auto& lod : target.lods)
    {
        glm::vec3 minB(std::numeric_limits<float>::max());
        glm::vec3 maxB(std::numeric_limits<float>::lowest());
        for (auto& vertex : lod.vertices)
        {
            vertex.position = rotation * vertex.position + translation;
            vertex.normal = rotation * vertex.normal;
            minB = glm::min(minB, vertex.position);
            maxB = glm::max(maxB, vertex.position);
        }
        lod.minBounds = minB;
        lod.maxBounds = maxB;
    }

    const auto fit = elite::model_asset::editor::fitGeometryAsRigidInstance(
        reference, target);
    require(fit.valid,
        "single-material LOD tessellation/area drift blocked safe instancing");
    require(fit.geometryMatched && fit.materialCompatible,
        "LOD area drift was misclassified as geometry/material mismatch");
}


void testRigidInstanceFitHandlesDegeneratePrincipalPlane()
{
    GeometryDefinition reference;
    reference.id = "radial_sector_reference";
    MeshLod lod;
    lod.vertices.push_back({{0.0f,0.0f,0.0f},{1,0,0},{0,0}});
    constexpr std::array<float, 8> degrees {{0.0f,20.0f,90.0f,110.0f,180.0f,200.0f,270.0f,290.0f}};
    for (float degree : degrees)
    {
        const float a = glm::radians(degree);
        lod.vertices.push_back({{0.0f,std::cos(a),std::sin(a)},{1,0,0},{0,0}});
    }
    for (std::uint32_t i = 0; i < 8; ++i)
    {
        const std::uint32_t next = (i + 1) % 8;
        lod.triangles.push_back({0, 1 + i, 1 + next, i, NoIndex, 0});
    }
    lod.minBounds = {0.0f,-1.0f,-1.0f};
    lod.maxBounds = {0.0f, 1.0f, 1.0f};
    reference.lods.push_back(lod);

    GeometryDefinition target = reference;
    target.id = "radial_sector_baked_120";
    const glm::mat3 expectedRotation = glm::mat3_cast(glm::angleAxis(
        glm::radians(120.0f), glm::vec3(1.0f, 0.0f, 0.0f)));
    const glm::vec3 expectedTranslation(3.0f, -5.0f, 7.0f);

    const std::array<std::uint32_t, 9> order {{5,2,8,0,4,7,1,6,3}};
    std::array<std::uint32_t, 9> inverse {{0,0,0,0,0,0,0,0,0}};
    std::vector<Vertex> reordered;
    reordered.reserve(order.size());
    glm::vec3 minB(std::numeric_limits<float>::max());
    glm::vec3 maxB(std::numeric_limits<float>::lowest());
    for (std::uint32_t newIndex = 0; newIndex < order.size(); ++newIndex)
    {
        inverse[order[newIndex]] = newIndex;
        Vertex vertex = reference.lods[0].vertices[order[newIndex]];
        vertex.position = expectedRotation * vertex.position + expectedTranslation;
        vertex.normal = expectedRotation * vertex.normal;
        minB = glm::min(minB, vertex.position);
        maxB = glm::max(maxB, vertex.position);
        reordered.push_back(vertex);
    }
    target.lods[0].vertices = std::move(reordered);
    target.lods[0].triangles.clear();
    for (auto it = reference.lods[0].triangles.rbegin();
         it != reference.lods[0].triangles.rend(); ++it)
    {
        Triangle t = *it;
        t.a = inverse[it->b];
        t.b = inverse[it->c];
        t.c = inverse[it->a];
        target.lods[0].triangles.push_back(t);
    }
    target.lods[0].minBounds = minB;
    target.lods[0].maxBounds = maxB;

    const auto fit = elite::model_asset::editor::fitGeometryAsRigidInstance(
        reference, target);
    require(fit.valid, fit.message.c_str());
    require(fit.maxErrorMeters < 1.0e-4f,
        "invariant landmark fit failed on PCA-degenerate radial geometry");
}

void testNativeImporterKeepsSmallValidTriangles()
{
    const auto path = std::filesystem::temp_directory_path() /
        "elite_model_asset_small_triangle.obj";
    {
        std::ofstream source(path);
        source << "v 0 0 0\n"
               << "v 0.01 0 0\n"
               << "v 0 0.01 0\n"
               << "f 1 2 3\n";
    }

    ModelAsset asset;
    MeshLod lod;
    std::string error;
    const bool ok = elite::model_asset::editor::importObjNative(
        path, asset, lod, &error);
    std::filesystem::remove(path);

    require(ok, error.c_str());
    require(lod.triangles.size() == 1,
        "native OBJ importer rejected a small but valid triangle");
}
}

int main()
{
    try
    {
        testNativeImporterKeepsSmallValidTriangles();
        testRigidInstanceFitRecoversBakedTransform();
        testRigidInstanceFitIgnoresLegacyObjIndexOrder();
        testRigidInstanceFitIgnoresDuplicatedSeamVertices();
        testRigidInstanceFitReportsMaterialDifferenceAfterGeometryMatch();
        testRigidInstanceFitAcceptsSingleMaterialLodAreaDrift();
        testRigidInstanceFitHandlesDegeneratePrincipalPlane();

        ModelAsset asset;
        asset.assetId = "station_test";
        asset.displayName = "Station Test";
        asset.sourceObjectType = 2;
        asset.sourceBasis.preset = "blender_model";
        asset.sourceBasis.up = AxisDirection::PositiveZ;
        asset.sourceBasis.forward = AxisDirection::NegativeY;
        asset.minBounds = {-10.0f, -20.0f, -30.0f};
        asset.maxBounds = {10.0f, 20.0f, 30.0f};

        MaterialDefinition material;
        material.id = "emit_nav_red";
        material.sourceName = "Emit.Nav.Red";
        material.emissiveColor = {1.0f, 0.1f, 0.05f};
        material.emissiveStrength = 2.0f;
        asset.materials.push_back(material);

        GeometryDefinition geometry;
        geometry.id = "habitat_segment";
        geometry.sourceLod0 = "segment.obj";
        geometry.surfaceMode = SurfaceMode::ThinTwoSided;
        MeshLod lod;
        lod.vertices = {
            {{0,0,0},{0,1,0},{0,0}},
            {{1,0,0},{0,1,0},{1,0}},
            {{0,0,1},{0,1,0},{0,1}}
        };
        lod.triangles.push_back({0,1,2,7,0,42});
        lod.edges.push_back({0,1,0,-1,EdgeBoundary,EdgeRenderElite});
        geometry.lods.push_back(lod);
        asset.geometries.push_back(geometry);

        Node a;
        a.id = "segment_a";
        a.geometryIndex = 0;
        a.joint.type = JointType::Revolute;
        a.joint.axis = {0.0f, 1.0f, 0.0f};
        a.joint.defaultRateDegPerSec = 2.0f;
        a.joint.breakable = true;
        a.joint.breakForceN = 250000.0f;
        a.physics.mode = MassPropertyMode::Manual;
        a.physics.massKg = 1200.0f;
        a.physics.centerOfMass = {0.2f, 0.0f, -0.1f};
        a.physics.inertiaDiagonal = {4.0f, 5.0f, 6.0f};
        a.physics.inertiaProducts = {0.1f, 0.2f, 0.3f};
        asset.nodes.push_back(a);
        Node b;
        b.id = "segment_b";
        b.geometryIndex = 0; // explicit instance of the same geometry definition
        b.localRotationDeg = {0.0f, 120.0f, 0.0f};
        asset.nodes.push_back(b);

        CollisionVolume box;
        box.id = "hit.segment.box";
        box.halfSize = {2.0f, 3.0f, 4.0f};
        asset.collisionVolumes.push_back(box);
        CollisionVolume capsule;
        capsule.id = "hit.segment.capsule";
        capsule.shape = CollisionShape::Capsule;
        capsule.radius = 1.25f;
        capsule.halfHeight = 3.5f;
        capsule.localRotationDeg = {0.0f, 0.0f, 45.0f};
        asset.collisionVolumes.push_back(capsule);

        Socket socket;
        socket.id = "light.nav.red";
        socket.kind = "light_point";
        socket.parentNodeIndex = 0;
        socket.localPosition = {0.0f, 1.0f, 2.0f};
        socket.light.type = LightType::Point;
        socket.light.color = {1.0f, 0.0f, 0.0f};
        socket.light.intensity = 12.0f;
        socket.light.rangeMeters = 40.0f;
        asset.sockets.push_back(socket);

        const auto path = std::filesystem::temp_directory_path() / "elite_model_asset_roundtrip.elmodel";
        std::string error;
        require(ModelAssetBinary::save(path.string(), asset, &error), error.c_str());

        ModelAsset loaded;
        require(ModelAssetBinary::load(path.string(), loaded, &error), error.c_str());
        std::filesystem::remove(path);

        require(loaded.assetId == asset.assetId, "asset id lost");
        require(loaded.sourceBasis.preset == "blender_model" && loaded.sourceBasis.up == AxisDirection::PositiveZ,
            "source basis metadata lost");
        require(loaded.materials.size() == 1 && loaded.materials[0].id == "emit_nav_red" && near(loaded.materials[0].emissiveStrength, 2.0f),
            "material semantic id/emissive data lost");
        require(loaded.geometries.size() == 1, "geometry count changed");
        require(loaded.geometries[0].lods[0].triangles[0].materialIndex == 0 && loaded.geometries[0].lods[0].triangles[0].smoothingGroupId == 42,
            "triangle material/smoothing data lost");
        require(loaded.nodes.size() == 2, "node count changed");
        require(loaded.nodes[0].geometryIndex == loaded.nodes[1].geometryIndex,
            "geometry instancing was expanded or lost");
        require(loaded.nodes[0].joint.type == JointType::Revolute && loaded.nodes[0].joint.breakable,
            "joint/break metadata lost");
        require(loaded.nodes[0].physics.mode == MassPropertyMode::Manual && near(loaded.nodes[0].physics.massKg, 1200.0f) && near(loaded.nodes[0].physics.inertiaProducts.z, 0.3f),
            "rigid mass properties lost");
        require(loaded.geometries[0].surfaceMode == SurfaceMode::ThinTwoSided,
            "thin-surface mode lost");
        require(loaded.geometries[0].lods[0].edges[0].renderMask == EdgeRenderElite,
            "edge render mask lost");
        require(loaded.collisionVolumes.size() == 2 && loaded.collisionVolumes[1].shape == CollisionShape::Capsule && near(loaded.collisionVolumes[1].halfHeight, 3.5f),
            "compound collision primitives lost");
        require(loaded.sockets.size() == 1 && loaded.sockets[0].light.type == LightType::Point && near(loaded.sockets[0].light.intensity, 12.0f),
            "typed light anchor lost");

        std::cout << "[PASS] model asset v2 roundtrip preserves materials/instances/collision/joints/mass/lights\n";
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "[FAIL] " << ex.what() << '\n';
        return 1;
    }
}
