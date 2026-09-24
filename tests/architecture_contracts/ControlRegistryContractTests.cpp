#include <cstdlib>
#include <iostream>

#include "src/game/server/ControlRegistry.h"

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
    using game::server::ControllerKind;
    using game::server::ControlRegistry;

    ControlRegistry registry;
    const PlayerId player {101};
    const PlayerId other {202};
    const EntityId ship {42};
    const EntityId otherShip {84};

    require(registry.bindHuman(player, ship), "initial Human binding failed");
    require(registry.bindHuman(other, otherShip), "second Human binding failed");
    require(registry.controlledEntity(player) == ship,
            "player -> ship identity missing before takeover");
    require(registry.controllerKind(ship) == ControllerKind::Human,
            "initial controller is not Human");

    require(registry.takeAutopilotControl(player, ship),
            "Human -> Autopilot takeover failed");
    require(registry.controllerKind(ship) == ControllerKind::Autopilot,
            "controller kind did not become Autopilot");
    require(registry.controlledEntity(player) == ship,
            "Autopilot takeover rewrote player -> ship identity");
    require(!registry.takeAutopilotControl(other, ship),
            "wrong player stole Autopilot authority");

    require(registry.restoreHumanControl(player, ship),
            "Autopilot -> Human hand-back failed");
    require(registry.controllerKind(ship) == ControllerKind::Human,
            "controller kind did not return to Human");
    require(registry.controlledEntity(player) == ship,
            "Human hand-back lost player -> ship identity");
    require(registry.controlledEntity(other) == otherShip,
            "temporary authority affected unrelated player");

    require(!registry.restoreHumanControl(other, ship),
            "wrong player restored another ship's authority");
    require(registry.controllerKind(ship) == ControllerKind::Human,
            "failed foreign restore mutated controller kind");

    std::cout
        << "[PASS] ControlRegistry Human <-> Autopilot preserves identity\n";
    return 0;
}
