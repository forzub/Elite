#pragma once
#include <cstdint>

using ActorCode = uint32_t;

struct SignalAddress
{
    ActorCode actor = 0;     // кто говорит
    uint32_t  channel = 0;   // 0 = публично

    bool isValid() const
    {
        return actor != 0;
    }
};
