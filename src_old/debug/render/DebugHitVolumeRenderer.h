#pragma once

#include <unordered_set>
#include <glm/glm.hpp>

#include "src/debug/render/DebugLineRenderer.h"
#include "src/game/geometry/ObjectAssembly.h"
#include "src/world/descriptors/IObjectDescriptor.h"
#include "src/debug/DebugSettings.h"

namespace debug::render
{

class DebugHitVolumeRenderer
{
public:
    static void renderObjectHitVolumes(
        DebugLineRenderer& lineRenderer,
        const IObjectDescriptor& descriptor,
        const game::ship::geometry::ObjectAssembly& assembly,
        const glm::mat4& objectModel,
        const std::unordered_set<std::string>& hiddenPartIds,
        const glm::mat4& view,
        const glm::mat4& proj
    );

private:
    static bool collectBoundsForDescriptor(
        const game::ship::geometry::ObjectAssembly& assembly,
        const ModuleDescriptor& desc,
        glm::vec3& outMin,
        glm::vec3& outMax
    );

    static glm::vec4 layerColor(int layerIndex, bool hidden);
    static void addAabbBoxLines(
        DebugLineRenderer& lineRenderer,
        const glm::vec3& minV,
        const glm::vec3& maxV,
        const glm::mat4& model,
        const glm::vec4& color
    );
};

} // namespace debug::render