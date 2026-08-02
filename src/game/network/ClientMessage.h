#pragma once

#include <variant>
#include <cstdint>

#include "src/game/ship/core/ShipControlState.h"
#include "src/game/network/ClientShipCommand.h"

namespace game::network
{

struct ClientMessage
{
    std::uint64_t clientTick = 0;
    std::variant<
        ShipControlState,
        ClientShipCommand
    > payload;
};

}