#pragma once

#include <optional>

#include <glm/glm.hpp>

#include "src/game/navigation/GalaxyNavigationGrid.h"
#include "src/game/system_map/MapIntent.h"
#include "src/render/types/Viewport.h"

namespace world::celestial
{
    struct GalaxyMapSnapshot;
}

namespace game::system_map
{
    class GalaxyMapView;

    struct GalaxyMapInputFrame
    {
        Viewport viewport;

        double mouseX = 0.0;
        double mouseY = 0.0;
        double localMouseX = 0.0;
        double localMouseY = 0.0;

        bool inside = false;
        bool leftDown = false;
        bool rightDown = false;

        bool zoomInKeyDown = false;
        bool zoomOutKeyDown = false;
        bool transitionActive = false;

        double nowSeconds = 0.0;
    };

    struct GalaxyMapInputResult
    {
        bool requestWindowFocus = false;

        // Presentation-only feedback consumed by SystemMapRenderer.
        std::optional<int> galaxyLevelChanged;

        // World or mode transition executed by SpaceState.
        std::optional<MapIntent> mapIntent;

        // Client-only navigation memory can turn an explicitly clicked empty
        // cube into a route waypoint without coupling that intent to server
        // simulation or to camera refinement.
        std::optional<navigation::GalaxyNavigationCell> clickedNavigationCell;
    };

    /*
        Galaxy-specific input, picking and camera interaction.

        The class owns no OpenGL resources and no persistent map state.
        It mutates GalaxyMapView through an explicit input frame and returns
        intents which the SystemMapRenderer facade can route to shared UI or
        transition services.
    */
    class GalaxyMapInteraction
    {
    public:
        GalaxyMapInputResult handleInput(
            GalaxyMapView& view,
            const world::celestial::GalaxyMapSnapshot& galaxy,
            const GalaxyMapInputFrame& frame,
            double& pendingScrollY
        ) const;

    private:
        bool pickNavigationCell(
            const GalaxyMapView& view,
            const Viewport& viewport,
            double localMouseX,
            double localMouseY,
            navigation::GalaxyNavigationCell& outCell
        ) const;

        void updateNavigationHoverFromCursor(
            GalaxyMapView& view,
            const Viewport& viewport,
            double localMouseX,
            double localMouseY
        ) const;

        int pickSystem(
            const GalaxyMapView& view,
            const Viewport& viewport,
            const world::celestial::GalaxyMapSnapshot& galaxy,
            double localMouseX,
            double localMouseY
        ) const;

        glm::vec3 nearestVisibleStarToScreenPoint(
            const GalaxyMapView& view,
            const Viewport& viewport,
            const world::celestial::GalaxyMapSnapshot& galaxy,
            double localMouseX,
            double localMouseY,
            float maxRadiusPx,
            bool& found
        ) const;
    };
}
