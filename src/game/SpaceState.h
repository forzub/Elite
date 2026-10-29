#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <unordered_map>
#include <deque>
#include <mutex>
#include <memory>
#include <atomic>
#include <chrono>
#include <glm/gtc/quaternion.hpp>

#include "core/GameState.h"

#include "src/game/ship/Ship.h"
#include "ui/HudRenderer.h"

// ============== components ==============
#include "ui/components/UIContainer.h"
#include "ui/components/minicamera/UICameraView.h"

// =========== симулейшен ==================

#include "game/ship/controller/PlayerInputMapper.h"
#include "game/ship/view/PlayerShipView.h"
#include "src/game/debug/IDebugSessionControl.h"
#include "src/game/client/GameClient.h"
#include "src/scene/SceneRenderer.h"
#include "src/render/cockpit/FlightVectorIndicatorRenderer.h"
#include "src/render/cockpit/EngineThrustIndicatorRenderer.h"
#include "src/render/cockpit/GuidanceCorridorRenderer.h"
#include "src/render/cockpit/GalacticCompassRenderer.h"
#include "src/ui/components/radar/RadarWidgetBase.h"
#include "src/ui/presentation/InSessionPresentationRenderer.h"

// #include "src/WebSocket/DebugServer.h"
#include "src/debug/FrustumDebugData.h"

#include "src/game/network/ClientShipCommand.h"

#include "game/damage/TestDamageHandler.h"
#include "game/debug/AttachmentEditorData.h"

#include "src/game/promo/PromoSceneScenario.h"
#include "src/game/traffic/StationTrafficSystem.h"
#include "src/audio/TerminalAudioAdapter.h"
#include "src/game/presentation/DockingStatusTerminal.h"
#include "src/game/navigation/ClientNavigationWorkspace.h"
#include "src/game/navigation/debug/NavigationStressHarness.h"
#include "src/game/debug/ShipRuntimeControlOracle.h"
#include "src/game/debug/RuntimeScenarioSelection.h"
#include "src/game/navigation/PreplanningSafetyCourse.h"
#include "src/game/navigation/AssistedSafeCourseRecovery.h"
#include "src/game/navigation/planner/RouteEntryBinder.h"
#include "src/game/ship/telemetry/ShipControlAuthorityTelemetry.h"
#include "src/game/ship/computer/MotherComputer.h"
#include "src/game/navigation/GalacticReferenceFrame.h"
#include "src/game/navigation/HubSemanticAnchorCatalog.h"
#include "src/game/navigation/DockingPortRuntimeStateCatalog.h"
#include "src/game/navigation/planner/RoutePlannerApi.h"
#include "src/game/navigation/autopilot/AssistedAutopilot.h"
#include "src/game/navigation/autopilot/NewtonianOptimalController.h"
#include "src/game/navigation/autopilot/DedicatedClientRouteAutopilotLaws.h"
#include "src/game/navigation/DockingAdvisoryCorridor.h"
#include "src/game/navigation/NavigationWorldPredictor.h"
#include "src/game/simulation/HubAttachmentSnapshot.h"
#include "src/game/navigation/NavigationModuleState.h"
#include "src/game/system_map/SystemMapRenderer.h"
#include "src/game/system_map/AuthoritativeMapInterpolator.h"
#include "src/world/celestial/SystemMapTypes.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

class StateStack;

namespace game::session
{
class IGameSession;
}


enum class PlayerNavigationMapLevel
{
    Galaxy,
    System,
    Detail,
    Local
};


enum class ScreenLayout
{
    Front_Main_Rear_Mini,
    Rear_Main_Front_Mini,
    Front_Main_Drone_Mini,
    Drone_Main_Front_Mini
};


class SpaceState : public GameState
{
public:
    enum class StartupMode
    {
        Immediate,
        Deferred
    };

    explicit SpaceState(
        StateStack& states,
        StartupMode startupMode = StartupMode::Immediate
    );
    bool advanceStartupInitialization();
    ~SpaceState();

    void renderUI() override;
    void renderHUD() override;
    void renderInSessionPresentationOverlay();

    void prepareFrame(float dt) override;
    void handleInput() override;
    void update(float dt) override;
    void render() override;
    bool m_mouseLook = false;

    void initServer();
    void initClient();
    void initHUD();



    void initServerAndClient();
    json shipCoreStatusToJson(const game::ShipCoreStatus& status);

    void testDamageSystem();

    void handleResize(int width, int height);
    void setFlightScreenLayout(ScreenLayout layout);
    ScreenLayout flightScreenLayout() const { return m_layout; }

    const ShipAttachmentOverrideMap& attachmentEditorOverrides() const { return m_attachmentEditorOverrides; }
    ShipAttachmentOverrideMap& attachmentEditorOverrides() { return m_attachmentEditorOverrides; }

    void pushAttachmentEditorState();
    void processHtmlCommands();
    void pushShipCoreState();
    void pushStructureDebugState();
    void requestStructureDebugStateRefresh();
    void deferDebugControlStatePush();
    void flushPendingDebugUiState();
    void pushVolumeViewerState();
    void pushFrustumDebugState(const nlohmann::json& payload);
    void pushDebugControlState();
    void loadDebugControlDefaults();
    bool saveDebugControlDefaults(const nlohmann::json& payload);
    void resetDebugControlSettings();
    void pushSystemMapState();
    void selectSystemMapSystem(int systemId);
    void setSystemMapGalaxyMode();
    void setSystemMapKnownSystemMode(int systemId);
    void setSystemMapLoadedSystemMode();
    void setSystemMapCurrentSystemMode();
    void setSystemMapPlayerSystemMode();
    void setSystemMapPlayerDetailMode();
    void setSystemMapPlayerLocalMode();
    bool preparePlayerNavigationMapLevel(PlayerNavigationMapLevel level);
    void toggleConstellationOverlay();

    // Temporary diagnostic ship-construction controls. These mutate the
    // authoritative ShipState through ClientShipCommand; they do not patch
    // planner/autopilot state locally.
    void cyclePhysicalControlLayout();
    void cyclePhysicalControlFamily();

    // Navigation equipment/control seam. Cockpit buttons, debug controls or
    // future ship equipment can switch computation and presentation modules
    // independently without reaching into individual renderers/planners.
    bool navigationModuleEnabled(
        game::navigation::NavigationModuleId module
    ) const noexcept;
    void setNavigationModuleEnabled(
        game::navigation::NavigationModuleId module,
        bool enabled
    ) noexcept;
    bool toggleNavigationModule(
        game::navigation::NavigationModuleId module
    ) noexcept;
    void setAllNavigationHudLayersEnabled(bool enabled) noexcept;
    void cycleSkyCulture();
    void onUiLanguageChanged() override;
    void setSystemMapEmptySectorMode(
        const glm::dvec3& positionLy
    );



    void applyDebugControlPayload(const nlohmann::json& payload);

    void requestGalaxyMapSnapshotOnce();
    bool composeSystemMapSnapshot(int systemId);

    void updateSystemMapLiveFlags();
    void updateLiveMapSnapshots(float dt);
    void updateLocalMapPresentationSnapshots(float dt);
    bool shouldRefreshSystemMapSnapshot() const;

    bool composeDetailMapSnapshot(
        const world::celestial::DetailTarget& target
    );

    void setSystemMapDetailMode();

    bool composeHubMapSnapshot(
        int systemId,
        const std::string& hubId
    );

    void beginSystemMapSystemTransition(int systemId);
    void beginSystemMapDetailTransition(
        const world::celestial::DetailTarget& target
    );
    void beginSystemMapHubTransition(
        int systemId,
        const std::string& hubId
    );
    void setSystemMapHubMode();
    void setSystemMapLoadedDetailMode();
private:
    enum class StartupStage
    {
        InitServer,
        InitClient,
        DebugDefaults,
        Shaders,
        SceneRenderer,
        SceneLocale,
        SystemMapRenderer,
        GalaxyRequest,
        Hud,
        Finalize,
        Complete
    };

    StartupStage m_startupStage = StartupStage::InitServer;
    std::chrono::steady_clock::time_point m_startupTotalBegin{};


    WorldParams                                 m_world;

    bool resolvePlayerGalacticPositionLy(glm::dvec3& outPositionLy) const;
    bool buildPlayerDetailTarget(
        world::celestial::DetailTarget& outTarget,
        bool preferReferenceContext
    ) const;
    bool buildSelectedMapDetailTarget(
        world::celestial::DetailTarget& outTarget
    ) const;
    void renderUniverseTimeSimulationOverlay(
        const Viewport& viewport
    );
    void renderUiLanguageIndicator(const Viewport& viewport);
    void renderNavigationStressOverlay(const Viewport& viewport);
    void renderRoutePlanningModal(const Viewport& viewport);
    void updateDockingAdvisory();
    void updateShipObservationInterest(float dt);
    game::presentation::SystemMapPanelPresentation
    buildNativeSystemMapPanelPresentation();
    bool handleNativeSystemMapPanelInput(const Viewport& viewport);
    void applyNativeSystemMapPanelAction(
        const game::presentation::SystemMapPanelAction& action
    );
    void openSelectedGalaxyMapTarget();
    void applyClientCatalogLocalization();
    void applyClientModeState();

    // std::vector<Planet>                         m_planets;                  // "world/Planet.h"
    // std::vector<WorldSignal>                    m_worldSignals;             // "world/WorldSignal.h"
    // std::vector<InterferenceSource>             m_interferenceSources;      // "world/InterferenceSource.h" - источники помех

    // --- HUD ---
    std::vector<HudStaticItem>                  m_hudStatics;
    std::vector<HudLineRect>                    m_hudRects;
    HudMessage*                                 m_activeMessage = nullptr;
    HudRenderer                                 m_hudRenderer;
    render::cockpit::FlightVectorIndicatorRenderer m_flightVectorIndicatorRenderer;
    render::cockpit::EngineThrustIndicatorRenderer m_engineThrustIndicatorRenderer;
    render::cockpit::GuidanceCorridorRenderer m_guidanceCorridorRenderer;
    render::cockpit::GalacticCompassRenderer m_galacticCompassRenderer;
    // Presentation-only terminal. It consumes a DTO snapshot and has no
    // navigation/control access; deleting this member cannot alter flight.
    game::presentation::DockingStatusTerminal m_dockingStatusTerminal {};
    // Presentation->audio seam only. No simulation/navigation state crosses it.
    audio::TerminalAudioAdapter m_terminalAudioAdapter {};

    WorldLabelRenderer                          m_worldLabelRenderer;



    std::unique_ptr<UIContainer>                uiRoot;
    UICameraView*                               rearView = nullptr;
    ScreenLayout                                m_layout = ScreenLayout::Front_Main_Rear_Mini; //  режими отображения камеры
    Camera*                                     m_activeMainCamera = nullptr;
    ShipCameraMode                              m_activeCameraMode = ShipCameraMode::Cockpit;


    game::session::IGameSession*                   m_session = nullptr;
    game::debug::IDebugSessionControl*              m_debugSession = nullptr;
    GameClient*                                      m_client = nullptr;

    // Client-only cockpit/avionics computer; authoritative ship dynamics
    // remain on ServerRuntime. Never passes a mutable ShipCore to its worker.
    game::ship::computer::MotherComputer m_motherComputer;
    std::uint64_t m_motherObservedServerTick = 0;
    std::uint64_t m_motherObservedShipInstanceId = 0;

    PlayerInputMapper                               m_inputMapper;
    bool                                            m_persistedPhysicalModelApplied = false;
    std::unique_ptr<PlayerShipView>                 m_playerView;
    EntityId                                        m_playerId;

    ShipControlState                                m_playerControl;
    std::deque<ShipControlState>                    m_sentInputs;
    uint32_t m_localTick = 0;

    double m_shipObservationInterestAccumulatorSeconds = 0.0;
    std::uint64_t m_shipObservationInterestSerial = 0;

    // =======================================
    //  ===========       HUD       ==========
    // =======================================
    RadarWidgetBase* m_radarWidget = nullptr;
    SceneRenderer m_sceneRenderer;
    PreparedScene m_preparedScene;
    game::navigation::ClientNavigationWorkspace m_navigationWorkspace;
    game::debug::RuntimeScenarioSelection
        m_runtimeScenarioSelection {};
    game::navigation::debug::NavigationStressHarness
        m_navigationStressHarness {};
    game::debug::ShipRuntimeControlOracle
        m_shipRuntimeControlOracle {};
    bool m_navigationStressCompleteAnnounced = false;
    bool m_shipControlOracleCompleteAnnounced = false;
    std::uint64_t m_lastNavigationExecutionSnapshotTick = 0;
    game::navigation::GalacticReferenceFrame m_galacticReferenceFrame;
    game::navigation::HubSemanticAnchorCatalog m_hubSemanticAnchorCatalog;
    game::navigation::DockingPortRuntimeStateCatalog m_dockingPortRuntimeStateCatalog;
    std::uint64_t m_lastDockingPathRequestSerial = 0;
    std::uint64_t m_lastDockingRequestTraceSerial = 0;

    enum class ClientDockingPhase : std::uint8_t
    {
        Idle = 0,
        Stabilizing = 1,
        Planning = 2,
        RouteReady = 3,
        Executing = 4,
        SecuringCourse = 5
    };

    ClientDockingPhase m_clientDockingPhase =
        ClientDockingPhase::Idle;
    bool m_clientAutopilotControlActive = false;
    ShipControlState m_clientAutopilotControl {};
    double m_clientDockingSettledSinceServerSeconds = -1.0;
    std::uint64_t m_clientDockingTraceTick = 0;
    double m_clientDockingRouteProgressMeters = 0.0;
    std::size_t m_clientDockingRouteCurveIndex = 0;

    // Read-only live execution diagnostics mirrored into the stress harness.
    // They never participate in control.
    double m_clientAutopilotCrossTrackMeters = 0.0;
    double m_clientAutopilotCourseErrorRad = 0.0;
    double m_clientAutopilotHullErrorRad = 0.0;
    double m_clientAutopilotTargetSpeedMps = 0.0;
    double m_clientAutopilotTargetLongitudinalAccelerationMps2 = 0.0;
    game::navigation::autopilot::AssistedAutopilot
        m_clientDockingAssistedStabilizePilotController {};
    game::navigation::autopilot::NewtonianOptimalController
        m_clientDockingNewtonianStabilizePilotController {};

    // Assisted docking-entry safety owns ONE selected inertial course.
    // It is deliberately not a trajectory/follower state.  Once the velocity
    // vector acquires this direction, linear acceleration is removed and the
    // ship coasts while route planning/entry binding finishes.
    struct DockAssistedSafeRecoveryState
    {
        bool active = false;
        bool stopFallback = false;
        std::uint64_t serial = 0;
        glm::dvec3 direction {0.0, 0.0, -1.0};
        double targetSpeedMps = 0.0;
        double sceneRayMeters = 0.0;
    };
    DockAssistedSafeRecoveryState m_dockAssistedSafeRecovery {};

    // Legacy generic safety-course state is retained for Newtonian until its
    // rotate/settle/burn recovery executor is implemented. Assisted no longer
    // uses this mutable direction state.
    struct DockSafeCourseState
    {
        bool active = false;
        bool stopFallback = false;
        std::uint64_t serial = 0;
        glm::dvec3 direction {0.0, 0.0, -1.0};
        double targetSpeedMps = 0.0;
        double sceneRayMeters = 0.0;
        double selectedTurnAngleRad = 0.0;
        double selectedTurnRadiusMeters = 0.0;
        double lastRecheckServerSeconds = -1.0;

        double hullRadiusMeters = 0.0;
        double additionalClearanceMeters = 0.0;
        double courseAccelerationMps2 = 0.0;
        double brakingAccelerationMps2 = 0.0;
        double maxAngularVelocityRadPerSecond = 0.0;
        bool newtonian = false;

        glm::dvec3 preferredDirection {0.0, 0.0, -1.0};
        std::vector<world::navigation::NavigationObstacle> obstacles;
    };
    DockSafeCourseState m_dockSafeCourse {};

    game::navigation::autopilot::AssistedClientRouteAutopilot::State
        m_clientAssistedRouteAutopilot {};
    game::navigation::autopilot::NewtonianClientRouteAutopilot::State
        m_clientNewtonianRouteAutopilot {};

    std::string m_activeDockingGuidanceCorridorId;
    bool m_noSafeDockingGuidanceSolution = false;
    // Newtonian: failed planning/handoff attempts of the current request
    // that were turned into a coast-and-replan instead of a final failure.
    int m_newtonianRouteRecoveries = 0;
    double m_newtonianLastRecoveryServerSeconds = -1.0e9;
    std::string m_dockingGuidanceFailureReason;
    // Presentation/diagnostic warning only. Never participates in control.
    std::string m_dockingStatusWarning;
    struct DockAdvice
    {
        double workerDurationMs = 0.0;
        game::navigation::planner::AssistedRoutePreset assistedRoutePreset =
            game::navigation::planner::AssistedRoutePreset::Standard;
        std::uint64_t serial = 0;
        int systemId = -1;
        std::string hubId;
        std::uint32_t targetObjectId = 0;
        std::uint64_t timelineRevision = 0;
        std::uint64_t lastValidatedTick = 0;
        game::navigation::HubSemanticAnchorDefinition portDefinition;
        game::simulation::HubAttachmentSnapshot portAttachment;

        // Frozen orientation reference from the SAME planning epoch as the
        // accepted route geometry. Automatic roll control and corridor
        // presentation must consume this exact vector; re-resolving the port
        // at execution/render time would rotate the frames independently of
        // the already-authored route.
        glm::dvec3 routeUpReference {0.0, 1.0, 0.0};

        // Planning-epoch dock roll reference expressed as TWO points in the
        // same hub-local frame: aperture center and a unit "bottom" point.
        // Dynamic tunnel roll must preserve this center->bottom radial
        // direction, evaluated around the terminal ROUTE axis.
        glm::dvec3 dockCenterReference {0.0};
        glm::dvec3 dockBottomReference {0.0, -1.0, 0.0};

        // SpaceState owns presentation/tracking state only. Planner output is
        // stored intact and consumed read-only: sparse HUD gates and dense map
        // geometry remain Planner-owned products.
        game::navigation::planner::RoutePlan plan;

        // Visual-only sampling of one canonical rotation-minimizing frame
        // field built from plan.routeCurves. Autopilot does not consume these.
        std::vector<glm::dvec3> visualGateUp;
        std::vector<glm::dvec3> visualExecutionGateUp;

        // Per-gate ownership of the live rotating dock phase. Intermediate
        // traffic stages own their own frame (0); final docking owns 1.
        std::vector<double> visualGateDockPhaseWeight;
        std::vector<double> visualExecutionGateDockPhaseWeight;
        double standoffMeters = 0.0;
        double widthMeters = 0.0;
        double heightMeters = 0.0;
        double shipWidthMeters = 0.0;
        double shipHeightMeters = 0.0;
        double lateralToleranceMeters = 0.0;
        double verticalToleranceMeters = 0.0;
        bool deviationWarning = false;
        bool deviationCritical = false;
        bool corridorDeparted = false;
        std::size_t nextGate = 0;
        game::navigation::DockingAdvisoryCorridorTracker tracker;
    } m_dockAdvice;
    struct DockAdviceJob
    {
        std::atomic<bool> ready {false};
        // A newer docking request retires this job. The geometric optimizer
        // itself is still synchronous, so cancellation is cooperative at the
        // expensive stage boundaries; stale output is never adopted.
        std::atomic<bool> cancelled {false};

        // Coarse asynchronous planning progress for HUD presentation.
        // This is phase completion, not an ETA. The expensive geometric
        // search is data-dependent and therefore intentionally does not claim
        // a continuously predictable remaining time.
        std::atomic<int> planningProgressPermille {0};
        std::atomic<std::uint8_t> planningProgressStage {0};
        std::uint64_t timelineRevision = 0;
        double startedServerSeconds = 0.0;
        std::chrono::steady_clock::time_point wallClockStart =
            std::chrono::steady_clock::now();
        double executionStartUniverseTimeSeconds = 0.0;
        double workerDurationMs = 0.0;
        // Time of RouteEntryBinder::bind itself; excludes autopilot compile
        // and all of the first expensive full-route worker.
        double entrySolveDurationMs = 0.0;
        DockAdvice context;
        game::navigation::planner::RoutePlan plan;

        // Late-bound entry state. The first expensive plan discovers the
        // immutable downstream backbone. If the craft has already consumed
        // the authored ballistic prefix when the worker finishes, only the
        // entry connector is replanned from the live state; the backbone is
        // never recomputed merely because wall-clock planning took longer.
        game::navigation::planner::RoutePlanRequest planningRequest;
        game::navigation::planner::LateBoundRouteBackbone entryBackbone;
        bool entryRebind = false;
        bool hasPreferredEntryAnchor = false;
        glm::dvec3 preferredEntryAnchor {0.0};
        glm::dvec3 preferredEntryForward {0.0, 0.0, -1.0};

        // ASYNC CONTRACT: Planner + expensive client route compilation are
        // completed entirely in the worker. The main thread only adopts these
        // already-built products; it must never run TrajectoryGenerator /
        // AcceptedManeuverProgramBuilder during a frame.
        bool autopilotCompileRequested = false;
        bool autopilotCompiled = false;
        std::string autopilotCompileFailure;
        game::navigation::LocalFlightControlLaw compiledAutopilotLaw =
            game::navigation::LocalFlightControlLaw::Assisted;
        game::navigation::autopilot::AssistedClientRouteAutopilot::State
            compiledAssistedAutopilotState {};
        game::navigation::autopilot::NewtonianClientRouteAutopilot::State
            compiledNewtonianAutopilotState {};
    };
    std::shared_ptr<DockAdviceJob> m_dockAdviceJob;

    struct DockLateEntryState
    {
        bool active = false;
        std::uint64_t serial = 0;
        game::navigation::planner::LateBoundRouteBackbone backbone;

        // Project using estimated time to compute the short connector, not
        // the ship's travel time to the first curve. Never discard/re-run
        // the expensive downstream route merely because the entry is stale.
        double leadSeconds = 0.0;
        std::uint32_t rebindAttempts = 0;
    };
    DockLateEntryState m_dockLateEntry {};
    // Measured total latency of last entry worker, including compile. A new
    // request can forecast its final connector without assuming that the
    // original full-route 12-second computation is repeated.
    double m_dockObservedConnectorWorkerSeconds = 0.0;

    // Server-authored motor basis accepted at worker launch/adoption.
    // A real route must not depend on sparse client module replicas.
    game::ship::telemetry::ShipControlAuthorityTelemetry
        m_dockPlannedAssistedAuthority {};
    game::ship::telemetry::ShipControlAuthorityTelemetry
        m_dockAcceptedAssistedAuthority {};

    std::shared_ptr<std::atomic<int>> m_dockWorkerCount =
        std::make_shared<std::atomic<int>>(0);
    SystemMapRenderer m_systemMapRenderer;
    ui::presentation::InSessionPresentationRenderer m_inSessionPresentationRenderer;


    // std::unique_ptr<game::debug::DebugServer>       m_debugServer;
    // std::unique_ptr<game::debug::DebugServer>       m_coreStatusServer;
    // std::unique_ptr<game::debug::DebugServer>       m_frustumDebugServer;

    std::vector<ClientShipCommand>                  m_debugCommands;
    std::mutex                                      m_debugCommandsMutex;                // для защиты очереди


    game::damage::TestDamageHandler                 handler;
    ShipAttachmentOverrideMap                       m_attachmentEditorOverrides;

    // =========================
    // HTML UI selections
    // =========================
    std::string m_attachmentEditorSelectedShipTypeId = "cobra_mk1";
    uint64_t    m_shipCoreSelectedShipEntityId = 0;
    uint64_t    m_structureDebugSelectedShipEntityId = 0;
    std::uint64_t m_debugControlSettingsRevision = 1;

    // Debug/control crosses an asynchronous server boundary. UI refreshes wait
    // for a newer copied diagnostic revision instead of assuming that a server
    // mutation completed synchronously inside the HTML command handler.
    bool m_structureDebugRefreshPending = false;
    std::uint64_t m_structureDebugAwaitingSnapshotRevision = 0;
    bool m_debugControlStatePushPending = false;
    std::uint64_t m_debugControlAwaitingStateRevision = 0;




    double m_perfFps = 0.0;
    double m_perfFrameMs = 0.0;
    double m_perfMainRenderMs = 0.0;
    double m_perfRearCameraMs = 0.0;
    double m_perfRenderUiMs = 0.0;

    float m_perfPushTimer = 0.0f;

    SceneRenderStats m_perfMainStats;
    SceneRenderStats m_perfRearStats;


    uint64_t m_perfFrameIndex = 0;

    double m_perfUpdateMs = 0.0;
    double m_perfProcessHtmlMs = 0.0;
    double m_perfFixedSimMs = 0.0;
    std::uint32_t m_perfServerFixedSteps = 0;
    double m_perfServerTickDebtMs = 0.0;
    double m_perfServerDiscardedMs = 0.0;
    double m_perfServerTotalDiscardedMs = 0.0;
    bool m_perfServerCatchUpLimited = false;
    double m_perfClientUpdateMs = 0.0;
    double m_perfDockingGuidanceMs = 0.0;
    double m_perfScenePrepareMs = 0.0;
    bool m_perfDockingRequestActive = false;
    double m_perfPlayerViewMs = 0.0;
    double m_perfUiRootUpdateMs = 0.0;
    double m_perfHudMs = 0.0;


    game::promo::PromoSceneScenario m_promoSceneScenario;
    game::traffic::StationTrafficSystem m_stationTrafficSystem;

    world::celestial::GalaxyMapSnapshot m_galaxyMapSnapshot;

    world::celestial::SystemMapSnapshot m_systemMapSnapshot;
    world::celestial::DetailMapSnapshot m_detailMapSnapshot;
    world::celestial::HubMapSnapshot m_hubMapSnapshot;

    bool m_hasHubMapSnapshot = false;
    int m_loadedHubMapSystemId = -1;
    std::string m_loadedHubMapHubId;
    std::uint64_t m_appliedHubMapServerTick = 0;

    bool m_hasDetailMapSnapshot = false;
    world::celestial::DetailTarget m_loadedDetailTarget;
    std::uint64_t m_appliedDetailMapServerTick = 0;

    int m_loadedSystemMapId = -1;
    std::uint64_t m_appliedSystemMapServerTick = 0;
    bool m_hasGalaxyMapSnapshot = false;
    bool m_hasGalaxyMapOverlay = false;
    bool m_hasSystemMapSnapshot = false;
    bool m_systemMapShowsEmptySector = false;
    int m_nextEmptySystemMapId = -2;
    double m_systemMapLiveRefreshTimer = 0.0;
    double m_detailMapLiveRefreshTimer = 0.0;
    double m_hubMapLiveRefreshTimer = 0.0;

    bool m_systemMapVisible = false;
    bool m_systemMapLiveSnapshotsEnabled = false;
    int m_liveSystemMapId = -1;

    game::system_map::AuthoritativeMapInterpolator
        m_authoritativeMapInterpolator;
    std::uint64_t m_mapUniverseTimelineRevision = 0;

};
