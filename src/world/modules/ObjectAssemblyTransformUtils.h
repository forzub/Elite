#pragma once

#include <optional>
#include <string>
#include <glm/glm.hpp>

#include "src/game/geometry/ObjectAssembly.h"
#include "src/world/modules/ObjectAssemblyRuntime.h"

namespace world::modules
{

const game::ship::geometry::AssemblyModule* findAssemblyModuleById(
    const game::ship::geometry::ObjectAssembly& assembly,
    const std::string& moduleId
);

const game::ship::geometry::AssemblyMeshPart* findAssemblyMeshPartById(
    const game::ship::geometry::ObjectAssembly& assembly,
    const std::string& meshPartId,
    const game::ship::geometry::AssemblyModule** outOwnerModule = nullptr
);

glm::mat4 buildAssemblyModuleOwnLocalModel(
    const game::ship::geometry::AssemblyModule& module,
    const ObjectAssemblyRuntime& assemblyRuntime
);

glm::mat4 buildAssemblyModuleHierarchicalLocalModel(
    const game::ship::geometry::ObjectAssembly& assembly,
    const ObjectAssemblyRuntime& assemblyRuntime,
    const std::string& moduleId
);

std::optional<glm::mat4> buildAssemblyMeshPartHierarchicalLocalModel(
    const game::ship::geometry::ObjectAssembly& assembly,
    const ObjectAssemblyRuntime& assemblyRuntime,
    const std::string& meshPartId
);

} // namespace world::modules