#pragma once

#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/CubicNavigationCameraFlight.h"
#include "src/game/navigation/CubicNavigationInteraction.h"
#include "src/game/navigation/GalaxyNavigationGrid.h"
#include "src/game/system_map/GalaxyMapVisualSettings.h"
#include "src/game/system_map/MapCameraState.h"
#include "src/game/system_map/MapIntent.h"
#include "src/render/types/Viewport.h"

namespace world::celestial
{
    struct GalaxyMapSnapshot;
    struct PlayerNavigationState;
}

namespace game::system_map
{
    struct GalaxyMapControlSettings
    {
        float rotateSensitivity = 0.008f;
        float pitchLimitRad = 1.52f;

        float panScaleByDistance = 0.0018f;

        float zoomInFactor = 0.935f;
        float zoomOutFactor = 1.07f;

        // Needed for the terminal ~260 AU Galaxy cell.
        float minDistance = 0.002f;
        float maxDistance = 700.0f;

        float pivotPickRadiusPx = 36.0f;
        float rotationMaxStepRad = 0.10f;
        float systemPickRadiusPx = 32.0f;

        float navigationCellInteractiveViewportFraction = 0.075f;
        float navigationCellInteractiveMinPx = 56.0f;

        float navigationHoverFadeInSeconds = 0.18f;
        float navigationHoverFadeOutSeconds = 0.14f;

        double cubeDoubleClickMaxIntervalSeconds = 0.38;
        double cubeDoubleClickMaxDistancePx = 12.0;
        double clickMoveThresholdPx = 5.0;
    };

    struct GalaxyMapScreenPoint
    {
        int systemId = -1;
        std::string name;
        glm::vec3 world {0.0f};
        glm::vec2 screen {0.0f};
        float depth = 0.0f;
        bool visible = false;
    };


    struct GalaxyMapEntryState
    {
        bool valid = false;
        int systemId = -1;
        navigation::GalaxyGridIndex terminalCell {};
        glm::dvec3 positionLy {0.0};
    };

    struct GalaxyMapViewState
    {
        GalaxyCameraState camera;
        navigation::CubicNavigationCameraFlight cameraFlight;
        navigation::GalaxyNavigationGrid navigationGrid;

        // Exact user-selected physical point. This is separate from camera target.
        glm::dvec3 navigationFocusLy {0.0};
        bool navigationFocusValid = false;

        std::optional<navigation::GalaxyNavigationCell> hoverVisualCell;
        float hoverVisualAlpha = 0.0f;

        std::optional<navigation::GalaxyNavigationCell> hoverOutgoingCell;
        float hoverOutgoingAlpha = 0.0f;
        double hoverVisualLastTimeSeconds = 0.0;

        navigation::CubicNavigationClickTracker<
            navigation::GalaxyGridIndex
        > cubeClickTracker;

        GalaxyMapEntryState entry;
        int selectedSystemId = -1;
        int focusedSystemId = -1;

        std::vector<GalaxyMapScreenPoint> screenPoints;

        glm::vec3 orbitPivotWorld {0.0f, 0.0f, 0.0f};
        bool orbitPivotActive = false;

        double mouseDownX = 0.0;
        double mouseDownY = 0.0;
    };

    /*
        Galaxy-specific persistent state and camera/navigation math.

        OpenGL resources and primitive drawing remain in SystemMapRenderer.
        This keeps one shared render-resource owner while removing Galaxy state
        from the old all-maps renderer. Later phases can move interaction and
        draw passes behind this boundary without changing saved map behaviour.
    */
    class GalaxyMapView
    {
    public:
        static constexpr float RenderUnitsPerLightYear = 2.2f;

        GalaxyMapView();

        GalaxyMapViewState& state() noexcept
        {
            return m_state;
        }

        const GalaxyMapViewState& state() const noexcept
        {
            return m_state;
        }

        GalaxyMapControlSettings& controls() noexcept
        {
            return m_controls;
        }

        const GalaxyMapControlSettings& controls() const noexcept
        {
            return m_controls;
        }

        GalaxyMapVisualSettings& visuals() noexcept
        {
            return m_visuals;
        }

        const GalaxyMapVisualSettings& visuals() const noexcept
        {
            return m_visuals;
        }

        void reset();

        void onEntered(
            const world::celestial::GalaxyMapSnapshot& galaxy,
            const world::celestial::PlayerNavigationState& playerNavigation
        );

        void synchronizeCatalogRoots(
            const world::celestial::GalaxyMapSnapshot& galaxy
        );

        void resetNavigationToEntry();

        glm::dvec3 playerPositionLy(
            const world::celestial::GalaxyMapSnapshot& galaxy,
            const world::celestial::PlayerNavigationState& playerNavigation,
            bool& outInsideKnownSystem
        ) const;

        glm::vec3 positionLyToRender(
            const glm::dvec3& positionLy
        ) const;

        glm::vec3 vectorLyToRender(
            const glm::dvec3& vectorLy
        ) const;

        glm::dvec3 renderToPositionLy(
            const glm::vec3& renderPosition
        ) const;

        glm::mat4 viewMatrix() const;
        glm::mat4 projectionMatrix(const Viewport& viewport) const;

        float navigationAnchorDiameterPx(
            const Viewport& viewport
        ) const;

        bool navigationCellsInteractive(
            const Viewport& viewport
        ) const;

        void syncNavigationAnchorToCameraTarget();

        void beginCameraFlight(
            const glm::vec3& destinationTarget,
            float destinationDistance,
            double nowSeconds
        );

        void updateCameraFlight(double nowSeconds);
        void cancelCameraFlight(bool snapToDestination);

        MapIntent entryIntentForPosition(
            const world::celestial::GalaxyMapSnapshot& galaxy,
            const glm::dvec3& positionLy,
            int explicitSystemId = -1
        ) const;

        bool focusSystem(
            int systemId,
            const world::celestial::GalaxyMapSnapshot& galaxy,
            bool animateCamera,
            double nowSeconds
        );

    private:
        GalaxyMapViewState m_state;
        GalaxyMapControlSettings m_controls;
        GalaxyMapVisualSettings m_visuals;
    };
}
