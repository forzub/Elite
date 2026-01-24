#pragma once

#include "core/GameState.h"

#include "render/Camera.h"
#include "render/TextRenderer.h"  
#include "render/WorldLabelRenderer.h" 

#include "game/ShipTransform.h"
#include "game/ShipParams.h"
#include "game/SignalReceiver.h"

#include "world/WorldParams.h"

#include "ui/HudTypes.h"
#include "ui/HudRenderer.h"
 
// #include "render/Camera.h"

struct WorldObject
{
    glm::vec3 position;
    std::string label;
};

class StateStack;

class SpaceState : public GameState
{
public:
    explicit SpaceState(StateStack& states);
    
    void renderUI() override;
    void renderHUD() override;
    
    void handleInput() override;
    void update(float dt) override;
    void render() override;
    bool m_mouseLook = false;

private:
    
    ShipTransform m_ship;   // состояние корабля
    Camera        m_camera; // камера, следующая за кораблём

    ShipParams m_params;
    WorldParams m_world;

    bool wantsConfirmExit() const override;
    bool onGlobalEscape() override;
    bool isInSafeZone() const;

    std::vector<WorldObject> m_worldObjects;
    
    // --- HUD ---
    std::vector<HudStaticItem> m_hudStatics;
    std::vector<HudLineRect> m_hudRects;
    HudMessage* m_activeMessage = nullptr;
    HudRenderer m_hudRenderer;
    
    
    std::vector<WorldSignal> m_worldSignals;
    SignalReceiver m_signalReceiver;
    WorldLabelRenderer m_worldLabelRenderer;

};
