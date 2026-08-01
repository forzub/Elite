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
#include "src/world/celestial/visual/CelestialGeneratedAssetLibrary.h"
#include "src/world/celestial/visual/CelestialEnvironmentProfile.h"

#include "src/render/celestial/CelestialShapeMesh.h"
#include "src/render/celestial/ProceduralCloudLayer.h"
#include "src/render/celestial/HubSphericalGridRenderer.h"
#include "src/render/celestial/HubPlanetSurfaceRenderer.h"
#include "src/render/celestial/PlanetGlobeMeshRenderer.h"
#include "src/render/starfield/GalaxyStarfieldRenderer.h"

#include "src/render/navigation/NavigationCoordinateOverlay.h"

#include "src/render/celestial/rings/PlanetRingRenderer.h"
#include "src/render/system_map/HubMapGpuGeometryRenderer.h"
#include "src/render/system_map/HubPlanetOverlayRenderer.h"


#include "src/game/system_map/SystemMapVisualSettings.h"
#include "src/game/system_map/DetailMapVisualSettings.h"
#include "src/game/system_map/HubMapVisualSettings.h"
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

struct GLFWwindow;
class SystemMapRenderer
    : private game::system_map::GalaxyMapRenderContext,
      private game::system_map::SystemMapRenderContext,
      private game::system_map::DetailMapRenderContext,
      private game::system_map::HubMapRenderContext
{
public:
    using Mode = game::system_map::MapMode;

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
        return m_hubMapPerformanceStats;
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

    using HubScreenPoint = game::system_map::DetailHubScreenPoint;
    using HubMapPickable = game::system_map::HubMapPickable;

    using SystemBodyVisualMetrics =
        game::system_map::SystemBodyVisualMetrics;

    using DetailControlSettings = game::system_map::LocalMapControlSettings;

    game::system_map::DetailMapView m_detailView;
    game::system_map::HubMapView m_hubView;
    game::system_map::LocalMapInteraction m_localMapInteraction;
    game::system_map::LocalMapPresentationBuilder
        m_localMapPresentationBuilder;
    game::system_map::DetailMapPresentation m_detailPresentation;
    game::system_map::HubMapPresentation m_hubPresentation;
    const game::system_map::LocalMapCameraSnapshot*
        m_activeLocalCameraSnapshot = nullptr;
    game::system_map::DetailMapSceneRenderer m_detailSceneRenderer;
    game::system_map::HubMapSceneRenderer m_hubSceneRenderer;


    void drawPlanetSphereGrid(
        const world::celestial::DetailMapSnapshot& planet,
        double scale,
        const glm::dvec2& centerPx
    );

    void drawPlanetFilledDisk(
        const world::celestial::DetailMapSnapshot& planet,
        double scale,
        const glm::dvec2& centerPx
    );

    void drawPlanetTexturedGlobe(
        const world::celestial::DetailMapSnapshot& planet,
        double scale,
        const glm::dvec2& centerPx
    );




    bool drawPlanetShapeModelDetail(
        const world::celestial::DetailMapSnapshot& planet,
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

    glm::mat3 planetBodyToDetailCameraMatrix(
        const world::celestial::DetailMapSnapshot& planet
    ) const;


    GLuint globalAlbedoTextureForPlanetSnapshot(
        const world::celestial::DetailMapSnapshot& planet
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


    const world::celestial::visual::CelestialGeneratedAssetSet*
    generatedAssetForIdentity(
        int systemId,
        const std::string& bodyId,
        const std::string& displayName
    ) const;





    GLuint mapPreviewTextureForHubSnapshot(
        const world::celestial::HubMapSnapshot& hub
    );

    GLuint mapPreviewTextureForGeneratedAsset(
        const world::celestial::visual::CelestialGeneratedAssetSet& asset
    );







    void renderHubMapPasses(
        const game::system_map::HubMapPresentation& presentation,
        const Viewport& viewport,
        const world::celestial::HubMapSnapshot& hub
    ) override;


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

    void endHubGpuStage();


    void drawHubMapBox(
        const glm::dvec3& center,
        const world::celestial::LocalSceneAxes& axes,
        const glm::dvec3& size,
        const glm::vec4& color,
        double scale,
        const glm::dvec2& centerPx
    );

    void drawHubMapAxes(
        const glm::dvec3& center,
        const world::celestial::LocalSceneAxes& axes,
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





    bool drawHubMapAssemblyWire(
        ObjectType typeId,
        const glm::dvec3& objectCenter,
        const world::celestial::LocalSceneAxes& objectAxes,
        const glm::vec4& color
    );

    void drawHubMapCircleLocalXY(
        const glm::dvec3& center,
        double radiusMeters,
        double scale,
        const glm::dvec2& centerPx,
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


    const world::celestial::visual::CelestialGeneratedAssetSet*
    generatedAssetForBody(
        const world::celestial::SystemMapBody& body
    ) const;

    void renderDetailMapPasses(
        const game::system_map::DetailMapPresentation& presentation,
        const Viewport& viewport,
        const world::celestial::DetailMapSnapshot& planet
    ) override;

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
        const world::celestial::LocalSceneAxes& axes,
        const world::celestial::DetailMapSnapshot& planet,
        double scale,
        const glm::dvec2& centerPx,
        double axisLenMeters
    );

    void drawPlanetMapVelocityArrow(
        const glm::dvec3& originMeters,
        const glm::dvec3& velocityMps,
        const world::celestial::DetailMapSnapshot& planet,
        double scale,
        const glm::dvec2& centerPx,
        double lenMeters
    );


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

    void drawDetailMapOrbit3D(
        const world::celestial::DetailMapOrbit& orbit,
        const world::celestial::DetailMapSnapshot& planet,
        double scale,
        const glm::dvec2& centerPx,
        int segments
    );


    const game::system_map::LocalMapCameraSnapshot&
    activeLocalCameraSnapshot() const;


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
        const world::celestial::DetailMapSnapshot& planet
    ) const;



    HubPlanetAtmosphereStyle planetAtmosphereStyleForPlanet(
        const world::celestial::DetailMapSnapshot& planet
    ) const;

    void drawPlanetEnvironmentLayers(
        const world::celestial::DetailMapSnapshot& planet,
        double scale,
        const glm::dvec2& centerPx,
        bool applySphericalSculpt
    );

    void drawPlanetDetailSculpt(
        const glm::dvec2& planetCenterPx,
        double planetRadiusPx
    );

    void drawPlanetAnimatedCloudLayers(
        const world::celestial::DetailMapSnapshot& planet,
        double scale,
        const glm::dvec2& centerPx,
        const render::celestial::ProceduralCloudStyle& baseStyle
    );

    void drawPlanetProceduralCloudGlobeLayer(
        const world::celestial::DetailMapSnapshot& planet,
        double scale,
        const glm::dvec2& centerPx,
        double cloudRadiusScale,
        double longitudeOffset,
        double timeSeconds,
        const render::celestial::ProceduralCloudStyle& style
    );


    void drawPlanetAtmosphereInterior(
        const world::celestial::DetailMapSnapshot& planet,
        double scale,
        const glm::dvec2& centerPx,
        const HubPlanetAtmosphereStyle& style
    );

    void drawPlanetAtmosphereLimb(
        const world::celestial::DetailMapSnapshot& planet,
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



    render::celestial::rings::PlanetRingRenderContext
    planetRingRenderContext(
        const world::celestial::DetailMapSnapshot& planet,
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



    DetailMapVisualSettings m_detailVisuals;
    HubMapVisualSettings m_hubVisuals;



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

    render::celestial::CelestialShapeMeshLibrary m_celestialShapeMeshes;
    render::celestial::HubPlanetSurfaceRenderer m_hubPlanetSurfaceRenderer;
    render::celestial::PlanetGlobeMeshRenderer m_planetGlobeMeshRenderer;

    render::system_map::HubMapGpuGeometryRenderer m_hubMapGpuGeometryRenderer;
    render::system_map::HubPlanetOverlayRenderer m_hubPlanetOverlayRenderer;

    render::celestial::rings::PlanetRingRenderer m_planetRingRenderer;


    GalaxyStarfieldRenderer m_mapStarfieldRenderer;
    GalaxyStarfieldRenderer m_galaxyBackdropStarfieldRenderer;

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
    bool m_galaxyBackdropStarfieldInitialized = false;
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
