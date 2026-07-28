#pragma once

#include <optional>

#include <glm/glm.hpp>

#include "src/game/navigation/GalaxyNavigationGrid.h"
#include "src/render/types/Viewport.h"

namespace world::celestial
{
    struct GalaxyMapSnapshot;
}

namespace game::system_map
{
    class GalaxyMapView;

    enum class GalaxyMapNavigationEventType
    {
        GalaxyLevelChanged,
        EnterSystemMap
    };

    struct GalaxyMapNavigationEvent
    {
        GalaxyMapNavigationEventType type =
            GalaxyMapNavigationEventType::GalaxyLevelChanged;

        int galaxyLevel = 0;
    };

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
        std::optional<GalaxyMapNavigationEvent> navigationEvent;
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
