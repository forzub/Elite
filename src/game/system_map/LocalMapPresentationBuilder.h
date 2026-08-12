#pragma once

#include <string>
#include <utility>

#include "src/game/system_map/LocalMapPresentation.h"
#include "src/render/types/Viewport.h"

namespace world::celestial
{
    struct DetailMapSnapshot;
    struct HubMapSnapshot;
}

namespace game::system_map
{

class DetailMapView;
class HubMapView;

class LocalMapPresentationBuilder
{
public:
    void setPlayerLabel(std::string label)
    {
        m_playerLabel = std::move(label);
    }

    DetailMapPresentation buildDetail(
        DetailMapView& view,
        const Viewport& viewport,
        const world::celestial::DetailMapSnapshot& snapshot
    ) const;

    HubMapPresentation buildHub(
        const HubMapView& view,
        const Viewport& viewport,
        const world::celestial::HubMapSnapshot& snapshot
    ) const;

private:
    std::string m_playerLabel = "PLAYER";
};

} // namespace game::system_map
