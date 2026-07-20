#pragma once

#include <glad/gl.h>

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

#include <array>
#include <cstddef>
#include <cstdint>

#include <glm/glm.hpp>

#include "render/types/Viewport.h"

#include "src/world/celestial/CelestialTypes.h"
#include "src/world/celestial/SystemMapTypes.h"
#include "src/game/navigation/GalaxyNavigationGrid.h"
#include "src/game/navigation/SystemNavigationGrid.h"
#include "src/game/navigation/NavigationRegionCatalog.h"
#include "src/world/celestial/visual/CelestialGeneratedAssetLibrary.h"
#include "src/world/celestial/visual/CelestialEnvironmentProfile.h"

#include "src/render/celestial/CelestialShapeMesh.h"
#include "src/render/celestial/ProceduralCloudLayer.h"
#include "src/render/celestial/HubSphericalGridRenderer.h"
#include "src/render/celestial/HubPlanetSurfaceRenderer.h"
#include "src/render/celestial/PlanetGlobeMeshRenderer.h"
#include "src/render/starfield/GalaxyStarfieldRenderer.h"


#include "src/render/celestial/clouds/HubBackdropCloudRenderer.h"
#include "src/render/celestial/rings/PlanetRingRenderer.h"
#include "src/render/system_map/HubMapGpuGeometryRenderer.h"
#include "src/render/system_map/HubPlanetOverlayRenderer.h"
#include "src/render/navigation/NavigationCellLabelLayer.h"

#include "src/game/system_map/GalaxyMapVisualSettings.h"
#include "src/game/system_map/MapTransitionController.h"

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

    struct HubMapPerformanceStats
    {
        /*
            CPU wall-clock timings.

            Они показывают стоимость подготовки команд:
            расчётов, циклов, immediate mode, сборки геометрии,
            вызовов renderer-ов и текста.
        */
        double cpuTotalMs = 0.0;
        double cpuBackgroundMs = 0.0;
        double cpuPlanetBackdropMs = 0.0;
        double cpuGeometryMs = 0.0;
        double cpuLabelsMs = 0.0;

        /*
            GPU timer query results.

            Значения приходят с задержкой в несколько кадров,
            чтобы не делать glFinish и не тормозить игру самим
            профайлером.
        */
        bool gpuValid = false;

        double gpuTotalMs = 0.0;
        double gpuBackgroundMs = 0.0;
        double gpuFallbackBodyMs = 0.0;
        double gpuSurfaceMs = 0.0;
        double gpuCloudsMs = 0.0;
        double gpuAtmosphereMs = 0.0;
        double gpuGeometryMs = 0.0;
        double gpuLabelsMs = 0.0;
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

    int consumeRequestedSystemEntry();

    void handleInput(
        const Viewport& vp,
        const world::celestial::GalaxyMapSnapshot& galaxy
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
        return m_hubMapPerformanceStats;
    }

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

    struct SystemBodyVisualMetrics
    {
        // Реальный радиус тела в map-world units.
        float physicalRadiusWorld = 0.0f;

        // Реальный экранный радиус тела.
        float physicalRadiusPx = 0.0f;

        // Радиус навигационного маркера.
        // Это НЕ физический размер тела.
        float markerRadiusPx = 0.0f;
        float markerRadiusWorld = 0.0f;

        // Радиус для mouse picking.
        float pickRadiusPx = 0.0f;

        bool drawPhysicalBody = false;
        bool drawMarker = false;
    };

    struct GalaxyCamera
    {
        float yaw = 0.65f;
        float pitch = 0.72f;
        float distance = 82.0f;
        glm::vec3 target {0.0f, 0.0f, 0.0f};

        bool rotating = false;
        bool panning = false;
        bool leftWasDown = false;
        bool rightWasDown = false;

        double lastMouseX = 0.0;
        double lastMouseY = 0.0;

        float lastWheelY = 0.0f;
    };




    struct GalaxyCameraFlight
    {
        bool active = false;

        glm::vec3 startTarget {0.0f};
        glm::vec3 destinationTarget {0.0f};

        float startDistance = 82.0f;
        float destinationDistance = 82.0f;

        double startTimeSeconds = 0.0;
        double durationSeconds = 0.0;
    };


    struct SystemCameraFlight
    {
        bool active = false;

        glm::dvec3 startTarget {0.0};
        glm::dvec3 destinationTarget {0.0};

        float startDistance = 95.0f;
        float destinationDistance = 95.0f;

        double startTimeSeconds = 0.0;
        double durationSeconds = 0.58;
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

        float zoomInFactor = 0.935f;
        float zoomOutFactor = 1.07f;

        /* Needed for the terminal ~260 AU Galaxy cell. */
        float minDistance = 0.002f;
        float maxDistance = 700.0f;

        // Радиус поиска pivot-звезды при старте вращения.
        float pivotPickRadiusPx = 120.0f;

        // Радиус кликового выбора системы.
        float systemPickRadiusPx = 32.0f;

        double clickMoveThresholdPx = 5.0;
    };

    struct SystemControlSettings
    {
        // Экранный радиус навигационного маркера для малых лун.
        // ВАЖНО: это не физический радиус тела.
        float tinyMoonProxyRadiusPx = 4.0f;

        // Ниже этого размера физическое тело не рисуем как сферу.
        // Вместо этого рисуется marker overlay.
        float minPhysicalBodyRadiusPx = 0.70f;

        // Маркеры для тел, которые физически меньше удобного экранного размера.
        float starMarkerRadiusPx = 3.0f;
        float planetMarkerRadiusPx = 3.5f;

        // Минимальный radius для мышиного выбора.
        float pickMinBodyRadiusPx = 6.0f;

        float rotateSensitivity = 0.008f;
        float pitchLimitRad = 1.52f;

        float zoomStep = 1.28f;

        double clickMoveThresholdPx = 5.0;

        // Terminal 100 km navigation cube must remain inspectable.
        // 0.1 km/px gives roughly 1000 px across that cube.
        double minKmPerPixel = 0.1;

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






    glm::dvec3 planetMapCameraSpaceRelative(
        const glm::dvec3& relativeMeters
    ) const;

    glm::mat3 planetBodyToDetailCameraMatrix(
        const world::celestial::PlanetMapSnapshot& planet
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

    GLuint globalNormalTextureForGeneratedAsset(
        const world::celestial::visual::CelestialGeneratedAssetSet& asset
    );

    GLuint globalNormalTextureForHubSnapshot(
        const world::celestial::HubMapSnapshot& hub
    );





    GLuint globalAlbedoTextureForHubSnapshot(
        const world::celestial::HubMapSnapshot& hub
    );

    GLuint globalCloudsTextureForGeneratedAsset(
        const world::celestial::visual::CelestialGeneratedAssetSet& asset
    );

    GLuint globalCloudsTextureForHubSnapshot(
        const world::celestial::HubMapSnapshot& hub
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


    enum class HubGpuStage : std::size_t
    {
        Background = 0,
        FallbackBody,
        Surface,
        Clouds,
        Atmosphere,
        Geometry,
        Labels,
        Count
    };

    static constexpr std::size_t kHubGpuStageCount =
        static_cast<std::size_t>(
            HubGpuStage::Count
        );

    static constexpr std::size_t kHubGpuQuerySlotCount =
        4;

    void ensureHubGpuQueries();
    void collectHubGpuQueries();

    void beginHubGpuFrame();
    void endHubGpuFrame();

    void beginHubGpuStage(
        HubGpuStage stage
    );

    void endHubGpuStage();

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
        const glm::vec4& color,
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



    void drawHubMapScreenMarker(
        const glm::dvec2& screenPx,
        double radiusPx,
        const glm::vec4& color,
        bool drawCross,
        int segments = 32
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
        const glm::vec4& color
    );

    void drawHubMapCircleLocalXY(
        const glm::dvec3& center,
        double radiusMeters,
        double scale,
        const glm::dvec2& centerPx,
        int segments = 192
    );

    



    
    
    void drawHubMapTexturedSphereDiskLayer(
    GLuint texture,
    const glm::dvec2& centerPx,
    double radiusPx,
    const glm::vec4& color,
    double uOffset = 0.0,
    int gridX = 120,
    int gridY = 120
);

void drawHubMapPlanetHorizonBand(
    const glm::dvec2& centerPx,
    double radiusPx,
    const glm::vec4& innerColor,
    const glm::vec4& outerColor,
    double innerRadiusFactor,
    double outerRadiusFactor,
    int segments = 192
);





struct HubPlanetAtmosphereStyle
{
    bool enabled = false;
    

    float visualIntensity = 1.0f;
    float radiusScale = 1.018f;

    glm::vec4 oceanInner {
        0.006f,
        0.035f,
        0.090f,
        0.96f
    };

    glm::vec4 oceanOuter {
        0.025f,
        0.095f,
        0.170f,
        0.96f
    };

    glm::vec4 surfaceHaze {
        0.68f,
        0.84f,
        1.00f,
        0.22f
    };

    glm::vec4 limbCore {
        0.88f,
        0.97f,
        1.00f,
        0.16f
    };

    glm::vec4 nearAtmosphere {
        0.30f,
        0.64f,
        1.00f,
        0.16f
    };

    glm::vec4 outerAtmosphere {
        0.12f,
        0.34f,
        0.78f,
        0.075f
    };
};



HubPlanetAtmosphereStyle hubPlanetAtmosphereStyleForHub(
    const world::celestial::HubMapSnapshot& hub
) const;




void drawHubMapPlanetSoftBand(
    const glm::dvec2& planetCenterPx,
    double planetRadiusPx,
    const glm::vec4& peakColor,
    double startRadiusFactor,
    double peakRadiusFactor,
    double endRadiusFactor,
    int radialSteps = 24,
    int segments = 256
);




void drawHubMapPlanetAtmosphereStack(
    const glm::dvec2& planetCenterPx,
    double planetRadiusPx,
    const HubPlanetAtmosphereStyle& style,
    bool premultipliedTarget = false
);


glm::mat3 hubCameraToParentPlanetBodyMatrix(
    const world::celestial::HubMapSnapshot& hub
) const;


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





    void ensureEnvironmentProfiles();

    void beginEnvironmentRenderSessionIfNeeded(
        Mode mode,
        int systemId,
        const std::string& bodyId
    );

    world::celestial::visual::CelestialEnvironmentProfile
    resolvedEnvironmentProfileForBody(
        int systemId,
        const std::string& bodyId,
        const std::string& displayName,
        const std::string& environmentPresetId
    ) const;



    std::vector<
        render::celestial::ProceduralCloudStyle
    >
    cloudStylesForBody(
        int systemId,
        const std::string& bodyId,
        const std::string& displayName,
        const std::string& environmentPresetId,
        double planetRadiusMeters,
        int textureWidth,
        int textureHeight
    ) const;



    HubPlanetAtmosphereStyle atmosphereStyleForBody(
        int systemId,
        const std::string& bodyId,
        const std::string& displayName,
        const std::string& environmentPresetId
    ) const;






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



    void addGalaxyStarHalo(
        const glm::vec3& center,
        float starRadius,
        float outerRadiusScale,
        float baseAlpha,
        const glm::vec4& color,
        const glm::mat4& view,
        int ringCount,
        int segments
    );




    void addSystemBodyMarker(
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

    glm::vec3 galaxyPositionLyToRender(
        const glm::dvec3& positionLy
    ) const;

    glm::vec3 galaxyVectorLyToRender(
        const glm::dvec3& vectorLy
    ) const;

    void addGalaxyNavigationCubeEdges(
        const glm::vec3& center,
        const glm::vec3& halfAxisX,
        const glm::vec3& halfAxisY,
        const glm::vec3& halfAxisZ,
        const glm::vec4& color
    );

    void drawGalaxyNavigationGrid(
        const Viewport& vp,
        const glm::mat4& mvp
    );

    void drawGalaxyNavigationCellLabels(
        const Viewport& vp,
        const glm::mat4& mvp
    );

    void drawSystemNavigationGrid(
        const Viewport& vp,
        const glm::mat4& mvp,
        float systemScale
    );

    void drawSystemNavigationCellLabels(
        const Viewport& vp,
        const glm::mat4& mvp,
        float systemScale
    );

    void updateSystemNavigationHoverFromCursor(
        const Viewport& vp,
        double localMouseX,
        double localMouseY
    );

    bool pickSystemNavigationCell(
        const Viewport& vp,
        double localMouseX,
        double localMouseY,
        game::navigation::CubicNavigationCell& outCell
    ) const;

    float systemNavigationAnchorDiameterPx(
        const Viewport& vp
    ) const;

    void updateGalaxyNavigationHoverFromCursor(
        const Viewport& vp,
        double localMouseX,
        double localMouseY
    );

    bool pickGalaxyNavigationCell(
        const Viewport& vp,
        double localMouseX,
        double localMouseY,
        game::navigation::GalaxyNavigationCell& outCell
    ) const;

    float galaxyNavigationAnchorDiameterPx(
        const Viewport& vp
    ) const;

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

    SystemBodyVisualMetrics computeSystemBodyVisualMetrics(
        const world::celestial::SystemMapBody& body,
        float physicalRadiusWorld,
        double worldUnitsPerPixel
    ) const;



    void beginGalaxyCameraFlight(
        const glm::vec3& destinationTarget,
        float destinationDistance
    );

    void updateGalaxyCameraFlight(
        double nowSeconds
    );

    void cancelGalaxyCameraFlight(
        bool snapToDestination
    );


    void beginSystemCameraFlight(
        const glm::dvec3& destinationTarget,
        float destinationDistance
    );

    void updateSystemCameraFlight(
        double nowSeconds
    );

    void cancelSystemCameraFlight(
        bool snapToDestination
    );


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

 
    std::vector<
        render::celestial::ProceduralCloudStyle
    >
    hubPlanetCloudStylesForHub(
        const world::celestial::HubMapSnapshot& hub
    ) const;



    std::vector<
        render::celestial::ProceduralCloudStyle
    >
    planetCloudStylesForPlanet(
        const world::celestial::PlanetMapSnapshot& planet
    ) const;



    HubPlanetAtmosphereStyle planetAtmosphereStyleForPlanet(
        const world::celestial::PlanetMapSnapshot& planet
    ) const;

    void drawPlanetEnvironmentLayers(
        const world::celestial::PlanetMapSnapshot& planet,
        double scale,
        const glm::dvec2& centerPx,
        bool applySphericalSculpt
    );

    void drawPlanetDetailSculpt(
        const glm::dvec2& planetCenterPx,
        double planetRadiusPx
    );

    void drawPlanetAnimatedCloudLayers(
        const world::celestial::PlanetMapSnapshot& planet,
        double scale,
        const glm::dvec2& centerPx,
        const render::celestial::ProceduralCloudStyle& baseStyle
    );

    void drawPlanetProceduralCloudGlobeLayer(
        const world::celestial::PlanetMapSnapshot& planet,
        double scale,
        const glm::dvec2& centerPx,
        double cloudRadiusScale,
        double longitudeOffset,
        double timeSeconds,
        const render::celestial::ProceduralCloudStyle& style
    );


    void drawPlanetAtmosphereInterior(
        const world::celestial::PlanetMapSnapshot& planet,
        double scale,
        const glm::dvec2& centerPx,
        const HubPlanetAtmosphereStyle& style
    );

    void drawPlanetAtmosphereLimb(
        const world::celestial::PlanetMapSnapshot& planet,
        double scale,
        const glm::dvec2& centerPx,
        const HubPlanetAtmosphereStyle& style
    );



 

   
    render::celestial::HubSphericalGridStyle hubSphericalGridStyleForHub(
        const world::celestial::HubMapSnapshot& hub
    ) const;




    void drawMapStarfield(
        const Viewport& viewport,
        const glm::dvec3& observerPositionLy
    );



    render::celestial::rings::PlanetRingRenderContext
    planetRingRenderContext(
        const world::celestial::PlanetMapSnapshot& planet,
        double scale,
        const glm::dvec2& centerPx,
        std::vector<
            world::celestial::SystemMapRing
        >& normalizedBands
    ) const;




   double environmentVisualTimeSeconds(
        double sourceTimeSeconds
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
    GalaxyCameraFlight m_galaxyCameraFlight;
    SystemCameraFlight m_systemCameraFlight;

    SystemCamera m_systemCamera;

    game::navigation::GalaxyNavigationGrid
        m_galaxyNavigationGrid;

    game::navigation::SystemNavigationGrid
        m_systemNavigationGrid;


    /*
        Физическая точка, которую должна содержать вся последующая
        цепочка дочерних Galaxy-кубов.

        Для выбранной звезды это точная координата звезды.
        Для выбранного центра куба это центр выбранного куба.
    */
    glm::dvec3 m_galaxyNavigationFocusLy {0.0};
    bool m_galaxyNavigationFocusValid = false;

    /*
        Аналогичная точка внутри карты System.
        Единицы: AU.
    */
    glm::dvec3 m_systemNavigationFocusAu {0.0};
    bool m_systemNavigationFocusValid = false;

    game::navigation::NavigationRegionCatalog
        m_navigationRegionCatalog;

    render::navigation::NavigationCellLabelLayer
        m_galaxyNavigationLabelLayer;

    render::navigation::NavigationCellLabelLayer
        m_systemNavigationLabelLayer;

    std::string m_navigationNamingFactionId = "sol_authority";
    std::string m_navigationNamingLocale = "ru";

   

    GalaxyControlSettings m_galaxyControls;
    GalaxyMapVisualSettings m_galaxyVisuals;
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
    int m_requestedSystemEntryId = -1;
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


    world::celestial::visual::CelestialEnvironmentProfileLibrary m_environmentProfiles;

    bool m_environmentProfilesAttempted = false;
    bool m_environmentProfilesLoaded = false;

    std::uint32_t m_environmentMapOpenSeed = 0u;
    std::string m_environmentRenderSessionKey;



    std::unordered_map<std::string, GLuint> m_mapPreviewTextureByAssetKey;
    std::unordered_map<std::string, GLuint> m_globalAlbedoTextureByAssetKey;
    std::unordered_map<std::string, GLuint> m_globalNormalTextureByAssetKey;
    std::unordered_map<std::string, GLuint> m_globalCloudsTextureByAssetKey;
    std::unordered_map<std::string, glm::dvec3> m_lastSystemBodyAbsolutePosById;
    render::celestial::HubBackdropCloudRenderer m_hubBackdropCloudRenderer;

    render::celestial::CelestialShapeMeshLibrary m_celestialShapeMeshes;
    render::celestial::HubPlanetSurfaceRenderer m_hubPlanetSurfaceRenderer;
    render::celestial::PlanetGlobeMeshRenderer m_planetGlobeMeshRenderer;

    render::system_map::HubMapGpuGeometryRenderer m_hubMapGpuGeometryRenderer;
    render::system_map::HubPlanetOverlayRenderer m_hubPlanetOverlayRenderer;

    render::celestial::rings::PlanetRingRenderer m_planetRingRenderer;


    GalaxyStarfieldRenderer m_mapStarfieldRenderer;

    /*
        Details-only screen-space sculpt pass.
        Shader принадлежит ShaderLibrary, поэтому удалять program
        внутри SystemMapRenderer не нужно.
    */
    GLuint m_planetDetailSculptShader = 0;
    GLuint m_planetDetailSculptVao = 0;

    bool m_planetDetailSculptWarningPrinted = false;

    double m_environmentVisualTimeSeconds = 0.0;
    double m_environmentLastSourceTimeSeconds = 0.0;
    double m_environmentLastWallClockSeconds = 0.0;
    bool m_environmentVisualTimeInitialized = false;

    bool m_mapStarfieldInitialized = false;
    float m_lastSystemScale = 1.0f;
    
    glm::dvec3 m_systemOrbitPivotAbsolute {0.0, 0.0, 0.0};
    bool m_systemOrbitPivotActive = false;

    glm::vec3 m_galaxyOrbitPivotWorld {0.0f, 0.0f, 0.0f};
    bool m_galaxyOrbitPivotActive = false;

    double m_galaxyMouseDownX = 0.0;
    double m_galaxyMouseDownY = 0.0;

    render::celestial::ProceduralCloudLayer m_proceduralCloudLayer;
    render::celestial::HubSphericalGridRenderer m_hubSphericalGridRenderer;
    

    double m_lastHubPlanetVisualRadiusPx = 0.0;
    glm::dvec2 m_lastHubPlanetVisualCenterPx {0.0, 0.0};


    HubMapPerformanceStats m_hubMapPerformanceStats;

    /*
        Четыре набора query позволяют читать результат
        старого кадра без ожидания GPU.
    */
    std::array<
        std::array<
            GLuint,
            kHubGpuStageCount
        >,
        kHubGpuQuerySlotCount
    > m_hubGpuQueries {};

    std::array<
        std::uint32_t,
        kHubGpuQuerySlotCount
    > m_hubGpuIssuedMasks {};

    std::array<
        bool,
        kHubGpuQuerySlotCount
    > m_hubGpuSlotPending {};

    std::array<
        std::uint64_t,
        kHubGpuQuerySlotCount
    > m_hubGpuSlotSerials {};

    bool m_hubGpuQueriesInitialized = false;
    bool m_hubGpuFrameActive = false;
    bool m_hubGpuStageOpen = false;

    std::size_t m_hubGpuCurrentSlot = 0;

    std::uint64_t m_hubGpuFrameSerial = 0;
    std::uint64_t m_hubGpuLastCollectedSerial = 0;



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
