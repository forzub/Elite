#include "src/render/legacy/CoreGlLegacyBridge.h"
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

    elite::render::core_legacy::begin(GL_LINES);

    for (int i = -divisions; i <= divisions; ++i)
    {
        float offset = i * step;

        // линии вдоль X
        elite::render::core_legacy::color3f(0.3f, 0.3f, 0.3f);
        elite::render::core_legacy::vertex3f(originX - half, 0.0f, originZ + offset);
        elite::render::core_legacy::vertex3f(originX + half, 0.0f, originZ + offset);

        // линии вдоль Z
        elite::render::core_legacy::vertex3f(originX + offset, 0.0f, originZ - half);
        elite::render::core_legacy::vertex3f(originX + offset, 0.0f, originZ + half);
    }

    // оси координат (локальные)
    elite::render::core_legacy::color3f(1, 0, 0);
    elite::render::core_legacy::vertex3f(originX - half, 0, originZ);
    elite::render::core_legacy::vertex3f(originX + half, 0, originZ);

    elite::render::core_legacy::color3f(0, 0, 1);
    elite::render::core_legacy::vertex3f(originX, 0, originZ - half);
    elite::render::core_legacy::vertex3f(originX, 0, originZ + half);

    elite::render::core_legacy::end();
}
