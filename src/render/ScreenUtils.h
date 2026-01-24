#pragma once

#include <glm/glm.hpp>

bool projectToScreen(
    const glm::vec3& worldPos,
    const glm::mat4& view,
    const glm::mat4& proj,
    int screenW,
    int screenH,
    glm::vec2& outScreen
);

void drawLine(
    const glm::vec2& a,
    const glm::vec2& b,
    const glm::vec3& color
);

void drawCircle(const glm::vec2& center, float radius);