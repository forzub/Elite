#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/CubicNavigationGrid.h"
#include "src/game/system_map/GalaxyHierarchy.h"
#include "src/game/system_map/GalaxyMapVisualSettings.h"
#include "src/game/system_map/GalaxyPointerResolver.h"
#include "src/render/types/Viewport.h"

namespace game::system_map
{
    class GalaxyMapRenderContext;

    // A text of the spatial frame: world anchored (render units) or
    // screen anchored (map viewport pixels).
    struct GalaxyFrameText
    {
        glm::vec3 position {0.0f};
        std::string text;
        glm::vec4 color {1.0f};
        int px = 10;
        bool axisName = false;
        bool screenSpace = false;
    };

    /*
        The spatial frame of the current parent cube, the SAME for the Galaxy
        Map and the System Map: the parent's edges, the grid on its inner
        floor / far wall (major / minor lines, labels), the hovered and
        selected child (outline, marker, projections onto the faces, child
        coordinates) and the hover layer. Units are the grid's (ly / AU);
        `toRender` maps them to the map's render space.
    */
    struct HierarchyFramePaintInput
    {
        GalaxyHierarchyBox box;
        GalaxyHierarchyFaces* faces = nullptr;   // in / out (hysteresis)

        // Camera: eye and view direction in grid units, basis and eye in
        // render space, the frame's projection.
        glm::dvec3 eyeUnits {0.0};
        glm::dvec3 viewInto {0.0, 0.0, -1.0};
        glm::vec3 cameraRight {1.0f, 0.0f, 0.0f};
        glm::vec3 cameraUp {0.0f, 1.0f, 0.0f};
        glm::vec3 cameraDirection {0.0f, 0.0f, 1.0f}; // target -> eye
        glm::vec3 cameraEyeRender {0.0f};
        glm::mat4 mvp {1.0f};
        Viewport viewport;
        float tanHalfFovY = 0.4452f;
        std::function<glm::vec3(const glm::dvec3&)> toRender;

        const GalaxyNavigationGridVisualSettings* visuals = nullptr;
        char levelPrefix = 'G';

        float frameFade = 1.0f;
        std::optional<GalaxyHierarchyBox> flightContextBox;

        std::optional<navigation::CubicNavigationCell> hoverCell;
        float hoverAlpha = 0.0f;
        std::optional<navigation::CubicNavigationCell> outgoingCell;
        float outgoingAlpha = 0.0f;
        bool hoverOnLayer = false;
        int layerAxis = 1;
        // The exact hovered object point (else the cube centre is used).
        std::optional<glm::dvec3> hoverObjectPoint;

        std::optional<navigation::CubicNavigationCell> selectedCell;
        // The selected cube is the context of a selected object: its marker
        // is not a pointer target; the object point is projected.
        bool objectSelection = false;
        std::optional<glm::dvec3> selectedObjectPoint;
    };

    // Lines go into the context's current line batch (flushed by the map).
    void paintHierarchyFrame(
        GalaxyMapRenderContext& context,
        const HierarchyFramePaintInput& input,
        std::vector<GalaxyNavigationMarker>& presentedMarkers,
        std::vector<GalaxyFrameText>& texts
    );

    // Frame texts + the XYZ orientation triad (own line / text passes).
    void paintHierarchyFrameTexts(
        GalaxyMapRenderContext& context,
        const Viewport& viewport,
        const glm::mat4& mvp,
        const glm::mat4& view,
        const std::vector<GalaxyFrameText>& texts
    );

    // Visual hover lifetime: fade in / out instead of blinking.
    struct HierarchyHoverFade
    {
        std::optional<navigation::CubicNavigationCell>* visualCell = nullptr;
        float* visualAlpha = nullptr;
        std::optional<navigation::CubicNavigationCell>* outgoingCell = nullptr;
        float* outgoingAlpha = nullptr;
        double* lastTimeSeconds = nullptr;
    };

    void updateHierarchyHoverFade(
        const HierarchyHoverFade& fade,
        const std::optional<navigation::CubicNavigationCell>& logicalHover,
        double nowSeconds,
        float fadeInSeconds,
        float fadeOutSeconds
    );
}
