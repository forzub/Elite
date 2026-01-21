#pragma once
#include <glm/glm.hpp>

class DebugGrid
{
public:
    static void drawInfinite(
        const glm::vec3& cameraPos,
        float size,
        int divisions
    );
};
