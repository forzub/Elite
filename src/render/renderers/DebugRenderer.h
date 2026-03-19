#pragma once

#include <glm/glm.hpp>

class DebugLineRenderer;

class DebugRenderer
{
public:

    DebugRenderer(DebugLineRenderer* lineRenderer);

    void renderAxes(
        const glm::mat4& model,
        const glm::mat4& view,
        const glm::mat4& proj
    );


    void renderPlayerGrid(
        const glm::mat4& model,
        const glm::mat4& view,
        const glm::mat4& proj
    );


    void renderFrustum(
        const glm::mat4& view,
        const glm::mat4& proj
    );


    void renderBoundingSphere(
        const glm::vec3& center,
        float radius,
        const glm::mat4& view,
        const glm::mat4& proj
    );

private:

    DebugLineRenderer* m_lineRenderer;
};





