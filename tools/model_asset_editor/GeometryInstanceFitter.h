#pragma once

#include <cstddef>
#include <string>

#include <glm/glm.hpp>

#include "src/model_asset/ModelAsset.h"

namespace elite::model_asset::editor
{

struct GeometryInstanceFit
{
    bool valid = false;
    bool geometryMatched = false;
    bool materialCompatible = false;
    glm::mat3 rotation {1.0f};
    glm::vec3 translation {0.0f};
    float rmsErrorMeters = 0.0f;
    float maxErrorMeters = 0.0f;
    float toleranceMeters = 0.0f;
    std::size_t comparedVertices = 0;
    std::string message;
};

// Finds a proper rigid transform that maps reference geometry coordinates into
// target geometry coordinates. The indexed path is kept as a cheap fast path;
// reordered OBJ vertices/triangles fall back to topology-independent point-cloud
// alignment. In asset v4 the caller passes exactly one active RenderLod
// representation, so instance identity is strictly LOD-local and this fitter
// never creates or assumes a relationship with another render LOD.
GeometryInstanceFit fitGeometryAsRigidInstance(
    const GeometryDefinition& reference,
    const GeometryDefinition& target);

} // namespace elite::model_asset::editor
