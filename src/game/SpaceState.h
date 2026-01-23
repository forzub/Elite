#pragma once

#include "core/GameState.h"
#include "render/Camera.h"
#include "render/TextRenderer.h"   

#include "game/ShipTransform.h"
#include "game/ShipParams.h"
#include "world/WorldParams.h"
 
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
    
    void submitRenderData() override;
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
};
