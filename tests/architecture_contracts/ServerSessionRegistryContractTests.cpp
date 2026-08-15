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

    const PlayerId playerA {101};
    const PlayerId playerB {202};

    const auto sessionA = registry.create(playerA);
    const auto sessionB = registry.create(playerB);

    require(static_cast<bool>(sessionA), "session A was not allocated");
    require(static_cast<bool>(sessionB), "session B was not allocated");
    require(sessionA != sessionB, "server session ids are not unique");
    require(registry.size() == 2, "registry did not retain two sessions");
    require(registry.connectedCount() == 2, "connected count is wrong");
    require(registry.player(sessionA) == playerA,
            "session A identity did not resolve to player A");
    require(registry.player(sessionB) == playerB,
            "session B identity did not resolve to player B");
    require(registry.isConnectedPlayer(playerA),
            "player A is not recognized as connected");
    require(registry.isConnectedPlayer(playerB),
            "player B is not recognized as connected");

    const auto duplicatePlayer = registry.create(playerA);
    require(!duplicatePlayer,
            "two live sessions were allowed for the same PlayerId");

    require(registry.disconnect(sessionA), "session A disconnect failed");
    require(registry.connectedCount() == 1,
            "disconnect did not reduce connected session count");
    require(!registry.player(sessionA),
            "disconnected session still resolves player identity");
    require(!registry.isConnectedPlayer(playerA),
            "disconnected player remained connected");
    require(registry.isConnectedPlayer(playerB),
            "disconnecting A affected session B identity");

    require(registry.reconnect(sessionA), "session A reconnect failed");
    require(registry.player(sessionA) == playerA,
            "reconnected session did not recover PlayerId");

    require(registry.disconnect(sessionA),
            "session A second disconnect failed");
    const auto replacementA = registry.create(playerA);
    require(static_cast<bool>(replacementA),
            "replacement session could not claim disconnected player A");
    require(!registry.reconnect(sessionA),
            "stale disconnected session reconnected over replacement identity");
    require(registry.player(replacementA) == playerA,
            "replacement session lost player A identity");

    const auto invalid = registry.create(PlayerId{});
    require(!invalid, "zero PlayerId created a valid server session");

    std::cout
        << "[PASS] server session registry session -> persistent PlayerId\n";
    return 0;
}
