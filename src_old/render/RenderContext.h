#pragma once

#include <glm/mat4x4.hpp>

struct RenderContext
{
    glm::mat4 projection;
    glm::mat4 view;
    glm::mat4 ortho;

    int viewportWidth;
    int viewportHeight;
};
