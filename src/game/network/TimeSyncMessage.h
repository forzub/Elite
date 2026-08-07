#pragma once

#include <cstdint>

namespace game::network
{

/*
    Tiny request/response pair used only to estimate the mapping between the
    client's local monotonic clock and the authoritative server simulation
    clock. Universe time is deliberately not synchronized here.
*/
struct TimeSyncRequest
{
    std::uint64_t sequence = 0;
    double clientSendTimeSeconds = 0.0;
};

struct TimeSyncResponse
{
    std::uint64_t sequence = 0;
    double clientSendTimeSeconds = 0.0;
    double serverReceiveTimeSeconds = 0.0;
};

} // namespace game::network
