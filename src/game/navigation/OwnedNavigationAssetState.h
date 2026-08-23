#pragma once

#include <algorithm>
#include <utility>
#include <vector>

#include "src/game/navigation/OwnedNavigationAsset.h"

namespace game::navigation
{

class OwnedNavigationAssetState
{
public:
    void replace(std::vector<OwnedNavigationAsset> assets)
    {
        m_assets = std::move(assets);
    }

    const std::vector<OwnedNavigationAsset>& all() const noexcept
    {
        return m_assets;
    }

    const OwnedNavigationAsset* find(const NavigationAssetRef& asset) const noexcept
    {
        const auto it = std::find_if(
            m_assets.begin(),
            m_assets.end(),
            [&](const OwnedNavigationAsset& candidate)
            {
                return sameNavigationAsset(candidate.asset, asset);
            }
        );
        return it == m_assets.end() ? nullptr : &*it;
    }

    bool commandable(const NavigationAssetRef& asset) const noexcept
    {
        const auto* found = find(asset);
        return found && found->commandable;
    }

    const OwnedNavigationAsset* firstCommandable() const noexcept
    {
        const auto it = std::find_if(
            m_assets.begin(),
            m_assets.end(),
            [](const OwnedNavigationAsset& asset)
            {
                return asset.commandable && asset.asset.valid();
            }
        );
        return it == m_assets.end() ? nullptr : &*it;
    }

private:
    std::vector<OwnedNavigationAsset> m_assets;
};

} // namespace game::navigation
