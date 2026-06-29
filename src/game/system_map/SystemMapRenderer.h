#pragma once

#include <glad/gl.h>

#include <string>
#include <vector>
#include <unordered_map>

#include <glm/glm.hpp>

#include "render/types/Viewport.h"

#include "src/world/celestial/CelestialTypes.h"
#include "src/world/celestial/SystemMapTypes.h"
#include "src/world/celestial/visual/CelestialGeneratedAssetLibrary.h"
#include "src/render/celestial/CelestialShapeMesh.h"

struct GLFWwindow;
class SystemMapRenderer
{
public:
    enum class Mode
    {
        Galaxy,
        System,
        Planet,
        Hub
    };

public:
    void init();
    void setRightPanelRatio(float ratio);
    void render(
        const Viewport& viewport,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const world::celestial::SystemMapSnapshot& system,
        const world::celestial::PlanetMapSnapshot& planet,
        const world::celestial::HubMapSnapshot& hub,
        const world::celestial::PlayerNavigationState& nav
    );

    void resetView();

    void focusGalaxySystem(
        int systemId,
        const world::celestial::GalaxyMapSnapshot& galaxy
    );

    int selectedSystemId() const;

    void handleInput(
        const Viewport& vp,
        const world::celestial::GalaxyMapSnapshot& galaxy
    );

    void setMode(Mode mode);
    Mode mode() const;
    int focusedSystemId() const;

    const std::string& selectedBodyId() const;

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

    struct ScreenPoint
    {
        int systemId = -1;
        std::string name;
        glm::vec3 world {0.0f};
        glm::vec2 screen {0.0f};
        float depth = 0.0f;
        bool visible = false;
    };


    struct BodyScreenPoint
    {
        std::string bodyId;
        std::string name;
        glm::vec2 screen {0.0f};
        float depth = 0.0f;
        bool visible = false;
        float screenRadiusPx = 0.0f;
    };

    struct GalaxyCamera
    {
        float yaw = 0.65f;
        float pitch = 0.72f;
        float distance = 150.0f;
        glm::vec3 target {0.0f, 0.0f, 0.0f};

        bool rotating = false;
        bool panning = false;
        bool leftWasDown = false;
        bool rightWasDown = false;

        double lastMouseX = 0.0;
        double lastMouseY = 0.0;

        float lastWheelY = 0.0f;
    };

    struct SystemCamera
    {
        float yaw = 0.45f;
        float pitch = 0.85f;
        float distance = 95.0f;
        glm::dvec3 target {0.0, 0.0, 0.0};

        bool rotating = false;
        bool panning = false;

        bool leftWasDown = false;
        bool rightWasDown = false;

        double lastMouseX = 0.0;
        double lastMouseY = 0.0;

        double mouseDownX = 0.0;
        double mouseDownY = 0.0;
    };

    struct DetailCamera
    {
        double yaw = 0.6;
        double pitch = 0.35;
        double zoom = 1.0;
        glm::dvec2 pan {0.0};

        bool rotating = false;
        bool panning = false;

        double lastMouseX = 0.0;
        double lastMouseY = 0.0;
    };

    DetailCamera m_planetCamera;
    DetailCamera m_hubCamera;
















    struct GalaxyControlSettings
    {
        float rotateSensitivity = 0.008f;
        float pitchLimitRad = 1.52f;

        float panScaleByDistance = 0.0018f;

        float zoomInFactor = 0.88f;
        float zoomOutFactor = 1.12f;

        float minDistance = 8.0f;
        float maxDistance = 700.0f;

        // Радиус поиска pivot-звезды при старте вращения.
        float pivotPickRadiusPx = 120.0f;

        // Радиус кликового выбора системы.
        float systemPickRadiusPx = 32.0f;

        double clickMoveThresholdPx = 5.0;
    };

    struct SystemControlSettings
    {
        // Минимальный экранный размер для малых лун в system map.
        // Фобос/Деймос не должны рисоваться OBJ-мешем на карте системы,
        // но должны быть видимыми навигационными маркерами.
        float tinyMoonProxyRadiusPx = 4.0f;

        float rotateSensitivity = 0.008f;
        float pitchLimitRad = 1.52f;

        float zoomStep = 1.28f;

        double clickMoveThresholdPx = 5.0;

        // Максимальное приближение system map:
        // 1 пиксель = 20 км.
        double minKmPerPixel = 20.0;

        // Picking видимых дисков планет/лун.
        float pickMaxBodyRadiusPx = 8000.0f;
        float pickHaloBasePx = 48.0f;
        float pickHaloRadiusFactor = 0.08f;
        float pickHaloMaxPx = 220.0f;
        float pickScoreDiskWeight = 10000.0f;

        // Если на экране мало тел, orbit-pivot можно цеплять
        // не только по строгому hit-test рядом с мышью,
        // а по ближайшему видимому телу в viewport.
        int sparsePivotMaxVisibleBodies = 2;

        // Небольшой запас за границами viewport.
        // Нужен для крупных тел, центр которых уже за экраном,
        // но диск всё ещё занимает видимую область.
        float sparsePivotViewportPaddingPx = 96.0f;
    };

    struct DetailControlSettings
    {
        double rotateSensitivity = 0.008;

        double zoomStep = 1.08;
        double minZoom = 0.15;
        double maxZoom = 16.0;
    };



    // Hub map orbit pivot.
    // This is the local-space point that should remain visually fixed
    // while the 3D hub map rotates.
    glm::dvec3 m_hubMapOrbitPivotLocalMeters {0.0, 0.0, 0.0};

    // Screen position where the orbit pivot was captured.
    // Kept for future picking/debug; current compensation uses project-before/project-after.
    glm::dvec2 m_hubMapOrbitPivotScreenPx {0.0, 0.0};

    // Last rendered hub map scale.
    // Input code needs this to convert mouse pixels back to hub-local meters.
    double m_lastHubMapScale = 1.0;

    glm::dvec2 m_lastHubMapCenterPx {0.0, 0.0};

    struct HubMapPickable
    {
        glm::dvec3 localCenterMeters {0.0, 0.0, 0.0};
        glm::dvec2 screenCenterPx {0.0, 0.0};

        double screenRadiusPx = 16.0;

        // Чем выше priority, тем охотнее выбираем объект.
        // Например: player > ship > station module.
        int priority = 0;

        std::string label;
    };

    std::vector<HubMapPickable> m_lastHubMapPickables;

    bool pickHubMapOrbitPivot(
        const glm::dvec2& mousePx,
        glm::dvec3& outPivotLocalMeters
    ) const;



    void drawPlanetSphereGrid(
        const world::celestial::PlanetMapSnapshot& planet,
        double scale,
        const glm::dvec2& centerPx
    );

    void drawPlanetFilledDisk(
        const world::celestial::PlanetMapSnapshot& planet,
        double scale,
        const glm::dvec2& centerPx
    );

    void drawPlanetTexturedGlobe(
        const world::celestial::PlanetMapSnapshot& planet,
        double scale,
        const glm::dvec2& centerPx
    );




    bool drawPlanetShapeModelDetail(
        const world::celestial::PlanetMapSnapshot& planet,
        double scale,
        const glm::dvec2& centerPx
    );












    glm::dvec3 planetMapCameraSpaceRelative(
        const glm::dvec3& relativeMeters
    ) const;

    GLuint globalAlbedoTextureForPlanetSnapshot(
        const world::celestial::PlanetMapSnapshot& planet
    );

    GLuint globalAlbedoTextureForGeneratedAsset(
        const world::celestial::visual::CelestialGeneratedAssetSet& asset
    );

    GLuint globalAlbedoTextureForBody(
        const world::celestial::SystemMapBody& body
    );





    const world::celestial::visual::CelestialGeneratedAssetSet*
    generatedAssetForIdentity(
        int systemId,
        const std::string& bodyId,
        const std::string& displayName
    ) const;



    void drawTexturedDisk2D(
        GLuint texture,
        const glm::dvec2& centerPx,
        double radiusPx,
        const glm::vec4& color,
        int segments = 192
    );

    void drawPlanetTexturedDisk(
        const world::celestial::PlanetMapSnapshot& planet,
        double scale,
        const glm::dvec2& centerPx
    );

    GLuint mapPreviewTextureForPlanetSnapshot(
        const world::celestial::PlanetMapSnapshot& planet
    );

    GLuint mapPreviewTextureForHubSnapshot(
        const world::celestial::HubMapSnapshot& hub
    );

    GLuint mapPreviewTextureForGeneratedAsset(
        const world::celestial::visual::CelestialGeneratedAssetSet& asset
    );







    void renderHubMap(
        const Viewport& viewport,
        const world::celestial::HubMapSnapshot& hub
    );

    glm::dvec2 hubMapProject(
        const glm::dvec3& localMeters,
        double scale,
        const glm::dvec2& centerPx
    ) const;

    glm::dvec3 hubMapUnprojectCursorToLocal(
        const glm::dvec2& mousePx,
        double scale,
        const glm::dvec2& centerPx
    ) const;


    void drawHubMapBox(
        const glm::dvec3& center,
        const world::celestial::PlanetMapAxisSet& axes,
        const glm::dvec3& size,
        double scale,
        const glm::dvec2& centerPx
    );

    void drawHubMapAxes(
        const glm::dvec3& center,
        const world::celestial::PlanetMapAxisSet& axes,
        double axisLenMeters,
        double scale,
        const glm::dvec2& centerPx
    );

    void drawHubMapVelocityArrow(
        const glm::dvec3& center,
        const glm::dvec3& velocity,
        double lenMeters,
        double scale,
        const glm::dvec2& centerPx
    );

    glm::dvec3 hubMapObjectLocalToHubLocal(
        const glm::dvec3& objectCenter,
        const world::celestial::PlanetMapAxisSet& objectAxes,
        const glm::dvec3& localPoint
    ) const;

    bool drawHubMapAssemblyWire(
        ObjectType typeId,
        const glm::dvec3& objectCenter,
        const world::celestial::PlanetMapAxisSet& objectAxes,
        double scale,
        const glm::dvec2& centerPx
    );

    void drawHubMapCircleLocalXY(
        const glm::dvec3& center,
        double radiusMeters,
        double scale,
        const glm::dvec2& centerPx,
        int segments = 192
    );

    void drawHubMapPlanetSurfaceHint(
        const world::celestial::HubMapSnapshot& hub,
        double scale,
        const glm::dvec2& centerPx
    );

    void drawHubMapAdaptiveGrid(
        const Viewport& viewport,
        double scale,
        const glm::dvec2& centerPx,
        double worldRadiusMeters
    );

    glm::dvec3 visualSizeForHubShip(
        const world::celestial::HubMapShip& ship,
        double scale
    ) const;
    

private:
    void ensureGlObjects();
    void ensureShader();

    void ensureTexturedGlObjects();
    void ensureTexturedShader();

    void ensureGeneratedCelestialAssets();

    void ensureBackground();
    void drawBackground(); 

    void beginLines();
    void addLine(const glm::vec3& a, const glm::vec3& b, const glm::vec4& color);
    
    void addCircleXZ(
        const glm::vec3& center,
        float radius,
        const glm::vec4& color,
        int segments = 96
    );

    void addCircleXY(
        const glm::vec3& center,
        float radius,
        const glm::vec4& color,
        int segments = 96
    );

    void addCircleYZ(
        const glm::vec3& center,
        float radius,
        const glm::vec4& color,
        int segments = 96
    );

    void addOrbitCircle3D(
        const glm::vec3& center,
        float radius,
        double inclinationDeg,
        double longitudeOfAscendingNodeDeg,
        double argumentOfPeriapsisDeg,
        const glm::vec4& color,
        int segments = 160
    );

    void addSphereWire(
        const glm::vec3& center,
        float radius,
        const glm::vec4& color
    );


    void beginSolids();

    void addBillboardBall(
        const glm::vec3& center,
        float radius,
        const glm::vec4& color,
        const glm::mat4& view,
        int segments = 32
    );

    void flushSolids(const glm::mat4& mvp);

    void beginTexturedBodies();

    void addTexturedBillboard(
        GLuint texture,
        const glm::vec3& center,
        float radius,
        const glm::vec4& color,
        const glm::mat4& view
    );

    void addTexturedSystemBodySphere(
        const world::celestial::SystemMapBody& body,
        GLuint texture,
        const glm::vec3& center,
        float radius,
        const glm::vec4& color,
        int latSegments = 28,
        int lonSegments = 56
    );



    void addTexturedSphere(
        GLuint texture,
        const glm::vec3& center,
        float radius,
        const glm::vec4& color,
        int latSegments = 24,
        int lonSegments = 48
    );

    void flushTexturedBodies(const glm::mat4& mvp);

    GLuint mapPreviewTextureForBody(
        const world::celestial::SystemMapBody& body
    );

    const world::celestial::visual::CelestialGeneratedAssetSet*
    generatedAssetForBody(
        const world::celestial::SystemMapBody& body
    ) const;

    void renderPlanetMap(
        const Viewport& viewport,
        const world::celestial::PlanetMapSnapshot& planet
    );

    glm::dvec2 planetMapProject(
        const glm::dvec3& worldMeters,
        const world::celestial::PlanetMapSnapshot& planet,
        double scale,
        const glm::dvec2& centerPx
    ) const;

    void drawPlanetMapCircle(
        const glm::dvec2& center,
        double radiusPx,
        int segments
    );

    void drawPlanetMapLine(
        const glm::dvec2& a,
        const glm::dvec2& b
    );

    void drawPlanetMapCross(
        const glm::dvec2& p,
        float size
    );

    void drawPlanetMapAxes(
        const glm::dvec3& originMeters,
        const world::celestial::PlanetMapAxisSet& axes,
        const world::celestial::PlanetMapSnapshot& planet,
        double scale,
        const glm::dvec2& centerPx,
        double axisLenMeters
    );

    void drawPlanetMapVelocityArrow(
        const glm::dvec3& originMeters,
        const glm::dvec3& velocityMps,
        const world::celestial::PlanetMapSnapshot& planet,
        double scale,
        const glm::dvec2& centerPx,
        double lenMeters
    );


    void addCross(
        const glm::vec3& center,
        float size,
        const glm::vec4& color
    );
    void flushLines(const glm::mat4& mvp);

    void renderGalaxy(
        const Viewport& vp,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const world::celestial::PlayerNavigationState& nav
    );



    void renderSystem(
        const Viewport& vp,
        const world::celestial::SystemMapSnapshot& system,
        const world::celestial::PlayerNavigationState& nav
    );

    void drawPanelText(
        const Viewport& vp,
        const std::string& title,
        const std::vector<std::string>& lines
    );

    glm::vec4 colorForStarType(const std::string& starType) const;
    glm::vec4 colorForBodyType(world::celestial::BodyType type) const;

    float bodyVisualRadius(
        const world::celestial::SystemMapBody& body,
        float distanceScale
    ) const;

    glm::mat4 galaxyViewMatrix() const;
    glm::mat4 galaxyProjectionMatrix(const Viewport& vp) const;

    glm::mat4 systemViewMatrix() const;
    glm::mat4 systemProjectionMatrix(const Viewport& vp) const;

    glm::vec3 galaxyStarPosition(
        const world::celestial::GalaxyMapSystem& s
    ) const;

    glm::vec2 projectToScreen(
        const glm::vec3& world,
        const glm::mat4& mvp,
        const Viewport& vp,
        bool& visible,
        float& depth
    ) const;

    int pickGalaxySystem(
        const Viewport& vp,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        double mouseX,
        double mouseY
    ) const;

    void drawGalaxyLabels(
        const Viewport& vp,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const glm::mat4& mvp
    );

    void drawSystemLabels(
        const Viewport& vp,
        const world::celestial::SystemMapSnapshot& system,
        const glm::mat4& mvp,
        const std::unordered_map<std::string, glm::vec3>& posById,
        const std::unordered_map<std::string, float>& drawRadiusById
    );

    int pickSystemBody(
        double mouseX,
        double mouseY
    ) const;


    int pickSystemOrbitPivotBody(
        double mouseX,
        double mouseY,
        const Viewport& vp
    ) const;



    void drawNavigationLayerPlaceholder();

    glm::vec3 nearestVisibleStarToScreenPoint(
        const Viewport& vp,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        double localMouseX,
        double localMouseY,
        float maxRadiusPx,
        bool& found
    ) const;

    void debugLabelTraceToFile(
        const std::string& name,
        const glm::vec2& screen,
        const glm::vec2& pos
    ) const;

    void addMapObjectCube(
        const glm::vec3& center,
        float size,
        const glm::vec4& color
    );

    glm::vec3 systemObjectVisualPosition(
        const world::celestial::SystemMapSnapshot& system,
        const world::celestial::SystemMapObject& obj,
        const std::unordered_map<std::string, glm::vec3>& posById,
        const std::unordered_map<std::string, float>& drawRadiusById,
        float systemScale
    ) const;

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
        const std::unordered_map<uint32_t, glm::vec3>& objectVisualPosById,
        const std::unordered_map<std::string, glm::vec3>& bodyVisualPosById,
        const std::unordered_map<std::string, float>& drawRadiusById,
        double worldUnitsPerPixel,
        float systemScale
    );

    void drawSystemObjectLabels(
        const Viewport& vp,
        const world::celestial::SystemMapSnapshot& system,
        const glm::mat4& mvp,
        const glm::mat4& view,
        const std::unordered_map<uint32_t, glm::vec3>& objectVisualPosById,
        const std::unordered_map<std::string, glm::vec3>& bodyVisualPosById,
        const std::unordered_map<std::string, float>& drawRadiusById
    );

    void drawPlanetMapOrbit3D(
        const world::celestial::PlanetMapOrbit& orbit,
        const world::celestial::PlanetMapSnapshot& planet,
        double scale,
        const glm::dvec2& centerPx,
        int segments
    );


    DetailCamera& activeDetailCamera();
    const DetailCamera& activeDetailCamera() const;

    const DetailControlSettings& activeDetailControls() const;  

    void handleSystemInput(
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

    void handleDetailInput(
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

    void handleGalaxyInput(
        const Viewport& vp,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        GLFWwindow* window,
        double mx,
        double my,
        double localMx,
        double localMy,
        bool inside,
        bool leftDown,
        bool rightDown
    );

 

 

   








    

   

    

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

    GalaxyCamera m_galaxyCamera;
    SystemCamera m_systemCamera;

    GalaxyControlSettings m_galaxyControls;
    SystemControlSettings m_systemControls;

    DetailControlSettings m_planetControls;
    DetailControlSettings m_hubControls = []()
    {
        DetailControlSettings settings;

        settings.zoomStep = 1.06;
        settings.minZoom = 0.15;
        settings.maxZoom = 8.0;

        return settings;
    }();

    int m_selectedSystemId = -1;
    int m_focusedSystemId = -1;
    bool m_comboOpen = false;

    std::vector<ScreenPoint> m_lastGalaxyScreenPoints;

    struct SmoothPoint
    {
        glm::vec3 visual {0.0f};
        bool initialized = false;
    };

    std::unordered_map<std::string, SmoothPoint> m_smoothBodyPositions;
    std::unordered_map<uint32_t, SmoothPoint> m_smoothObjectPositions;

    double m_lastSmoothTimeSeconds = 0.0;

    float smoothingAlpha() const;
    glm::vec3 smoothVec3(
        SmoothPoint& point,
        const glm::vec3& target,
        float alpha
    );

    std::string m_selectedBodyId;
    std::vector<BodyScreenPoint> m_lastSystemBodyScreenPoints;
    
    world::celestial::visual::CelestialGeneratedAssetLibrary m_generatedCelestialAssets;

    bool m_generatedCelestialAssetsAttempted = false;
    bool m_generatedCelestialAssetsLoaded = false;

    std::unordered_map<std::string, GLuint> m_mapPreviewTextureByAssetKey;
    std::unordered_map<std::string, GLuint> m_globalAlbedoTextureByAssetKey;
    std::unordered_map<std::string, glm::dvec3> m_lastSystemBodyAbsolutePosById;

    render::celestial::CelestialShapeMeshLibrary m_celestialShapeMeshes;


    float m_lastSystemScale = 1.0f;
    

    glm::dvec3 m_systemOrbitPivotAbsolute {0.0, 0.0, 0.0};
    bool m_systemOrbitPivotActive = false;

    glm::vec3 m_galaxyOrbitPivotWorld {0.0f, 0.0f, 0.0f};
    bool m_galaxyOrbitPivotActive = false;

    double m_galaxyMouseDownX = 0.0;
    double m_galaxyMouseDownY = 0.0;
};


