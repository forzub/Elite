#pragma once

#include "src/galaxy/Actors.h"

// ВРЕМЕННОЕ хранилище ActorId ключевых акторов игры
// Заменяется при внедрении нормального доступа к GalaxyDatabase
namespace ActorIds
{
    inline ActorId Player()
    {
        // 1 — временное допущение
        // ДОЛЖНО совпадать с actorId игрока в GalaxyDatabase
        return static_cast<ActorId>(1);
    }

    inline ActorId Unknown()
    {
        return static_cast<ActorId>(0);
    }
}
