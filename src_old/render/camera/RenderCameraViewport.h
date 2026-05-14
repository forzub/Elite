#pragma once

#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>

#include "src/render/Camera.h"
#include "core/StateContext.h"

struct RenderCameraViewport
{
    static void render(
        Camera& camera,
        const Viewport& fullVp,
        int x,
        int y,
        int width,
        int height,
        std::function<void(const glm::mat4&, const glm::mat4&)> drawScene
    );
};