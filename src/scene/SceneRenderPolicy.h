#pragma once

#include "src/game/ship/core/ShipRole.h"
#include "src/game/visual/VisualShipKind.h"
#include "src/world/types/ObjectType.h"

// Client-only visibility policy. These switches must never decide whether an
// authoritative entity exists or is simulated; they only filter presentation.
struct SceneRenderPolicy
{
    bool drawStarfield = true;
    bool drawCelestial = true;
    bool drawFarStationProxy = true;
    bool drawLabels = true;
    bool drawDebug = true;
    bool drawVisualShips = true;
    bool drawVisualDrones = true;
    bool drawObjects = true;

    bool drawRealShips = true;
    bool drawPlayerShip = true;
    bool drawNpcShips = true;
    bool drawTrafficShips = true;
    bool drawHubs = true;
    bool drawLargeObjects = true;

    int maxVisualShipsToDraw = -1;

    // Secondary/miniview cameras may trade nearby assembly detail for stable
    // frame pacing. This is presentation-only and never changes simulation or
    // the prepared scene itself.
    bool forceAssemblyLod1 = false;

    bool shouldDrawRealShip(ShipRole role) const noexcept
    {
        if (!drawRealShips)
            return false;

        return role == ShipRole::Player
            ? drawPlayerShip
            : drawNpcShips;
    }

    bool shouldDrawVisualShip(game::visual::VisualShipKind kind) const noexcept
    {
        if (!drawVisualShips)
            return false;

        if (kind == game::visual::VisualShipKind::Traffic)
            return drawTrafficShips;

        return true;
    }

    bool shouldDrawObject(ObjectType type) const noexcept
    {
        if (!drawObjects)
            return false;

        switch (type)
        {
            case ObjectType::Station:
                return drawHubs;

            case ObjectType::Planet:
                return drawCelestial;

            default:
                return drawLargeObjects;
        }
    }
};
