#pragma once

#include <map>
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
#include "src/game/navigation/ClientNavigationWorkspace.h"
#include "src/game/navigation/HubSemanticAnchorCatalog.h"
#include "src/game/navigation/DockingPortRuntimeStateCatalog.h"

#include "src/render/celestial/CelestialShapeMesh.h"

#include "src/render/navigation/NavigationCoordinateOverlay.h"



#include "src/game/system_map/SystemMapVisualSettings.h"
#include "src/game/system_map/MapTransitionController.h"
#include "src/game/system_map/MapMode.h"
#include "src/game/system_map/MapIntent.h"
#include "src/game/system_map/MapContextMenu.h"
#include "src/game/system_map/SystemMapPlanetCarousel.h"
#include "src/game/system_map/MapCameraState.h"
#include "src/game/system_map/GalaxyMapView.h"
#include "src/game/system_map/GalaxyMapInteraction.h"
#include "src/game/system_map/GalaxyHierarchyDevScript.h"
#include "src/game/system_map/HierarchyFramePainter.h"
#include "src/game/system_map/SystemHierarchyDevScript.h"
#include "src/game/system_map/GalaxyPointerDevScript.h"
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
#include "src/game/system_map/SystemMapGpuCircleBatch.h"
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
#include "src/game/system_map/MapObjectOverlayRenderer.h"
#include "src/game/system_map/NavigationRouteOverlay.h"
#include "src/game/system_map/NavigationMapTextProfile.h"
#include "src/world/celestial/WorldSitesCatalog.h"

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
    explicit SystemMapRenderer(game::navigation::ClientNavigationWorkspace& navigationWorkspace);

    void init();

    void setRightPanelRatio(float ratio);

    void setNavigationOverlayTextProfile(
        const render::navigation::NavigationOverlayTextProfile& profile
    )
    {
        m_navigationCoordinateOverlay.setTextProfile(profile);
        m_galaxyRenderer.setPlayerLabel(profile.player);
        m_hubBackend.setPlayerLabel(profile.player);
        m_localMapPresentationBuilder.setPlayerLabel(profile.player);
    }


    void setHubDockingDataSources(
        const game::navigation::HubSemanticAnchorCatalog* anchors,
        const game::navigation::DockingPortRuntimeStateCatalog* runtimeStates
    ) noexcept
    {
        m_hubSemanticAnchors = anchors;
        m_dockingPortRuntimeStates = runtimeStates;
    }

    void setNavigationNamingLocale(const std::string& locale)
    {
        m_navigationNamingLocale = locale;
    }
    void setNavigationMapTextProfile(
        const game::system_map::NavigationMapTextProfile& profile
    )
    {
        m_navigationMapTextProfile = profile;
    }
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

    void focusGalaxySystem(
        int systemId,
        const world::celestial::GalaxyMapSnapshot& galaxy
    );

    int selectedSystemId() const;
    std::optional<game::system_map::MapIntent> selectedGalaxyEntryIntent(
        const world::celestial::GalaxyMapSnapshot& galaxy
    ) const;

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
    void cancelMapTransition();

    const HubMapPerformanceStats&
    hubMapPerformanceStats() const
    {
        return m_hubBackend.performanceStats();
    }

    const std::string& selectedBodyId() const;
    const std::string& selectedHubId() const;
    // The object the map currently presents as selected (site, hub, ship) - right panel source.
    const std::string& activeObjectId() const noexcept { return m_objectOverlayState.activeObjectId(); }
    // The active tactical object of the current System / Detail map (hub,
    // ship, world site): its local-neighbourhood hub (may be empty) and its
    // system-barycentric position. False for no / a non-tactical selection.
    bool activeTacticalObjectContext(std::string& hubId, glm::dvec3& worldMeters) const;
    const std::string& selectedHubParentBodyId() const;

    bool canOpenSelectedDetail() const;
    // Context-sensitive local drill used by the native HUB button: an actual
    // parent Hub when the tactical target is attached, otherwise the terminal
    // System cube containing the active free-space target.
    bool canOpenSelectedLocalContext() const;

    std::optional<world::celestial::DetailSpatialCell>
    selectedTerminalDetailCell() const;



private:

    struct Vertex
    {
        glm::vec3 pos;
        glm::vec4 color;
    };

    struct TexturedSphereVertex
    {
        glm::vec3 unitPosition;
        glm::vec2 uv;
    };

    struct TexturedSphereGpuMesh
    {
        GLuint vao = 0;
        GLuint vertexBuffer = 0;
        GLuint indexBuffer = 0;
        GLsizei indexCount = 0;
    };

    struct TexturedBodyDraw
    {
        glm::vec3 center { 0.0f };
        float radius = 0.0f;
        glm::vec3 primeAxis { 1.0f, 0.0f, 0.0f };
        glm::vec3 northAxis { 0.0f, 1.0f, 0.0f };
        glm::vec3 eastAxis { 0.0f, 0.0f, -1.0f };
        glm::vec4 color { 1.0f };
        bool highResolution = false;
    };

    struct TexturedBatch
    {
        GLuint texture = 0;
        std::vector<TexturedBodyDraw> bodies;
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
    game::system_map::MapObjectOverlayState m_objectOverlayState;
    game::system_map::MapObjectOverlayRenderer m_objectOverlayRenderer;
    game::system_map::NavigationRouteOverlayState m_routeOverlayState;
    game::system_map::NavigationRouteOverlayRenderer m_routeOverlayRenderer;
    game::navigation::ClientNavigationWorkspace& m_navigationWorkspace;
    std::uint64_t m_pendingRouteFocusNodeId = 0;
    bool m_pendingRouteFocusContextApplied = false;
    game::system_map::MapObjectOverlayFrame m_galaxyInfoOverlayFrame;
    std::optional<game::system_map::MapObjectOverlayItem>
        m_galaxyWaypointCandidate;
    std::optional<game::system_map::MapObjectOverlayItem>
        m_systemWaypointCandidate;
    std::string m_activeTacticalLocalTargetObjectId;
    std::optional<world::celestial::DetailSpatialCell>
        m_activeTacticalDetailCell;
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

    void toggleSelectedBodyTracking();

    // ---- map buttons, back history, context menu -------------------------
    // (SystemMapRendererContextMenu.cpp)
    struct MapButton
    {
        std::string id; // "back", "up", "track", "level0"
        render::navigation::NavigationOverlayButton view;
    };
    std::vector<MapButton> mapButtons(const Viewport& viewport) const;

    struct MapHistoryEntry
    {
        Mode mode = Mode::Galaxy;
        game::system_map::GalaxyMapBookmark galaxy;
        game::system_map::SystemMapBookmark system;
        game::system_map::DetailCameraState detailCamera;
        std::string detailHubId;
        std::string detailHubParentBodyId;
        game::system_map::DetailCameraState hubCamera;
    };
    MapHistoryEntry captureHistoryEntry(Mode mode) const;
    void recordMapHistory(Mode previousMode, Mode mode);
    void applyMapHistoryRestore(Mode mode);
    std::optional<game::system_map::MapIntent> navigateBack();

    game::system_map::MapObjectOverlayItem makeWaypointCandidate(
        char prefix,
        const game::navigation::CubicNavigationCell& cell,
        const glm::dvec3& worldPositionMeters,
        const glm::dvec2& screenPx
    ) const;
    void appendContextActionRows(const game::system_map::MapObjectOverlayItem& item);
    bool openContextMenu(
        const Viewport& vp,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const world::celestial::SystemMapSnapshot& system,
        const world::celestial::HubMapSnapshot& hub,
        const glm::vec2& at,
        double nowSeconds
    );
    std::optional<game::system_map::MapIntent> executeContextCommand(
        const game::system_map::MapContextMenu& menu,
        const std::string& command,
        const Viewport& vp,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const world::celestial::SystemMapSnapshot& system,
        double nowSeconds
    );
    bool handleContextMenuPointer(
        const Viewport& vp,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const world::celestial::SystemMapSnapshot& system,
        const world::celestial::HubMapSnapshot& hub,
        GLFWwindow* window,
        double mx,
        double my,
        double localMx,
        double localMy,
        bool inside,
        bool leftDown,
        bool rightDown,
        double nowSeconds,
        std::optional<game::system_map::MapIntent>& intent
    );
    void drawContextMenu(const Viewport& vp);

    void updateSelectedBodyTracking(
        const game::system_map::SystemMapPresentation& presentation
    );

    void synchronizeNavigationTracking(
        game::system_map::MapObjectOverlayFrame& frame
    );

    void refreshGalaxyWaypointCandidate(
        const Viewport& viewport,
        const world::celestial::GalaxyMapSnapshot& galaxy
    );

    void refreshSystemWaypointCandidate(
        const Viewport& viewport,
        const world::celestial::SystemMapSnapshot& system
    );


    void decorateHubDockingOverlay(
        const world::celestial::HubMapSnapshot& hub
    );

    void applyDockingAction(
        const std::string& objectId,
        const std::string& actionKey
    );

    void cancelDockingTaskForClosedCard(
        const std::string& objectId
    );

    void decorateActiveGuidanceTrajectory(
        const Viewport& viewport,
        const world::celestial::SystemMapSnapshot& system,
        const world::celestial::DetailMapSnapshot& detail,
        const world::celestial::HubMapSnapshot& hub
    );

    // Hub dock traffic on Detail / Hub maps: the selected traffic ship's
    // replicated route (all routes + the authored dock lanes, holds and
    // queue gates while the T toggle is on).
    void decorateTrafficRoutes(
        const world::celestial::DetailMapSnapshot& detail,
        const world::celestial::HubMapSnapshot& hub
    );
    bool m_showTrafficLanes = false;
    bool m_showNetworkRoutes = true;
    const world::celestial::WorldSitesCatalog* m_worldSites = nullptr;
    std::string m_transferTunnelBodyId;
    std::string m_pendingOrbitalTransferHubId;
public:
    // bodyId: an in-system jump to that body (systemId = the map's system)
    struct JumpRequest { int kind = 0; int systemId = -1; std::string bodyId; };
private:
    JumpRequest m_pendingJump;
    int m_jumpCalculatedSystemId = -1;
    int m_jumpChargingSystemId = -1;
    std::string m_jumpCalculatedBodyId;   // in-system jumps: the calculated / charging target body
    std::string m_jumpChargingBodyId;
    int m_pendingRouteLaunch = 0;   // 1 official, 2 unofficial, 3 calculate only (finish info window)
    int m_pendingRouteControl = 0;  // 1 pause, 2 resume
    std::string m_devFocusBodyId;
    std::string m_devFocusHubId;
    std::string m_devHoverObjectId;
    std::string m_devEnterId;
    std::string m_devPointerSpec;
    int m_devPointerFrame = 0;
    glm::dvec2 m_devPointerAt {-1.0};
    // the System map's presentation + scene frame for this input pass (picking / selection context)
    void prepareSystemInputFrame(const Viewport& vp, const world::celestial::SystemMapSnapshot& system,
                                 double nowSeconds);
    bool driveDevPointer(const Viewport& vp, double& mx, double& my, bool& leftDown, bool& rightDown,
                         const world::celestial::GalaxyMapSnapshot* galaxy = nullptr);
    // a hub / beacon picked from the panel list; kept a few frames until its marker exists
    std::string m_listFocusObjectId;
    int m_listFocusFramesLeft = 0;
    std::size_t m_devLastNetworkLines = 0;
    std::size_t m_devLastTunnelPoints = 0;
    std::vector<glm::dvec3> m_transferTunnel;
    void decorateSystemNetworkRoutes(const Viewport& viewport, const world::celestial::SystemMapSnapshot& system);

public:
    // T: on the System Map the aggregate network routes; on Detail / Hub the dock traffic lanes.
    void toggleTrafficLanes() noexcept
    {
        if (m_modeState.current() == Mode::System)
            m_showNetworkRoutes = !m_showNetworkRoutes;
        else
            m_showTrafficLanes = !m_showTrafficLanes;
    }
    // World-model facts for the System Map (routes, sites): read-only, owned by the client catalogs.
    void setWorldSitesCatalog(const world::celestial::WorldSitesCatalog* catalog) noexcept { m_worldSites = catalog; }
    // The player's Orbital Transfer plan (body-relative tunnel) shown as a solid line until arrival.
    void setTransferTunnel(std::string parentBodyId, std::vector<glm::dvec3> tunnelBodyRelativeMeters)
    {
        m_transferTunnelBodyId = std::move(parentBodyId);
        m_transferTunnel = std::move(tunnelBodyRelativeMeters);
    }
    void clearTransferTunnel() { m_transferTunnel.clear(); m_transferTunnelBodyId.clear(); }
    // Orbital Transfer requested from the map (context menu or O on a selected hub); consumed by SpaceState.
    std::optional<std::string> consumeOrbitalTransferRequest()
    {
        if (m_pendingOrbitalTransferHubId.empty())
            return std::nullopt;
        std::string id = std::move(m_pendingOrbitalTransferHubId);
        m_pendingOrbitalTransferHubId.clear();
        return id;
    }
    // The interstellar jump from a star's context menu (kind 1 calculate, 2 START, 3 cancel; the
    // target system); SpaceState consumes it and tells back what is calculated / charging.
    JumpRequest consumeJumpRequest() { return std::exchange(m_pendingJump, JumpRequest {}); }
    void setJumpState(int calculatedSystemId, int chargingSystemId, const std::string& calculatedBodyId = {},
                      const std::string& chargingBodyId = {})
    {
        m_jumpCalculatedSystemId = calculatedSystemId;
        m_jumpChargingSystemId = chargingSystemId;
        m_jumpCalculatedBodyId = calculatedBodyId;
        m_jumpChargingBodyId = chargingBodyId;
    }
    // Route launch from the finish's info window: 1 official, 2 unofficial; consumed by SpaceState.
    int consumeRouteLaunchRequest() { return std::exchange(m_pendingRouteLaunch, 0); }
    int consumeRouteControlRequest() { return std::exchange(m_pendingRouteControl, 0); }
    void requestRouteLaunch(bool official) { m_pendingRouteLaunch = official ? 1 : 2; }
    void requestOrbitalTransferToSelectedHub()
    {
        if (!m_systemView.state().selectedHubId.empty())
            m_pendingOrbitalTransferHubId = m_systemView.state().selectedHubId;
    }
    // G: optional cube overlay of the System Map (hybrid map; hidden by default).
    void toggleSystemGridOverlay() noexcept
    {
        auto& st = m_systemView.state();
        st.gridOverlayVisible = !st.gridOverlayVisible;
        m_systemSceneFrameDirty = true;
    }
    bool systemGridOverlayVisible() const noexcept { return m_systemView.state().gridOverlayVisible; }
    // V: plan (2.5D, default) <-> free 3D view; N: beacon relays and links
    void toggleSystemPlanView() noexcept
    {
        auto& st = m_systemView.state();
        st.planView = !st.planView;
        m_systemSceneFrameDirty = true;
    }
    bool systemPlanView() const noexcept { return m_systemView.state().planView; }
    void toggleSystemBeacons() noexcept
    {
        m_systemView.state().showBeacons = !m_systemView.state().showBeacons;
        m_systemSceneFrameDirty = true;
    }
    // Arrow keys: move the selected child cube to a sibling in the screen plane
    // locked at the first press (dx = right, dy = up). Returns false when nothing moved.
    bool moveSystemCubeSelection(int dx, int dy, double nowSeconds);
    // Panel list (hubs and beacons): select the object as a click on its marker does, keep its
    // marker drawn and highlighted whatever the zoom / filters, fly the camera to its planet.
    void focusObjectFromList(const std::string& objectId)
    {
        m_listFocusObjectId = objectId;
        m_listFocusFramesLeft = 30;
        m_systemSceneFrameDirty = true;
    }
    // DEV map script: select an object as if clicked.
    void devActivateObject(const std::string& objectId) { m_objectOverlayState.activate(objectId); }
    // DEV acceptance script: focus a body / hub through the ordinary selection path; hover an object.
    void devFocusBody(const std::string& bodyId) { m_devFocusBodyId = bodyId; }
    void devFocusHub(const std::string& hubId) { m_devFocusHubId = hubId; }
    // flight: the map textures (albedo + clouds) of the body the ship is near, ready before the map opens
    void prewarmBodyMap(int systemId, const std::string& bodyId, double universeTimeSeconds)
    {
        m_mapResources.prewarmBody(systemId, bodyId, universeTimeSeconds);
    }
    void devHover(const std::string& objectId) { m_devHoverObjectId = objectId; }
    // DEV: the double-click approach on a body (body id) or a hub (hub id) - one cube level per call.
    void devEnter(const std::string& id) { m_devEnterId = id; }
    void devZoom(double steps) { m_pendingScrollY += steps; }
    // DEV: turn the Detail / Hub map camera (yaw, pitch in radians) - the result of a drag
    void devDetailOrbit(double yaw, double pitch) { m_detailView.camera().yaw += yaw; m_detailView.camera().pitch += pitch; }
    // DEV: a scripted pointer gesture on the current map, fed through the
    // production input path: "click:<objectId>", "dclick:<objectId>",
    // "rclick:<objectId>" (right click = context menu), "menu:<command>"; <objectId> may be
    // "carousel:<bodyId>" / "carousel:#N" (a globe of the System map planet strip),
    // (left click on the open context-menu row with that command).
    void devPointer(const std::string& spec) { m_devPointerSpec = spec; m_devPointerFrame = 0; }
    // DEV: overlay objects of the current map (id, name, next-map hub, screen px).
    std::string devOverlayReport() const;
    // DEV: what the map shows right now (scripted checks: marker <-> world object identity)
    std::vector<std::string> devVisibleObjectIds() const
    {
        std::vector<std::string> ids;
        for (const auto& item : m_systemSceneFrame.interaction.objectOverlay.items)
            if (item.visible && item.drawGlyph)
                ids.push_back(item.objectId);
        return ids;
    }
    std::size_t devNetworkRouteLines() const { return m_devLastNetworkLines; }
    bool devTransferTunnelShown() const { return m_devLastTunnelPoints >= 2; }
    bool trafficLanesVisible() const noexcept { return m_showTrafficLanes; }

private:

    void applyWaypointAction(
        const std::string& objectId,
        const std::string& actionKey
    );

    std::optional<game::system_map::MapIntent> focusRouteWaypoint(
        std::uint64_t routeNodeId,
        const Viewport& viewport,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const world::celestial::SystemMapSnapshot& system,
        double nowSeconds
    );

    std::optional<game::system_map::MapIntent> advancePendingRouteFocus(
        const Viewport& viewport,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const world::celestial::SystemMapSnapshot& system,
        double nowSeconds
    );

    void revealPendingRouteFocus(
        const game::system_map::MapObjectOverlayFrame& frame,
        const Viewport& viewport
    );

    const game::system_map::MapObjectOverlayItem*
    currentOverlayItem(const std::string& objectId) const;

    void updateActiveTacticalLocalContext(
        const game::system_map::MapObjectOverlayItem& item
    );

private:
    void ensureGlObjects();
    void ensureShader();

    void ensureTexturedGlObjects();
    void createTexturedSphereMesh(
        TexturedSphereGpuMesh& mesh,
        int latitudeSegments,
        int longitudeSegments
    );
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

    void beginGpuCircles() override
    {
        m_gpuCircleBatch.begin();
    }

    void addGpuCircleXZ(
        const glm::vec3& center,
        float radius,
        const glm::vec4& color,
        int segments
    ) override
    {
        m_gpuCircleBatch.add(
            center,
            radius,
            color,
            segments,
            game::system_map::SystemMapGpuCircleBatch::Plane::XZ
        );
    }

    void addGpuCircleXY(
        const glm::vec3& center,
        float radius,
        const glm::vec4& color,
        int segments
    ) override
    {
        m_gpuCircleBatch.add(
            center,
            radius,
            color,
            segments,
            game::system_map::SystemMapGpuCircleBatch::Plane::XY
        );
    }

    void flushGpuCircles(const glm::mat4& mvp) override
    {
        m_gpuCircleBatch.flush(mvp);
    }


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

    void beginVolumes() override;

    void addVolumeSprite(
        const glm::vec3& center,
        float radius,
        const glm::vec4& color,
        const glm::mat4& view
    ) override;

    void flushVolumes(const glm::mat4& mvp) override;

    void drawMediumShells(
        const world::celestial::GalaxyMapMediumAtlas& atlas,
        const std::vector<world::celestial::GalaxyMapMediumAtlas::Range>& ranges,
        const glm::mat4& mvpLy
    ) override;

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

    TexturedSphereGpuMesh m_texturedSphereLow;
    TexturedSphereGpuMesh m_texturedSphereHigh;
    GLuint m_texturedShader = 0;
    GLint  m_texturedMvpLoc = -1;
    GLint  m_texturedSamplerLoc = -1;

    GLuint m_bgVao = 0;
    GLuint m_bgVbo = 0;
    GLuint m_bgShader = 0;

    std::vector<Vertex> m_vertices;
    std::vector<Vertex> m_solidVertices;
    std::vector<Vertex> m_volumeVertices;
    // Info card of the selected Galaxy-map cloud / icy field ("" = none).
    std::string m_galaxyMediumCardId;
    // GPU copy of the current GalaxyMapMediumAtlas (one static buffer).
    std::uint64_t m_mediumAtlasUid = 0;
    unsigned int m_mediumVao = 0;
    unsigned int m_mediumVbo = 0;
    std::vector<int> m_mediumFirst;
    std::vector<int> m_mediumCount;
    std::vector<TexturedBatch> m_texturedBatches;
    game::system_map::SystemMapGpuCircleBatch m_gpuCircleBatch;

    game::system_map::MapModeState m_modeState;
    float m_rightPanelRatio = 0.28f;

    double m_pendingScrollY = 0.0;

    game::system_map::GalaxyMapView m_galaxyView;
    game::system_map::GalaxyMapInteraction m_galaxyInteraction;
    // DEV only (ELITE_DEV_GALAXY_POINTER_SCRIPT): scripted pointer input.
    game::system_map::GalaxyPointerDevScript m_galaxyPointerDevScript;
    game::system_map::GalaxyHierarchyDevScript m_galaxyHierarchyDevScript;
    game::system_map::SystemHierarchyDevScript m_systemHierarchyDevScript;
    // System spatial frame texts of the current frame (drawn with labels).
    std::vector<game::system_map::GalaxyFrameText> m_systemFrameTexts;

    // Map buttons and breadcrumbs (the hovered button id, "" = none).
    std::string m_hoveredMapButton;
    // System map planet strip (bottom): globes in orbit order, a click selects the planet
    game::system_map::SystemMapPlanetCarousel m_planetCarousel;
    // double click on a carousel planet = the map's own double click on it (approach + follow)
    game::presentation::PointerClickTracker<std::string> m_carouselClickTracker;
    int m_galaxyBreadcrumbHovered = -1;

    // Back history (previous maps with their views).
    std::vector<MapHistoryEntry> m_mapHistory;
    std::optional<MapHistoryEntry> m_pendingHistoryRestore;
    std::optional<game::system_map::GalaxyMapBookmark> m_pendingGalaxyBookmark;
    // Last view of every map: returning continues the inspection where it was
    // left (Detail by body, Hub by hub, Galaxy as one view).
    std::map<std::string, game::system_map::DetailCameraState> m_detailCameraMemory;
    std::map<std::string, game::system_map::DetailCameraState> m_hubCameraMemory;
    std::string m_detailMemoryKey;
    std::string m_hubMemoryKey;
    bool m_galaxyViewEntered = false;

    // Context menu (right click / Ctrl + click) and its pointer state.
    game::system_map::MapContextMenu m_contextMenu;
    bool m_contextLeftWasDown = false;
    bool m_contextRightWasDown = false;
    bool m_contextRightCandidate = false;
    bool m_contextSwallowLeft = false;
    glm::vec2 m_contextRightPressPx {0.0f};
    // Info card of a Galaxy star opened from its context menu.
    std::optional<game::system_map::MapObjectOverlayItem> m_galaxySystemCard;
    // a body without an overlay card (moons): its route card from the context menu
    std::optional<game::system_map::MapObjectOverlayItem> m_bodyRouteCard;
public:
    // the flight facts of a body (from the client's catalog): mu, radius, the J floor, landable
    struct BodyFlightFacts
    {
        double muM3s2 = 0.0;
        double radiusM = 0.0;
        double floorAltitudeM = 0.0;
        bool landable = false;
    };
    void setBodyFlightFacts(std::function<std::optional<BodyFlightFacts>(const std::string&)> lookup)
    {
        m_bodyFlightFacts = std::move(lookup);
    }
private:
    std::function<std::optional<BodyFlightFacts>(const std::string&)> m_bodyFlightFacts;
    void appendBodyFinishRows(const std::string& bodyId);
    int m_galaxySystemCardSystemId = -1;
    std::vector<std::string> galaxyBreadcrumbs() const;
    std::vector<std::string> mapBreadcrumbs() const;
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

    render::navigation::NavigationCoordinateOverlay
        m_navigationCoordinateOverlay;

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
    game::system_map::NavigationMapTextProfile m_navigationMapTextProfile;

    const game::navigation::HubSemanticAnchorCatalog*
        m_hubSemanticAnchors = nullptr;
    const game::navigation::DockingPortRuntimeStateCatalog*
        m_dockingPortRuntimeStates = nullptr;
    ObjectType m_cachedDockingEnvelopeShipType = ObjectType::None;
    game::navigation::ShipDockingEnvelope m_cachedDockingEnvelope;



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
