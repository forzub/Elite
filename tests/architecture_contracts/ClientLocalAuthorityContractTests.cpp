#include <cstdlib>
#include <iostream>

#include "src/game/client/ClientLocalAuthority.h"

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "[FAIL] " << message << '\n';
        std::exit(1);
    }
}
}

int main()
{
    const EntityId local {17};
    const EntityId remoteHuman {23};
    const EntityId none {0};

    require(
        game::client::isLocalControlledEntity(local, local),
        "server-assigned controlled entity must be local"
    );
    require(
        !game::client::isLocalControlledEntity(remoteHuman, local),
        "another human entity must remain remote"
    );
    require(
        !game::client::isLocalControlledEntity(local, none),
        "zero controlled identity must never authorize local prediction"
    );

    std::cout << "[PASS] client local controlled-entity identity\n";
    return 0;
}
