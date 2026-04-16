#pragma once

#include <vector>
#include <glm/glm.hpp>

#include "src/debug/render/DebugLineRenderer.h"
#include "src/debug/DebugSettings.h"
#include "src/game/simulation/DebugHitVolumeSnapshot.h"

namespace debug::render
{

class ServerHitVolumeRenderer
{
public:
    static void render(
        DebugLineRenderer& lineRenderer,
        const std::vector<game::simulation::DebugHitVolumeSnapshot>& volumes,
        const glm::mat4& objectModel,
        const glm::mat4& view,
        const glm::mat4& proj
    );

private:
    static glm::vec4 colorForLayer(int layerIndex, bool destroyed);
    static void addBox(
        DebugLineRenderer& lineRenderer,
        const glm::vec3& center,
        const glm::vec3& halfSize,
        const glm::mat3& orientation,
        const glm::mat4& model,
        const glm::vec4& color
    );
};

} // namespace debug::render