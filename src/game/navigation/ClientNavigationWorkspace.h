#pragma once

#include <vector>

#include "src/game/navigation/OwnedNavigationAssetState.h"
#include "src/game/navigation/GuidanceCorridor.h"
#include "src/game/navigation/NavigationModuleState.h"
#include "src/game/navigation/RoutePlan.h"
#include "src/game/navigation/TargetTrackingState.h"

namespace game::navigation
{

/*
    Client-owned navigation product state shared by maps, HUD and future
    trajectory/autopilot layers. Renderers edit/present this workspace but do
    not own it.
*/
class ClientNavigationWorkspace
{
public:
    TargetTrackingState& targets() noexcept { return m_targets; }
    const TargetTrackingState& targets() const noexcept { return m_targets; }

    RoutePlan& routePlan() noexcept { return m_routePlan; }
    const RoutePlan& routePlan() const noexcept { return m_routePlan; }

    OwnedNavigationAssetState& ownedAssets() noexcept { return m_ownedAssets; }
    const OwnedNavigationAssetState& ownedAssets() const noexcept
    {
        return m_ownedAssets;
    }

    NavigationGuidanceState& guidance() noexcept { return m_guidance; }
    const NavigationGuidanceState& guidance() const noexcept
    {
        return m_guidance;
    }

    NavigationModuleState& modules() noexcept { return m_modules; }
    const NavigationModuleState& modules() const noexcept
    {
        return m_modules;
    }

    const NavigationAssetRef& localControlledAsset() const noexcept
    {
        return m_localControlledAsset;
    }

    void syncOwnedAssets(
        std::vector<OwnedNavigationAsset> assets,
        ShipInstanceId localControlledShipId
    )
    {
        m_ownedAssets.replace(std::move(assets));
        m_localControlledAsset = localControlledShipId != 0
            ? NavigationAssetRef::ship(localControlledShipId)
            : NavigationAssetRef{};

        // Preserve an explicitly selected remote executor while it is still
        // commandable. Otherwise recover to the occupied ship, then to the
        // first server-authorized asset.
        if (m_routePlan.hasStart() &&
            m_ownedAssets.commandable(m_routePlan.start().executor))
        {
            return;
        }

        if (m_localControlledAsset.valid() &&
            m_ownedAssets.commandable(m_localControlledAsset))
        {
            m_routePlan.setStartExecutor(m_localControlledAsset);
            return;
        }

        if (const auto* fallback = m_ownedAssets.firstCommandable())
        {
            m_routePlan.setStartExecutor(fallback->asset);
            return;
        }

        m_routePlan.clearStart();
    }

    bool selectRouteExecutor(const NavigationAssetRef& asset)
    {
        if (!m_ownedAssets.commandable(asset))
            return false;
        m_routePlan.setStartExecutor(asset);
        return true;
    }

private:
    TargetTrackingState m_targets;
    RoutePlan m_routePlan;
    OwnedNavigationAssetState m_ownedAssets;
    NavigationGuidanceState m_guidance;
    NavigationModuleState m_modules;
    NavigationAssetRef m_localControlledAsset;
};

} // namespace game::navigation
