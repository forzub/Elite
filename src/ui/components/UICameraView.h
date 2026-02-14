#pragma once

#include "UIComponent.h"
#include "render/Camera.h"
#include "render/camera/RenderCameraViewport.h"

#include <functional>

class UICameraView : public UIComponent
{
public:
    Camera* camera = nullptr;

    // FBO
    GLuint fbo = 0;
    GLuint colorTex = 0;
    GLuint depthRb = 0;

    int fboWidth = 0;
    int fboHeight = 0;

    // ---- style ----
    float borderThicknessRel = 0.01f;   // 1% от меньшей стороны
    float cornerRadiusRel    = 0.08f;   // 8% от меньшей стороны
    glm::vec3 borderColor {0.2f, 0.8f, 1.0f};

    ~UICameraView();
    
    // инициализация
    void initFBO(int width, int height);
    void renderToTexture(const Viewport& vp,
        std::function<void(const glm::mat4&, const glm::mat4&)> drawScene);
    

    std::function<void(const glm::mat4&, const glm::mat4&)> drawCallback;


private:
    void ensureFBO(const Viewport& vp);

protected:
    void render(
        const Viewport& vp,
        float parentX,
        float parentY,
        float parentW,
        float parentH
    ) override;
};
