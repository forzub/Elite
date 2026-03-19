#pragma once

#include "src/world/descriptors/IObjectDescriptor.h"
#include <glm/glm.hpp>
#include <string>
#include <optional>

class StationDescriptor : public IObjectDescriptor
{
public:
    std::string m_meshId;

    float m_rotationSpeed;
    float m_boundingRadius;

    // -------- MESH MODEL ---
    glm::vec3                               meshSizeMeters {1.0f,1.0f,1.0f};
    std::vector<ShipMeshPart>               meshParts;

    const std::string& meshId() const override
    {
        return m_meshId;
    }

    bool isLargeObject() const override
    {
        return true;
    }

    glm::vec3 getMeshSizeMeters() const override { return meshSizeMeters;}

};