#include "src/render/geometry/AssemblyGpuLibrary.h"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <unordered_map>

#include "src/game/geometry/AssemblyMeshLibrary.h"

namespace render::geometry
{
namespace
{
    using ResourcePtr = std::unique_ptr<ObjectAssemblyGpuResources>;

    std::unordered_map<std::uint16_t, ResourcePtr> g_cache;

    std::unordered_map<const game::ship::geometry::ObjectAssembly*, ResourcePtr> g_byAssembly;

    ObjectAssemblyGpuResources buildGpuResources(
        const game::ship::geometry::ObjectAssembly& assembly)
    {
        ObjectAssemblyGpuResources gpu;
        gpu.typeId = assembly.typeId;
        gpu.modules.resize(assembly.modules.size());

        /*
            GPU data is a presentation-only sidecar and intentionally mirrors
            the CPU assembly's module/mesh ordering. The CPU assembly remains
            the shared definition used by both authoritative/headless code and
            the client; OpenGL resources must never leak back into it.
        */
        for (std::size_t moduleIndex = 0;
             moduleIndex < assembly.modules.size();
             ++moduleIndex)
        {
            const auto& cpuModule = assembly.modules[moduleIndex];
            auto& gpuModule = gpu.modules[moduleIndex];
            gpuModule.meshes.resize(cpuModule.meshes.size());

            for (std::size_t meshIndex = 0;
                 meshIndex < cpuModule.meshes.size();
                 ++meshIndex)
            {
                const auto& cpuPart = cpuModule.meshes[meshIndex];
                auto& gpuPart = gpuModule.meshes[meshIndex];

                gpuPart.lod0.upload(cpuPart.lod0Mesh);
                gpuPart.lod1.upload(cpuPart.lod1Mesh);
            }
        }

        if (assembly.hasWholeShipProxy)
            gpu.wholeShipProxy.upload(assembly.wholeShipProxyMesh);

        return gpu;
    }
}

const ObjectAssemblyGpuResources& AssemblyGpuLibrary::get(ObjectType typeId)
{
    const std::uint16_t key = static_cast<std::uint16_t>(typeId);

    const auto it = g_cache.find(key);
    if (it != g_cache.end())
        return *it->second;

    auto resources =
        std::make_unique<ObjectAssemblyGpuResources>(
            buildGpuResources(
                game::ship::geometry::AssemblyMeshLibrary::get(typeId))
        );

    const auto [insertedIt, inserted] =
        g_cache.emplace(key, std::move(resources));

    if (!inserted)
        throw std::runtime_error(
            "[AssemblyGpuLibrary] failed to cache GPU assembly resources"
        );

    return *insertedIt->second;
}

const ObjectAssemblyGpuResources& AssemblyGpuLibrary::get(
    const game::ship::geometry::ObjectAssembly& assembly)
{
    // Registered hull types keep their typeId cache; a catalog ship's
    // procedural assembly (typeId None) is keyed by its never-destroyed
    // CPU assembly.
    if (assembly.typeId != ObjectType::None)
        return get(assembly.typeId);
    const auto it = g_byAssembly.find(&assembly);
    if (it != g_byAssembly.end())
        return *it->second;
    auto resources =
        std::make_unique<ObjectAssemblyGpuResources>(buildGpuResources(assembly));
    return *g_byAssembly.emplace(&assembly, std::move(resources)).first->second;
}

} // namespace render::geometry
