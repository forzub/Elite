#include "render/ScreenUtils.h"
#include <glad/gl.h>



// ===================================================================
// project To Screen
// ===================================================================
bool projectToScreen(
    const glm::vec3& worldPos,
    const glm::mat4& view,
    const glm::mat4& proj,
    int screenW,
    int screenH,
    glm::vec2& outScreen
)
{
    glm::vec4 clip = proj * view * glm::vec4(worldPos, 1.0f);

    if (clip.w <= 0.0f)
        return false;

    glm::vec3 ndc = glm::vec3(clip) / clip.w;

    if (ndc.x < -1.0f || ndc.x > 1.0f ||
        ndc.y < -1.0f || ndc.y > 1.0f)
        return false;

    outScreen.x = (ndc.x * 0.5f + 0.5f) * screenW;
    outScreen.y = (1.0f - (ndc.y * 0.5f + 0.5f)) * screenH;

    return true;
}

// ===================================================================
// draw Line
// ===================================================================
void drawLine(
    const glm::vec2& a,
    const glm::vec2& b,
    const glm::vec3& color
)
{
    glColor3f(color.r, color.g, color.b);
    glBegin(GL_LINES);
        glVertex2f(a.x, a.y);
        glVertex2f(b.x, b.y);
    glEnd();
}


// ===================================================================
// draw Circle
// ===================================================================
void drawCircle(const glm::vec2& center, float radius)
{
    const int segments = 32;

    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < segments; ++i)
    {
        float a = (i / (float)segments) * 2.0f * 3.1415926f;
        glVertex2f(
            center.x + std::cos(a) * radius,
            center.y + std::sin(a) * radius
        );
    }
    glEnd();
}


