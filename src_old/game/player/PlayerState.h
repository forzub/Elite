#pragma once
#include "src/galaxy/Actors.h"

using PlayerId = uint32_t;

struct PlayerState
{
    PlayerId    id;
    ActorId     actor;       // к какому актору принадлежит
    ActorCode   activeCode;  // текущий код (может меняться)
    int         credits;
};
