#pragma once

#include "src/render/geometry/ObjectAssemblyGpuResources.h"
#include "src/game/geometry/ObjectAssembly.h"

namespace render::geometry
{

class AssemblyGpuLibrary
{
public:
    // Presentation-only sidecar for the shared CPU assembly definition.
    // Must be called only while a valid OpenGL context exists.
    static const ObjectAssemblyGpuResources& get(ObjectType typeId);
    // Any CPU assembly, including a catalog ship's procedural placeholder.
    static const ObjectAssemblyGpuResources& get(
        const game::ship::geometry::ObjectAssembly& assembly);
};

} // namespace render::geometry
