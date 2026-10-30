#pragma once

#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/CubicNavigationGrid.h"
#include "src/game/system_map/GalaxyHierarchy.h"
#include "src/render/types/Viewport.h"

namespace world::celestial
{
    struct GalaxyMapSnapshot;
}

namespace game::system_map
{
    /*
        ONE Galaxy Map pointer hit resolver.

        Input: what the last frame actually PRESENTED (the star screen points
        and the navigation-cube markers published by GalaxyMapRenderer, the
        overlay glyphs of SystemMapRenderer, whether a UI panel is under the
        cursor) plus the camera for the ray against the medium shells.
        Output: ONE semantic target. The input dispatcher acts on it; no
        picker runs after another with "first one found wins".

        Ranking (WYSIWYG):
        1. a UI panel under the cursor;
        2. drawn controls: the cursor INSIDE a drawn star glyph (foreground
           star first) / cube marker / overlay glyph wins; otherwise the
           drawn shape whose edge is nearest within its UX margin
           (PointerInteractionSettings::glyphHitMarginPx); exact ties:
           star > cube marker > overlay glyph;
        3. star proximity (systemPickRadiusPx around a star, nearest; within
           1 px the foreground star; then the lower id);
        4. cloud shell (ray, nearest hit) / icy field marker;
        5. the child cube of the current parent where the cursor ray meets
           the hover layer (GalaxyHierarchyLayer), when a hierarchy is given;
        6. empty space.
    */
    enum class GalaxyPointerTargetKind
    {
        Empty,
        Panel,
        OverlayGlyph,
        Star,
        NavigationCube,
        Medium
    };

    enum class GalaxyPointerHitReason
    {
        None,
        PanelArea,
        StarGlyph,
        CubeMarker,
        OverlayGlyph,
        StarProximity,
        MediumShell,
        IcyFieldMarker,
        // An overlay glyph of the cube that completes this cube's double
        // click (same semantic target, other presentation element).
        CubeSequenceGlyph,
        // Empty space inside the current parent: the child on the layer.
        HierarchyLayer
    };

    const char* galaxyPointerTargetKindName(GalaxyPointerTargetKind kind);
    const char* galaxyPointerHitReasonName(GalaxyPointerHitReason reason);

    struct GalaxyPointerTarget
    {
        GalaxyPointerTargetKind kind = GalaxyPointerTargetKind::Empty;
        GalaxyPointerHitReason reason = GalaxyPointerHitReason::None;

        int systemId = -1;                                  // Star
        std::optional<navigation::CubicNavigationCell> cell; // cube / glyph of a cube
        std::string overlayObjectId;                        // OverlayGlyph
        int mediumObject = -1;                              // Medium
        glm::dvec3 hitLy {0.0};                             // Medium / layer point

        float screenDistancePx = 0.0f;
        float depth = 1.0f;                                 // NDC z, smaller = nearer

        // Same semantic object (cube by level + index, star by id ...).
        bool sameSemanticTarget(const GalaxyPointerTarget& other) const;
    };

    // A star as presented by the last rendered frame.
    struct GalaxyPointerStar
    {
        int systemId = -1;
        glm::vec2 screen {0.0f};
        float glyphRadiusPx = 0.0f;
        float depth = 1.0f;
        bool visible = false;
    };

    // A navigation-cube centre marker as drawn by the last rendered frame.
    struct GalaxyNavigationMarker
    {
        navigation::CubicNavigationCell cell;
        glm::vec2 screen {0.0f};
        float radiusPx = 0.0f;
        float depth = 1.0f;
        bool visible = false;
    };

    // A pointer-interactive overlay glyph (route waypoint / waypoint
    // candidate). semanticCell: the navigation cube it represents, if any.
    struct GalaxyPointerOverlayGlyph
    {
        std::string objectId;
        glm::vec2 screen {0.0f};
        float hitRadiusPx = 0.0f;
        std::optional<navigation::CubicNavigationCell> semanticCell;
    };

    // The current parent and its hover layer (empty-space child cubes).
    struct GalaxyPointerHierarchy
    {
        const navigation::GalaxyNavigationGrid* grid = nullptr;
        GalaxyHierarchyBox box;
        GalaxyHierarchyLayer layer;
    };

    /*
        The camera of the presented frame. The cursor ray is built from it
        directly: un-projecting the far plane of an mvp whose near / far
        ratio is beyond float precision (the fitted overview) gives no ray.
    */
    struct GalaxyPointerCamera
    {
        glm::dvec3 eyeLy {0.0};
        glm::dvec3 right {1.0, 0.0, 0.0};
        glm::dvec3 up {0.0, 1.0, 0.0};
        glm::dvec3 forward {0.0, 0.0, -1.0}; // into the scene
        double tanHalfFovY = 0.4452;
        double aspect = 1.0;
    };

    struct GalaxyPointerScene
    {
        const std::vector<GalaxyPointerStar>* stars = nullptr;
        const std::vector<GalaxyNavigationMarker>* cubeMarkers = nullptr;
        const std::vector<GalaxyPointerOverlayGlyph>* overlayGlyphs = nullptr;
        bool panelUnderCursor = false;

        // Medium: the camera of the presented frame (render units).
        const world::celestial::GalaxyMapSnapshot* galaxy = nullptr;
        glm::mat4 mvp {1.0f};
        Viewport viewport;
        float renderUnitsPerLy = 1.0f;

        float glyphHitMarginPx = 4.0f;
        float starProximityRadiusPx = 32.0f;
        float icyFieldPickRadiusPx = 32.0f;
        // Medium LOD: a structure projected smaller than this is not a target.
        float mediumMinRadiusPx = 0.0f;

        std::optional<GalaxyPointerHierarchy> hierarchy;
        std::optional<GalaxyPointerCamera> camera;
    };

    struct GalaxyPointerResolution
    {
        GalaxyPointerTarget winner;
        // Every candidate considered (only when collectCandidates: trace).
        std::vector<GalaxyPointerTarget> candidates;
    };

    GalaxyPointerResolution resolveGalaxyPointer(
        const GalaxyPointerScene& scene,
        const glm::vec2& mousePx,
        bool collectCandidates = false
    );

    // Medium structure / icy field under the cursor: projected bounding
    // prefilter, then the ray against the coarse shell triangles of the few
    // candidates (icy fields: marker distance). Nearest hit to the camera.
    int pickGalaxyMedium(
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const glm::mat4& mvp,
        const Viewport& viewport,
        float renderUnitsPerLy,
        const glm::vec2& mousePx,
        const glm::dvec3& rayOriginLy,
        const glm::dvec3& rayDirectionLy,
        float icyFieldPickRadiusPx,
        glm::dvec3& hitLy,
        float& hitDepth,
        float minRadiusPx = 0.0f
    );

    // The cursor ray in light-years (from the scene camera, else from
    // un-projecting the mvp).
    void galaxyPointerRayLy(
        const GalaxyPointerScene& scene,
        const glm::vec2& mousePx,
        glm::dvec3& originLy,
        glm::dvec3& directionLy
    );

    /*
        Left-button gesture: a press never moves the camera. While the
        pointer stays within the drag threshold the gesture is a
        PendingClick (the target resolved at the press is kept); beyond it
        the click is cancelled and the drag rotates the camera. A release
        before the threshold is a click on the press target. A right press,
        a wheel step or a suppressed frame cancels a pending click.
    */
    class GalaxyPointerGesture
    {
    public:
        enum class Phase
        {
            Idle,
            PendingClick,
            Dragging,
            Cancelled
        };

        enum class Event
        {
            None,
            Pressed,
            DragStarted,
            Clicked,
            Released
        };

        Event update(
            bool leftDown,
            bool inside,
            double x,
            double y,
            double nowSeconds,
            double dragThresholdPx
        );

        void cancelPendingClick();
        // Another layer (overlay, UI, camera flight, transition) owns this
        // frame's button: follow its state, never click or drag from it.
        void suppress(bool leftDown);
        // A new map session: a button already held is not a map press.
        void reset();

        Phase phase() const { return m_phase; }
        bool leftWasDown() const { return m_leftWasDown; }
        double pressX() const { return m_pressX; }
        double pressY() const { return m_pressY; }
        double pressTimeSeconds() const { return m_pressTime; }
        double maxMovementPx() const { return m_maxMovementPx; }

    private:
        Phase m_phase = Phase::Idle;
        bool m_leftWasDown = false;
        double m_pressX = 0.0;
        double m_pressY = 0.0;
        double m_pressTime = 0.0;
        double m_maxMovementPx = 0.0;
    };
}
