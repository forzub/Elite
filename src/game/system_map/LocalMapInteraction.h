#pragma once

#include "src/game/system_map/MapMode.h"
#include "src/render/types/Viewport.h"

struct GLFWwindow;

namespace game::system_map
{
class DetailMapView;
class HubMapView;
class SystemMapView;

class LocalMapInteraction
{
public:
    void handle(
        MapMode mode,
        DetailMapView& detailView,
        HubMapView& hubView,
        SystemMapView& systemView,
        const Viewport& viewport,
        GLFWwindow* window,
        double mouseX,
        double mouseY,
        double localMouseX,
        double localMouseY,
        bool inside,
        bool leftDown,
        bool rightDown,
        double& pendingScrollY
    ) const;
};

} // namespace game::system_map
