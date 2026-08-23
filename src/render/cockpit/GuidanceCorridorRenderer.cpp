#include "src/render/cockpit/GuidanceCorridorRenderer.h"

#include <algorithm>
#include <cmath>

namespace render::cockpit
{

void GuidanceCorridorRenderer::init()
{
    m_batch.init();
}

bool GuidanceCorridorRenderer::projectPoint(
    const glm::dvec3& relativeWorldMeters,
    const glm::mat4& viewMatrix,
    const glm::mat4& projectionMatrix,
    const Viewport& viewport,
    glm::vec2& outPx
) const
{
    if (!std::isfinite(relativeWorldMeters.x) ||
        !std::isfinite(relativeWorldMeters.y) ||
        !std::isfinite(relativeWorldMeters.z))
    {
        return false;
    }

    // Relative-to-player vector uses w=0 so camera/world translation cannot
    // reintroduce large-coordinate precision loss. The view rotation is still
    // applied exactly like the existing cockpit navigation markers.
    const glm::vec4 view = viewMatrix * glm::vec4(
        glm::vec3(relativeWorldMeters),
        0.0f
    );
    if (view.z >= -0.05f)
        return false;

    const glm::vec4 clip = projectionMatrix * glm::vec4(
        view.x,
        view.y,
        view.z,
        1.0f
    );
    if (std::abs(clip.w) <= 1.0e-6f)
        return false;

    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    if (!std::isfinite(ndc.x) || !std::isfinite(ndc.y))
        return false;

    outPx = {
        (ndc.x * 0.5f + 0.5f) * static_cast<float>(viewport.width),
        (1.0f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(viewport.height)
    };
    return true;
}

GuidanceCorridorRenderer::ProjectedFrame
GuidanceCorridorRenderer::projectFrame(
    const game::presentation::GuidanceHudFramePresentation& frame,
    const glm::mat4& viewMatrix,
    const glm::mat4& projectionMatrix,
    const Viewport& viewport
) const
{
    ProjectedFrame out;
    const double halfW = std::max(0.5, frame.widthMeters * 0.5);
    const double halfH = std::max(0.5, frame.heightMeters * 0.5);

    const glm::dvec3 right = frame.orientation * glm::dvec3(1.0, 0.0, 0.0);
    const glm::dvec3 up = frame.orientation * glm::dvec3(0.0, 1.0, 0.0);

    const glm::dvec3 corners[4] = {
        frame.relativeCenterMeters - right * halfW - up * halfH,
        frame.relativeCenterMeters + right * halfW - up * halfH,
        frame.relativeCenterMeters + right * halfW + up * halfH,
        frame.relativeCenterMeters - right * halfW + up * halfH
    };

    for (int i = 0; i < 4; ++i)
    {
        if (!projectPoint(
                corners[i],
                viewMatrix,
                projectionMatrix,
                viewport,
                out.corners[i]))
        {
            return out;
        }
    }

    out.valid = true;
    return out;
}

void GuidanceCorridorRenderer::render(
    const game::presentation::GuidanceCorridorHudPresentation& presentation,
    const glm::mat4& viewMatrix,
    const glm::mat4& projectionMatrix,
    const Viewport& viewport
)
{
    if (!presentation.visible || presentation.frames.empty() ||
        viewport.width <= 0 || viewport.height <= 0)
    {
        return;
    }

    const float confidence = static_cast<float>(std::clamp(
        presentation.confidence,
        0.15,
        1.0
    ));
    const glm::vec4 frameColor = presentation.noSafePrimarySolution
        ? glm::vec4(
            1.0f,
            0.38f,
            0.24f,
            0.08f + confidence * 0.28f
          )
        : glm::vec4(
            0.34f,
            0.92f,
            1.0f,
            0.06f + confidence * 0.20f
          );
    const glm::vec4 connectorColor(
        frameColor.r,
        frameColor.g,
        frameColor.b,
        frameColor.a * 0.18f
    );

    m_batch.begin(viewport.width, viewport.height);

    ProjectedFrame previous;
    bool havePrevious = false;
    for (const auto& frame : presentation.frames)
    {
        const ProjectedFrame projected = projectFrame(
            frame,
            viewMatrix,
            projectionMatrix,
            viewport
        );
        if (!projected.valid)
            continue;

        for (int edge = 0; edge < 4; ++edge)
        {
            m_batch.line(
                projected.corners[edge],
                projected.corners[(edge + 1) % 4],
                0.85f,
                frameColor
            );
        }

        if (havePrevious)
        {
            for (int corner = 0; corner < 4; ++corner)
            {
                m_batch.line(
                    previous.corners[corner],
                    projected.corners[corner],
                    0.45f,
                    connectorColor
                );
            }
        }

        previous = projected;
        havePrevious = true;
    }

    m_batch.flush();
}

} // namespace render::cockpit
