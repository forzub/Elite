#pragma once
#include <cstdint>

using ActorCode = uint32_t;

// ВРЕМЕННО: простой генератор кодов
inline ActorCode generateActorCode()
{
    static ActorCode next = 1;
    return next++;
}
