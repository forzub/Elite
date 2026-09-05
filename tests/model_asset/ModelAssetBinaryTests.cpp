#include <algorithm>
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
#include "src/model_asset/ModelAssetLodSelection.h"
#include "src/model_asset/ModelAssetBinary.h"
#include "src/model_asset/ModelAssetMigration.h"
#include "src/model_asset/ModelAssetVariantNaming.h"
#include "tools/model_asset_editor/NativeObjImporter.h"
#include "tools/model_asset_editor/CanonicalMeshBuilder.h"
#include "src/model_asset/RuntimeMeshNormalizer.h"
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

void testNativeImporterDoesNotMarkFanDiagonalNonManifold()
{
    const auto path = std::filesystem::temp_directory_path() /
        "elite_model_asset_fan_diagonal.obj";
    {
        std::ofstream source(path);
        source << "v 0 0 0\n"
               << "v 2 0 0\n"
               << "v 2 2 0\n"
               << "v 0 2 0\n"
               << "v 1 1 1\n"
               // Quad fan creates internal triangulation diagonal 1-3.
               << "f 1 2 3 4\n"
               // Another polygon uses 1-3 as a real perimeter edge. This is
               // not a three-face source edge and must not be flagged NM.
               << "f 1 3 5\n";
    }

    ModelAsset asset;
    MeshLod lod;
    std::string error;
    const bool ok = elite::model_asset::editor::importObjNative(path, asset, lod, &error);
    std::filesystem::remove(path);
    require(ok, error.c_str());
    for (const auto& edge : lod.edges)
        require((edge.flags & EdgeNonManifold) == 0,
            "fan triangulation diagonal was misclassified as source non-manifold");
}
}


MeshLod makeCanonicalCube(bool removeTop = false)
{
    MeshLod mesh;
    mesh.vertices = {
        {{-1,-1,-1},{0,0,0},{0,0}}, {{ 1,-1,-1},{0,0,0},{1,0}},
        {{ 1, 1,-1},{0,0,0},{1,1}}, {{-1, 1,-1},{0,0,0},{0,1}},
        {{-1,-1, 1},{0,0,0},{0,0}}, {{ 1,-1, 1},{0,0,0},{1,0}},
        {{ 1, 1, 1},{0,0,0},{1,1}}, {{-1, 1, 1},{0,0,0},{0,1}}
    };
    // Outward winding.
    mesh.triangles = {
        {0,2,1,0,0,0},{0,3,2,0,0,0},       // -Z
        {4,5,6,1,0,0},{4,6,7,1,0,0},       // +Z
        {0,1,5,2,0,0},{0,5,4,2,0,0},       // -Y
        {3,7,6,3,0,0},{3,6,2,3,0,0},       // +Y
        {0,4,7,4,0,0},{0,7,3,4,0,0},       // -X
        {1,2,6,5,0,0},{1,6,5,5,0,0}        // +X
    };
    if (removeTop)
        mesh.triangles.erase(mesh.triangles.begin() + 6, mesh.triangles.begin() + 8);
    mesh.minBounds = {-1,-1,-1};
    mesh.maxBounds = { 1, 1, 1};
    return mesh;
}

void testCanonicalBuilderRepairsWindingAndOutwardNormals()
{
    using namespace elite::model_asset::editor;

    auto locallyReversed = makeCanonicalCube(false);
    std::swap(locallyReversed.triangles[0].b, locallyReversed.triangles[0].c);
    std::swap(locallyReversed.triangles[7].b, locallyReversed.triangles[7].c);
    const auto localBuild = canonicalizeMesh(locallyReversed);
    require(localBuild.success, localBuild.error.c_str());
    require(localBuild.flippedTriangles >= 2,
        "canonical builder did not repair locally reversed cube triangles");
    const auto localAudit = analyzeCanonicalMesh(locallyReversed);
    require(localAudit.windingFlipsRequired == 0 && localAudit.windingConflicts == 0 &&
            localAudit.insideOutClosedComponents == 0,
        "canonical builder left inconsistent or inward winding after local repair");

    auto insideOut = makeCanonicalCube(false);
    for (auto& triangle : insideOut.triangles)
        std::swap(triangle.b, triangle.c);
    const auto insideOutBuild = canonicalizeMesh(insideOut);
    require(insideOutBuild.success, insideOutBuild.error.c_str());
    require(insideOutBuild.raycastPatches >= 1 && insideOutBuild.raycastFlippedTriangles > 0,
        "Embree raycast did not reorient an inside-out closed shell");
    const auto insideAudit = analyzeCanonicalMesh(insideOut);
    require(insideAudit.closedComponents == 1 && insideAudit.insideOutClosedComponents == 0 &&
            insideAudit.windingFlipsRequired == 0,
        "closed shell remained inward after canonical preparation");

    for (const auto& triangle : insideOut.triangles)
    {
        const glm::vec3 a = insideOut.vertices[triangle.a].position;
        const glm::vec3 b = insideOut.vertices[triangle.b].position;
        const glm::vec3 c = insideOut.vertices[triangle.c].position;
        const glm::vec3 faceNormal = glm::normalize(glm::cross(b - a, c - a));
        const glm::vec3 faceCenter = (a + b + c) / 3.0f;
        require(glm::dot(faceNormal, faceCenter) > 0.0f,
            "closed cube face normal does not point outward after preparation");
    }
}

void testCanonicalBuilderClosedPlateBreachContracts()
{
    using namespace elite::model_asset::editor;

    auto cube = makeCanonicalCube(false);
    const auto cubeBuild = canonicalizeMesh(cube);
    require(cubeBuild.success, cubeBuild.error.c_str());
    const auto cubeAudit = analyzeCanonicalMesh(cube);
    require(!cubeAudit.structuralInvalid && cubeAudit.closedComponents == 1 &&
            cubeAudit.openComponents == 0 && cubeAudit.boundaryEdges == 0,
        "canonical closed cube did not remain one closed volume");
    require(cube.edges.size() == 18,
        "canonical builder did not rebuild cube edge topology from final triangles");

    MeshLod plate;
    plate.vertices = {
        {{0,0,0},{0,0,0},{0,0}}, {{2,0,0},{0,0,0},{1,0}},
        {{2,2,0},{0,0,0},{1,1}}, {{0,2,0},{0,0,0},{0,1}}
    };
    plate.triangles = {{0,1,2,0,0,1},{0,2,3,0,0,1}};
    plate.minBounds = {0,0,0}; plate.maxBounds = {2,2,0};
    const auto plateBuild = canonicalizeMesh(plate);
    require(plateBuild.success, plateBuild.error.c_str());
    const auto plateAudit = analyzeCanonicalMesh(plate);
    require(!plateAudit.structuralInvalid && plateAudit.openComponents == 1 &&
            plateAudit.boundaryEdges == 4,
        "canonical thin sheet was incorrectly closed or rejected");

    auto breached = makeCanonicalCube(true);
    const std::size_t before = breached.triangles.size();
    const auto breachedBuild = canonicalizeMesh(breached);
    require(breachedBuild.success, breachedBuild.error.c_str());
    const auto breachedAudit = analyzeCanonicalMesh(breached);
    require(!breachedAudit.structuralInvalid && breachedAudit.openComponents == 1 &&
            breachedAudit.boundaryEdges == 4,
        "canonical breached volume did not preserve its authored opening");
    require(breached.triangles.size() == before,
        "canonical builder filled an authored breach");
}

void testCanonicalBuilderOrientsBreachedShellWithEmbree()
{
    using namespace elite::model_asset::editor;

    auto breached = makeCanonicalCube(true);
    for (auto& triangle : breached.triangles)
        std::swap(triangle.b, triangle.c);

    const auto built = canonicalizeMesh(breached);
    require(built.success, built.error.c_str());
    require(built.raycastPatches >= 1 && built.raycastFlippedTriangles > 0,
        "Embree raycast did not reorient an inward breached/open shell");

    const auto audit = analyzeCanonicalMesh(breached);
    require(!audit.structuralInvalid && audit.openComponents == 1 && audit.windingFlipsRequired == 0,
        "breached shell remained topologically inconsistent after preparation");
    require(audit.boundaryEdges == 4,
        "breached shell opening was filled while orienting the parent shell");

    for (const auto& triangle : breached.triangles)
    {
        const glm::vec3 a = breached.vertices[triangle.a].position;
        const glm::vec3 b = breached.vertices[triangle.b].position;
        const glm::vec3 c = breached.vertices[triangle.c].position;
        const glm::vec3 faceNormal = glm::normalize(glm::cross(b - a, c - a));
        const glm::vec3 faceCenter = (a + b + c) / 3.0f;
        require(glm::dot(faceNormal, faceCenter) > 0.0f,
            "breached cube face normal does not point outward after Embree repair");
    }
}

void testCanonicalBuilderRemovesGarbageAndPreservesUvSeams()
{
    using namespace elite::model_asset::editor;
    MeshLod mesh;
    mesh.vertices = {
        {{0,0,0},{0,0,-1},{0,0}}, {{2,0,0},{0,0,-1},{1,0}}, {{2,2,0},{0,0,-1},{1,1}},
        // Same geometric corners, deliberately split by UVs for triangle two.
        {{0,0,0},{0,0,1},{0.25f,0}}, {{2,2,0},{0,0,1},{0.75f,1}}, {{0,2,0},{0,0,1},{0,1}}
    };
    mesh.triangles = {
        {0,1,2,0,3,7},
        {3,4,5,0,3,7},
        {2,1,0,9,3,7}, // reversed duplicate of the first triangle
        {0,0,1,10,3,7} // degenerate garbage
    };
    mesh.minBounds = {0,0,0}; mesh.maxBounds = {2,2,0};

    const auto built = canonicalizeMesh(mesh);
    require(built.success, built.error.c_str());
    require(built.removedDuplicateTriangles == 1,
        "canonical builder did not remove a duplicate geometric triangle");
    require(built.removedDegenerateTriangles == 1,
        "canonical builder did not remove a collapsed triangle");
    require(mesh.triangles.size() == 2,
        "canonical cleanup produced the wrong triangle count");
    const auto audit = analyzeCanonicalMesh(mesh);
    require(audit.geometricPoints == 4,
        "canonical point weld did not merge coincident OBJ corners");
    require(mesh.vertices.size() == 6,
        "canonical builder destroyed UV render-vertex seams");
    require(glm::dot(mesh.vertices[0].normal, mesh.vertices[3].normal) > 0.999f,
        "UV seam vertices did not receive the same canonical smooth normal");
}

void testCanonicalBuilderCollapsesAuthoredNormalOnlySplits()
{
    using namespace elite::model_asset::editor;
    MeshLod mesh;
    mesh.vertices = {
        {{0,0,0},{0,0,-1},{0,0}}, {{2,0,0},{0,0,-1},{1,0}}, {{2,2,0},{0,0,-1},{1,1}},
        // Same geometric corners within the 1e-4 weld bucket, same UVs, but
        // deliberately different authored normals / source render identities.
        {{0.00004f,0,0},{1,0,0},{0,0}}, {{2.00004f,2,0},{1,0,0},{1,1}}, {{0,2,0},{1,0,0},{0,1}}
    };
    mesh.triangles = {
        {0,1,2,0,0,7},
        {3,4,5,1,0,7}
    };
    mesh.minBounds = {0,0,0}; mesh.maxBounds = {2.00004f,2,0};

    const auto built = canonicalizeMesh(mesh);
    require(built.success, built.error.c_str());
    const auto audit = analyzeCanonicalMesh(mesh);
    require(audit.geometricPoints == 4,
        "canonical weld did not identify near-coincident geometric points");
    require(mesh.vertices.size() == 4,
        "authored-normal-only render splits survived canonical render rebuild");
    require(std::abs(mesh.maxBounds.x - 2.0f) < 1.0e-6f,
        "canonical positional weld did not snap render vertices to one representative geometric position");
    for (const auto& vertex : mesh.vertices)
        require(vertex.normal.z > 0.999f,
            "authored normals leaked through canonical normal reconstruction");
}

void testCanonicalPreparationKeepsCoincidentSheetsIndependent()
{
    using namespace elite::model_asset::editor;
    MeshLod mesh;
    // Three independent sheets occupy the same positional edge. A 1e-4 match
    // is only a weld candidate; canonical authoring must not manufacture one
    // three-face non-manifold edge from unrelated sheets.
    mesh.vertices = {
        {{0,0,0},{0,0,0},{0,0}}, {{1,0,0},{0,0,0},{1,0}}, {{0,1,0},{0,0,0},{0,1}},
        {{0,0,0},{0,0,0},{0,0}}, {{1,0,0},{0,0,0},{1,0}}, {{0,-1,0},{0,0,0},{0,1}},
        {{0,0,0},{0,0,0},{0,0}}, {{1,0,0},{0,0,0},{1,0}}, {{0,0,1},{0,0,0},{0,1}}
    };
    mesh.triangles = {
        {0,1,2,0,0,0},
        {4,3,5,1,0,0},
        {6,7,8,2,0,0}
    };
    // Explicit boundary edges are authoritative evidence that these sheets are
    // not adjacent despite coincident positions.
    for (std::size_t ti = 0; ti < mesh.triangles.size(); ++ti)
    {
        const auto& t = mesh.triangles[ti];
        const std::uint32_t v[3] = {t.a,t.b,t.c};
        for (int e=0;e<3;++e)
        {
            Edge edge;
            edge.a=v[e]; edge.b=v[(e+1)%3]; edge.triangleA=static_cast<std::int32_t>(ti);
            edge.flags=EdgeBoundary|EdgePolygonBoundary;
            mesh.edges.push_back(edge);
        }
    }
    mesh.minBounds={0,-1,0}; mesh.maxBounds={1,1,1};

    const auto built = canonicalizeMesh(mesh);
    require(built.success, built.error.c_str());
    const auto after = analyzeCanonicalMesh(mesh);
    require(!after.structuralInvalid && after.canonicalMultiUseEdges == 0 &&
            after.components == 3 && after.openComponents == 3,
        "canonical preparation fused independent coincident sheets");
    require(std::all_of(mesh.edges.begin(), mesh.edges.end(), [](const Edge& edge) {
        return (edge.flags & EdgeCanonicalTopology) != 0;
    }), "canonical rebuilt edges were not marked authoritative");
}

void testCanonicalPreparationRebuildsHardNormalIslands()
{
    using namespace elite::model_asset::editor;
    MeshLod mesh;
    mesh.vertices = {
        {{0,0,0},{0,0,-1},{0,0}},
        {{1,0,0},{0,0,-1},{1,0}},
        {{0,1,0},{0,0,-1},{0,1}},
        {{0,0,0},{1,0,0},{0,0}},
        {{1,0,0},{1,0,0},{1,0}},
        {{0,0,1},{1,0,0},{0,1}}
    };
    mesh.triangles = {
        {0,1,2,0,0,0},
        {3,5,4,1,0,0}
    };
    mesh.minBounds = {0,0,0}; mesh.maxBounds = {1,1,1};

    const auto built = canonicalizeMesh(mesh);
    require(built.success, built.error.c_str());
    require(built.normalIslands >= 2,
        "canonical preparation did not reconstruct separate hard-normal islands");
    require(mesh.vertices.size() == 6,
        "hard edge was incorrectly collapsed into one smooth GPU vertex pair");
    bool hasZAxis = false;
    bool hasYAxis = false;
    for (const auto& vertex : mesh.vertices)
    {
        require(std::isfinite(vertex.normal.x) && std::isfinite(vertex.normal.y) && std::isfinite(vertex.normal.z),
            "canonical preparation produced a non-finite reconstructed normal");
        // This synthetic L-shaped open sheet has no authored semantic outside.
        // The macro-patch envelope may flip the whole component, but the hard
        // normal islands must remain the two expected orthogonal directions.
        hasZAxis = hasZAxis || std::abs(vertex.normal.z) > 0.99f;
        hasYAxis = hasYAxis || std::abs(vertex.normal.y) > 0.99f;
    }
    require(hasZAxis && hasYAxis,
        "authored normals leaked through or hard-normal directions were lost");
}

void testRuntimeNormalizerRemainsTolerantRenderContract()
{
    using namespace elite::model_asset;
    // The game normalizer deliberately performs global positional weld. This
    // remains a tolerant render contract and is intentionally NOT the editor's
    // canonical topology contract.
    std::vector<glm::vec3> positions = {
        {0,0,0}, {1,0,0}, {0,1,0},
        {0,0,0}, {1,0,0}, {0,-1,0},
        {0,0,0}, {1,0,0}, {0,0,1}
    };
    std::vector<RuntimeMeshTriangleInput> triangles = {
        {0,1,2,10}, {4,3,5,11}, {6,7,8,12}
    };
    const auto normalized = normalizeRuntimeMeshTopology(positions, triangles);
    require(normalized.success, normalized.error.c_str());
    require(normalized.positions.size() == 5 && normalized.triangles.size() == 3,
        "game runtime normalizer no longer follows its tolerant positional-weld contract");
}

void testCanonicalBuilderFingerprintTracksStructuralPayload()
{
    using namespace elite::model_asset::editor;
    auto mesh = makeCanonicalCube(false);
    const auto built = canonicalizeMesh(mesh);
    require(built.success, built.error.c_str());
    const auto original = canonicalMeshFingerprint(mesh);
    require(!mesh.edges.empty(), "canonical fingerprint test has no rebuilt edges");

    const auto secondPass = canonicalizeMesh(mesh);
    require(secondPass.success && !secondPass.changed,
        "canonical builder is not idempotent on its own working payload");
    require(canonicalMeshFingerprint(mesh) == original,
        "canonical builder changed fingerprint on an idempotent second pass");

    mesh.edges[0].renderMask ^= EdgeRenderElite;
    require(canonicalMeshFingerprint(mesh) == original,
        "authoring-only edge render mask invalidated canonical preparation");

    mesh.edges[0].triangleA = mesh.edges[0].triangleA == 0 ? 1 : 0;
    require(canonicalMeshFingerprint(mesh) != original,
        "structural edge adjacency did not invalidate canonical preparation fingerprint");
}

void testPreparationRejectsUnreadableAndRepairsNonManifold()
{
    using namespace elite::model_asset::editor;
    MeshLod broken = makeCanonicalCube(false);
    broken.triangles[0].a = 9999;
    const auto indexAudit = analyzeCanonicalMesh(broken);
    require(indexAudit.structuralInvalid && indexAudit.invalidTriangles == 1,
        "broken triangle indexing was not classified Invalid");
    const auto indexBuild = canonicalizeMesh(broken);
    require(!indexBuild.success,
        "canonical builder silently repaired broken indexing");

    // A stale/source diagnostic flag is not allowed to gate the builder. The
    // final edge topology is authoritative and must not inherit EdgeNonManifold
    // from the authored payload when cleanup leaves an ordinary two-face edge.
    MeshLod staleFlag = makeCanonicalCube(false);
    const auto initialStaleBuild = canonicalizeMesh(staleFlag);
    require(initialStaleBuild.success && !staleFlag.edges.empty(),
        "canonical cube helper did not produce rebuilt edges");
    staleFlag.edges.front().flags |= EdgeNonManifold;
    const auto staleAudit = analyzeCanonicalMesh(staleFlag);
    require(!staleAudit.structuralInvalid && staleAudit.sourceNonManifoldEdges == 1,
        "RAW EdgeNonManifold evidence incorrectly blocked pre-build analysis");
    const auto staleBuild = canonicalizeMesh(staleFlag);
    require(staleBuild.success,
        "RAW EdgeNonManifold evidence incorrectly prevented canonicalization");
    const auto staleAfter = analyzeCanonicalMesh(staleFlag);
    require(!staleAfter.structuralInvalid && staleAfter.sourceNonManifoldEdges == 0,
        "canonical edge rebuild inherited stale EdgeNonManifold evidence");

    MeshLod repairable;
    repairable.vertices = {
        {{0,0,0},{0,0,0},{0,0}}, {{1,0,0},{0,0,0},{0,0}},
        {{0,1,0},{0,0,0},{0,0}}, {{0,-1,0},{0,0,0},{0,0}}
    };
    repairable.triangles = {
        {0,1,2,0,0,0}, {1,0,3,1,0,0}, {0,1,2,2,0,0}
    };
    Edge repairableEdge; repairableEdge.a=0; repairableEdge.b=1; repairableEdge.triangleA=0; repairableEdge.triangleB=1; repairableEdge.flags=EdgeNonManifold;
    repairable.edges.push_back(repairableEdge);
    repairable.minBounds={0,-1,0}; repairable.maxBounds={1,1,0};
    const auto repairableBuild = canonicalizeMesh(repairable);
    require(repairableBuild.success && repairableBuild.removedDuplicateTriangles == 1,
        "source non-manifold evidence caused by a duplicate face was not healed by cleanup");
    require(analyzeCanonicalMesh(repairable).sourceNonManifoldEdges == 0,
        "healed duplicate-face non-manifold evidence leaked into canonical edges");

    // A genuine three-face source edge remains invalid only when it is still
    // multi-use after degenerate/duplicate cleanup. This check happens inside
    // the builder, after cleanup, rather than as a RAW preflight gate.
    MeshLod nonManifold;
    nonManifold.vertices = {
        {{0,0,0},{0,0,0},{0,0}}, {{1,0,0},{0,0,0},{0,0}},
        {{0,1,0},{0,0,0},{0,0}}, {{0,-1,0},{0,0,0},{0,0}}, {{0,0,1},{0,0,0},{0,0}}
    };
    nonManifold.triangles = {{0,1,2,0,0,0},{1,0,3,1,0,0},{0,1,4,2,0,0}};
    Edge sourceEdge; sourceEdge.a=0; sourceEdge.b=1; sourceEdge.triangleA=0; sourceEdge.triangleB=1; sourceEdge.flags=EdgeNonManifold;
    nonManifold.edges.push_back(sourceEdge);
    nonManifold.minBounds={0,-1,0}; nonManifold.maxBounds={1,1,1};
    const auto nmAudit = analyzeCanonicalMesh(nonManifold);
    require(nmAudit.structuralInvalid && nmAudit.sourceNonManifoldEdges == 1 && nmAudit.canonicalMultiUseEdges == 1,
        "genuine shared-vertex non-manifold topology was not diagnosed");
    const auto nmBuild = canonicalizeMesh(nonManifold);
    require(nmBuild.success, nmBuild.error.c_str());
    require(nmBuild.splitTopologyVertices > 0,
        "split_nonmanifold did not split a genuine three-face geometric edge");
    const auto repairedAudit = analyzeCanonicalMesh(nonManifold);
    require(!repairedAudit.structuralInvalid && repairedAudit.canonicalMultiUseEdges == 0,
        "libigl preparation left genuine non-manifold topology unresolved");
}

int main()
{
    try
    {
        testNativeImporterKeepsSmallValidTriangles();
        testNativeImporterDoesNotMarkFanDiagonalNonManifold();
        testCanonicalBuilderRepairsWindingAndOutwardNormals();
        testCanonicalBuilderClosedPlateBreachContracts();
        testCanonicalBuilderOrientsBreachedShellWithEmbree();
        testCanonicalBuilderRemovesGarbageAndPreservesUvSeams();
        testCanonicalBuilderCollapsesAuthoredNormalOnlySplits();
        testCanonicalPreparationKeepsCoincidentSheetsIndependent();
        testCanonicalPreparationRebuildsHardNormalIslands();
        testRuntimeNormalizerRemainsTolerantRenderContract();
        testCanonicalBuilderFingerprintTracksStructuralPayload();
        testPreparationRejectsUnreadableAndRepairsNonManifold();
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
        material.baseColor = {0.4f, 0.45f, 0.5f, 0.85f};
        material.emissiveColor = {0.1f, 0.2f, 0.3f};
        material.emissiveStrength = 2.5f;
        material.metallic = 0.7f;
        material.roughness = 0.28f;
        material.twoSided = true;
        material.baseColorTexture = "textures/hull_base.png";
        material.emissiveTexture = "textures/hull_emit.png";
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
        lod0.relativeGeometricError = 0.0f;
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
        lod1.relativeGeometricError = 0.01f;
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
        lod2.relativeGeometricError = 0.04f;
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
        require(manifestOnly.renderLods[0].declaredGeometryCount == 2 &&
                manifestOnly.renderLods[0].declaredNodeCount == 3 &&
                manifestOnly.renderLods[1].declaredGeometryCount == 1 &&
                manifestOnly.renderLods[1].declaredNodeCount == 1 &&
                manifestOnly.renderLods[2].declaredGeometryCount == 2 &&
                manifestOnly.renderLods[2].declaredNodeCount == 2,
            "manifest-only load lost declared render graph counts");
        require(near(manifestOnly.renderLods[0].relativeGeometricError, 0.0f) &&
                near(manifestOnly.renderLods[1].relativeGeometricError, 0.01f) &&
                near(manifestOnly.renderLods[2].relativeGeometricError, 0.04f),
            "v4 LERR manifest extension lost runtime screen-space LOD metadata");
        require(ModelAssetBinary::loadLod(path.string(), manifestOnly, 0, &error), error.c_str());
        require(manifestOnly.renderLods[0].geometries.size() == 2 && manifestOnly.renderLods[1].geometries.empty(),
            "LOD0-only load also loaded sibling render graphs");
        require(manifestOnly.renderLods[0].nodes[0].geometryIndex == manifestOnly.renderLods[0].nodes[1].geometryIndex,
            "LOD0 render instancing was lost");
        require(near(manifestOnly.renderLods[0].relativeGeometricError, 0.0f),
            "loading the heavy LOD0 payload erased manifest SSE metadata");

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
        ModelAsset manifestAfterMetadataSave;
        require(ModelAssetBinary::loadManifest(path.string(), manifestAfterMetadataSave, &legacyPackage, &error), error.c_str());
        require(manifestAfterMetadataSave.renderLods[1].declaredGeometryCount == 1 &&
                manifestAfterMetadataSave.renderLods[1].declaredNodeCount == 1 &&
                manifestAfterMetadataSave.renderLods[2].declaredGeometryCount == 2 &&
                manifestAfterMetadataSave.renderLods[2].declaredNodeCount == 2,
            "metadata-only manifest save zeroed counts for unloaded LOD payloads");
        require(near(manifestAfterMetadataSave.renderLods[1].relativeGeometricError, 0.01f) &&
                near(manifestAfterMetadataSave.renderLods[2].relativeGeometricError, 0.04f),
            "metadata-only manifest save lost per-LOD runtime screen-space error");

        ModelAsset loaded;
        require(ModelAssetBinary::load(path.string(), loaded, &error), error.c_str());
        require(loaded.assetId == asset.assetId, "asset id lost");
        require(loaded.materials.size() == 1 && loaded.materials[0].id == "hull_outer",
            "material identity lost");
        require(near(loaded.materials[0].baseColor.w, 0.85f) &&
                near(loaded.materials[0].emissiveStrength, 2.5f) &&
                near(loaded.materials[0].metallic, 0.7f) && near(loaded.materials[0].roughness, 0.28f) &&
                loaded.materials[0].twoSided && loaded.materials[0].baseColorTexture == "textures/hull_base.png" &&
                loaded.materials[0].emissiveTexture == "textures/hull_emit.png",
            "surface material properties lost in binary round trip");
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
        require(near(loaded.renderLods[0].relativeGeometricError, 0.0f) &&
                near(loaded.renderLods[1].relativeGeometricError, 0.01f) &&
                near(loaded.renderLods[2].relativeGeometricError, 0.04f),
            "full v4 load lost per-LOD runtime screen-space error");

        // Runtime SSE is scale independent. The same projected apparent size
        // produces the same decision even when the real game object is many
        // kilometres rather than a few Blender/source units. Hysteresis keeps
        // the active LOD stable around the 2 px visibility boundary.
        std::vector<RenderLod> sseLods(3);
        sseLods[0].level = 0; sseLods[0].relativeGeometricError = 0.0f;
        sseLods[1].level = 1; sseLods[1].relativeGeometricError = 0.01f;
        sseLods[2].level = 2; sseLods[2].relativeGeometricError = 0.04f;
        require(selectRenderLodScreenSpace(sseLods, 0, 100.0f) == 1,
            "runtime SSE did not select the coarsest LOD below the 1.8 px coarsen threshold");
        require(selectRenderLodScreenSpace(sseLods, 1, 40.0f) == 2,
            "runtime SSE did not coarsen to LOD2 when its projected error became sub-pixel");
        require(selectRenderLodScreenSpace(sseLods, 1, 190.0f) == 1,
            "runtime SSE hysteresis did not hold LOD1 inside the 1.8/2.2 px band");
        require(selectRenderLodScreenSpace(sseLods, 1, 230.0f) == 0,
            "runtime SSE did not refine when current LOD error exceeded 2.2 px");
        sseLods[2].relativeGeometricError = -1.0f;
        require(selectRenderLodScreenSpace(sseLods, 1, 20.0f) == 1,
            "runtime SSE crossed an unknown/manual LOD metadata boundary");
        constexpr float kPi = 3.14159265358979323846f;
        const float stationProjected = perspectiveProjectedCharacteristicPixels(4000.0f, 20000.0f, 70.0f * kPi / 180.0f, 1440.0f);
        const float scaledSameView = perspectiveProjectedCharacteristicPixels(20.0f, 100.0f, 70.0f * kPi / 180.0f, 1440.0f);
        require(near(stationProjected, scaledSameView, 1.0e-3f),
            "runtime SSE helper depends on source scale instead of final size/distance projection");

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
        require(near(legacy.renderLods[0].relativeGeometricError, 0.0f) &&
                legacy.renderLods[1].relativeGeometricError < 0.0f,
            "legacy migration invented unsafe runtime SSE metadata");
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

        const auto damagedGeometryId = makeRenderVariantGeometryId("breached_01");
        require(damagedGeometryId == "source_variant.breached_01",
            "source variant geometry ID convention changed unexpectedly");
        const auto damagedIdentity = renderVariantIdentity(damagedGeometryId);
        require(damagedIdentity.isVariant &&
                damagedIdentity.legacyBaseGeometryId.empty() &&
                damagedIdentity.variantId == "breached_01",
            "flat source variant geometry ID did not round-trip variant identity");
        const auto legacyDamagedIdentity = renderVariantIdentity(
            "station_habitat_s1.variant.breached_01");
        require(legacyDamagedIdentity.isVariant &&
                legacyDamagedIdentity.legacyBaseGeometryId == "station_habitat_s1" &&
                legacyDamagedIdentity.variantId == "breached_01",
            "v0.9.4 source variant identity is no longer readable");
        require(!renderVariantIdentity("station_habitat_s1").isVariant,
            "ordinary geometry was misclassified as a source variant");

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
