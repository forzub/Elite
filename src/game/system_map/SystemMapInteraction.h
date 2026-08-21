#pragma once

#include <optional>
#include <string>

#include <glm/glm.hpp>

#include "src/game/navigation/CubicNavigationGrid.h"
#include "src/render/types/Viewport.h"

namespace game::system_map
{
    class SystemMapView;

    struct SystemMapHubSelection
    {
        std::string hubId;
        std::string parentBodyId;
    };

    struct SystemMapCameraBodyTarget
    {
        std::string bodyId;
        glm::dvec3 absolutePosition {0.0};
        double physicalRadiusWorld = 0.0;
    };

    struct SystemMapInputFrame
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

        double nowSeconds = 0.0;
    };

    struct SystemMapInputResult
    {
        std::optional<int> systemLevelChanged;
        std::optional<std::string> clickedBodyId;
        std::optional<navigation::CubicNavigationCell> clickedNavigationCell;
    };

    /*
        Semantic hit-test contract over one prepared System presentation
        frame. SystemMapInteraction receives only picks and authoritative
        absolute positions; it does not see OpenGL resources or render batches.
    */
    class SystemMapInteractionContext
    {
    public:
        virtual ~SystemMapInteractionContext() = default;

        virtual std::optional<std::string> pickSystemBodyId(
            double localMouseX,
            double localMouseY
        ) const = 0;

        virtual std::optional<SystemMapHubSelection> pickSystemHubSelection(
            double localMouseX,
            double localMouseY
        ) const = 0;

        virtual std::optional<SystemMapCameraBodyTarget>
        pickSystemCameraBodyTarget(
            double localMouseX,
            double localMouseY,
            const Viewport& viewport
        ) const = 0;

        virtual std::optional<glm::dvec3> systemBodyAbsolutePosition(
            const std::string& bodyId
        ) const = 0;

        virtual std::optional<glm::dvec3> systemObjectAbsolutePosition(
            const std::string& objectId
        ) const = 0;
    };

    /*
        System-specific input, picking coordination and camera navigation.

        The class owns no persistent state and no rendering resources. All
        mutable map state lives in SystemMapView. A concrete frame adapter
        supplies hit-test results from the same presentation used for render.
    */
    class SystemMapInteraction
    {
    public:
        SystemMapInputResult handleInput(
            SystemMapView& view,
            const SystemMapInteractionContext& context,
            const SystemMapInputFrame& frame,
            double& pendingScrollY
        ) const;

        // Tactical object overlays use the same canonical Hub-selection path
        // as ordinary System-map picking so opening an info card cannot break
        // the existing Details/Hub navigation contract.
        void focusHubSelection(
            SystemMapView& view,
            const SystemMapInteractionContext& context,
            const SystemMapHubSelection& hub,
            double nowSeconds
        ) const
        {
            focusHub(view, context, hub, nowSeconds);
        }

        void focusBodySelection(
            SystemMapView& view,
            const SystemMapInteractionContext& context,
            const std::string& bodyId,
            double nowSeconds
        ) const
        {
            focusBody(view, context, bodyId, nowSeconds);
        }

        // Ships/infrastructure remain tactical selections.  Activating one
        // clears body/cube focus without moving the camera, but may preserve a
        // parent Hub as the semantic local-neighborhood target for the HUB
        // drill button.
        void focusTacticalObjectSelection(
            SystemMapView& view,
            const std::string& navigationHubId = {},
            const std::string& navigationHubParentBodyId = {}
        ) const;

    private:
        void updateNavigationHoverFromCursor(
            SystemMapView& view,
            const Viewport& viewport,
            double localMouseX,
            double localMouseY
        ) const;

        bool pickNavigationCell(
            const SystemMapView& view,
            const Viewport& viewport,
            double localMouseX,
            double localMouseY,
            navigation::CubicNavigationCell& outCell
        ) const;

        void focusBody(
            SystemMapView& view,
            const SystemMapInteractionContext& context,
            const std::string& bodyId,
            double nowSeconds
        ) const;

        void focusHub(
            SystemMapView& view,
            const SystemMapInteractionContext& context,
            const SystemMapHubSelection& hub,
            double nowSeconds
        ) const;
    };
}
