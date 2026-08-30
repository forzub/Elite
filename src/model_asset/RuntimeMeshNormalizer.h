#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace elite::model_asset
{

inline constexpr double RuntimeMeshWeldEpsilon = 1.0e-4;
inline constexpr const char* RuntimeMeshNormalizerAlgorithmId = "runtime_mesh_normalizer_v1";

struct RuntimeMeshTriangleInput
{
    std::uint32_t a = 0;
    std::uint32_t b = 0;
    std::uint32_t c = 0;
    std::int32_t sourceTag = -1;
};

struct RuntimeMeshTriangle
{
    std::uint32_t a = 0;
    std::uint32_t b = 0;
    std::uint32_t c = 0;
    std::size_t sourceTriangleIndex = 0;
    std::int32_t sourceTag = -1;
};

struct RuntimeMeshNormalizationResult
{
    bool success = false;
    std::string error;

    std::vector<glm::vec3> positions;
    std::vector<std::uint32_t> pointForInputVertex;
    std::vector<std::uint32_t> representativeInputVertex;
    std::vector<RuntimeMeshTriangle> triangles;

    std::size_t removedDegenerateTriangles = 0;
    std::size_t removedDuplicateTriangles = 0;
};

// Runtime render-normalization contract used by the game OBJ loader. It does
// the deliberately tolerant runtime work: positional weld, collapsed/duplicate
// triangle cleanup and remap. It does NOT repair winding or recover authoring
// topology. The Model Asset Editor uses its separate topology-aware canonical
// builder because render normalization and authoring canonicalization are
// intentionally different contracts.
RuntimeMeshNormalizationResult normalizeRuntimeMeshTopology(
    const std::vector<glm::vec3>& positions,
    const std::vector<RuntimeMeshTriangleInput>& triangles,
    double weldEpsilon = RuntimeMeshWeldEpsilon);

} // namespace elite::model_asset
