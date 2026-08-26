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
// target geometry coordinates. The indexed path is kept as a cheap fast path.
// Legacy OBJ copies with reordered vertices/triangles fall back to a
// topology-independent LOD0 point-cloud alignment. LODs are independent
// authored representations: LOD1/LOD2 topology never blocks LOD0 instance
// identity. Consolidated nodes intentionally share the reference geometry's
// complete LOD set.
GeometryInstanceFit fitGeometryAsRigidInstance(
    const GeometryDefinition& reference,
    const GeometryDefinition& target);

} // namespace elite::model_asset::editor
