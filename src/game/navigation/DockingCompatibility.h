#pragma once

#include <algorithm>
#include <cstdint>
#include <string>

#include "src/game/navigation/HubSemanticAnchor.h"

namespace game::navigation
{

enum class DockingOperationalState : std::uint8_t
{
    Unknown = 0,
    Online,
    Offline,
    Damaged
};

enum class DockingOccupancyState : std::uint8_t
{
    Unknown = 0,
    Free,
    Occupied,
    Reserved
};

enum class DockingAccessState : std::uint8_t
{
    Unknown = 0,
    Allowed,
    ClearanceRequired,
    Denied
};

struct DockingPortRuntimeState
{
    std::string hubModuleId;
    std::string anchorId;
    DockingOperationalState operational = DockingOperationalState::Unknown;
    DockingOccupancyState occupancy = DockingOccupancyState::Unknown;
    DockingAccessState access = DockingAccessState::Unknown;

    bool operationalNow() const noexcept
    {
        return operational == DockingOperationalState::Online;
    }

    bool freeNow() const noexcept
    {
        return occupancy == DockingOccupancyState::Free;
    }

    bool accessAllowedNow() const noexcept
    {
        return access == DockingAccessState::Allowed;
    }
};

struct ShipDockingEnvelope
{
    double lengthMeters = 0.0;
    double widthMeters = 0.0;
    double heightMeters = 0.0;
    bool valid = false;
};

struct DockingCompatibilityResult
{
    bool geometryFits = false;
    bool operational = false;
    bool free = false;
    bool accessAllowed = false;
    bool routeAvailable = false;

    double openingWidthMeters = 0.0;
    double openingHeightMeters = 0.0;
    double requiredClearanceMeters = 0.0;
    double usableWidthMeters = 0.0;
    double usableHeightMeters = 0.0;
    double widthMarginMeters = 0.0;
    double heightMarginMeters = 0.0;
};

inline DockingCompatibilityResult evaluateDockingCompatibility(
    const ShipDockingEnvelope& ship,
    const HubSemanticAnchorDefinition& dock,
    const DockingPortRuntimeState& runtime
) noexcept
{
    DockingCompatibilityResult result;
    result.openingWidthMeters = std::max(0.0, dock.extentMeters.x);
    result.openingHeightMeters = std::max(0.0, dock.extentMeters.y);
    result.requiredClearanceMeters = std::max(0.0, dock.requiredClearanceMeters);

    result.usableWidthMeters = std::max(
        0.0,
        result.openingWidthMeters - 2.0 * result.requiredClearanceMeters
    );
    result.usableHeightMeters = std::max(
        0.0,
        result.openingHeightMeters - 2.0 * result.requiredClearanceMeters
    );

    if (ship.valid)
    {
        result.widthMarginMeters = result.usableWidthMeters - ship.widthMeters;
        result.heightMarginMeters = result.usableHeightMeters - ship.heightMeters;
        result.geometryFits =
            result.widthMarginMeters >= 0.0 &&
            result.heightMarginMeters >= 0.0;
    }

    result.operational = runtime.operationalNow();
    result.free = runtime.freeNow();
    result.accessAllowed = runtime.accessAllowedNow();
    result.routeAvailable =
        result.geometryFits &&
        result.operational &&
        result.free &&
        result.accessAllowed;
    return result;
}

} // namespace game::navigation
