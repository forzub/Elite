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

    const std::vector<LogicalCollisionBox>&
    logicalCollisionBoxes() const override
    {
        // 360 x 360 x 900 outer body with a real 190 x 110 through aperture.
        // Four wall OBBs replace the old solid whole-object fallback.
        static const std::vector<LogicalCollisionBox> boxes {
            {
                glm::vec3(-137.5f, 0.0f, 0.0f),
                glm::vec3(42.5f, 180.0f, 450.0f),
                glm::mat3(1.0f),
                "dock_wall_left"
            },
            {
                glm::vec3(137.5f, 0.0f, 0.0f),
                glm::vec3(42.5f, 180.0f, 450.0f),
                glm::mat3(1.0f),
                "dock_wall_right"
            },
            {
                glm::vec3(0.0f, 117.5f, 0.0f),
                glm::vec3(95.0f, 62.5f, 450.0f),
                glm::mat3(1.0f),
                "dock_wall_top"
            },
            {
                glm::vec3(0.0f, -117.5f, 0.0f),
                glm::vec3(95.0f, 62.5f, 450.0f),
                glm::mat3(1.0f),
                "dock_wall_bottom"
            }
        };
        return boxes;
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

    const std::vector<LogicalCollisionBox>&
    logicalCollisionBoxes() const override
    {
        // Conservative rectangular wall decomposition around the visible
        // 200 x 120 aperture. It intentionally preserves the full 1200 m
        // through passage instead of turning the cylinder into a solid OBB.
        static const std::vector<LogicalCollisionBox> boxes {
            {
                glm::vec3(-155.0f, 0.0f, 0.0f),
                glm::vec3(55.0f, 210.0f, 600.0f),
                glm::mat3(1.0f),
                "dock_wall_left"
            },
            {
                glm::vec3(155.0f, 0.0f, 0.0f),
                glm::vec3(55.0f, 210.0f, 600.0f),
                glm::mat3(1.0f),
                "dock_wall_right"
            },
            {
                glm::vec3(0.0f, 135.0f, 0.0f),
                glm::vec3(100.0f, 75.0f, 600.0f),
                glm::mat3(1.0f),
                "dock_wall_top"
            },
            {
                glm::vec3(0.0f, -135.0f, 0.0f),
                glm::vec3(100.0f, 75.0f, 600.0f),
                glm::mat3(1.0f),
                "dock_wall_bottom"
            }
        };
        return boxes;
    }
};

} // namespace game::station
