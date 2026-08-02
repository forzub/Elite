#pragma once

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <optional>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "render/types/Viewport.h"

#include "src/game/navigation/CubicNavigationInteraction.h"
#include "src/game/navigation/SystemNavigationGrid.h"
#include "src/game/system_map/MapCameraSnapshot.h"
#include "src/game/system_map/MapCameraState.h"
#include "src/game/system_map/SystemMapVisualSettings.h"

namespace game::system_map
{
    /*
        Input and navigation tuning owned by the System map view.

        These values describe presentation and interaction only. They are
        deliberately separate from server simulation and celestial data.
    */
    struct SystemMapControlSettings
    {
        /*
            The outermost known body is normalized to this map radius.
            A newly opened system uses initialFitPadding around that radius,
            so the system is readable instead of appearing as a point.
        */
        float fittedSystemRadiusWorld = 70.0f;
        float initialFitPadding = 1.35f;

        /*
            Ring-and-cross markers are reserved for stars and planets.
            Sub-pixel moons use a compact filled point proxy instead; this
            preserves the satellite hierarchy without recreating reticle
            clutter around giant planets.
        */
        float proxyFadeStartRadiusRatio = 0.62f;

        float minPhysicalBodyRadiusPx = 0.70f;
        float starMarkerRadiusPx = 3.0f;
        float planetMarkerRadiusPx = 3.5f;
        float moonPointProxyRadiusPx = 2.25f;
        float moonPointProxyFadeEndRadiusPx = 2.75f;
        float pickMinBodyRadiusPx = 6.0f;

        float rotateSensitivity = 0.0055f;
        float pitchLimitRad = 1.32f;

        float cameraBodyAnchorMaxDistancePx = 28.0f;
        float rotationMaxStepRad = 0.10f;

        float zoomInFactor = 0.935f;
        float zoomOutFactor = 1.07f;
        float bodyZoomClearanceScale = 1.12f;
        double clickMoveThresholdPx = 5.0;

        float navigationCellInteractiveViewportFraction = 0.075f;
        float navigationCellInteractiveMinPx = 56.0f;
        float navigationCellPickRadiusPx = 18.0f;

        float navigationHoverFadeInSeconds = 0.18f;
        float navigationHoverFadeOutSeconds = 0.14f;

        /*
            At maximum zoom-out the single S0 root must not become smaller
            than this fraction of the shorter viewport side.
        */
        float navigationParentMinViewportFraction = 0.60f;

        double cubeDoubleClickMaxIntervalSeconds = 0.38;
        double cubeDoubleClickMaxDistancePx = 12.0;

        /*
            System navigation stops at S5 (about 2500 km).
            2.5 km/px keeps that terminal cube inspectable at roughly
            1000 px. Smaller scales belong to Details.
        */
        double minKmPerPixel = 2.5;

        float pickMaxBodyRadiusPx = 8000.0f;
        float pickHaloBasePx = 48.0f;
        float pickHaloRadiusFactor = 0.08f;
        float pickHaloMaxPx = 220.0f;
        float pickScoreDiskWeight = 10000.0f;

        int sparsePivotMaxVisibleBodies = 2;
        float sparsePivotViewportPaddingPx = 96.0f;
    };

    /*
        Mutable state belonging exclusively to the System map mode.

        Input is coordinated by SystemMapInteraction and draw orchestration
        by SystemMapSceneRenderer. SystemMapRenderer owns only shared rendering
        resources and the frame-local pick cache.
    */
    struct SystemMapViewState
    {
        SystemCameraFlightState cameraFlight;
        SystemCameraState camera;

        navigation::SystemNavigationGrid navigationGrid;

        std::optional<navigation::CubicNavigationCell>
            hoverVisualCell;

        float hoverVisualAlpha = 0.0f;

        std::optional<navigation::CubicNavigationCell>
            hoverOutgoingCell;

        float hoverOutgoingAlpha = 0.0f;
        double hoverVisualLastTimeSeconds = 0.0;

        navigation::CubicNavigationClickTracker<
                navigation::CubicGridIndex
            > cubeClickTracker;

        int lastCameraFitSystemId = -1;

        std::string selectedBodyId;
        std::string selectedHubId;
        std::string selectedHubParentBodyId;

        /*
            CubicNavigationGrid owns a selected root cell immediately after
            reset so it can render a stable navigation frame. That implicit
            selection is not a user command and must not enable Details.
        */
        bool navigationCellExplicitlySelected = false;

        float lastScale = 1.0f;

        int presentationSystemId = -1;
        double presentationSourceTimeSeconds = 0.0;
        double presentationWallTimeSeconds = 0.0;
        double presentationTimeScale = 1.0;

        glm::dvec3 orbitPivotAbsolute {0.0, 0.0, 0.0};
        bool orbitPivotActive = false;

        /*
            TRACK keeps the camera in the selected body's moving reference
            frame. The camera target is translated by the body's presentation
            delta every frame, so the user may still orbit, pan and zoom while
            following it.
        */
        bool selectedBodyTrackingEnabled = false;
        std::string trackedBodyId;
        glm::dvec3 trackedBodyLastAbsolute {0.0, 0.0, 0.0};
        bool trackedBodyPositionValid = false;
    };

    class SystemMapView
    {
    public:
        /*
            The camera distance keeps the historical "visible half-height"
            semantic even though the map uses a perspective projection.
        */
        static constexpr float minimumCameraHalfHeight =
            0.0000005f;

        static constexpr float maximumCameraHalfHeight =
            5000.0f;

        SystemMapViewState& state() noexcept;

        const SystemMapViewState& state() const noexcept;

        SystemMapControlSettings& controls() noexcept;

        const SystemMapControlSettings& controls() const noexcept;

        SystemMapVisualSettings& visuals() noexcept;

        const SystemMapVisualSettings& visuals() const noexcept;

        void reset();

        void resetNavigationToLevelZero(
            const Viewport& viewport
        );

        void suppressCameraGesture(
            bool leftDown,
            bool rightDown,
            double mouseX,
            double mouseY
        );

        SystemMapCameraSnapshot cameraSnapshot(
            const Viewport& viewport
        ) const;

        glm::mat4 viewMatrix() const;

        glm::mat4 projectionMatrix(
            const Viewport& viewport
        ) const;

        glm::dvec3 cameraDirectionWorld() const;

        glm::dvec3 cameraUpWorld() const;

        glm::dvec3 cameraRightWorld() const;

        double cameraEyeDistance() const;

        glm::dvec3 cameraEyeAbsolute() const;

        double targetPlaneWorldUnitsPerPixel(
            const Viewport& viewport
        ) const;

        glm::vec2 projectAbsoluteToScreen(
            const Viewport& viewport,
            const glm::dvec3& absolutePosition,
            bool& visible,
            float& depth
        ) const;

        glm::dvec3 targetPlanePointFromScreen(
            const Viewport& viewport,
            double localMouseX,
            double localMouseY
        ) const;

        void panCameraByScreenDelta(
            const Viewport& viewport,
            double deltaX,
            double deltaY
        );

        void orbitCameraAroundPivot(
            const glm::dvec3& pivotAbsolute,
            float yawDelta,
            float pitchDelta,
            float pitchLimitRad
        );

        void zoomCameraAroundPivot(
            const glm::dvec3& pivotAbsolute,
            float zoomFactor,
            float minimumDistance,
            float maximumDistance,
            double minimumEyeDistanceFromPivot = 0.0
        );

        void beginCameraFlight(
            const glm::dvec3& destinationTarget,
            float destinationDistance,
            double nowSeconds
        );

        void updateCameraFlight(
            double nowSeconds
        );

        void cancelCameraFlight(
            bool snapToDestination
        );

        float navigationAnchorDiameterPx(
            const Viewport& viewport
        ) const;

        bool navigationCellsInteractive(
            const Viewport& viewport
        ) const;

        void updateNavigationHoverPresentation(
            const Viewport& viewport,
            double nowSeconds
        );

        glm::dvec3 navigationCursorAu() const;

        void syncNavigationAnchorToCursor();

        std::optional<navigation::CubicNavigationCell>
        resolvedTerminalSelection() const;

        float navigationMaximumCameraDistance(
            const Viewport& viewport
        ) const;

        glm::dvec3 navigationBoundaryCenterWorld() const;

        void constrainCameraToNavigationBoundary(
            const Viewport& viewport
        );

    private:
        static float wrapAngleRad(
            float angle
        );

        static constexpr float depthMultiplier =
            8.0f;

        static float depthHalfRange(
            float cameraHalfHeight
        );

        static float nearPlane(
            float cameraHalfHeight
        );

        static float perspectiveEyeDistance(
            float cameraHalfHeight,
            float fieldOfViewDeg
        );

        static float farPlane(
            float cameraHalfHeight,
            float fieldOfViewDeg
        );

    private:
        SystemMapViewState m_state;
        SystemMapControlSettings m_controls;
        SystemMapVisualSettings m_visuals;
    };
}
