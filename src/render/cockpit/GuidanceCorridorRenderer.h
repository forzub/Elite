#pragma once

#include <array>

#include <glm/glm.hpp>

#include "src/game/presentation/GuidanceHudPresentation.h"
#include "src/render/HUD/HudPrimitiveBatch.h"
#include "src/render/types/Viewport.h"

namespace render::cockpit
{

class GuidanceCorridorRenderer
{
public:
    void init();

    void render(
        const game::presentation::GuidanceCorridorHudPresentation& presentation,
        const glm::mat4& viewMatrix,
        const glm::mat4& projectionMatrix,
        const Viewport& viewport
    );

private:
    struct ProjectedFrame
    {
        bool valid = false;
        std::array<glm::vec2, 4> corners {};
    };

    bool projectPoint(
        const glm::dvec3& relativeWorldMeters,
        const glm::mat4& viewMatrix,
        const glm::mat4& projectionMatrix,
        const Viewport& viewport,
        glm::vec2& outPx
    ) const;

    ProjectedFrame projectFrame(
        const game::presentation::GuidanceHudFramePresentation& frame,
        const glm::mat4& viewMatrix,
        const glm::mat4& projectionMatrix,
        const Viewport& viewport
    ) const;

    render::hud::HudPrimitiveBatch m_batch;
};

} // namespace render::cockpit
