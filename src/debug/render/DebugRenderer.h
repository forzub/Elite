#pragma once

#include <glm/glm.hpp>

class DebugLineRenderer;

class DebugRenderer
{
public:
    explicit DebugRenderer(DebugLineRenderer* lineRenderer);

    void renderAxes(
        const glm::mat4& model,
        const glm::mat4& view,
        const glm::mat4& proj,
        float axisLength
    );

    void renderCross(
        const glm::vec3& center,
        float size,
        const glm::vec4& color,
        const glm::mat4& view,
        const glm::mat4& proj
    );

    void renderLine(
        const glm::vec3& a,
        const glm::vec3& b,
        const glm::vec4& color,
        const glm::mat4& view,
        const glm::mat4& proj
    );

private:
    DebugLineRenderer* m_lineRenderer = nullptr;
};