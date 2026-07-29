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
    };

    /*
        Frame-local hit-test data owned by the System renderer.

        SystemMapInteraction receives only semantic picks and authoritative
        absolute positions. It does not see OpenGL resources, render batches
        or the renderer's private screen-point containers.
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

        virtual std::optional<std::string> pickSystemOrbitPivotBodyId(
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
        mutable map state lives in SystemMapView; the renderer only supplies
        hit-test results from the most recent presentation frame.
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
