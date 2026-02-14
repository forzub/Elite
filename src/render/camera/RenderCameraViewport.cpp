#include "src/render/camera/RenderCameraViewport.h"

void RenderCameraViewport::render(
    Camera& camera,
    const Viewport& fullVp,
    int x,
    int y,
    int width,
    int height,
    std::function<void(const glm::mat4&, const glm::mat4&)> drawScene
)
{
    glViewport(x, y, width, height);

    glEnable(GL_DEPTH_TEST);

    float aspect = (float)width / (float)height;

    glm::mat4 projection = glm::perspective(
        glm::radians(camera.fov()),
        aspect,
        camera.nearPlane(),
        camera.farPlane()
    );

    glm::mat4 view = camera.viewMatrix();

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(glm::value_ptr(projection));

    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(glm::value_ptr(view));

    drawScene(view, projection);

    glViewport(0, 0, fullVp.width, fullVp.height);
}