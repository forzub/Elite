#pragma once

#include <unordered_map>
#include <deque>
#include <mutex>
#include <glm/gtc/quaternion.hpp>

#include "core/GameState.h"

#include "src/game/ship/Ship.h"
#include "ui/HudRenderer.h"

// ============== components ==============
#include "ui/components/UIContainer.h"
#include "ui/components/minicamera/UICameraView.h"

// =========== симулейшен ==================

#include "game/simulation/GameSimulation.h"
#include "game/simulation/SimulationSnapshot.h"
#include "game/ship/controller/PlayerInputMapper.h"
#include "game/ship/view/PlayerShipView.h"
#include "src/game/client/ClientWorldState.h"
#include "src/game/server/GameServer.h"
#include "src/game/client/GameClient.h"
#include "src/game/network/ITransport.h"
#include "src/game/network/LocalLoopbackTransport.h"
#include "src/scene/SceneRenderer.h"
#include "src/ui/components/radar/RadarWidgetBase.h" 

// #include "src/WebSocket/DebugServer.h" 
#include "src/debug/FrustumDebugData.h"

#include "src/game/network/ClientShipCommand.h"

#include "game/damage/TestDamageHandler.h"
#include "game/debug/AttachmentEditorData.h"

#include "src/game/promo/PromoSceneScenario.h"
#include "src/game/traffic/StationTrafficSystem.h"
#include "src/game/system_map/SystemMapRenderer.h"
#include "src/world/celestial/SystemMapTypes.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

class StateStack;


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
    explicit SpaceState(StateStack& states);
    ~SpaceState();
    
    void renderUI() override;
    void renderHUD() override;
    
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

    const ShipAttachmentOverrideMap& attachmentEditorOverrides() const { return m_attachmentEditorOverrides; }
    ShipAttachmentOverrideMap& attachmentEditorOverrides() { return m_attachmentEditorOverrides; }

    void pushAttachmentEditorState();
    void processHtmlCommands();
    void pushShipCoreState();
    void pushStructureDebugState();
    void pushVolumeViewerState();
    void pushFrustumDebugState(const nlohmann::json& payload);
    void pushDebugControlState();
    void pushSystemMapState();
    void pushSystemMapPanelState();
    void selectSystemMapSystem(int systemId);
    void setSystemMapGalaxyMode();
    void setSystemMapCurrentSystemMode();

    void applyDebugControlPayload(const nlohmann::json& payload);

    void requestGalaxyMapSnapshotOnce();
    void requestSystemMapSnapshot(
        int systemId,
        bool forceRefresh = false
    );

    void updateSystemMapLiveFlags();
    bool shouldRefreshSystemMapSnapshot() const;

    void requestPlanetMapSnapshot(
        int systemId,
        const std::string& planetBodyId,
        bool forceRefresh = false
    );

    void setSystemMapPlanetMode();

    void requestHubMapSnapshot(
        int systemId,
        const std::string& hubId,
        bool forceRefresh = false
    );

    void setSystemMapHubMode();
    void setSystemMapLoadedPlanetMode();
private:

    

    WorldParams                                 m_world;

    bool wantsConfirmExit() const override;
    bool onGlobalEscape() override;
    bool isInSafeZone() const;
    
    // std::vector<Planet>                         m_planets;                  // "world/Planet.h"
    // std::vector<WorldSignal>                    m_worldSignals;             // "world/WorldSignal.h"
    // std::vector<InterferenceSource>             m_interferenceSources;      // "world/InterferenceSource.h" - источники помех

    // --- HUD ---
    std::vector<HudStaticItem>                  m_hudStatics;
    std::vector<HudLineRect>                    m_hudRects;
    HudMessage*                                 m_activeMessage = nullptr;
    HudRenderer                                 m_hudRenderer;
    
    WorldLabelRenderer                          m_worldLabelRenderer;



    std::unique_ptr<UIContainer>                uiRoot;
    UICameraView*                               rearView = nullptr; 
    ScreenLayout                                m_layout = ScreenLayout::Front_Main_Rear_Mini; //  режими отображения камеры
    Camera*                                     m_activeMainCamera = nullptr;
    ShipCameraMode                              m_activeCameraMode = ShipCameraMode::Cockpit;

    
    // std::unique_ptr<GameSimulation>                 m_simulation;
    std::unique_ptr<GameServer>                     m_server;
    std::unique_ptr<GameClient>                     m_client;
    std::unique_ptr<ITransport>                     m_transport;
    std::unique_ptr<ClientWorldState>               m_clientWorld;

    PlayerInputMapper                               m_inputMapper;
    std::unique_ptr<PlayerShipView>                 m_playerView;
    EntityId                                        m_playerId;
    
    ShipControlState                                m_playerControl;
    std::deque<ShipControlState>                    m_sentInputs;
    uint32_t m_localTick = 0;

    // =======================================
    //  =========== Simulation Tick ==========
    // =======================================
    float                                           m_simAccumulator = 0.0f;
    static constexpr float                          SIM_FIXED_DT = 0.02f;           // 50 Hz

    // =======================================
    //  ===========       HUD       ==========
    // =======================================
    RadarWidgetBase* m_radarWidget = nullptr;
    SceneRenderer m_sceneRenderer;
    PreparedScene m_preparedScene;
    SystemMapRenderer m_systemMapRenderer;


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
    double m_perfClientUpdateMs = 0.0;
    double m_perfPlayerViewMs = 0.0;
    double m_perfUiRootUpdateMs = 0.0;
    double m_perfHudMs = 0.0;


    game::promo::PromoSceneScenario m_promoSceneScenario;
    game::traffic::StationTrafficSystem m_stationTrafficSystem;
    void updatePromoPlayerShipTracking(float dt);

    bool m_promoTrackingInitialized = false;
    glm::quat m_promoPlayerOrientation {1.0f, 0.0f, 0.0f, 0.0f};

    bool m_promoRollStarted = false;
    bool m_promoRollFinished = false;

    float m_promoRollAngle = 0.0f;


    world::celestial::GalaxyMapSnapshot m_galaxyMapSnapshot;

    world::celestial::SystemMapSnapshot m_systemMapSnapshot;
    world::celestial::PlanetMapSnapshot m_planetMapSnapshot;
    world::celestial::HubMapSnapshot m_hubMapSnapshot;

    bool m_hasHubMapSnapshot = false;
    int m_loadedHubMapSystemId = -1;
    std::string m_loadedHubMapHubId;

    bool m_hasPlanetMapSnapshot = false;
    int m_loadedPlanetMapSystemId = -1;
    std::string m_loadedPlanetMapBodyId;

    int m_loadedSystemMapId = -1;
    bool m_hasGalaxyMapSnapshot = false;
    bool m_hasSystemMapSnapshot = false;
    double m_systemMapLiveRefreshTimer = 0.0;

    bool m_systemMapVisible = false;
    bool m_systemMapLiveSnapshotsEnabled = false;
    int m_liveSystemMapId = -1;
    
};
