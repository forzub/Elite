#pragma once

#include <unordered_map>

#include "core/GameState.h"

#include "render/Camera.h"
#include "render/HUD/TextRenderer.h"  
#include "render/HUD/WorldLabelRenderer.h" 
#include "render/camera/ShipCameraController.h" 


#include "game/signals/SignalReceiver.h"
#include "game/ship/ShipInstance.h"


#include "world/WorldParams.h"
#include "world/WorldObject.h"
#include "world/Planet.h"
#include "world/InterferenceSource.h"


#include "ui/HudTypes.h"
#include "ui/HudRenderer.h"
#include "ui/hud/HudEdgeMapper.h"


 
// #include "render/Camera.h"



class StateStack;

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
    
    // ShipTransform                               m_ship;   // состояние корабля
    Camera                                      m_camera; // камера, следующая за кораблём

    // ShipParams                                  m_params;
    WorldParams                                 m_world;
    float                                       m_receiverNoiseFloor; // допустимый уровень шума для приемника

    bool wantsConfirmExit() const override;
    bool onGlobalEscape() override;
    bool isInSafeZone() const;

    std::vector<WorldObject>                    m_worldObjects;             // "world/WorldParams.h"
    std::vector<Planet>                         m_planets;                  // "world/Planet.h"
    std::vector<SignalReceptionResult>          m_signalResults;            // "world/WorldSignal.h"
    std::vector<WorldSignal>                    m_worldSignals;             // "world/WorldSignal.h"
    std::vector<InterferenceSource>             m_interferenceSources;      // "world/InterferenceSource.h" - источники помех

    std::unordered_map<const WorldSignal*, 
            WorldLabel>                         m_worldLabels;
    
    // --- HUD ---
    std::vector<HudStaticItem>                  m_hudStatics;
    std::vector<HudLineRect>                    m_hudRects;
    HudMessage*                                 m_activeMessage = nullptr;
    HudRenderer                                 m_hudRenderer;
    
    
    SignalReceiver                              m_signalReceiver;
    WorldLabelRenderer                          m_worldLabelRenderer;

    WorldLabel& getOrCreateWorldLabel(const SignalReceptionResult& result);


    // перенос в отдельные сучности
    // ShipController                          m_shipController;
    // ShipCameraController                    m_cameraController;
    // HudEdgeMapper                           m_hudEdgeMapper;
    // ShipDescriptor                          m_shipDescriptor;
    ShipInstance                            m_playerShip;
};
