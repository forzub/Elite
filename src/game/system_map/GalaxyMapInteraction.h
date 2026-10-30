#pragma once

#include <optional>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/GalaxyNavigationGrid.h"
#include "src/game/system_map/GalaxyPointerResolver.h"
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

        // Presentation owned by SystemMapRenderer that the ONE pointer
        // resolver must see: pointer-interactive overlay glyphs and whether
        // a UI panel (info card) is under the cursor.
        std::vector<GalaxyPointerOverlayGlyph> overlayGlyphs;
        bool panelUnderCursor = false;
        // The caller already resolved this frame's press (resolvePressTarget).
        bool pressTargetResolved = false;
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

        // Resolve the semantic target under a left press from what the
        // last frame presented; stored as the gesture's press target. The
        // waypoint glyph of a cube that completes the cube's double click
        // resolves to that cube.
        GalaxyPointerTarget resolvePressTarget(
            GalaxyMapView& view,
            const world::celestial::GalaxyMapSnapshot& galaxy,
            const GalaxyMapInputFrame& frame
        ) const;

        // DEV trace of a press the overlay (card / glyph) owns.
        void traceOverlayOwnedPress(
            const GalaxyMapView& view,
            const world::celestial::GalaxyMapSnapshot& galaxy,
            const GalaxyMapInputFrame& frame
        ) const
        {
            tracePointerSequence(view, galaxy, frame, "overlay", false);
        }

    private:
        GalaxyPointerScene pointerScene(
            const GalaxyMapView& view,
            const world::celestial::GalaxyMapSnapshot& galaxy,
            const GalaxyMapInputFrame& frame
        ) const;

        // Click dispatch on the ONE resolved target; true = double click.
        bool applyClick(
            GalaxyMapView& view,
            const world::celestial::GalaxyMapSnapshot& galaxy,
            const Viewport& viewport,
            const GalaxyPointerTarget& target,
            double pressX,
            double pressY,
            double pressTimeSeconds,
            GalaxyMapInputResult& result
        ) const;

        bool applyCubeClick(
            GalaxyMapView& view,
            const world::celestial::GalaxyMapSnapshot& galaxy,
            const Viewport& viewport,
            const navigation::GalaxyNavigationCell& pickedCell,
            double localMouseX,
            double localMouseY,
            double nowSeconds,
            GalaxyMapInputResult& result
        ) const;

        // Cloud / icy field: single click selects (card), double click flies
        // to the navigation cube containing the clicked point.
        bool applyMediumClick(
            GalaxyMapView& view,
            const Viewport& viewport,
            int object,
            const glm::dvec3& hitLy,
            double localMouseX,
            double localMouseY,
            double nowSeconds
        ) const;

        void updateNavigationHoverFromCursor(
            GalaxyMapView& view,
            const Viewport& viewport,
            double localMouseX,
            double localMouseY
        ) const;

        // DEV (ELITE_GALAXY_PICK_TRACE=1): one CSV row per pointer sequence
        // in D:/tmp/elite_galaxy_pick_trace.csv; no cost when off.
        void tracePointerSequence(
            const GalaxyMapView& view,
            const world::celestial::GalaxyMapSnapshot& galaxy,
            const GalaxyMapInputFrame& frame,
            const char* gesture,
            bool doubleClick
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
