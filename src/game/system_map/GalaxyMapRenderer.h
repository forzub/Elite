#pragma once

#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "src/render/types/Viewport.h"

namespace world::celestial
{
    struct GalaxyMapSnapshot;
    struct PlayerNavigationState;
}

namespace game::system_map
{
    class GalaxyMapView;
    struct GalaxyMapCameraSnapshot;
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
        Galaxy draw-pass orchestrator.

        It owns no OpenGL objects. Every shared GPU operation is performed
        through GalaxyMapRenderContext, whose implementation remains the
        SystemMapRenderer facade during the incremental extraction.
    */
    class GalaxyMapRenderer
    {
    public:
        void setPlayerLabel(std::string label)
        {
            m_playerLabel = std::move(label);
        }

        void render(
            GalaxyMapView& view,
            GalaxyMapRenderContext& context,
            const Viewport& viewport,
            const world::celestial::GalaxyMapSnapshot& galaxy,
            const world::celestial::PlayerNavigationState& navigation
        ) const;

        // "What is here?" list: drawn last, above the map's UI cards.
        void drawWhatIsHere(
            GalaxyMapView& view,
            GalaxyMapRenderContext& context,
            const Viewport& viewport
        ) const;

    private:
        void drawMediumVolumes(
            GalaxyMapView& view,
            GalaxyMapRenderContext& context,
            const world::celestial::GalaxyMapSnapshot& galaxy,
            const glm::mat4& cameraView,
            const glm::mat4& mvp,
            const Viewport& viewport
        ) const;

        void drawNavigationGrid(
            GalaxyMapView& view,
            GalaxyMapRenderContext& context,
            const Viewport& viewport,
            const GalaxyMapCameraSnapshot& camera,
            const world::celestial::GalaxyMapSnapshot& galaxy,
            std::vector<GalaxyFrameText>& frameTexts
        ) const;

        void drawFrameTexts(
            GalaxyMapView& view,
            GalaxyMapRenderContext& context,
            const Viewport& viewport,
            const GalaxyMapCameraSnapshot& camera,
            const std::vector<GalaxyFrameText>& frameTexts
        ) const;

        void drawLabels(
            GalaxyMapView& view,
            GalaxyMapRenderContext& context,
            const Viewport& viewport,
            const world::celestial::GalaxyMapSnapshot& galaxy,
            const world::celestial::PlayerNavigationState& navigation,
            const glm::mat4& mvp
        ) const;

        void drawPlayerMarker(
            GalaxyMapView& view,
            GalaxyMapRenderContext& context,
            const Viewport& viewport,
            const world::celestial::GalaxyMapSnapshot& galaxy,
            const world::celestial::PlayerNavigationState& navigation,
            const glm::mat4& mvp
        ) const;

    private:
        std::string m_playerLabel = "PLAYER";
    };
}
