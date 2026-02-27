#pragma once
#include "src/game/client/ClientWorldState.h"

class SceneRenderer
{
public:
    void render(const ClientWorldState& world,
            EntityId playerId);
};