#pragma once
#include <cstdint>

using ActorId      = uint32_t;
using SubActorId   = uint32_t;
using NodeId       = uint32_t;
using StarSystemId = uint32_t;
using RouteId      = uint32_t;
using GenerationShipId = uint32_t;
using ArchiveEntryId   = uint32_t;

// 🔐 КОДЫ ШИФРОВАНИЯ
using ActorCode   = uint32_t;

constexpr ActorCode ActorCodeInvalid = 0;
constexpr ActorCode ActorCodeAll     = 0xFFFFFFFF;

constexpr ActorId      InvalidActor      = 0;
constexpr SubActorId   InvalidSubActor   = 0;
constexpr NodeId       InvalidNode       = 0;
constexpr StarSystemId InvalidStarSystem = 0;
constexpr RouteId      InvalidRoute      = 0;
