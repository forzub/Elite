#include "ServerHitVolumeRenderer.h"

#include <array>
#include "src/debug/DebugSettings.h"

namespace debug::render
{

glm::vec4 ServerHitVolumeRenderer::colorForLayer(int layerIndex, bool destroyed)
{
    glm::vec4 c;

    switch (layerIndex)
    {
        case 0: c = glm::vec4(1.0f, 0.2f, 0.2f, 1.0f); break;
        case 1: c = glm::vec4(1.0f, 0.8f, 0.2f, 1.0f); break;
        case 2: c = glm::vec4(0.2f, 1.0f, 0.3f, 1.0f); break;
        case 3: c = glm::vec4(0.2f, 0.7f, 1.0f, 1.0f); break;
        default: c = glm::vec4(0.9f, 0.2f, 1.0f, 1.0f); break;
    }

    if (destroyed)
    {
        c *= 0.5f;
        c.a = 0.35f;
    }

    return c;
}

void ServerHitVolumeRenderer::addBox(
    DebugLineRenderer& lineRenderer,
    const glm::vec3& center,
    const glm::vec3& halfSize,
    const glm::mat3& orientation,
    const glm::mat4& model,
    const glm::vec4& color
)
{
    std::array<glm::vec3, 8> p =
    {{
        {-halfSize.x, -halfSize.y, -halfSize.z},
        { halfSize.x, -halfSize.y, -halfSize.z},
        { halfSize.x,  halfSize.y, -halfSize.z},
        {-halfSize.x,  halfSize.y, -halfSize.z},

        {-halfSize.x, -halfSize.y,  halfSize.z},
        { halfSize.x, -halfSize.y,  halfSize.z},
        { halfSize.x,  halfSize.y,  halfSize.z},
        {-halfSize.x,  halfSize.y,  halfSize.z},
    }};

    for (auto& v : p)
    {
        glm::vec3 oriented = center + orientation * v;
        glm::vec4 w = model * glm::vec4(oriented, 1.0f);
        v = glm::vec3(w);
    }

    auto L = [&](int a, int b)
    {
        lineRenderer.addLine(p[a], p[b], color);
    };

    L(0,1); L(1,2); L(2,3); L(3,0);
    L(4,5); L(5,6); L(6,7); L(7,4);
    L(0,4); L(1,5); L(2,6); L(3,7);
}



void ServerHitVolumeRenderer::render(
    DebugLineRenderer& lineRenderer,
    const std::vector<game::simulation::DebugHitVolumeSnapshot>& volumes,
    const glm::mat4& objectModel,
    const glm::mat4& view,
    const glm::mat4& proj
)
{
    const auto& dbg = debug::get().render;
    
    if (!dbg.drawHitVolumes)
        return;

    if (!lineRenderer.isInitialized())
        return;

    glm::mat4 mvp = proj * view;

    lineRenderer.begin();

    for (const auto& v : volumes)
    {
                glm::vec4 color = colorForLayer(v.layerIndex, v.destroyed);
        addBox(
            lineRenderer,
            v.center,
            v.halfSize,
            v.orientation,
            objectModel,
            color
        );
    }

    if (dbg.hitVolumesOverlay)
        lineRenderer.endOverlay(mvp);
    else
        lineRenderer.end(mvp);
}

} // namespace debug::render