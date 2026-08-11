#pragma once

#include "src/render/geometry/ObjectAssemblyGpuResources.h"

namespace render::geometry
{

class AssemblyGpuLibrary
{
public:
    // Presentation-only sidecar for the shared CPU assembly definition.
    // Must be called only while a valid OpenGL context exists.
    static const ObjectAssemblyGpuResources& get(ObjectType typeId);
};

} // namespace render::geometry
