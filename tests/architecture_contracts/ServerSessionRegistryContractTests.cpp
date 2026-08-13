#include <cstdlib>
#include <iostream>

#include "src/game/server/ServerSessionRegistry.h"

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "[FAIL] " << message << "\n";
        std::exit(1);
    }
}
}

int main()
{
    game::server::ServerSessionRegistry registry;

    const EntityId shipA {101};
    const EntityId shipB {202};

    const auto sessionA = registry.create(shipA);
    const auto sessionB = registry.create(shipB);

    require(static_cast<bool>(sessionA), "session A was not allocated");
    require(static_cast<bool>(sessionB), "session B was not allocated");
    require(sessionA != sessionB, "server session ids are not unique");
    require(registry.size() == 2, "registry did not retain two sessions");
    require(registry.connectedCount() == 2, "connected count is wrong");
    require(registry.controlledEntity(sessionA) == shipA,
            "session A authority did not resolve to ship A");
    require(registry.controlledEntity(sessionB) == shipB,
            "session B authority did not resolve to ship B");
    require(registry.isControlledEntity(shipA),
            "ship A is not recognized as player-controlled");
    require(registry.isControlledEntity(shipB),
            "ship B is not recognized as player-controlled");

    const auto duplicateController = registry.create(shipA);
    require(!duplicateController,
            "two live sessions were allowed to control the same entity");

    require(registry.disconnect(sessionA), "session A disconnect failed");
    require(registry.connectedCount() == 1,
            "disconnect did not reduce connected session count");
    require(registry.controlledEntity(sessionA).value == 0,
            "disconnected session still resolves command authority");
    require(!registry.isControlledEntity(shipA),
            "disconnected ship remained player-controlled");
    require(registry.isControlledEntity(shipB),
            "disconnecting A affected session B authority");

    require(registry.reconnect(sessionA), "session A reconnect failed");
    require(registry.controlledEntity(sessionA) == shipA,
            "reconnected session did not recover its controlled entity");

    require(registry.disconnect(sessionA),
            "session A second disconnect failed");
    const auto replacementA = registry.create(shipA);
    require(static_cast<bool>(replacementA),
            "replacement session could not claim disconnected ship A");
    require(!registry.reconnect(sessionA),
            "stale disconnected session reconnected over replacement authority");
    require(registry.controlledEntity(replacementA) == shipA,
            "replacement session lost ship A authority");

    const auto invalid = registry.create(EntityId{});
    require(!invalid, "zero EntityId created a valid server session");

    std::cout
        << "[PASS] server session registry authority + disconnect/reconnect\n";
    return 0;
}
