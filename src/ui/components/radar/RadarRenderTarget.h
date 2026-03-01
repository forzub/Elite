#pragma once
#include <glad/gl.h>

class RadarRenderTarget
{
public:
    RadarRenderTarget();
    ~RadarRenderTarget();

    void ensureSize(int width, int height);
    void bind();
    void unbind();

    GLuint texture() const { return m_colorTex; }

    int width() const { return m_width; }
    int height() const { return m_height; }

    GLuint getFbo() const { return m_fbo; } 

private:
    void create(int width, int height);
    void destroy();

private:
    GLuint m_fbo = 0;
    GLuint m_colorTex = 0;
    GLuint m_depthRb = 0;

    int m_width = 0;
    int m_height = 0;

    GLint m_previousFBO = 0;
    GLint m_previousViewport[4] = {0, 0, 0, 0};
};

// 1. bind FBO
// 2. нарисовать радар (круг, метки, sweep)
// 3. unbind FBO
// 4. нарисовать получившуюся текстуру в UI