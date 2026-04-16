// #pragma once

// #include "src/world/descriptors/IObjectDescriptor.h"
// #include <glm/glm.hpp>
// #include <string>
// #include <optional>

// class StationDescriptor : public IObjectDescriptor
// {
// public:
//     std::string m_meshId;

//     float m_rotationSpeed;
//     float m_boundingRadius;

//     // -------- MESH MODEL ---
//     glm::vec3                               meshSizeMeters {1.0f,1.0f,1.0f};
//     std::vector<ShipMeshPart>               meshParts;

//     const std::string& meshId() const override
//     {
//         return m_meshId;
//     }

//     bool isLargeObject() const override
//     {
//         return true;
//     }

//     glm::vec3 getMeshSizeMeters() const override { return meshSizeMeters;}

// };



#pragma once

#include "src/world/descriptors/IObjectDescriptor.h"
#include "src/world/descriptors/ModuleDescriptor.h"
#include "src/world/descriptors/LogicalDimensions.h"

#include <glm/glm.hpp>
#include <string>
#include <vector>

class StationDescriptor : public IObjectDescriptor
{
public:
    std::string m_meshId;

    float m_rotationSpeed = 0.0f;
    float m_boundingRadius = 1.0f;

    glm::vec3 meshSizeMeters {1.0f,1.0f,1.0f};
    LogicalDimensions logicalDimensionsValue {};
    glm::vec3 visualBasisRotationDegValue {0.0f, 0.0f, 0.0f};

    glm::vec3 meshForwardAxisValue {0.0f, 0.0f, -1.0f};
    glm::vec3 meshUpAxisValue      {0.0f, 1.0f,  0.0f};

    // НОВОЕ
    std::vector<ModuleDescriptor> modules;

    const std::vector<ModuleDescriptor>& moduleDescriptors() const override
    {
        return modules;
    }

    const std::string& meshId() const override
    {
        return m_meshId;
    }

    bool isLargeObject() const override
    {
        return true;
    }

    glm::vec3 getMeshSizeMeters() const override
    {
        return meshSizeMeters;
    }

    const LogicalDimensions& logicalDimensions() const override
    {
        return logicalDimensionsValue;
    }

    const glm::vec3& visualBasisRotationDeg() const override
    {
        return visualBasisRotationDegValue;
    }

    const glm::vec3& meshForwardAxis() const override
    {
        return meshForwardAxisValue;
    }

    const glm::vec3& meshUpAxis() const override
    {
        return meshUpAxisValue;
    }


};