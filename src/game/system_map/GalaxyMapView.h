#pragma once

#include "src/game/presentation/PointerClickTracker.h"
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/CubicNavigationCameraFlight.h"
#include "src/game/navigation/CubicNavigationInteraction.h"
#include "src/game/navigation/GalaxyNavigationGrid.h"
#include "src/game/system_map/GalaxyHierarchy.h"
#include "src/game/system_map/GalaxyMapVisualSettings.h"
#include "src/game/system_map/GalaxyPointerResolver.h"
#include "src/game/system_map/MapCameraSnapshot.h"
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

        // Needed for the terminal ~260 AU Galaxy cell. The upper bound only
        // guards the overview fit; the wheel range is the current parent's
        // (GalaxyMapView::cameraDistanceLimits).
        float minDistance = 0.0002f;
        float maxDistance = 20000.0f;

        // Wheel range relative to the fitted current parent (the level
        // hand-over happens inside it: a child filling the screen becomes
        // the parent, a parent smaller than the screen hands over up).
        float parentZoomInFraction = 0.02f;
        float parentZoomOutFactor = 1.6f;
        // Hand-over hysteresis: up below this screen ratio of the parent.
        float parentHandOverUpRatio = 0.94f;

        float pivotPickRadiusPx = 36.0f;
        float rotationMaxStepRad = 0.10f;
        float systemPickRadiusPx = 32.0f;
        // Medium LOD: a cloud projected smaller than this is not a target.
        float mediumPickMinRadiusPx = 4.0f;

        float navigationHoverFadeInSeconds = 0.18f;
        float navigationHoverFadeOutSeconds = 0.14f;

        double cubeDoubleClickMaxIntervalSeconds =
            game::presentation::PointerInteractionSettings {}.doubleClickMaxIntervalSeconds;
        double cubeDoubleClickMaxDistancePx =
            game::presentation::PointerInteractionSettings {}.doubleClickMaxDistancePx;
        double clickMoveThresholdPx = 5.0;
    };


    struct GalaxyMapEntryState
    {
        bool valid = false;
        int systemId = -1;
        navigation::GalaxyGridIndex terminalCell {};
        glm::dvec3 positionLy {0.0};
    };

    /*
        Alt + click: the compact "What is here?" list of every semantic
        target the ONE resolver found under the cursor (star, cloud, cube).
        A row click is a normal single click on that target.
    */
    struct GalaxyWhatIsHereMenu
    {
        bool open = false;
        glm::vec2 originPx {0.0f};
        std::vector<GalaxyPointerTarget> targets;
        std::vector<std::string> labels;
        int hovered = -1;

        static constexpr float WidthPx = 300.0f;
        static constexpr float RowHeightPx = 21.0f;
        static constexpr float HeaderHeightPx = 24.0f;

        // Row i bounds (left, top, right, bottom) in map viewport pixels.
        glm::vec4 rowBounds(int i) const
        {
            const float top = originPx.y + HeaderHeightPx + RowHeightPx * static_cast<float>(i);
            return glm::vec4(originPx.x, top, originPx.x + WidthPx, top + RowHeightPx);
        }
        glm::vec4 bounds() const
        {
            return glm::vec4(originPx.x, originPx.y, originPx.x + WidthPx,
                             originPx.y + HeaderHeightPx + RowHeightPx * static_cast<float>(labels.size()) + 4.0f);
        }
        int rowAt(double x, double y) const
        {
            for (int i = 0; i < static_cast<int>(labels.size()); ++i)
            {
                const glm::vec4 b = rowBounds(i);
                if (x >= b.x && x <= b.z && y >= b.y && y < b.w)
                    return i;
            }
            return -1;
        }
    };

    /*
        A saved Galaxy Map view (map back history): the frame, the camera
        and the selection, restored when the user returns to this map.
    */
    struct GalaxyMapBookmark
    {
        GalaxyHierarchyParent parent;
        GalaxyCameraState camera;
        std::optional<navigation::GalaxyNavigationCell> selectedCell;
        int selectedSystemId = -1;
        int focusedSystemId = -1;
        glm::dvec3 navigationFocusLy {0.0};
        bool navigationFocusValid = false;
        int selectedMediumObject = -1;
        glm::dvec3 selectedMediumHitLy {0.0};
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

        /*
            Spatial frame (GalaxyHierarchy.h): the current parent cube. The
            grid's anchor level is always its child level. It changes only
            by an explicit command (double click down, up / breadcrumb,
            LEVEL 0, object double click), never with the camera.
        */
        GalaxyHierarchyParent hierarchyParent;
        int hierarchyRevision = 0;
        GalaxyHierarchyFaces hierarchyFaces;
        int hoverLayerAxis = -1;
        // The parent the camera flies away from (drawn while flying).
        std::optional<GalaxyHierarchyBox> flightContextBox;
        // The running flight frames a (new) current parent.
        bool nestedFrameFlight = false;
        // Roots the galaxy overview frames: those holding star systems and
        // the player's (synchronizeCatalogRoots / onEntered).
        std::vector<navigation::GalaxyGridIndex> overviewRoots;
        // The last viewport the map was drawn / driven with (camera fit).
        Viewport lastViewport {0, 0, 1280, 720};

        // Interstellar medium structures / icy fields (GalaxyMapSnapshot
        // object indices): hover and selection are separate states.
        int hoveredMediumObject = -1;
        int selectedMediumObject = -1;
        glm::dvec3 selectedMediumHitLy {0.0};
        game::presentation::PointerClickTracker<int> mediumClickTracker;
        game::presentation::PointerClickTracker<int> starClickTracker;

        /*
            Pointer hover: the ONE resolver's winner under the cursor and its
            semantic point; re-resolved when the cursor, the camera or the
            frame changes. The hovered child cube is derived from it.
        */
        GalaxyPointerTarget hoverTarget;
        bool hoverTargetValid = false;
        GalaxyWhatIsHereMenu whatIsHere;
        double hoverMouseX = -1.0e9;
        double hoverMouseY = -1.0e9;
        GalaxyCameraState hoverCamera;
        int hoverHierarchyRevision = -1;

        // What the last rendered frame presented (GalaxyMapRenderer): the
        // pointer resolver hits exactly these, never a re-derived geometry.
        std::vector<GalaxyPointerStar> presentedStars;
        std::vector<GalaxyNavigationMarker> presentedCubeMarkers;

        // Left-button gesture (press -> pending click -> click | drag) and
        // the semantic target resolved at the press.
        GalaxyPointerGesture pointerGesture;
        GalaxyPointerTarget pressTarget;
        GalaxyPointerResolution pressResolution; // candidates: trace only
        GalaxyCameraState pressCamera;           // trace: camera changed?

        glm::vec3 orbitPivotWorld {0.0f, 0.0f, 0.0f};
        bool orbitPivotActive = false;
    };

    /*
        Galaxy-specific persistent state and camera/navigation math.

        GalaxyMapRenderer owns scene orchestration and GalaxyMapInteraction
        owns input. SystemMapRenderer is only the shared OpenGL/resource
        backend used by the map render contexts.
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

        // Transient pointer state (hover, cloud selection, click trackers,
        // gesture, presented geometry) never survives a map session.
        void resetTransientPointerState();

        void suppressCameraGesture(
            bool leftDown,
            bool rightDown,
            double mouseX,
            double mouseY
        );

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

        GalaxyMapCameraSnapshot cameraSnapshot(
            const Viewport& viewport
        ) const;

        glm::mat4 viewMatrix() const;
        glm::mat4 projectionMatrix(const Viewport& viewport) const;

        glm::dvec3 cameraDirectionWorld() const;
        glm::dvec3 cameraUpWorld() const;
        glm::dvec3 cameraRightWorld() const;
        glm::dvec3 cameraEyeWorld() const;

        float navigationAnchorDiameterPx(
            const Viewport& viewport
        ) const;

        // ---- spatial frame (current parent cube) ----
        GalaxyHierarchyBox hierarchyBox() const;

        // A parent "on the whole screen": its projected corners fill this
        // fraction of the viewport in the limiting direction.
        static constexpr float ParentFillRatio = 0.98f;

        // Projected size of a cube with the current camera (or a given
        // target / distance): max of its NDC extent over the viewport,
        // 1 = fills the screen in the limiting direction.
        float cubeScreenRatio(
            const glm::dvec3& centerLy,
            double edgeLy,
            const Viewport& viewport
        ) const;
        // Apparent size of a cube edge at the current zoom, wherever the
        // cube is (as if centred on the camera target): the hierarchy
        // hand-over measure, independent of where the user looks.
        float cubeSizeRatio(
            double edgeLy,
            const Viewport& viewport
        ) const;
        // fitInside: the largest |NDC| of a corner instead (1 = a corner on
        // the viewport edge) - the framing measure; perspective makes the
        // projected cube asymmetric around the view centre.
        float cubeScreenRatioAt(
            const glm::dvec3& centerLy,
            double edgeLy,
            const glm::vec3& target,
            float distance,
            const Viewport& viewport,
            bool fitInside = false
        ) const;
        // Camera distance at which every corner of the cube lies within
        // `fill` of the viewport (largest |NDC| = fill).
        float fillDistanceAt(
            const glm::dvec3& centerLy,
            double edgeLy,
            const glm::vec3& target,
            const Viewport& viewport,
            float fill
        ) const;

        // Distance at which the current parent fills the screen.
        float hierarchyFitDistance(const Viewport& viewport) const;

        // Wheel range inside the current parent: from a child filling the
        // view to a little beyond the fitted parent.
        void cameraDistanceLimits(
            const Viewport& viewport,
            float& minimumDistance,
            float& maximumDistance
        ) const;

        // The navigable child containing the point (nullopt outside).
        std::optional<navigation::GalaxyNavigationCell> hierarchyChildAt(
            const glm::dvec3& positionLy
        ) const;

        void frameHierarchyParent(
            const Viewport& viewport,
            double nowSeconds,
            bool animate
        );

        // Double click down: the cell becomes the current parent (false for
        // a terminal cell - the caller enters the System Map instead).
        // frameCamera false: the wheel already brought the cube to the
        // screen (no flight).
        bool enterHierarchyCell(
            const navigation::GalaxyNavigationCell& cell,
            const Viewport& viewport,
            double nowSeconds,
            bool frameCamera = true
        );

        // Up: the parent of the current parent; the cube the user came
        // from stays selected inside it.
        bool ascendHierarchy(
            const Viewport& viewport,
            double nowSeconds
        );

        // Breadcrumb: an ancestor (-1 overview .. parent level - 1).
        bool jumpToHierarchyLevel(
            int level,
            const Viewport& viewport,
            double nowSeconds,
            bool frameCamera = true
        );

        // Fly to a point of the current parent: centred on it, the parent's
        // size filling the screen (a star / cloud double click).
        void approachHierarchyPoint(
            const glm::dvec3& pointLy,
            const Viewport& viewport,
            double nowSeconds
        );

        GalaxyMapBookmark bookmark() const;
        void restoreBookmark(const GalaxyMapBookmark& bookmark);

        // Galaxy overview with the root containing the point selected.
        void showGalaxyOverview(
            const glm::dvec3& selectRootAtLy,
            bool animate,
            double nowSeconds
        );

        // Projection far plane: the fitted overview must never be clipped.
        float farPlane() const;

        void beginCameraFlight(
            const glm::vec3& destinationTarget,
            float destinationDistance,
            double nowSeconds,
            bool nestedFrameFlight = false
        );

        void updateCameraFlight(double nowSeconds);
        void cancelCameraFlight(bool snapToDestination);

        MapIntent entryIntentForPosition(
            const world::celestial::GalaxyMapSnapshot& galaxy,
            const glm::dvec3& positionLy,
            int explicitSystemId = -1
        ) const;

        // Select a point (star, route waypoint): the current parent stays
        // unless the point lies outside it - then the parent moves to the
        // cube of the same level containing the point.
        void focusHierarchyPoint(
            const glm::dvec3& positionLy,
            bool animateCamera,
            double nowSeconds
        );

        bool focusSystem(
            int systemId,
            const world::celestial::GalaxyMapSnapshot& galaxy,
            bool animateCamera,
            double nowSeconds
        );

    private:
        void setHierarchyParent(const GalaxyHierarchyParent& parent);

        GalaxyMapViewState m_state;
        GalaxyMapControlSettings m_controls;
        GalaxyMapVisualSettings m_visuals;
    };
}
