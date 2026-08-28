#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "src/model_asset/ModelAsset.h"

namespace elite::model_asset::editor
{

inline constexpr double CanonicalMeshWeldEpsilon = 1.0e-4;
inline constexpr const char* CanonicalMeshAlgorithmId = "canonical_mesh_builder_v2";

struct CanonicalMeshAnalysis
{
    std::size_t renderVertices = 0;
    std::size_t geometricPoints = 0;
    std::size_t triangles = 0;
    std::size_t invalidTriangles = 0;
    std::size_t degenerateTriangles = 0;
    std::size_t duplicateTriangles = 0;
    std::size_t boundaryEdges = 0;
    std::size_t canonicalMultiUseEdges = 0;
    std::size_t sourceNonManifoldEdges = 0;
    std::size_t windingFlipsRequired = 0;
    std::size_t windingConflicts = 0;
    std::size_t insideOutClosedComponents = 0;
    std::size_t components = 0;
    std::size_t closedComponents = 0;
    std::size_t openComponents = 0;
    bool finitePositions = true;
    bool structuralInvalid = false;
    std::string invalidReason;
};

struct CanonicalMeshBuildResult
{
    bool success = false;
    bool changed = false;
    CanonicalMeshAnalysis before;
    CanonicalMeshAnalysis after;
    std::size_t removedDegenerateTriangles = 0;
    std::size_t removedDuplicateTriangles = 0;
    std::size_t flippedTriangles = 0;
    std::size_t flippedClosedComponents = 0;
    std::size_t rebuiltRenderVertices = 0;
    std::size_t normalIslands = 0;
    std::size_t rebuiltEdges = 0;
    std::string error;
};

CanonicalMeshAnalysis analyzeCanonicalMesh(const MeshLod& mesh);
CanonicalMeshBuildResult canonicalizeMesh(MeshLod& mesh);
std::uint64_t canonicalMeshFingerprint(const MeshLod& mesh);

} // namespace elite::model_asset::editor
