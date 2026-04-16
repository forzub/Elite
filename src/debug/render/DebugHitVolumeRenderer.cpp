#include "DebugHitVolumeRenderer.h"

#include <unordered_set>
#include <array>
#include "src/debug/DebugSettings.h"

namespace debug::render
{

using game::ship::geometry::ObjectAssembly;

bool DebugHitVolumeRenderer::collectBoundsForDescriptor(
    const ObjectAssembly& assembly,
    const ModuleDescriptor& desc,
    glm::vec3& outMin,
    glm::vec3& outMax
)
{
    if (desc.meshPartIds.empty())
        return false;

    std::unordered_set<std::string> wanted(
        desc.meshPartIds.begin(),
        desc.meshPartIds.end()
    );

    bool found = false;
    glm::vec3 minV(0.0f);
    glm::vec3 maxV(0.0f);

    for (const auto& module : assembly.modules)
    {
        for (const auto& part : module.meshes)
        {
            if (wanted.find(part.id) == wanted.end())
                continue;

            if (!found)
            {
                minV = part.minBounds;
                maxV = part.maxBounds;
                found = true;
            }
            else
            {
                minV = glm::min(minV, part.minBounds);
                maxV = glm::max(maxV, part.maxBounds);
            }
        }
    }

    if (!found)
        return false;

    outMin = minV;
    outMax = maxV;
    return true;
}

glm::vec4 DebugHitVolumeRenderer::layerColor(int layerIndex, bool hidden)
{
    glm::vec4 c;

    switch (layerIndex)
    {
        case 0: c = glm::vec4(1.0f, 0.2f, 0.2f, 1.0f); break; // красный
        case 1: c = glm::vec4(1.0f, 0.8f, 0.2f, 1.0f); break; // жёлтый
        case 2: c = glm::vec4(0.2f, 1.0f, 0.3f, 1.0f); break; // зелёный
        case 3: c = glm::vec4(0.2f, 0.7f, 1.0f, 1.0f); break; // голубой
        default: c = glm::vec4(0.9f, 0.2f, 1.0f, 1.0f); break; // фиолетовый
    }

    if (hidden)
    {
        c.a = 0.35f;
        c.r *= 0.5f;
        c.g *= 0.5f;
        c.b *= 0.5f;
    }

    return c;
}

void DebugHitVolumeRenderer::addAabbBoxLines(
    DebugLineRenderer& lineRenderer,
    const glm::vec3& minV,
    const glm::vec3& maxV,
    const glm::mat4& model,
    const glm::vec4& color
)
{
    std::array<glm::vec3, 8> p =
    {{
        {minV.x, minV.y, minV.z},
        {maxV.x, minV.y, minV.z},
        {maxV.x, maxV.y, minV.z},
        {minV.x, maxV.y, minV.z},

        {minV.x, minV.y, maxV.z},
        {maxV.x, minV.y, maxV.z},
        {maxV.x, maxV.y, maxV.z},
        {minV.x, maxV.y, maxV.z},
    }};

    for (auto& v : p)
    {
        glm::vec4 w = model * glm::vec4(v, 1.0f);
        v = glm::vec3(w);
    }

    auto L = [&](int a, int b)
    {
        lineRenderer.addLine(p[a], p[b], color);
    };

    // near face
    L(0,1); L(1,2); L(2,3); L(3,0);
    // far face
    L(4,5); L(5,6); L(6,7); L(7,4);
    // connectors
    L(0,4); L(1,5); L(2,6); L(3,7);
}

void DebugHitVolumeRenderer::renderObjectHitVolumes(
    DebugLineRenderer& lineRenderer,
    const IObjectDescriptor& descriptor,
    const ObjectAssembly& assembly,
    const glm::mat4& objectModel,
    const std::unordered_set<std::string>& hiddenPartIds,
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

    for (const auto& modDesc : descriptor.moduleDescriptors())
    {
        if (!modDesc.enabled)
            continue;

        glm::vec3 minV(0.0f);
        glm::vec3 maxV(0.0f);

        if (!collectBoundsForDescriptor(assembly, modDesc, minV, maxV))
            continue;

        bool hidden = false;
        for (const auto& partId : modDesc.meshPartIds)
        {
            if (hiddenPartIds.find(partId) != hiddenPartIds.end())
            {
                hidden = true;
                break;
            }
        }

        glm::vec4 color = layerColor(modDesc.layerIndex, hidden);
        addAabbBoxLines(lineRenderer, minV, maxV, objectModel, color);
    }

    if (dbg.hitVolumesOverlay)
        lineRenderer.endOverlay(mvp);
    else
        lineRenderer.end(mvp);
}

} // namespace debug::render