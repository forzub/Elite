#pragma once

#include <unordered_map>
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
#include "world/Planet.h"
#include "world/InterferenceSource.h"
#include "ui/HudRenderer.h"

// ============== components ==============
#include "ui/components/UIContainer.h"
#include "ui/components/UICameraView.h"

 

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


private:

    WorldParams                                 m_world;

    bool wantsConfirmExit() const override;
    bool onGlobalEscape() override;
    bool isInSafeZone() const;
    
    std::vector<Planet>                         m_planets;                  // "world/Planet.h"
    std::vector<WorldSignal>                    m_worldSignals;             // "world/WorldSignal.h"
    std::vector<InterferenceSource>             m_interferenceSources;      // "world/InterferenceSource.h" - источники помех

    // --- HUD ---
    std::vector<HudStaticItem>                  m_hudStatics;
    std::vector<HudLineRect>                    m_hudRects;
    HudMessage*                                 m_activeMessage = nullptr;
    HudRenderer                                 m_hudRenderer;
    
    WorldLabelRenderer                          m_worldLabelRenderer;

    // Camera                                      m_camera;               // камера, следующая за кораблём
    Ship                                        m_playerShip;
    std::vector<Ship>                           m_npcShips;             // корабли NPC и remote players

    std::unique_ptr<UIContainer>                uiRoot;
    UICameraView*                               rearView = nullptr; 
    ScreenLayout                                m_layout = ScreenLayout::Front_Main_Rear_Mini; //  режими отображения камеры
    Camera*                                     m_activeMainCamera = nullptr;
    ShipCameraMode                              m_activeCameraMode = ShipCameraMode::Cockpit;
};
