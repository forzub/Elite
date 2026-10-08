#pragma once

#include <string>
#include <unordered_map>

#include "src/world/navigation/NavigationVolume.h"

namespace game::navigation
{

class NavigationVolumeCatalog final
{
public:
    bool load(
        const std::string& path =
            "assets/data/navigation/hub_navigation_volumes.json"
    );

    const world::navigation::NavigationVolumeDefinition* find(
        const std::string& id
    ) const noexcept;

    const std::string& failure() const noexcept
    {
        return m_failure;
    }

private:
    std::unordered_map<
        std::string,
        world::navigation::NavigationVolumeDefinition
    > m_byId;
    std::string m_failure;
};

} // namespace game::navigation
