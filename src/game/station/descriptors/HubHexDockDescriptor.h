#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/station/descriptors/HubDockServiceBlockCollision.generated.h"
#include "src/game/station/descriptors/HubHexDockCollision.generated.h"
#include "src/world/descriptors/IObjectDescriptor.h"

namespace game::station
{

/*
    Rotating hexagonal dock of the Earth Orbital Hub and its fixed coaxial
    service block. Geometry, collision boxes and semantics share the numbers
    of src/game/docking/model/HexDockFacility.h; the OBJ meshes and the
    *Collision.generated.h headers come from
    tools/hub_meshes/generate_hub_dock_meshes.py.

    Hex dock local frame (spins about +Z): entry slot 160 x 60 m on the -Z
    face, 100 m entry corridor, hexagonal chamber (inner apothem 120 m,
    z -100..180) with one pad per wall, service port in the back wall.
*/
class HubHexDockDescriptor final : public IObjectDescriptor
{
public:
    const std::string& meshId() const override
    {
        static const std::string id = "hub_hex_dock";
        return id;
    }
    bool isLargeObject() const override { return true; }
    glm::vec3 getMeshSizeMeters() const override { return glm::vec3(346.4f, 300.0f, 400.0f); }
    const LogicalDimensions& logicalDimensions() const override
    {
        static const LogicalDimensions dims {
            .length = 400.0f, .width = 346.4f, .height = 300.0f,
            .scaleReference = ScaleReference::Length, .enabled = true};
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
    // Walls / end plates only: the slot, corridor and chamber stay free.
    const std::vector<LogicalCollisionBox>& logicalCollisionBoxes() const override
    {
        return hubHexDockCollisionBoxes();
    }
};

class HubDockServiceBlockDescriptor final : public IObjectDescriptor
{
public:
    const std::string& meshId() const override
    {
        static const std::string id = "hub_dock_service_block";
        return id;
    }
    bool isLargeObject() const override { return true; }
    glm::vec3 getMeshSizeMeters() const override { return glm::vec3(600.0f, 240.0f, 600.0f); }
    const LogicalDimensions& logicalDimensions() const override
    {
        // Tube 240 x 240 x 600 m; the radiator wings make it 600 m wide.
        static const LogicalDimensions dims {
            .length = 600.0f, .width = 600.0f, .height = 240.0f,
            .scaleReference = ScaleReference::Length, .enabled = true};
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
    // Four tube walls + wings: the 80 x 80 m tunnel is a real passage.
    const std::vector<LogicalCollisionBox>& logicalCollisionBoxes() const override
    {
        return hubDockServiceBlockCollisionBoxes();
    }
};

} // namespace game::station
