#pragma once

#include <optional>
#include <string>

#include <glm/glm.hpp>

#include "src/game/navigation/CubicNavigationGrid.h"
#include "src/render/types/Viewport.h"

namespace game::system_map
{
    class SystemMapView;
    struct SystemPointerTarget;

    struct SystemMapHubSelection
    {
        std::string hubId;
        std::string parentBodyId;
    };

    struct SystemMapNearObject
    {
        std::string bodyId;
        std::string hubId;
        std::string hubParentBodyId;
        glm::dvec3 absolutePosition {0.0};
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
        // Single click on a body: selected (with its cube), no card.
        std::optional<std::string> selectedBodyId;
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

        // The body / hub nearest the point (its disk within radiusPx): the
        // object "near the mouse" a rotation turns about.
        virtual std::optional<SystemMapNearObject> nearestSystemObject(
            double localMouseX,
            double localMouseY,
            float radiusPx
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

        // The ONE resolver (hub > body > selected marker > layer child) at
        // a viewport point: the context menu asks the same question.
        SystemPointerTarget resolvePointer(
            const SystemMapView& view,
            const SystemMapInteractionContext& context,
            const Viewport& viewport,
            double localX,
            double localY
        ) const;

        // Single-click selection of a target (no click tracking).
        void selectTarget(
            SystemMapView& view,
            const SystemMapInteractionContext& context,
            const SystemPointerTarget& target,
            SystemMapInputResult& result
        ) const;

        // The double-click action: a body / hub is approached (centred,
        // its cube entered, the camera follows a body); a cube is entered.
        bool descendTarget(
            SystemMapView& view,
            const SystemMapInteractionContext& context,
            const Viewport& viewport,
            const SystemPointerTarget& target,
            double nowSeconds,
            SystemMapInputResult& result
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
        void applyClick(
            SystemMapView& view,
            const SystemMapInteractionContext& context,
            const Viewport& viewport,
            const SystemPointerTarget& target,
            double x,
            double y,
            double nowSeconds,
            SystemMapInputResult& result
        ) const;

        void updatePointerHover(
            SystemMapView& view,
            const SystemMapInteractionContext& context,
            const SystemMapInputFrame& frame
        ) const;

        void applyWheelLevels(
            SystemMapView& view,
            const SystemMapInputFrame& frame,
            bool zoomIn,
            SystemMapInputResult& result
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
