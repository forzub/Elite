#include "DebugGrid.h"
#include <glad/gl.h>
#include <cmath>

void DebugGrid::drawInfinite(
    const glm::vec3& cameraPos,
    float size,
    int divisions
)
{
    float step = size / divisions;
    float half = size * 0.5f;

    // центрируем сетку под камерой
    float originX = std::floor(cameraPos.x / step) * step;
    float originZ = std::floor(cameraPos.z / step) * step;

    glBegin(GL_LINES);

    for (int i = -divisions; i <= divisions; ++i)
    {
        float offset = i * step;

        // линии вдоль X
        glColor3f(0.3f, 0.3f, 0.3f);
        glVertex3f(originX - half, 0.0f, originZ + offset);
        glVertex3f(originX + half, 0.0f, originZ + offset);

        // линии вдоль Z
        glVertex3f(originX + offset, 0.0f, originZ - half);
        glVertex3f(originX + offset, 0.0f, originZ + half);
    }

    // оси координат (локальные)
    glColor3f(1, 0, 0);
    glVertex3f(originX - half, 0, originZ);
    glVertex3f(originX + half, 0, originZ);

    glColor3f(0, 0, 1);
    glVertex3f(originX, 0, originZ - half);
    glVertex3f(originX, 0, originZ + half);

    glEnd();
}
