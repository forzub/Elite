#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/world/descriptors/IObjectDescriptor.h"

namespace game::drone
{

class RepairDroneDescriptor final : public IObjectDescriptor
{
public:
    const std::string& meshId() const override
    {
        static const std::string id = "repair_drone_debug";
        return id;
    }

    bool isLargeObject() const override
    {
        return false;
    }

    glm::vec3 getMeshSizeMeters() const override
    {
        return glm::vec3(1.2f, 0.8f, 1.2f);
    }

    const LogicalDimensions& logicalDimensions() const override
    {
        static LogicalDimensions dims {
            .length = 1.2f,
            .width = 1.2f,
            .height = 0.8f,
            .scaleReference = ScaleReference::Width,
            .enabled = true
        };

        return dims;
    }

    const glm::vec3& visualBasisRotationDeg() const override
    {
        static const glm::vec3 rot(0.0f, 0.0f, 0.0f);
        return rot;
    }

    const glm::vec3& meshForwardAxis() const override
    {
        // OBJ делаем уже в игровом базисе:
        // +X right, +Y up, -Z forward
        static const glm::vec3 forward(0.0f, 0.0f, -1.0f);
        return forward;
    }

    const glm::vec3& meshUpAxis() const override
    {
        static const glm::vec3 up(0.0f, 1.0f, 0.0f);
        return up;
    }

    const std::vector<ModuleDescriptor>& moduleDescriptors() const override
    {
        static const std::vector<ModuleDescriptor> empty;
        return empty;
    }
};

} // namespace game::drone