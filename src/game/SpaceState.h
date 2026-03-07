#pragma once

#include <unordered_map>
#include <deque>

#include "core/GameState.h"

// #include "render/Camera.h"
// #include "render/HUD/TextRenderer.h"  
// #include "render/HUD/WorldLabelRenderer.h" 
// #include "render/RenderContext.h"
// #include "src/game/equipment/signalNode/processing/SignalReceiver.h"
// #include "src/game/ship/view/ShipCameraController.h" 
// #include "world/WorldParams.h"
// #include "ui/HudTypes.h"
// #include "ui/hud/HudEdgeMapper.h"

#include "src/game/ship/Ship.h"
// #include "world/Planet.h"
// #include "world/InterferenceSource.h"
#include "ui/HudRenderer.h"

// ============== components ==============
#include "ui/components/UIContainer.h"
#include "ui/components/UICameraView.h"

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

#include "src/WebSocket/DebugServer.h" 
#include "src/game/network/ClientShipCommand.h"

#include "game/damage/TestDamageHandler.h"

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
    RadarWidgetBase*                                m_radarWidget = nullptr;
    SceneRenderer                                   m_sceneRenderer;


    std::unique_ptr<game::debug::DebugServer>       m_debugServer;
    std::vector<ClientShipCommand>                  m_debugCommands;
    std::mutex                                      m_debugCommandsMutex;                // для защиты очереди


    game::damage::TestDamageHandler handler;
};
