#pragma once

#include <glad/gl.h>

struct PostProcessSettings
{
    bool enabled = true;

    float bloomThreshold = 0.58f;
    float bloomKnee = 0.25f;
    float bloomIntensity = 0.42f;

    float softening = 0.18f;

    float saturation = 0.86f;
    float contrast = 1.08f;
    float vignette = 0.22f;
    float grain = 0.012f;
    float haze = 0.24f;
};

class Renderer
{
public:
    bool init();
    void shutdown();

    void beginFrame();
    void endFrame();

    bool beginPostProcess(
        int framebufferWidth,
        int framebufferHeight,
        int gameViewportX,
        int gameViewportY,
        int gameViewportWidth,
        int gameViewportHeight
    );

    void endPostProcess(float timeSeconds);

    PostProcessSettings& postProcessSettings() { return m_postProcess; }
    const PostProcessSettings& postProcessSettings() const { return m_postProcess; }

    void drawGrid(int size, float step);

private:
    bool ensurePostProcessTargets(int width, int height);
    bool createPostProcessTargets(int width, int height);
    void destroyPostProcessTargets();
    void drawFullscreenTriangle() const;

private:
    PostProcessSettings m_postProcess;

    bool m_initialized = false;
    bool m_initializationFailed = false;
    bool m_postProcessActive = false;

    int m_width = 0;
    int m_height = 0;
    int m_bloomWidth = 0;
    int m_bloomHeight = 0;
    int m_msaaSamples = 1;

    int m_gameViewportX = 0;
    int m_gameViewportY = 0;
    int m_gameViewportWidth = 1;
    int m_gameViewportHeight = 1;

    GLuint m_fullscreenVao = 0;
    GLuint m_fullscreenVbo = 0;

    GLuint m_bloomExtractProgram = 0;
    GLuint m_blurProgram = 0;
    GLuint m_compositeProgram = 0;

    GLuint m_sceneFramebuffer = 0;
    GLuint m_sceneColorRenderbuffer = 0;
    GLuint m_sceneDepthStencilRenderbuffer = 0;

    GLuint m_resolveFramebuffer = 0;
    GLuint m_sceneTexture = 0;

    GLuint m_bloomFramebuffers[2] = {0, 0};
    GLuint m_bloomTextures[2] = {0, 0};
};