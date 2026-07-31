#pragma once

#include <string>

#include "src/game/system_map/MapMode.h"
#include "src/render/types/Viewport.h"

struct GLFWwindow;

namespace game::system_map
{
class DetailMapView;
class HubMapView;

struct LocalMapInteractionResult
{
    enum class SelectionAction
    {
        None = 0,
        SelectHub,
        ClearHub
    };

    SelectionAction selectionAction = SelectionAction::None;
    std::string hubId;
    std::string parentBodyId;
};

class LocalMapInteraction
{
public:
    LocalMapInteractionResult handle(
        MapMode mode,
        DetailMapView& detailView,
        HubMapView& hubView,
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
