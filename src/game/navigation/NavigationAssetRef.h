#pragma once

#include <cstdint>

#include "src/game/identity/DroneInstanceId.h"
#include "src/game/identity/ShipInstanceId.h"

namespace game::navigation
{

enum class NavigationAssetKind : std::uint8_t
{
    None = 0,
    Ship,
    Drone
};

/*
    Stable identity of the vehicle that will execute a route.

    Runtime EntityId deliberately does not participate here. A ship/drone may
    dematerialize and later receive another runtime entity while the authored
    route must continue to name the same durable asset.
*/
struct NavigationAssetRef
{
    NavigationAssetKind kind = NavigationAssetKind::None;
    ShipInstanceId shipInstanceId = 0;
    DroneInstanceId droneInstanceId {};

    static NavigationAssetRef ship(ShipInstanceId id) noexcept
    {
        NavigationAssetRef out;
        out.kind = NavigationAssetKind::Ship;
        out.shipInstanceId = id;
        return out;
    }

    static NavigationAssetRef drone(DroneInstanceId id) noexcept
    {
        NavigationAssetRef out;
        out.kind = NavigationAssetKind::Drone;
        out.droneInstanceId = id;
        return out;
    }

    bool valid() const noexcept
    {
        switch (kind)
        {
            case NavigationAssetKind::Ship:
                return shipInstanceId != 0 && !droneInstanceId;
            case NavigationAssetKind::Drone:
                return shipInstanceId == 0 && static_cast<bool>(droneInstanceId);
            case NavigationAssetKind::None:
                return false;
        }
        return false;
    }
};

inline bool sameNavigationAsset(
    const NavigationAssetRef& a,
    const NavigationAssetRef& b
) noexcept
{
    if (a.kind != b.kind)
        return false;

    switch (a.kind)
    {
        case NavigationAssetKind::Ship:
            return a.shipInstanceId != 0 &&
                   a.shipInstanceId == b.shipInstanceId;
        case NavigationAssetKind::Drone:
            return static_cast<bool>(a.droneInstanceId) &&
                   a.droneInstanceId == b.droneInstanceId;
        case NavigationAssetKind::None:
            return false;
    }
    return false;
}

} // namespace game::navigation
