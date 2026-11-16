#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/world/descriptors/IObjectDescriptor.h"

namespace game::station
{

/*
    Hub district prototype modules (author 2026-11-16: «начинай прототип района хаба»): the kit of
    tools/models/units/hub_district.py (trusses, habitat blocks, towers, mushroom platforms, factories,
    tank farms, the anchor asteroid), editor hub modules <assets_source>/models/hub/hubk_<kind>, packages
    assets/compiled/hub_modules/<id>/<id>.elmodel. Static decoration attached to a hub as dock
    modules (initial_world_state.json orbital_hubs[].dock_modules, type = the folder id): the package is
    drawn in its own metres (logical scaling off), +Y up, -Z forward like the authoring basis.
*/
class HubKitModuleDescriptor final : public IObjectDescriptor
{
public:
    HubKitModuleDescriptor(std::string meshId, glm::vec3 sizeMeters)
        : m_meshId(std::move(meshId)), m_size(sizeMeters),
          m_dims{.length = sizeMeters.z, .width = sizeMeters.x, .height = sizeMeters.y,
                 .scaleReference = ScaleReference::None, .enabled = false}
    {
    }

    const std::string& meshId() const override { return m_meshId; }
    bool isLargeObject() const override { return true; }
    glm::vec3 getMeshSizeMeters() const override { return m_size; }
    const LogicalDimensions& logicalDimensions() const override { return m_dims; }
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

private:
    std::string m_meshId;
    glm::vec3 m_size;
    LogicalDimensions m_dims;
};

} // namespace game::station
