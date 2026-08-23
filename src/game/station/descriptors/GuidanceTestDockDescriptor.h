#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/world/descriptors/IObjectDescriptor.h"

namespace game::station
{

/*
    Diagnostic hub modules for navigation/guidance development.

    Geometry is intentionally external under assets/models/hub/guidance_test/.
    Replacing the OBJ changes presentation only; docking/guidance semantics are
    stored separately in assets/data/navigation/hub_semantic_anchors.json.
*/
class GuidanceDockCubeDescriptor final : public IObjectDescriptor
{
public:
    const std::string& meshId() const override
    {
        static const std::string id = "guidance_dock_cube";
        return id;
    }

    bool isLargeObject() const override { return true; }
    glm::vec3 getMeshSizeMeters() const override
    {
        return glm::vec3(360.0f, 360.0f, 900.0f);
    }

    const LogicalDimensions& logicalDimensions() const override
    {
        static const LogicalDimensions dims {
            .length = 900.0f,
            .width = 360.0f,
            .height = 360.0f,
            .scaleReference = ScaleReference::Length,
            .enabled = true
        };
        return dims;
    }

    const glm::vec3& visualBasisRotationDeg() const override
    {
        static const glm::vec3 value(0.0f);
        return value;
    }

    const glm::vec3& meshForwardAxis() const override
    {
        static const glm::vec3 value(0.0f, 0.0f, -1.0f);
        return value;
    }

    const glm::vec3& meshUpAxis() const override
    {
        static const glm::vec3 value(0.0f, 1.0f, 0.0f);
        return value;
    }

    const std::vector<ModuleDescriptor>& moduleDescriptors() const override
    {
        static const std::vector<ModuleDescriptor> empty;
        return empty;
    }
};

class GuidanceDockCylinderDescriptor final : public IObjectDescriptor
{
public:
    const std::string& meshId() const override
    {
        static const std::string id = "guidance_dock_cylinder";
        return id;
    }

    bool isLargeObject() const override { return true; }
    glm::vec3 getMeshSizeMeters() const override
    {
        return glm::vec3(420.0f, 420.0f, 1200.0f);
    }

    const LogicalDimensions& logicalDimensions() const override
    {
        static const LogicalDimensions dims {
            .length = 1200.0f,
            .width = 420.0f,
            .height = 420.0f,
            .scaleReference = ScaleReference::Length,
            .enabled = true
        };
        return dims;
    }

    const glm::vec3& visualBasisRotationDeg() const override
    {
        static const glm::vec3 value(0.0f);
        return value;
    }

    const glm::vec3& meshForwardAxis() const override
    {
        static const glm::vec3 value(0.0f, 0.0f, -1.0f);
        return value;
    }

    const glm::vec3& meshUpAxis() const override
    {
        static const glm::vec3 value(0.0f, 1.0f, 0.0f);
        return value;
    }

    const std::vector<ModuleDescriptor>& moduleDescriptors() const override
    {
        static const std::vector<ModuleDescriptor> empty;
        return empty;
    }
};

} // namespace game::station
