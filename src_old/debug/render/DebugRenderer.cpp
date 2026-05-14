#include "DebugRenderer.h"
#include "DebugLineRenderer.h"
#include "src/debug/DebugSettings.h"

#include <glm/gtc/matrix_transform.hpp>

DebugRenderer::DebugRenderer(DebugLineRenderer* lineRenderer)
    : m_lineRenderer(lineRenderer)
{
}

void DebugRenderer::renderAxes(
    const glm::mat4& model,
    const glm::mat4& view,
    const glm::mat4& proj,
    float axisLength
)
{
    if (!m_lineRenderer || !m_lineRenderer->isInitialized())
        return;

    const auto& dbg = debug::get().render;

    const glm::vec3 origin = glm::vec3(model[3]);

    const glm::vec3 x = origin + glm::normalize(glm::vec3(model[0])) * axisLength;
    const glm::vec3 y = origin + glm::normalize(glm::vec3(model[1])) * axisLength;
    const glm::vec3 z = origin + glm::normalize(glm::vec3(model[2])) * axisLength;

    const glm::mat4 mvp = proj * view;

    m_lineRenderer->begin();
    m_lineRenderer->addLine(origin, x, dbg.axisXColor);
    m_lineRenderer->addLine(origin, y, dbg.axisYColor);
    m_lineRenderer->addLine(origin, z, dbg.axisZColor);

    if (dbg.axesOverlay)
        m_lineRenderer->endOverlay(mvp);
    else
        m_lineRenderer->end(mvp);
}

void DebugRenderer::renderCross(
    const glm::vec3& center,
    float size,
    const glm::vec4& color,
    const glm::mat4& view,
    const glm::mat4& proj
)
{
    if (!m_lineRenderer || !m_lineRenderer->isInitialized())
        return;

    const auto& dbg = debug::get().render;
    const glm::mat4 mvp = proj * view;

    m_lineRenderer->begin();
    m_lineRenderer->addLine(center + glm::vec3(-size, 0.0f, 0.0f), center + glm::vec3(size, 0.0f, 0.0f), color);
    m_lineRenderer->addLine(center + glm::vec3(0.0f, -size, 0.0f), center + glm::vec3(0.0f, size, 0.0f), color);
    m_lineRenderer->addLine(center + glm::vec3(0.0f, 0.0f, -size), center + glm::vec3(0.0f, 0.0f, size), color);

    if (dbg.crossesOverlay)
        m_lineRenderer->endOverlay(mvp);
    else
        m_lineRenderer->end(mvp);
}

void DebugRenderer::renderLine(
    const glm::vec3& a,
    const glm::vec3& b,
    const glm::vec4& color,
    const glm::mat4& view,
    const glm::mat4& proj
)
{
    if (!m_lineRenderer || !m_lineRenderer->isInitialized())
        return;

    const auto& dbg = debug::get().render;
    const glm::mat4 mvp = proj * view;

    m_lineRenderer->begin();
    m_lineRenderer->addLine(a, b, color);

    if (dbg.linesOverlay)
        m_lineRenderer->endOverlay(mvp);
    else
        m_lineRenderer->end(mvp);
}