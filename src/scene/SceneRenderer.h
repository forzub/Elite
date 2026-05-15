#pragma once
#include <glm/glm.hpp>
#include <memory>

#include "src/game/client/ClientWorldState.h"
#include "src/game/geometry/MeshLibrary.h"
#include "src/render/renderers/MeshRenderer.h"
#include "src/debug/render/DebugRenderer.h"
#include "src/debug/render/DebugLineRenderer.h"
#include "src/render/frustum/Frustum.h"
#include "src/render/starfield/GalaxyStarfieldRenderer.h"


#include "src/debug/FrustumDebugData.h"
#include <functional>
#include "src/debug/DebugSettings.h"

#include "src/render/Font.h"

// Forward declaration
class DebugLineRenderer;





struct SceneRenderStats
{
    uint32_t drawCalls = 0;

    uint32_t modulesDrawn = 0;
    uint32_t modulesCulled = 0;

    uint32_t partsDrawn = 0;
    uint32_t partsCulled = 0;

    void reset()
    {
        drawCalls = 0;
        modulesDrawn = 0;
        modulesCulled = 0;
        partsDrawn = 0;
        partsCulled = 0;
    }

    void add(const SceneRenderStats& other)
    {
        drawCalls += other.drawCalls;
        modulesDrawn += other.modulesDrawn;
        modulesCulled += other.modulesCulled;
        partsDrawn += other.partsDrawn;
        partsCulled += other.partsCulled;
    }
};











class SceneRenderer
{
public:
    SceneRenderer();
    ~SceneRenderer() = default;
    
    void render(const ClientWorldState& world,
                EntityId playerId,
                const glm::mat4& view,
                const glm::mat4& proj,
                int cameraId,           
                const std::string& cameraName
            );

    // --- DEBUG ---
    void setDebugCallback(std::function<void(const DebugFrameData&)> callback)
    {
        m_debugCallback = callback;
    }

     void renderStarSystemLabels(
        const glm::mat4& view,
        const glm::mat4& proj
    );

    std::unique_ptr<Font> m_starLabelFont;

    const SceneRenderStats& lastStats() const
    {
        return m_lastStats;
    }


private:
    
    std::unique_ptr<DebugLineRenderer>  m_debugLines;;
    bool                                m_initialized;
    MeshRenderer                        m_meshRenderer;
    DebugRenderer                       m_debugRenderer;
    GalaxyStarfieldRenderer             m_starfieldRenderer;
    
    // --- DEBUG ---
    std::function<void(const DebugFrameData&)> m_debugCallback;
    uint64_t m_frameCounter = 0;

    SceneRenderStats m_lastStats;
    
};