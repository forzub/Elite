#include <cstdlib>
#include <iostream>

#include "src/game/server/ControlRegistry.h"
#include "src/game/server/PlayerRegistry.h"
#include "src/game/server/ShipInstanceRegistry.h"

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
    game::server::ShipInstanceRegistry ships;

    game::server::ShipInstanceRecord shipA;
    shipA.instanceId = 1001;
    shipA.materializedEntityId = EntityId{11};
    shipA.typeId = ObjectType::CobraMk1;
    shipA.roleType = ShipRoleType::Civilian;

    game::server::ShipInstanceRecord shipB;
    shipB.instanceId = 1002;
    shipB.materializedEntityId = EntityId{12};
    shipB.typeId = ObjectType::CobraMk1;
    shipB.roleType = ShipRoleType::Civilian;

    require(ships.registerMaterialized(shipA), "ship A registration failed");
    require(ships.registerMaterialized(shipB), "ship B registration failed");
    require(ships.instanceForEntity(EntityId{11}) == 1001,
            "EntityId -> ShipInstanceId mapping failed");
    require(ships.materializedEntity(1002) == EntityId{12},
            "ShipInstanceId -> EntityId mapping failed");
    auto conflictingShipA = shipA;
    conflictingShipA.materializedEntityId = EntityId{99};
    require(!ships.registerMaterialized(conflictingShipA),
            "one live ShipInstanceId was mapped to two EntityIds");

    game::server::PlayerRegistry players;
    const PlayerId playerA = players.create(1001);
    const PlayerId playerB = players.create(1002);

    require(playerA && playerB && playerA != playerB,
            "persistent PlayerIds were not allocated uniquely");
    require(players.playerForShip(1001) == playerA,
            "player A is not assigned to persistent ship A");
    require(!players.create(1001),
            "two PlayerIds were assigned to the same persistent ship");

    game::server::ControlRegistry controls;
    require(controls.bindHuman(playerA, EntityId{11}),
            "player A human control binding failed");
    require(controls.bindHuman(playerB, EntityId{12}),
            "player B human control binding failed");
    require(controls.controlledEntity(playerA) == EntityId{11},
            "PlayerId -> controlled EntityId mapping failed");
    require(controls.humanPlayerForEntity(EntityId{12}) == playerB,
            "controlled EntityId -> PlayerId mapping failed");
    require(!controls.bindHuman(playerB, EntityId{11}),
            "two players were allowed to control one runtime entity");

    require(ships.markDematerialized(1002),
            "ship B dematerialization failed");
    require(ships.materializedEntity(1002).value == 0,
            "dematerialized ship retained a runtime EntityId");

    shipB.materializedEntityId = EntityId{44};
    require(ships.registerMaterialized(shipB),
            "persistent ship could not be rematerialized with a new EntityId");
    require(ships.instanceForEntity(EntityId{44}) == 1002,
            "rematerialized ship lost persistent identity");

    std::cout
        << "[PASS] PlayerId + ShipInstanceId + control authority registries\n";
    return 0;
}
