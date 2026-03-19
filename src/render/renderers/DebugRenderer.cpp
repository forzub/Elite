#include "DebugRenderer.h"
#include "DebugLineRenderer.h"

#include <glm/gtc/matrix_transform.hpp>

DebugRenderer::DebugRenderer(DebugLineRenderer* lineRenderer)
    : m_lineRenderer(lineRenderer)
{
}




void DebugRenderer::renderAxes(
    const glm::mat4& model,
    const glm::mat4& view,
    const glm::mat4& proj)
{
    if (!m_lineRenderer || !m_lineRenderer->isInitialized())
        return;

    glm::vec3 origin = glm::vec3(model[3]);

    float axisLength = 5.0f;

    glm::vec3 x = origin + glm::normalize(glm::vec3(model[0])) * axisLength;
    glm::vec3 y = origin + glm::normalize(glm::vec3(model[1])) * axisLength;
    glm::vec3 z = origin + glm::normalize(glm::vec3(model[2])) * axisLength;

    glm::mat4 mvp = proj * view;

    m_lineRenderer->begin();

    m_lineRenderer->addLine(origin, x, glm::vec4(1,0,0,1));
    m_lineRenderer->addLine(origin, y, glm::vec4(0,1,0,1));
    m_lineRenderer->addLine(origin, z, glm::vec4(0,0,1,1));

    m_lineRenderer->end(mvp);
}




void DebugRenderer::renderPlayerGrid(
    const glm::mat4& model,
    const glm::mat4& view,
    const glm::mat4& proj)
{
    if (!m_lineRenderer || !m_lineRenderer->isInitialized())
        return;

    glm::mat4 mvp = proj * view * model;

    m_lineRenderer->begin();

    float gridWidth = 120.0f;
    float widthStep = 20.0f;
    float depthStep = 25.0f;
    float gridDepth = 300.0f;
    int depthCells = 10;
    float gridLength = depthCells * depthStep;


    // центральная линия глубины
    m_lineRenderer->addLine(
        glm::vec3(0,0,0),
        glm::vec3(0,0, 300.0f),
        glm::vec4(1,1,1,1)
    );

    // горизонтальная линия
    m_lineRenderer->addLine(
        glm::vec3(-100,0,0),
        glm::vec3(100,0,0),
        glm::vec4(0.7f,0.7f,0.7f,1)
    );

    


    float zEnd = gridLength;
    float scaleEnd = 1.0f - (zEnd / gridDepth);

    // линии глубины
    for (float x = -gridWidth; x <= gridWidth; x += widthStep)
    {
        float xEnd = x * scaleEnd;

        m_lineRenderer->addLine(
            glm::vec3(x,0,0),
            glm::vec3(xEnd,0,-zEnd),
            glm::vec4(0.3f,0.8f,1.0f,0.25f)
        );
    }

    // горизонтальные срезы
    for (float z = depthStep; z <= gridLength; z += depthStep)
    {
        float scale = 1.0f - (z / gridDepth);

        m_lineRenderer->addLine(
            glm::vec3(-gridWidth * scale,0,-z),
            glm::vec3(gridWidth * scale,0,-z),
            glm::vec4(0.3f,0.8f,1.0f,0.25f)
        );
    }

    // боковая сетка
    float sideOffset = 60.0f;

    // линии глубины
    for (float y = -gridWidth; y <= gridWidth; y += widthStep)
    {
        m_lineRenderer->addLine(
            glm::vec3(sideOffset, y, 0),
            glm::vec3(sideOffset * scaleEnd, y * scaleEnd, -gridLength),
            glm::vec4(1.0f,0.5f,0.2f,0.15f)
        );
    }

    // горизонтальные срезы
    for (float z = depthStep; z <= gridLength; z += depthStep)
    {
        float scale = 1.0f - (z / gridDepth);
        float yMin = -gridWidth * scale;
        float yMax =  gridWidth * scale;
        float x = sideOffset * scale;

        m_lineRenderer->addLine(
            glm::vec3(x, yMin, -z),
            glm::vec3(x, yMax, -z),
            glm::vec4(1.0f,0.5f,0.2f,0.15f)
        );
    }

    m_lineRenderer->end(mvp);
}






void DebugRenderer::renderFrustum(
    const glm::mat4& view,
    const glm::mat4& proj)
{
    if (!m_lineRenderer || !m_lineRenderer->isInitialized())
        return;

    glm::mat4 viewProj = proj * view;
    glm::mat4 invViewProj = glm::inverse(viewProj);

    // NDC вершины куба
    glm::vec3 ndc[8] =
    {
        {-1,-1,-1},
        { 1,-1,-1},
        { 1, 1,-1},
        {-1, 1,-1},

        {-1,-1, 1},
        { 1,-1, 1},
        { 1, 1, 1},
        {-1, 1, 1}
    };

    glm::vec3 world[8];

    // переводим в world space
    for (int i = 0; i < 8; i++)
    {
        glm::vec4 p = invViewProj * glm::vec4(ndc[i], 1.0f);
        world[i] = glm::vec3(p) / p.w;
    }

    
    for (int i = 0; i < 8; i++)
    {
        world[i] += glm::vec3(0,0,-50);
    }
        

    glm::mat4 mvp = proj * view;

    m_lineRenderer->begin();

    glm::vec4 color(1,1,0,1);

    // near plane
    m_lineRenderer->addLine(world[0], world[1], color);
    m_lineRenderer->addLine(world[1], world[2], color);
    m_lineRenderer->addLine(world[2], world[3], color);
    m_lineRenderer->addLine(world[3], world[0], color);

    // far plane
    m_lineRenderer->addLine(world[4], world[5], color);
    m_lineRenderer->addLine(world[5], world[6], color);
    m_lineRenderer->addLine(world[6], world[7], color);
    m_lineRenderer->addLine(world[7], world[4], color);

    // соединяющие рёбра
    m_lineRenderer->addLine(world[0], world[4], color);
    m_lineRenderer->addLine(world[1], world[5], color);
    m_lineRenderer->addLine(world[2], world[6], color);
    m_lineRenderer->addLine(world[3], world[7], color);

    m_lineRenderer->end(mvp);
}



void DebugRenderer::renderBoundingSphere(
    const glm::vec3& center,
    float radius,
    const glm::mat4& view,
    const glm::mat4& proj)
{
    if (!m_lineRenderer || !m_lineRenderer->isInitialized())
        return;

    glm::mat4 mvp = proj * view;

    const int segments = 24;
    const float step = glm::two_pi<float>() / segments;

    glm::vec4 color(1,0,0,1);

    m_lineRenderer->begin();

    for(int i=0;i<segments;i++)
    {
        float a0 = i * step;
        float a1 = (i+1) * step;

        // XY circle
        glm::vec3 p0 =
            center + glm::vec3(
                cos(a0)*radius,
                sin(a0)*radius,
                0);

        glm::vec3 p1 =
            center + glm::vec3(
                cos(a1)*radius,
                sin(a1)*radius,
                0);

        m_lineRenderer->addLine(p0,p1,color);

        // XZ circle
        p0 =
            center + glm::vec3(
                cos(a0)*radius,
                0,
                sin(a0)*radius);

        p1 =
            center + glm::vec3(
                cos(a1)*radius,
                0,
                sin(a1)*radius);

        m_lineRenderer->addLine(p0,p1,color);

        // YZ circle
        p0 =
            center + glm::vec3(
                0,
                cos(a0)*radius,
                sin(a0)*radius);

        p1 =
            center + glm::vec3(
                0,
                cos(a1)*radius,
                sin(a1)*radius);

        m_lineRenderer->addLine(p0,p1,color);
    }

    m_lineRenderer->end(mvp);
}
