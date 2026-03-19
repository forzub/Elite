#pragma once
#include <glm/glm.hpp>
#include <memory>

#include "src/game/client/ClientWorldState.h"
#include "src/game/geometry/MeshLibrary.h"
#include "src/render/renderers/MeshRenderer.h"
#include "src/render/renderers/DebugRenderer.h"
#include "src/render/renderers/DebugLineRenderer.h"
#include "src/render/frustum/Frustum.h"


#include "src/debug/FrustumDebugData.h"
#include <functional>


// Forward declaration
class DebugLineRenderer;

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

private:
    
    std::unique_ptr<DebugLineRenderer>  m_debugLines;;
    bool                                m_initialized;
    MeshRenderer                        m_meshRenderer;
    DebugRenderer                       m_debugRenderer;
    
    // --- DEBUG ---
    std::function<void(const DebugFrameData&)> m_debugCallback;
    uint64_t m_frameCounter = 0;
    
};