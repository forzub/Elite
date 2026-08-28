#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <set>
#include <vector>

#include "src/model_asset/ModelAsset.h"
#include "src/model_asset/ModelAssetIdentity.h"
#include "src/model_asset/ModelAssetBinary.h"
#include "src/model_asset/ModelAssetMigration.h"
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


std::vector<char> readFileBytes(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::vector<char>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
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
        lod.triangles.push_back({0, 1 + i, 1 + next, static_cast<std::int32_t>(i), NoIndex, 0});
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
        asset.sourceBasis.preset = "game_current";
        asset.minBounds = {-10.0f, -20.0f, -30.0f};
        asset.maxBounds = {10.0f, 20.0f, 30.0f};

        MaterialDefinition material;
        material.id = "hull_outer";
        material.sourceName = "Hull.Outer";
        material.baseColor = {0.4f, 0.45f, 0.5f, 1.0f};
        asset.materials.push_back(material);

        Node a;
        a.id = "habitat_a";
        a.moduleId = "habitat";
        a.defaultStateId = "intact";
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

        Node b = a;
        b.id = "habitat_b";
        b.localRotationDeg = {0.0f, 120.0f, 0.0f};
        asset.nodes.push_back(b);

        StateVariant breached;
        breached.id = "breached";
        breached.displayName = "Breached";
        breached.nodeIndex = 1;
        breached.transformOverride = true;
        breached.localPosition = {0.15f, -0.02f, 0.08f};
        breached.localRotationDeg = {3.0f, 120.0f, -1.0f};
        breached.pivot = {-0.5f, 0.0f, 0.0f};
        breached.physicsOverride = true;
        breached.physics = b.physics;
        breached.physics.massKg = 980.0f;
        breached.detached = false;
        asset.stateVariants.push_back(breached);

        CollisionVolume intactCollision;
        intactCollision.id = "hit.habitat_b.intact";
        intactCollision.parentNodeIndex = 1;
        intactCollision.shape = CollisionShape::Box;
        intactCollision.halfSize = {2.0f, 3.0f, 4.0f};
        intactCollision.activeStates = {"intact"};
        asset.collisionVolumes.push_back(intactCollision);

        CollisionVolume breachedCollision = intactCollision;
        breachedCollision.id = "hit.habitat_b.breached";
        breachedCollision.halfSize = {1.2f, 3.0f, 4.0f};
        breachedCollision.activeStates = {"breached"};
        asset.collisionVolumes.push_back(breachedCollision);

        Socket sparks;
        sparks.id = "vfx.breach.sparks";
        sparks.kind = "vfx";
        sparks.parentNodeIndex = 1;
        sparks.localPosition = {0.0f, 0.5f, 1.0f};
        sparks.activeStates = {"breached"};
        asset.sockets.push_back(sparks);

        HitRegion hit;
        hit.id = "damage.exposed_interior";
        hit.parentNodeIndex = 1;
        hit.activeStates = {"breached"};
        hit.localPosition = {0.0f, 0.0f, 0.5f};
        hit.halfSize = {1.0f, 1.5f, 1.0f};
        asset.hitRegions.push_back(hit);

        Opening opening;
        opening.id = "breach.main";
        opening.parentNodeIndex = 1;
        opening.activeStates = {"breached"};
        opening.localPosition = {0.0f, 0.0f, 1.0f};
        opening.halfSize = {0.8f, 1.0f, 0.6f};
        opening.traversable = true;
        opening.lineOfFire = true;
        asset.openings.push_back(opening);

        RepairTarget repair;
        repair.id = "repair.breach.main";
        repair.kind = "hull_patch";
        repair.parentNodeIndex = 1;
        repair.activeStates = {"breached"};
        repair.localPosition = {0.0f, 0.0f, 1.0f};
        repair.repairedStateId = "intact";
        asset.repairTargets.push_back(repair);

        auto makeMesh = [](float scale, std::int32_t polygonId) {
            MeshLod mesh;
            mesh.vertices = {
                {{0,0,0},{0,1,0},{0,0}},
                {{scale,0,0},{0,1,0},{1,0}},
                {{0,0,scale},{0,1,0},{0,1}}
            };
            mesh.triangles.push_back({0,1,2,polygonId,0,42});
            mesh.edges.push_back({0,1,0,-1,EdgeBoundary,EdgeRenderElite});
            mesh.minBounds = {0.0f, 0.0f, 0.0f};
            mesh.maxBounds = {scale, 0.0f, scale};
            return mesh;
        };

        // LOD0: detailed assembly. Two render nodes instance the same intact
        // habitat geometry; the breached state substitutes a different mesh.
        RenderLod lod0;
        lod0.level = 0;
        lod0.sourceKind = "source";
        RenderGeometryDefinition detail;
        detail.id = "habitat_detail";
        detail.sourcePath = "assets/models/stations/LOD0/habitat.obj";
        detail.surfaceMode = SurfaceMode::ThinTwoSided;
        detail.mesh = makeMesh(1.0f, 7);
        lod0.geometries.push_back(detail);
        RenderGeometryDefinition breachGeometry;
        breachGeometry.id = "habitat_breached_detail";
        breachGeometry.sourcePath = "assets/models/stations/damage/habitat_breached.obj";
        breachGeometry.mesh = makeMesh(1.25f, 8);
        lod0.geometries.push_back(breachGeometry);
        RenderNode ra;
        ra.id = "habitat_a.render";
        ra.geometryIndex = 0;
        ra.semanticNodeIndex = 0;
        lod0.nodes.push_back(ra);
        RenderNode rb;
        rb.id = "habitat_b.intact.render";
        rb.geometryIndex = 0; // true LOD-local instance of habitat_a geometry
        rb.semanticNodeIndex = 1;
        rb.activeStates = {"intact"};
        rb.localRotationDeg = {0.0f, 120.0f, 0.0f};
        lod0.nodes.push_back(rb);
        RenderNode rbDamaged = rb;
        rbDamaged.id = "habitat_b.breached.render";
        rbDamaged.geometryIndex = 1;
        rbDamaged.activeStates = {"breached"};
        lod0.nodes.push_back(rbDamaged);
        asset.renderLods.push_back(lod0);

        // LOD1: deliberately unrelated render graph: one welded shell.
        RenderLod lod1;
        lod1.level = 1;
        lod1.sourceKind = "source";
        RenderGeometryDefinition shell;
        shell.id = "station_welded_shell";
        shell.sourcePath = "assets/models/stations/LOD1/station_shell.obj";
        shell.mesh = makeMesh(4.0f, 99);
        lod1.geometries.push_back(shell);
        RenderNode shellNode;
        shellNode.id = "station_shell.render";
        shellNode.geometryIndex = 0;
        shellNode.semanticNodeIndex = NoIndex;
        lod1.nodes.push_back(shellNode);
        asset.renderLods.push_back(lod1);

        // LOD2: a couple of coarse proxy primitives, again with no structural
        // dependency on either LOD0 or LOD1.
        RenderLod lod2;
        lod2.level = 2;
        lod2.sourceKind = "manual";
        RenderGeometryDefinition proxyA;
        proxyA.id = "proxy_core";
        proxyA.mesh = makeMesh(8.0f, 200);
        lod2.geometries.push_back(proxyA);
        RenderGeometryDefinition proxyB;
        proxyB.id = "proxy_ring";
        proxyB.mesh = makeMesh(12.0f, 201);
        lod2.geometries.push_back(proxyB);
        RenderNode proxyNodeA;
        proxyNodeA.id = "proxy_core.render";
        proxyNodeA.geometryIndex = 0;
        lod2.nodes.push_back(proxyNodeA);
        RenderNode proxyNodeB;
        proxyNodeB.id = "proxy_ring.render";
        proxyNodeB.geometryIndex = 1;
        lod2.nodes.push_back(proxyNodeB);
        asset.renderLods.push_back(lod2);

        const auto path = std::filesystem::temp_directory_path() / "elite_model_asset_v4_roundtrip.elmodel";
        std::string error;
        require(ModelAssetBinary::save(path.string(), asset, &error), error.c_str());
        const auto lod0Path = ModelAssetBinary::lodPayloadPath(path.string(), 0);
        const auto lod1Path = ModelAssetBinary::lodPayloadPath(path.string(), 1);
        const auto lod2Path = ModelAssetBinary::lodPayloadPath(path.string(), 2);
        require(std::filesystem::exists(lod0Path), "v4 LOD0 payload was not written");
        require(std::filesystem::exists(lod1Path), "v4 LOD1 payload was not written");
        require(std::filesystem::exists(lod2Path), "v4 LOD2 payload was not written");

        ModelAsset manifestOnly;
        bool legacyPackage = true;
        require(ModelAssetBinary::loadManifest(path.string(), manifestOnly, &legacyPackage, &error), error.c_str());
        require(!legacyPackage, "v4 manifest was misclassified as legacy");
        require(manifestOnly.formatVersion == 4 && manifestOnly.renderLods.size() == 3,
            "v4 manifest did not preserve render LOD descriptors");
        require(manifestOnly.renderLods[0].geometries.empty() && manifestOnly.renderLods[1].geometries.empty(),
            "manifest-only load eagerly pulled heavy render graphs");
        require(ModelAssetBinary::loadLod(path.string(), manifestOnly, 0, &error), error.c_str());
        require(manifestOnly.renderLods[0].geometries.size() == 2 && manifestOnly.renderLods[1].geometries.empty(),
            "LOD0-only load also loaded sibling render graphs");
        require(manifestOnly.renderLods[0].nodes[0].geometryIndex == manifestOnly.renderLods[0].nodes[1].geometryIndex,
            "LOD0 render instancing was lost");

        const auto lod1Before = readFileBytes(lod1Path);
        manifestOnly.renderLods[0].geometries[0].mesh.vertices[0].position.x += 0.125f;
        require(ModelAssetBinary::saveLod(path.string(), manifestOnly, 0, &error), error.c_str());
        require(readFileBytes(lod1Path) == lod1Before,
            "saving LOD0 rewrote or changed independent LOD1 payload");

        const auto lod0BeforeManifestSave = readFileBytes(lod0Path);
        manifestOnly.nodes[0].localPosition.x += 3.0f;
        require(ModelAssetBinary::saveManifest(path.string(), manifestOnly, &error), error.c_str());
        require(readFileBytes(lod0Path) == lod0BeforeManifestSave,
            "saving semantic manifest rewrote LOD0 payload");
        require(readFileBytes(lod1Path) == lod1Before,
            "saving semantic manifest rewrote LOD1 payload");

        ModelAsset loaded;
        require(ModelAssetBinary::load(path.string(), loaded, &error), error.c_str());
        require(loaded.assetId == asset.assetId, "asset id lost");
        require(loaded.nodes.size() == 2 && loaded.nodes[0].geometryIndex == NoIndex,
            "semantic nodes retained a render-LOD geometry dependency");
        require(loaded.stateVariants.size() == 1 && loaded.stateVariants[0].id == "breached",
            "semantic damage state lost");
        require(loaded.stateVariants[0].transformOverride && near(loaded.stateVariants[0].localPosition.x, 0.15f) &&
                near(loaded.stateVariants[0].pivot.x, -0.5f),
            "state transform/pivot override lost");
        require(loaded.stateVariants[0].physicsOverride && near(loaded.stateVariants[0].physics.massKg, 980.0f),
            "state rigid-body override lost");
        require(loaded.collisionVolumes.size() == 2 && loaded.collisionVolumes[1].activeStates.size() == 1 &&
                loaded.collisionVolumes[1].activeStates[0] == "breached",
            "state-scoped collision lost");
        require(loaded.hitRegions.size() == 1 && loaded.hitRegions[0].activeStates[0] == "breached",
            "state-scoped hit region lost");
        require(loaded.openings.size() == 1 && loaded.openings[0].traversable && loaded.openings[0].lineOfFire,
            "breach opening semantics lost");
        require(loaded.repairTargets.size() == 1 && loaded.repairTargets[0].repairedStateId == "intact",
            "repair target semantics lost");
        require(loaded.renderLods.size() == 3, "independent render LOD count changed");
        require(loaded.renderLods[0].geometries.size() == 2 && loaded.renderLods[0].nodes.size() == 3,
            "detailed LOD0 render graph lost");
        require(loaded.renderLods[0].nodes[0].geometryIndex == loaded.renderLods[0].nodes[1].geometryIndex,
            "LOD-local geometry instancing was expanded or lost");
        require(loaded.renderLods[1].geometries.size() == 1 && loaded.renderLods[1].nodes.size() == 1 &&
                loaded.renderLods[1].geometries[0].id == "station_welded_shell",
            "unrelated welded LOD1 graph was forced into LOD0 structure");
        require(loaded.renderLods[2].geometries.size() == 2 && loaded.renderLods[2].nodes.size() == 2,
            "coarse LOD2 proxy graph was lost");

        // Legacy v2/v3-style shared geometry migrates once into independent
        // render graphs; after migration each LOD can diverge freely.
        ModelAsset legacy;
        legacy.assetId = "legacy_station";
        GeometryDefinition legacyGeometry;
        legacyGeometry.id = "legacy_habitat";
        legacyGeometry.sourceLods = {"lod0.obj", "lod1.obj"};
        legacyGeometry.lods = {makeMesh(1.0f, 1), makeMesh(3.0f, 2)};
        legacy.geometries.push_back(legacyGeometry);
        Node legacyA; legacyA.id = "legacy_a"; legacyA.geometryIndex = 0;
        Node legacyB; legacyB.id = "legacy_b"; legacyB.geometryIndex = 0; legacyB.localRotationDeg = {0.0f, 120.0f, 0.0f};
        legacy.nodes = {legacyA, legacyB};
        buildIndependentRenderLodsFromLegacy(legacy);
        require(legacy.renderLods.size() == 2, "legacy migration did not create one independent graph per LOD");
        require(legacy.renderLods[0].geometries.size() == 1 && legacy.renderLods[1].geometries.size() == 1,
            "legacy migration did not split per-LOD geometry pools");
        require(legacy.renderLods[0].nodes.size() == 2 && legacy.renderLods[1].nodes.size() == 2,
            "legacy migration did not seed per-LOD render hierarchies");
        require(legacy.renderLods[0].nodes[0].geometryIndex == legacy.renderLods[0].nodes[1].geometryIndex,
            "legacy instance relation was not preserved inside migrated LOD0");
        require(legacy.nodes[0].geometryIndex == NoIndex && legacy.nodes[1].geometryIndex == NoIndex,
            "legacy render binding leaked into semantic nodes after migration");

        // Stable source identity: a module and its child mesh are allowed to use
        // the same source-facing name, but they must not become the same Node ID.
        std::set<std::string> stationNodeIds {"station_solar_panels"};
        const auto solarMeshId = allocateChildStableId(
            "station_solar_panels", "station_solar_panels", "mesh", stationNodeIds);
        require(solarMeshId == "station_solar_panels.mesh",
            "moduleId == meshId did not receive deterministic child-qualified identity");
        stationNodeIds.insert(solarMeshId);
        require(allocateChildStableId(
            "station_solar_panels", "station_solar_panels", "mesh", stationNodeIds) ==
            "station_solar_panels.mesh.2",
            "repeated child identity did not receive deterministic numeric suffix");

        // Legacy assets may already contain duplicate semantic IDs. Migration must
        // repair those before copying semantic identity into independent RenderNodes.
        ModelAsset legacyDuplicateIds;
        legacyDuplicateIds.assetId = "legacy_duplicate_ids";
        GeometryDefinition duplicateGeometry = legacyGeometry;
        duplicateGeometry.lods.resize(1);
        duplicateGeometry.sourceLods.resize(1);
        legacyDuplicateIds.geometries.push_back(duplicateGeometry);
        Node duplicateModule; duplicateModule.id = "station_solar_panels";
        Node duplicateMesh; duplicateMesh.id = "station_solar_panels"; duplicateMesh.parentIndex = 0; duplicateMesh.geometryIndex = 0;
        legacyDuplicateIds.nodes = {duplicateModule, duplicateMesh};
        buildIndependentRenderLodsFromLegacy(legacyDuplicateIds);
        require(legacyDuplicateIds.nodes[0].id == "station_solar_panels" &&
                legacyDuplicateIds.nodes[1].id == "station_solar_panels.2",
            "legacy migration did not deterministically repair duplicate semantic Node IDs");
        require(legacyDuplicateIds.renderLods[0].nodes[0].id == "station_solar_panels" &&
                legacyDuplicateIds.renderLods[0].nodes[1].id == "station_solar_panels.2",
            "legacy migration copied duplicate semantic IDs into LOD0 RenderNodes");
        error.clear();
        require(ModelAssetBinary::validate(legacyDuplicateIds, &error),
            "legacy duplicate-ID repair still produced an invalid v4 asset");

        // Preflight diagnostics must identify the actual duplicate and both indices.
        ModelAsset invalidRenderIds = loaded;
        invalidRenderIds.renderLods[0].nodes[1].id = invalidRenderIds.renderLods[0].nodes[0].id;
        const std::string duplicateRenderId = invalidRenderIds.renderLods[0].nodes[0].id;
        error.clear();
        require(!ModelAssetBinary::validate(invalidRenderIds, &error),
            "duplicate RenderNode ID unexpectedly passed v4 preflight");
        require(error.find("LOD0 duplicate RenderNode id '") != std::string::npos &&
                error.find(duplicateRenderId) != std::string::npos &&
                error.find("node[0]") != std::string::npos && error.find("node[1]") != std::string::npos,
            "duplicate RenderNode diagnostic does not identify ID and both indices");

        ModelAsset invalidSemanticIds = loaded;
        invalidSemanticIds.nodes[1].id = invalidSemanticIds.nodes[0].id;
        error.clear();
        require(!ModelAssetBinary::validate(invalidSemanticIds, &error),
            "duplicate semantic Node ID unexpectedly passed v4 preflight");
        require(error.find("duplicate semantic Node id '") != std::string::npos &&
                error.find("node[0]") != std::string::npos && error.find("node[1]") != std::string::npos,
            "duplicate semantic Node diagnostic does not identify both indices");

        ModelAsset oneLod = loaded;
        oneLod.renderLods.resize(1);
        require(ModelAssetBinary::save(path.string(), oneLod, &error), error.c_str());
        require(!std::filesystem::exists(lod1Path) && !std::filesystem::exists(lod2Path),
            "stale render LOD payloads survived a reduced-LOD save");

        std::filesystem::remove(path);
        std::filesystem::remove(lod0Path);
        std::cout << "[PASS] model asset v4 semantic states + independent render LOD graphs preserve damage/repair semantics\n";
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "[FAIL] " << ex.what() << '\n';
        return 1;
    }
}
