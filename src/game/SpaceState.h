#pragma once

#include "core/GameState.h"
#include "render/Camera.h"

#include "game/ShipTransform.h"
#include "game/ShipParams.h"
#include "world/WorldParams.h"
 
// #include "render/Camera.h"

class StateStack;

class SpaceState : public GameState
{
public:
    explicit SpaceState(StateStack& states);
    

    void handleInput() override;
    void update(float dt) override;
    void render() override;
    bool m_mouseLook = false;

private:
    StateStack& m_states;
    ShipTransform m_ship;   // состояние корабля
    Camera        m_camera; // камера, следующая за кораблём

    ShipParams m_params;
    WorldParams m_world;
};
