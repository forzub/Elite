#pragma once

#include "src/game/presentation/PointerClickTracker.h"
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "render/types/Viewport.h"

#include "src/game/navigation/CubicNavigationInteraction.h"
#include "src/game/navigation/SystemNavigationGrid.h"
#include "src/game/system_map/MapCameraSnapshot.h"
#include "src/game/system_map/GalaxyHierarchy.h"
#include "src/game/system_map/GalaxyMapVisualSettings.h"
#include "src/game/system_map/GalaxyPointerResolver.h"
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
        // An object this close to the press / cursor (disk distance) is
        // what a rotation / zoom turns about.
        float pivotPickRadiusPx = 36.0f;

        float navigationCellInteractiveViewportFraction = 0.075f;
        float navigationCellInteractiveMinPx = 56.0f;

        float navigationHoverFadeInSeconds = 0.18f;
        float navigationHoverFadeOutSeconds = 0.14f;

        /*
            At maximum zoom-out the single S0 root must not become smaller
            than this fraction of the shorter viewport side.
        */
        float navigationParentMinViewportFraction = 0.60f;

        double cubeDoubleClickMaxIntervalSeconds =
            game::presentation::PointerInteractionSettings {}.doubleClickMaxIntervalSeconds;
        double cubeDoubleClickMaxDistancePx =
            game::presentation::PointerInteractionSettings {}.doubleClickMaxDistancePx;

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
        One semantic pointer target of the System map (the same contract as
        the Galaxy resolver): a hub, a body, a child cube of the current
        parent (selected-cube marker or the hover-layer cube) or nothing.
        pointAu: the object / layer point in the system grid (AU).
    */
    struct SystemPointerTarget
    {
        enum class Kind { Empty, Hub, Body, Cube };
        Kind kind = Kind::Empty;
        std::string bodyId;
        std::string hubId;
        std::string hubParentBodyId;
        std::optional<navigation::CubicNavigationCell> cell;
        glm::dvec3 pointAu {0.0};
        bool onLayer = false;
    };

    /*
        A saved System map view (map back history): the frame, the camera
        and the selection, restored when the user returns to this map.
    */
    struct SystemMapBookmark
    {
        int systemId = -1;
        GalaxyHierarchyParent parent {0, {}};
        SystemCameraState camera;
        std::string selectedBodyId;
        std::string selectedHubId;
        std::string selectedHubParentBodyId;
        std::optional<navigation::CubicNavigationCell> selectedCell;
        bool cellExplicitlySelected = false;
        std::string cameraFollowBodyId;
        bool tracking = false;
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

        /*
            Camera focus object: the body the user zoomed / rotated about or
            double-clicked. The camera moves with it (like TRACK) so it stays
            in view; the current parent moves sideways when it leaves it.
            Cleared by a pan or by framing a cube.
        */
        std::string cameraFollowBodyId;
        glm::dvec3 cameraFollowLastAbsolute {0.0, 0.0, 0.0};
        bool cameraFollowPositionValid = false;

        // Back history: applied once the system is presented (builder).
        std::optional<SystemMapBookmark> pendingBookmark;

        /*
            Spatial frame - the SAME model as the Galaxy Map
            (GalaxyHierarchy.h): the user is inside one current parent cube
            (level 0 = the S0 root, the top); its children are the working
            cubes; double click / the wheel enter a child, up / the wheel
            leave; nothing outside the parent is drawn or a target.
        */
        GalaxyHierarchyParent hierarchyParent {0, {}};
        int hierarchyRevision = 0;
        GalaxyHierarchyFaces hierarchyFaces;
        int hoverLayerAxis = -1;
        GalaxyPointerGesture pointerGesture;
        SystemPointerTarget pressTarget;
        SystemPointerTarget hoverTarget;
        bool hoverTargetValid = false;
        double hoverMouseX = -1.0e9;
        double hoverMouseY = -1.0e9;
        SystemCameraState hoverCamera;
        int hoverHierarchyRevision = -1;
        game::presentation::PointerClickTracker<std::string> bodyClickTracker;
        // Markers the last frame presented (selected cube only).
        std::vector<GalaxyNavigationMarker> presentedCubeMarkers;
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


        void updateNavigationHoverPresentation(
            const Viewport& viewport,
            double nowSeconds
        );



        std::optional<navigation::CubicNavigationCell>
        resolvedTerminalSelection() const;

        float navigationMaximumCameraDistance(
            const Viewport& viewport
        ) const;


        void constrainCameraToNavigationBoundary(
            const Viewport& viewport
        );

        // ---- spatial frame (current parent cube), AU -------------------
        static constexpr float ParentFillRatio = 0.98f;
        static constexpr float ParentHandOverUpRatio = 0.94f;

        GalaxyHierarchyBox hierarchyBox() const;
        std::optional<navigation::CubicNavigationCell> hierarchyChildAt(
            const glm::dvec3& positionAu
        ) const;
        bool hierarchyContains(const glm::dvec3& positionAu) const;

        // Projected size of a cube (AU) for a camera target (world) and
        // half-height: extent ratio, or with fitInside the largest |NDC|.
        float cubeScreenRatioAt(
            const glm::dvec3& centerAu,
            double edgeAu,
            const glm::dvec3& targetWorld,
            float halfHeight,
            const Viewport& viewport,
            bool fitInside = false
        ) const;
        float cubeScreenRatio(
            const glm::dvec3& centerAu,
            double edgeAu,
            const Viewport& viewport
        ) const;
        // Apparent size of a cube edge at the current zoom, wherever the
        // cube is (as if centred on the camera target): the hierarchy
        // hand-over measure, independent of where the user looks.
        float cubeSizeRatio(
            double edgeAu,
            const Viewport& viewport
        ) const;
        float fillHalfHeightAt(
            const glm::dvec3& centerAu,
            double edgeAu,
            const glm::dvec3& targetWorld,
            const Viewport& viewport,
            float fill
        ) const;
        // The camera half-height at which the parent fills the screen.
        float hierarchyFitHalfHeight(const Viewport& viewport) const;
        void hierarchyHalfHeightLimits(
            const Viewport& viewport,
            float& minimumHalfHeight,
            float& maximumHalfHeight
        ) const;

        void frameHierarchyParent(
            const Viewport& viewport,
            double nowSeconds,
            bool animate
        );
        bool enterHierarchyCell(
            const navigation::CubicNavigationCell& cell,
            const Viewport& viewport,
            double nowSeconds,
            bool frameCamera = true
        );
        bool ascendHierarchy(
            const Viewport& viewport,
            double nowSeconds
        );
        bool jumpToHierarchyLevel(
            int level,
            const Viewport& viewport,
            double nowSeconds,
            bool frameCamera = true
        );
        void resetHierarchyToRoot(const Viewport& viewport);
        // Fly to a point (AU) of the current parent: centred on it, the
        // parent's size filling the screen (an object double click).
        // halfHeight > 0 overrides the size (a terminal cube: closer).
        void approachHierarchyPoint(
            const glm::dvec3& pointAu,
            const Viewport& viewport,
            double nowSeconds,
            float halfHeight = -1.0f
        );
        // The parent moves sideways (same level) to the cube holding the
        // point; the camera does not move.
        bool shiftHierarchyParentTo(const glm::dvec3& pointAu);

        void followBody(const std::string& bodyId);
        void clearFollow();

        SystemMapBookmark bookmark(int systemId) const;
        void restoreBookmark(const SystemMapBookmark& bookmark);

    private:
        void setHierarchyParent(const GalaxyHierarchyParent& parent);

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
