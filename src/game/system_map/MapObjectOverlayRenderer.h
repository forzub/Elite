#pragma once

#include <string>

#include "src/game/system_map/MapObjectOverlay.h"
#include "src/game/system_map/NavigationMapTextProfile.h"
#include "src/render/types/Viewport.h"

namespace game::system_map
{

class MapObjectOverlayRenderer
{
public:
    void render(
        const Viewport& viewport,
        const MapObjectOverlayFrame& frame,
        MapObjectOverlayState& state,
        const NavigationMapTextProfile& textProfile
    ) const;

private:
    static std::string text(
        const NavigationMapTextProfile& textProfile,
        const std::string& key
    );
};

} // namespace game::system_map
