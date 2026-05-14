#pragma once
#include <string>
#include <cstdint>

#include "src/galaxy/Actors.h"   // ActorId / ActorCode
#include "src/game/ship/ShipRoleType.h"   // ActorId / ActorCode


using ShipInstanceId = uint64_t;

// Юридическая и социальная идентичность корабля
struct ShipRegistry
{
    ShipInstanceId instanceId = 0;                      // уникальный ID экземпляра

    // Владелец
    std::string    ownerName;                           // имя / название организации
    ActorId        ownerActor = 0;                      // к какому актору принадлежит владелец

    // Регистрация
    std::string    registrationId;                      // гос. номер / бортовой номер
    std::string    homePort;                            // порт приписки

    // Назначение
    ShipRoleType shipRole = ShipRoleType::Unknown;      // торговый / военный / охрана / частный

};
