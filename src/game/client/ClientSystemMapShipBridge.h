#pragma once

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "src/game/client/ClientSystemMapShipSampler.h"
#include "src/game/ship/ShipDescriptorRegistry.h"
#include "src/world/celestial/CelestialTypes.h"
#include "src/world/celestial/SystemMapTypes.h"

namespace game::client
{

inline bool isServerDiagnosticSystemMapShip(
    const world::celestial::SystemMapObject& object
) noexcept
{
    return
        object.kind == world::celestial::SystemMapObjectKind::Ship &&
        object.id.value == 0 &&
        object.stableId.rfind("diagnostic:", 0) == 0;
}

/*
    Real ship positions already arrive through normal authoritative replication.
    System-map responses therefore carry only map-specific objects plus optional
    explicit diagnostic probes. Rebuild the ordinary ship layer from the exact
    replicated server-time sample selected by ClientSystemMapShipSampler.
*/
inline void rebuildSystemMapShipLayer(
    world::celestial::SystemMapSnapshot& map,
    const std::vector<SystemMapShipSample>& ships
)
{
    using world::celestial::SystemMapObject;
    using world::celestial::SystemMapObjectKind;

    map.objects.erase(
        std::remove_if(
            map.objects.begin(),
            map.objects.end(),
            [](const SystemMapObject& object)
            {
                return
                    object.kind == SystemMapObjectKind::Ship &&
                    !isServerDiagnosticSystemMapShip(object);
            }
        ),
        map.objects.end()
    );

    for (const auto& ship : ships)
    {
        if (ship.systemId != map.systemId)
            continue;

        SystemMapObject object;
        object.id = ship.id;
        object.stableId =
            ship.role == ShipRole::Player
                ? "player"
                : "entity:" + std::to_string(ship.id.value);

        if (ship.role == ShipRole::Player)
        {
            object.name = "Player";
        }
        else if (ship.motionLabKind !=
                 game::diagnostics::HubMotionLabActorKind::None)
        {
            object.name =
                game::diagnostics::hubMotionLabLabel(ship.motionLabKind);
        }
        else
        {
            const auto& descriptor = ShipDescriptorRegistry::get(ship.typeId);
            object.name = descriptor.identity.shipName.empty()
                ? "Ship " + std::to_string(ship.id.value)
                : descriptor.identity.shipName;
        }

        object.parentBodyId = ship.parentBodyId;
        object.kind = SystemMapObjectKind::Ship;
        object.positionAu =
            world::coordinates::fullMeters(ship.worldPosition) /
            world::celestial::MetersPerAu;
        object.systemId = ship.systemId;
        object.hasOrbit = false;

        map.objects.push_back(std::move(object));
    }
}

} // namespace game::client
