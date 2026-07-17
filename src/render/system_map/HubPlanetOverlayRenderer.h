#pragma once

#include <glad/gl.h>

namespace render::system_map
{

class HubPlanetOverlayRenderer
{
public:
    /*
        Начинает рендер мягких Hub-слоёв в пониженном разрешении.

        Возвращает false, если ресурсы не удалось создать. В этом случае
        вызывающий код должен рисовать слои напрямую в основной target.
    */
    bool begin(
        int viewportWidth,
        int viewportHeight,
        float resolutionScale
    );

    /*
        Возвращает пониженную texture обратно в основной framebuffer.
        Texture хранит premultiplied RGB/alpha, поэтому composite использует
        GL_ONE / GL_ONE_MINUS_SRC_ALPHA.
    */
    void endAndComposite();

    bool active() const
    {
        return m_active;
    }

    float resolutionScale() const
    {
        return m_resolutionScale;
    }

private:
    void ensureResources(
        int targetWidth,
        int targetHeight
    );

private:
    GLuint m_framebuffer = 0;
    GLuint m_colorTexture = 0;
    GLuint m_fullscreenVao = 0;
    GLuint m_compositeShader = 0;

    GLint m_textureLocation = -1;

    int m_targetWidth = 0;
    int m_targetHeight = 0;

    float m_resolutionScale = 1.0f;
    bool m_active = false;
    bool m_warningPrinted = false;

    GLint m_previousFramebuffer = 0;
    GLint m_previousViewport[4] {0, 0, 1, 1};
    GLint m_previousScissorBox[4] {0, 0, 1, 1};

    GLfloat m_previousClearColor[4] {
        0.0f,
        0.0f,
        0.0f,
        0.0f
    };

    GLint m_previousProgram = 0;
    GLint m_previousVao = 0;
    GLint m_previousActiveTexture = 0;
    GLint m_previousTexture0 = 0;

    GLint m_previousBlendSourceRgb = GL_SRC_ALPHA;
    GLint m_previousBlendDestinationRgb = GL_ONE_MINUS_SRC_ALPHA;
    GLint m_previousBlendSourceAlpha = GL_SRC_ALPHA;
    GLint m_previousBlendDestinationAlpha = GL_ONE_MINUS_SRC_ALPHA;

    GLint m_previousBlendEquationRgb = GL_FUNC_ADD;
    GLint m_previousBlendEquationAlpha = GL_FUNC_ADD;

    GLboolean m_blendWasEnabled = GL_FALSE;
    GLboolean m_depthWasEnabled = GL_FALSE;
    GLboolean m_cullWasEnabled = GL_FALSE;
    GLboolean m_scissorWasEnabled = GL_FALSE;
};

} // namespace render::system_map
