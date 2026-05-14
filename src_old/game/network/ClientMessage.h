#pragma once

#include <variant>
#include <cstdint>

#include "src/game/ship/core/ShipControlState.h"
#include "src/game/network/ClientShipCommand.h"

namespace game::network
{

// Типы сообщений клиента
enum class ClientMessageType
{
    ControlInput,           // уже существующий поток управления
    ClientShipCommand       // новые событийные команды
};


struct ClientMessage
{
    uint32_t clientTick = 0;
    ClientMessageType type;

    std::variant<
        ShipControlState,
        ClientShipCommand
    > payload;
};

}