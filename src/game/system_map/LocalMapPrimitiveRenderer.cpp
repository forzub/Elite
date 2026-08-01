#include "src/game/system_map/LocalMapPrimitiveRenderer.h"

#include <algorithm>
#include <cmath>

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/constants.hpp>

namespace game::system_map
{
void drawLocalMapLine(
    const glm::dvec2& a,
    const glm::dvec2& b
)
{
    glBegin(GL_LINES);
    glVertex2d(a.x, a.y);
    glVertex2d(b.x, b.y);
    glEnd();
}

void drawLocalMapCross(
    const glm::dvec2& point,
    float size
)
{
    glBegin(GL_LINES);
    glVertex2d(point.x - size, point.y);
    glVertex2d(point.x + size, point.y);
    glVertex2d(point.x, point.y - size);
    glVertex2d(point.x, point.y + size);
    glEnd();
}

void drawLocalMapCircle(
    const glm::dvec2& center,
    double radiusPx,
    int segments
)
{
    segments = std::max(segments, 8);

    glBegin(GL_LINE_LOOP);

    for (int segment = 0; segment < segments; ++segment)
    {
        const double angle =
            glm::two_pi<double>() *
            static_cast<double>(segment) /
            static_cast<double>(segments);

        glVertex2d(
            center.x + std::cos(angle) * radiusPx,
            center.y + std::sin(angle) * radiusPx
        );
    }

    glEnd();
}
}
