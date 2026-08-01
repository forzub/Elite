#pragma once

#include <glad/gl.h>

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <glm/glm.hpp>

#include "render/types/Viewport.h"

#include "src/world/celestial/CelestialTypes.h"
#include "src/world/celestial/SystemMapTypes.h"
#include "src/game/navigation/GalaxyNavigationGrid.h"
#include "src/game/navigation/SystemNavigationGrid.h"

#include "src/game/navigation/CubicNavigationCameraFlight.h"
#include "src/game/navigation/CubicNavigationHierarchy.h"
#include "src/game/navigation/CubicNavigationInteraction.h"


#include "src/game/navigation/NavigationAddressFormatter.h"
#include "src/game/navigation/NavigationRegionCatalog.h"

#include "src/render/celestial/CelestialShapeMesh.h"

#include "src/render/navigation/NavigationCoordinateOverlay.h"



#include "src/game/system_map/SystemMapVisualSettings.h"
#include "src/game/system_map/MapTransitionController.h"
#include "src/game/system_map/MapMode.h"
#include "src/game/system_map/MapIntent.h"
#include "src/game/system_map/MapCameraState.h"
#include "src/game/system_map/GalaxyMapView.h"
#include "src/game/system_map/GalaxyMapInteraction.h"
#include "src/game/system_map/GalaxyMapRenderContext.h"
#include "src/game/system_map/GalaxyMapRenderer.h"
#include "src/game/system_map/SystemMapView.h"
#include "src/game/system_map/SystemMapInteraction.h"
#include "src/game/system_map/SystemMapFrameInteractionContext.h"
#include "src/game/system_map/SystemMapFrameData.h"
#include "src/game/system_map/SystemMapPresentation.h"
#include "src/game/system_map/SystemMapPresentationBuilder.h"
#include "src/game/system_map/SystemMapSceneFrame.h"
#include "src/game/system_map/SystemMapSceneFrameBuilder.h"
#include "src/game/system_map/SystemMapRenderContext.h"
#include "src/game/system_map/SystemMapSceneRenderer.h"
#include "src/game/system_map/DetailMapView.h"
#include "src/game/system_map/HubMapView.h"
#include "src/game/system_map/LocalMapInteraction.h"
#include "src/game/system_map/LocalMapPresentation.h"
#include "src/game/system_map/LocalMapPresentationBuilder.h"
#include "src/game/system_map/DetailMapRenderContext.h"
#include "src/game/system_map/DetailMapSceneRenderer.h"
#include "src/game/system_map/HubMapRenderContext.h"
#include "src/game/system_map/HubMapSceneRenderer.h"
#include "src/game/system_map/DetailMapBackend.h"
#include "src/game/system_map/HubMapBackend.h"
#include "src/game/system_map/MapCelestialRenderResources.h"

struct GLFWwindow;
class SystemMapRenderer
    : private game::system_map::GalaxyMapRenderContext,
      private game::system_map::SystemMapRenderContext
{
public:
    using Mode = game::system_map::MapMode;

    using HubMapPerformanceStats =
        game::system_map::HubMapPerformanceStats;

public:
    SystemMapRenderer();

    void init();

    void setRightPanelRatio(float ratio);
    void render(
        const Viewport& viewport,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const world::celestial::SystemMapSnapshot& system,
        const world::celestial::DetailMapSnapshot& planet,
        const world::celestial::HubMapSnapshot& hub,
        const world::celestial::PlayerNavigationState& nav
    );

    void resetView();

    void onGalaxyMapEntered(
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const world::celestial::PlayerNavigationState& nav
    );

    void cycleNavigationCoordinateFormat();

    void focusGalaxySystem(
        int systemId,
        const world::celestial::GalaxyMapSnapshot& galaxy
    );

    int selectedSystemId() const;

    std::optional<game::system_map::MapIntent> handleInput(
        const Viewport& vp,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const world::celestial::SystemMapSnapshot& system,
        const world::celestial::DetailMapSnapshot& detail,
        const world::celestial::HubMapSnapshot& hub
    );

    void setMode(Mode mode);
    Mode mode() const;
    int focusedSystemId() const;

    bool beginMapTransition(
        const MapTransitionSpec& spec,
        std::function<void()> applyNewState
    );

    const HubMapPerformanceStats&
    hubMapPerformanceStats() const
    {
        return m_hubBackend.performanceStats();
    }

    const std::string& selectedBodyId() const;
    const std::string& selectedHubId() const;
    const std::string& selectedHubParentBodyId() const;

    bool canOpenSelectedDetail() const;

    std::optional<world::celestial::DetailSpatialCell>
    selectedTerminalDetailCell() const;

private:

    struct Vertex
    {
        glm::vec3 pos;
        glm::vec4 color;
    };

    struct TexturedVertex
    {
        glm::vec3 pos;
        glm::vec2 uv;
        glm::vec4 color;
    };

    struct TexturedBatch
    {
        GLuint texture = 0;
        std::vector<TexturedVertex> vertices;
    };

    using HubMapPickable = game::system_map::HubMapPickable;

    using SystemBodyVisualMetrics =
        game::system_map::SystemBodyVisualMetrics;


    game::system_map::DetailMapView m_detailView;
    game::system_map::HubMapView m_hubView;
    game::system_map::LocalMapInteraction m_localMapInteraction;
    game::system_map::LocalMapPresentationBuilder
        m_localMapPresentationBuilder;
    game::system_map::DetailMapPresentation m_detailPresentation;
    game::system_map::HubMapPresentation m_hubPresentation;
    game::system_map::DetailMapSceneRenderer m_detailSceneRenderer;
    game::system_map::HubMapSceneRenderer m_hubSceneRenderer;
    game::system_map::MapCelestialRenderResources m_mapResources;
    game::system_map::DetailMapBackend m_detailBackend;
    game::system_map::HubMapBackend m_hubBackend;

    void ensureMapTransitionSnapshot(
    const Viewport& viewport
    );

    void captureMapTransitionSnapshot(
        const Viewport& viewport
    );

    void drawMapTransitionSnapshot(
        const Viewport& viewport,
        float alpha
    );

    void drawNavigationCoordinateOverlay(
        const Viewport& viewport,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const world::celestial::SystemMapSnapshot& system,
        const world::celestial::PlayerNavigationState& nav
    );

    void announceNavigationLevel(
        char mapPrefix,
        int level
    );

    void resetNavigationViewToLevelZero(
        const Viewport& viewport
    );

private:
    void ensureGlObjects();
    void ensureShader();

    void ensureTexturedGlObjects();
    void ensureTexturedShader();

    void ensureBackground();
    void drawBackground();
    void drawMapAtmosphereVeil(
        float centerAlpha,
        float edgeAlpha,
        float aquaStrength
    ) override;

    double currentTimeSeconds() const override;

    void beginTextFrame(
        int viewportWidth,
        int viewportHeight
    ) override;

    void drawTextPx(
        const std::string& text,
        float x,
        float y,
        int pixelHeight,
        const glm::vec4& color
    ) override;

    void endTextFrame() override;



    void beginLines() override;
    void addLine(
        const glm::vec3& a,
        const glm::vec3& b,
        const glm::vec4& color
    ) override;

    void addCircleXZ(
        const glm::vec3& center,
        float radius,
        const glm::vec4& color,
        int segments = 96
    ) override;

    void addCircleXY(
        const glm::vec3& center,
        float radius,
        const glm::vec4& color,
        int segments = 96
    ) override;


    void addOrbitCircle3D(
        const glm::vec3& center,
        float radius,
        double inclinationDeg,
        double longitudeOfAscendingNodeDeg,
        double argumentOfPeriapsisDeg,
        const glm::vec4& color,
        int segments = 160
    );


    void beginSolids() override;

    void addBillboardBall(
        const glm::vec3& center,
        float radius,
        const glm::vec4& color,
        const glm::mat4& view,
        int segments = 32
    ) override;



    void addBillboardHalo(
        const glm::vec3& center,
        float starRadius,
        float outerRadiusScale,
        float baseAlpha,
        const glm::vec4& color,
        const glm::mat4& view,
        int ringCount,
        int segments
    ) override;




    void addSystemBodyMarkerPrimitive(
        const glm::vec3& center,
        float radius,
        const glm::vec4& color,
        const glm::mat4& view,
        int segments = 32
    );

    void flushSolids(const glm::mat4& mvp) override;

    void beginTexturedBodies() override;


    void addTexturedSystemBodySphere(
        const world::celestial::SystemMapBody& body,
        GLuint texture,
        const glm::vec3& center,
        float radius,
        const glm::vec4& color,
        int latSegments = 28,
        int lonSegments = 56
    );




    void flushTexturedBodies(const glm::mat4& mvp) override;


    void addCross(
        const glm::vec3& center,
        float size,
        const glm::vec4& color
    ) override;
    void flushLines(const glm::mat4& mvp) override;

    void addNavigationCubeEdges(
        const glm::vec3& center,
        const glm::vec3& halfAxisX,
        const glm::vec3& halfAxisY,
        const glm::vec3& halfAxisZ,
        const glm::vec4& color
    );

    void drawSystemNavigationGrid(
        const Viewport& vp,
        const glm::mat4& mvp,
        float systemScale
    ) override;


    void ensureSystemRenderResources() override;

    bool renderSystemBodyRings(
        const world::celestial::SystemMapBody& body,
        const glm::vec3& center,
        const game::system_map::SystemBodyVisualMetrics& metrics,
        const glm::mat4& view,
        const glm::mat4& mvp,
        const Viewport& viewport,
        game::system_map::SystemMapRingPart part
    ) override;

    void addSystemBodyGeometry(
        const world::celestial::SystemMapBody& body,
        const glm::vec3& center,
        const game::system_map::SystemBodyVisualMetrics& metrics,
        const glm::vec4& fallbackColor,
        const glm::mat4& view
    ) override;

    void addSystemBodyMarker(
        const world::celestial::SystemMapBody& body,
        const glm::vec3& center,
        const game::system_map::SystemBodyVisualMetrics& metrics,
        const glm::vec4& fallbackColor,
        const glm::mat4& view
    ) override;

    void renderSystem(
        const Viewport& vp,
        const world::celestial::SystemMapSnapshot& system,
        const world::celestial::PlayerNavigationState& nav
    );


    glm::vec4 colorForBodyType(
        world::celestial::BodyType type
    ) const override;

    float bodyVisualRadius(
        const world::celestial::SystemMapBody& body,
        float distanceScale
    ) const override;

    game::system_map::SystemBodyVisualMetrics
    computeSystemBodyVisualMetrics(
        const world::celestial::SystemMapBody& body,
        float physicalRadiusWorld,
        double worldUnitsPerPixel
    ) const override;




    glm::vec2 projectToScreen(
        const glm::vec3& world,
        const glm::mat4& mvp,
        const Viewport& vp,
        bool& visible,
        float& depth
    ) const override;

    void drawSystemLabels(
        const Viewport& vp,
        const world::celestial::SystemMapSnapshot& system,
        const glm::mat4& mvp,
        const std::unordered_map<std::string, glm::vec3>& posById,
        const std::unordered_map<
            std::string,
            game::system_map::SystemBodyVisualMetrics
        >& presentationById
    ) override;




    void addMapObjectCube(
        const glm::vec3& center,
        float size,
        const glm::vec4& color
    );


    float systemObjectOcclusionAlpha(
        const world::celestial::SystemMapObject& obj,
        const glm::vec3& objectVisualPos,
        const glm::mat4& view,
        const std::unordered_map<std::string, glm::vec3>& bodyVisualPosById,
        const std::unordered_map<std::string, float>& drawRadiusById
    ) const;

    void drawSystemObjectOverlays(
        const world::celestial::SystemMapSnapshot& system,
        const glm::mat4& view,
        const glm::mat4& mvp,
        const std::unordered_map<std::string, glm::vec3>& objectVisualPosById,
        const std::unordered_map<std::string, glm::vec3>& bodyVisualPosById,
        const std::unordered_map<std::string, float>& drawRadiusById,
        double worldUnitsPerPixel,
        float systemScale
    ) override;

    void drawSystemObjectLabels(
        const Viewport& vp,
        const world::celestial::SystemMapSnapshot& system,
        const glm::mat4& mvp,
        const glm::mat4& view,
        const std::unordered_map<std::string, glm::vec3>& objectVisualPosById,
        const std::unordered_map<std::string, glm::vec3>& bodyVisualPosById,
        const std::unordered_map<std::string, float>& drawRadiusById
    ) override;



    void handleDetailAndHubInput(
        const Viewport& vp,
        GLFWwindow* window,
        double mx,
        double my,
        double localMx,
        double localMy,
        bool inside,
        bool leftDown,
        bool rightDown
    );

    void drawMapStarfield(
        const Viewport& viewport,
        const glm::dvec3& observerPositionLy,
        const glm::mat4& cameraView,
        float fieldOfViewDeg,
        float sizeScale,
        bool distantGalaxyBackdrop,
        float starBrightnessScale = 1.0f,
        float milkyWayIntensityScale = 1.0f,
        const glm::vec3& milkyWayColorTint = glm::vec3(1.0f)
    ) override;


private:
    bool m_initialized = false;

    GLuint m_vao = 0;
    GLuint m_vbo = 0;

    GLuint m_shader = 0;
    GLint  m_mvpLoc = -1;

    GLuint m_texturedVao = 0;
    GLuint m_texturedVbo = 0;
    GLuint m_texturedShader = 0;
    GLint  m_texturedMvpLoc = -1;
    GLint  m_texturedSamplerLoc = -1;

    GLuint m_bgVao = 0;
    GLuint m_bgVbo = 0;
    GLuint m_bgShader = 0;

    std::vector<Vertex> m_vertices;
    std::vector<Vertex> m_solidVertices;
    std::vector<TexturedBatch> m_texturedBatches;

    Mode m_mode = Mode::Galaxy;
    float m_rightPanelRatio = 0.28f;

    double m_pendingScrollY = 0.0;

    game::system_map::GalaxyMapView m_galaxyView;
    game::system_map::GalaxyMapInteraction m_galaxyInteraction;
    game::system_map::GalaxyMapRenderer m_galaxyRenderer;

    game::system_map::SystemMapView m_systemView;
    game::system_map::SystemMapInteraction m_systemInteraction;
    game::system_map::SystemMapPresentationBuilder
        m_systemPresentationBuilder;
    game::system_map::SystemMapPresentation m_systemPresentation;
    game::system_map::SystemMapSceneFrameBuilder
        m_systemSceneFrameBuilder;
    game::system_map::SystemMapSceneFrame m_systemSceneFrame;
    game::system_map::SystemMapSceneRenderer m_systemSceneRenderer;

    bool m_systemFramePrepared = false;
    bool m_systemSceneFrameDirty = true;
    bool m_detailFramePrepared = false;
    bool m_detailFrameDirty = true;
    bool m_hubFramePrepared = false;
    bool m_hubFrameDirty = true;

    game::navigation::NavigationRegionCatalog
        m_navigationRegionCatalog;

    game::navigation::NavigationCoordinateFormat
        m_navigationCoordinateFormat =
            game::navigation::NavigationCoordinateFormat::Hierarchical;

    render::navigation::NavigationCoordinateOverlay
        m_navigationCoordinateOverlay;

    bool m_navigationLevelZeroButtonHovered = false;
    bool m_navigationOverlayLeftWasDown = false;

    struct NavigationLevelAnnouncement
    {
        std::string text;
        double startedAtSeconds = -1.0;
        double durationSeconds = 1.35;
    };

    NavigationLevelAnnouncement
        m_navigationLevelAnnouncement;

    std::string m_navigationNamingFactionId = "sol_authority";
    std::string m_navigationNamingLocale = "ru";



    /*
        Отдельный single-sample framebuffer нужен для разрешения
        MSAA-кадра карты в обычную текстуру перехода.
    */
    GLuint m_mapTransitionSnapshotFramebuffer = 0;
    GLuint m_mapTransitionSnapshotTexture = 0;

    int m_mapTransitionSnapshotWidth = 0;
    int m_mapTransitionSnapshotHeight = 0;

    bool m_mapTransitionSnapshotReady = false;

    MapTransitionController m_mapTransition;
};
