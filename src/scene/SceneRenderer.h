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
#include "src/render/planet/PlanetWireRenderer.h"
#include "src/world/coordinates/WorldFrame.h"


#include "src/scene/PreparedScene.h"

// Forward declaration
class DebugLineRenderer;





struct SceneRenderStats
{
    uint32_t drawCalls = 0;

    uint32_t modulesDrawn = 0;
    uint32_t modulesCulled = 0;

    uint32_t partsDrawn = 0;
    uint32_t partsCulled = 0;

    uint32_t realShipsDrawn = 0;
    uint32_t realShipPartsDrawn = 0;

    uint32_t visualShipsDrawn = 0;
    uint32_t visualShipsCulled = 0;

    uint32_t visualProxyShipsDrawn = 0;
    uint32_t visualFullShipsDrawn = 0;
    uint32_t visualShipPartsDrawn = 0;

    void reset()
    {
        drawCalls = 0;

        modulesDrawn = 0;
        modulesCulled = 0;

        partsDrawn = 0;
        partsCulled = 0;

        realShipsDrawn = 0;
        realShipPartsDrawn = 0;

        visualShipsDrawn = 0;
        visualShipsCulled = 0;

        visualProxyShipsDrawn = 0;
        visualFullShipsDrawn = 0;
        visualShipPartsDrawn = 0;
    }

    void add(const SceneRenderStats& other)
    {
        drawCalls += other.drawCalls;

        modulesDrawn += other.modulesDrawn;
        modulesCulled += other.modulesCulled;

        partsDrawn += other.partsDrawn;
        partsCulled += other.partsCulled;

        realShipsDrawn += other.realShipsDrawn;
        realShipPartsDrawn += other.realShipPartsDrawn;

        visualShipsDrawn += other.visualShipsDrawn;
        visualShipsCulled += other.visualShipsCulled;

        visualProxyShipsDrawn += other.visualProxyShipsDrawn;
        visualFullShipsDrawn += other.visualFullShipsDrawn;
        visualShipPartsDrawn += other.visualShipPartsDrawn;
    }
};





struct SceneRenderPolicy
{
    bool drawStarfield = true;
    bool drawCelestial = true;
    bool drawFarStationProxy = true;
    bool drawLabels = true;
    bool drawDebug = true;
    bool drawVisualShips = true;
    bool drawVisualDrones = true;
    bool drawObjects = true;

    bool drawRealShips = true;
    bool drawPlayerShip = true;
    bool drawNpcShips = true;
    bool drawTrafficShips = true;

    int maxVisualShipsToDraw = -1;
};





class SceneRenderer
{
public:
    SceneRenderer();
    ~SceneRenderer() = default;

    bool initializeStaticResources();


    PreparedScene prepareScene(
        const ClientWorldState& world,
        EntityId playerId
    );

    void renderPrepared(
        const PreparedScene& prepared,
        const SceneCameraParams& camera,
        const SceneRenderPolicy& policy
    );


    
    void render(const ClientWorldState& world,
            EntityId playerId,
            const glm::mat4& view,
            const glm::mat4& proj,
            int cameraId,
            const std::string& cameraName,
            const SceneRenderPolicy& policy
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

    void renderConstellationHoverOverlay(
        const glm::mat4& view,
        const glm::mat4& proj
    );

    std::unique_ptr<Font> m_starLabelFont;

    const SceneRenderStats& lastStats() const
    {
        return m_lastStats;
    }


private:

    glm::mat4 makePerspectiveForCurrentViewport(
        float fovDeg,
        float nearPlane,
        float farPlane
    ) const;


    void renderInternal(
    const PreparedScene& prepared,
    const glm::mat4& view,
    const glm::mat4& proj,
    int cameraId,
    const std::string& cameraName,
    const SceneRenderPolicy& policy
);



    void renderCelestialPass(
        const glm::mat4& view,
        const world::coordinates::WorldFrame& frame,
        float timeSeconds
    );

    void renderFarStationProxyPass(
        const ClientWorldState& world,
        const glm::mat4& view,
        const glm::vec3& cameraPos,
        const world::coordinates::WorldFrame& frame
    );

    void renderVisualShips(
    const ClientWorldState& world,
    const Frustum& frustum,
    const glm::mat4& view,
    const glm::mat4& proj,
    const glm::vec3& cameraPos,
    const world::coordinates::WorldFrame& frame,
    unsigned int fillShader,
    unsigned int edgeShader,
    int maxVisualShipsToDraw
);


    void renderVisualDrones(
        const ClientWorldState& world,
        const Frustum& frustum,
        const glm::mat4& view,
        const glm::mat4& proj,
        const glm::vec3& cameraPos,
        const world::coordinates::WorldFrame& frame,
        unsigned int fillShader,
        unsigned int edgeShader
    );
    
    std::unique_ptr<DebugLineRenderer>  m_debugLines;;
    bool                                m_initialized;
    MeshRenderer                        m_meshRenderer;
    DebugRenderer                       m_debugRenderer;
    GalaxyStarfieldRenderer             m_starfieldRenderer;
    
    // --- DEBUG ---
    std::function<void(const DebugFrameData&)> m_debugCallback;
    uint64_t m_frameCounter = 0;

    SceneRenderStats m_lastStats;
    PlanetWireRenderer m_planetRenderer;
    
};