#pragma once

#include <cassert>
#include <cstddef>
#include <vector>

#include "src/game/geometry/MeshGPU.h"
#include "src/game/geometry/ObjectAssembly.h"
#include "src/world/types/ObjectType.h"

namespace render::geometry
{

struct AssemblyMeshPartGpuResources
{
    render::MeshGPU lod0;
    render::MeshGPU lod1;
};

struct AssemblyModuleGpuResources
{
    std::vector<AssemblyMeshPartGpuResources> meshes;
};

struct ObjectAssemblyGpuResources
{
    ObjectType typeId = ObjectType::None;
    std::vector<AssemblyModuleGpuResources> modules;
    render::MeshGPU wholeShipProxy;

    const AssemblyMeshPartGpuResources& forPart(
        const game::ship::geometry::ObjectAssembly& assembly,
        const game::ship::geometry::AssemblyModule& module,
        const game::ship::geometry::AssemblyMeshPart& part
    ) const
    {
        const auto* moduleBegin = assembly.modules.data();
        const auto* moduleEnd = moduleBegin + assembly.modules.size();
        assert(&module >= moduleBegin && &module < moduleEnd);

        const std::size_t moduleIndex =
            static_cast<std::size_t>(&module - moduleBegin);

        const auto* partBegin = module.meshes.data();
        const auto* partEnd = partBegin + module.meshes.size();
        assert(&part >= partBegin && &part < partEnd);

        const std::size_t partIndex =
            static_cast<std::size_t>(&part - partBegin);

        assert(moduleIndex < modules.size());
        assert(partIndex < modules[moduleIndex].meshes.size());
        return modules[moduleIndex].meshes[partIndex];
    }
};

} // namespace render::geometry
