#pragma once

#include <glm/glm.hpp>

#include "src/render/types/Viewport.h"

namespace world::celestial
{
    struct GalaxyMapSnapshot;
    struct PlayerNavigationState;
}

namespace game::system_map
{
    /*
        Shared rendering backend used by GalaxyMapRenderer.

        The Galaxy view owns Galaxy-specific state and draw orchestration,
        while SystemMapRenderer remains the single owner of OpenGL buffers,
        shaders, text services and the shared starfield renderer.

        This interface is intentionally limited to rendering operations.
        It contains no input, transition or world-mutation methods.
    */
    class GalaxyMapRenderContext
    {
    public:
        virtual ~GalaxyMapRenderContext() = default;

        virtual void drawMapStarfield(
            const Viewport& viewport,
            const glm::dvec3& observerPositionLy,
            const glm::mat4& cameraView,
            float fieldOfViewDeg,
            float sizeScale,
            bool distantGalaxyBackdrop,
            float starBrightnessScale,
            float milkyWayIntensityScale,
            const glm::vec3& milkyWayColorTint
        ) = 0;

        virtual void drawMapAtmosphereVeil(
            float centerAlpha,
            float edgeAlpha,
            float aquaStrength
        ) = 0;

        virtual void beginLines() = 0;

        virtual void addLine(
            const glm::vec3& a,
            const glm::vec3& b,
            const glm::vec4& color
        ) = 0;

        virtual void beginSolids() = 0;

        virtual void addBillboardBall(
            const glm::vec3& center,
            float radius,
            const glm::vec4& color,
            const glm::mat4& view,
            int segments
        ) = 0;

        virtual void flushSolids(const glm::mat4& mvp) = 0;
        virtual void flushLines(const glm::mat4& mvp) = 0;

        /*
            Temporary high-level bridges.

            Navigation-grid, labels and player-marker passes still use the
            old shared primitive implementation. Later Galaxy extraction
            steps will move these passes into GalaxyMapRenderer and shrink
            this interface without changing GPU ownership.
        */
        virtual void drawGalaxyNavigationGrid(
            const Viewport& viewport,
            const glm::mat4& mvp
        ) = 0;

        virtual void drawNavigationLayerPlaceholder() = 0;

        virtual void drawGalaxyLabels(
            const Viewport& viewport,
            const world::celestial::GalaxyMapSnapshot& galaxy,
            const glm::mat4& mvp
        ) = 0;

        virtual void drawGalaxyPlayerMarker(
            const Viewport& viewport,
            const world::celestial::GalaxyMapSnapshot& galaxy,
            const world::celestial::PlayerNavigationState& navigation,
            const glm::mat4& mvp
        ) = 0;
    };
}
