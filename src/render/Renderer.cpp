#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "Renderer.h"
#include "render/ShaderUtils.h"

#include <algorithm>
#include <iostream>
#include <string>

namespace
{
bool framebufferComplete(const char* name)
{
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    if (status == GL_FRAMEBUFFER_COMPLETE)
        return true;

    std::cerr
        << "[PostProcess] framebuffer incomplete: "
        << name
        << " status="
        << static_cast<unsigned int>(status)
        << "\n";

    return false;
}

void setSampler(GLuint program, const char* name, GLint unit)
{
    const GLint location = glGetUniformLocation(program, name);
    if (location >= 0)
        glUniform1i(location, unit);
}

void setFloat(GLuint program, const char* name, float value)
{
    const GLint location = glGetUniformLocation(program, name);
    if (location >= 0)
        glUniform1f(location, value);
}

void setVec2(GLuint program, const char* name, float x, float y)
{
    const GLint location = glGetUniformLocation(program, name);
    if (location >= 0)
        glUniform2f(location, x, y);
}

void setVec4(GLuint program, const char* name, float x, float y, float z, float w)
{
    const GLint location = glGetUniformLocation(program, name);
    if (location >= 0)
        glUniform4f(location, x, y, z, w);
}
} // namespace

bool Renderer::init()
{
    if (m_initialized)
        return true;

    if (m_initializationFailed)
        return false;

    m_bloomExtractProgram = compileShaderFromFiles(
        "assets/shaders/postprocess/fullscreen.vert",
        "assets/shaders/postprocess/bloom_extract.frag"
    );

    m_blurProgram = compileShaderFromFiles(
        "assets/shaders/postprocess/fullscreen.vert",
        "assets/shaders/postprocess/gaussian_blur.frag"
    );

    m_compositeProgram = compileShaderFromFiles(
        "assets/shaders/postprocess/fullscreen.vert",
        "assets/shaders/postprocess/cinematic_composite.frag"
    );

    if (m_bloomExtractProgram == 0 ||
        m_blurProgram == 0 ||
        m_compositeProgram == 0)
    {
        std::cerr
            << "[PostProcess] shader initialization failed; rendering continues without post-process\n";

        shutdown();
        m_initializationFailed = true;
        return false;
    }

    const float fullscreenTriangle[] =
    {
        -1.0f, -1.0f,
         3.0f, -1.0f,
        -1.0f,  3.0f
    };

    glGenVertexArrays(1, &m_fullscreenVao);
    glGenBuffers(1, &m_fullscreenVbo);

    glBindVertexArray(m_fullscreenVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_fullscreenVbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(fullscreenTriangle),
        fullscreenTriangle,
        GL_STATIC_DRAW
    );

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        2,
        GL_FLOAT,
        GL_FALSE,
        2 * static_cast<GLsizei>(sizeof(float)),
        nullptr
    );

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    GLint maxSamples = 1;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    m_msaaSamples = std::clamp(maxSamples, 1, 4);

    glUseProgram(m_bloomExtractProgram);
    setSampler(m_bloomExtractProgram, "uScene", 0);

    glUseProgram(m_blurProgram);
    setSampler(m_blurProgram, "uImage", 0);

    glUseProgram(m_compositeProgram);
    setSampler(m_compositeProgram, "uScene", 0);
    setSampler(m_compositeProgram, "uBloom", 1);

    glUseProgram(0);

    m_initialized = true;
    m_initializationFailed = false;

    std::cout
        << "[PostProcess] initialized, MSAA samples="
        << m_msaaSamples
        << "\n";

    return true;
}

void Renderer::shutdown()
{
    m_postProcessActive = false;

    destroyPostProcessTargets();

    if (m_fullscreenVbo != 0)
    {
        glDeleteBuffers(1, &m_fullscreenVbo);
        m_fullscreenVbo = 0;
    }

    if (m_fullscreenVao != 0)
    {
        glDeleteVertexArrays(1, &m_fullscreenVao);
        m_fullscreenVao = 0;
    }

    if (m_bloomExtractProgram != 0)
    {
        glDeleteProgram(m_bloomExtractProgram);
        m_bloomExtractProgram = 0;
    }

    if (m_blurProgram != 0)
    {
        glDeleteProgram(m_blurProgram);
        m_blurProgram = 0;
    }

    if (m_compositeProgram != 0)
    {
        glDeleteProgram(m_compositeProgram);
        m_compositeProgram = 0;
    }

    m_initialized = false;
}

void Renderer::beginFrame()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0.02f, 0.02f, 0.03f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::endFrame()
{
    // swapBuffers делается в Window
}

bool Renderer::beginPostProcess(
    int framebufferWidth,
    int framebufferHeight,
    int gameViewportX,
    int gameViewportY,
    int gameViewportWidth,
    int gameViewportHeight
)
{
    m_postProcessActive = false;

    if (!m_postProcess.enabled ||
        framebufferWidth <= 0 ||
        framebufferHeight <= 0 ||
        gameViewportWidth <= 0 ||
        gameViewportHeight <= 0)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    if (!m_initialized && !init())
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    if (!ensurePostProcessTargets(framebufferWidth, framebufferHeight))
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    m_gameViewportX = gameViewportX;
    m_gameViewportY = gameViewportY;
    m_gameViewportWidth = gameViewportWidth;
    m_gameViewportHeight = gameViewportHeight;

    glBindFramebuffer(GL_FRAMEBUFFER, m_sceneFramebuffer);
    m_postProcessActive = true;
    return true;
}

void Renderer::endPostProcess(float timeSeconds)
{
    if (!m_postProcessActive)
        return;

    m_postProcessActive = false;

    glDisable(GL_SCISSOR_TEST);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_sceneFramebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_resolveFramebuffer);

    glBlitFramebuffer(
        0, 0, m_width, m_height,
        0, 0, m_width, m_height,
        GL_COLOR_BUFFER_BIT,
        GL_NEAREST
    );

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDisable(GL_MULTISAMPLE);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glDepthMask(GL_FALSE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    // 1. bloom extract
    glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFramebuffers[0]);
    glViewport(0, 0, m_bloomWidth, m_bloomHeight);

    glUseProgram(m_bloomExtractProgram);
    setVec2(
        m_bloomExtractProgram,
        "uSceneTexelSize",
        1.0f / static_cast<float>(m_width),
        1.0f / static_cast<float>(m_height)
    );
    setFloat(m_bloomExtractProgram, "uThreshold", std::clamp(m_postProcess.bloomThreshold, 0.0f, 2.0f));
    setFloat(m_bloomExtractProgram, "uKnee", std::clamp(m_postProcess.bloomKnee, 0.001f, 1.0f));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_sceneTexture);
    drawFullscreenTriangle();

    // 2. blur X
    glUseProgram(m_blurProgram);

    glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFramebuffers[1]);
    setVec2(
        m_blurProgram,
        "uDirection",
        1.0f / static_cast<float>(m_bloomWidth),
        0.0f
    );
    glBindTexture(GL_TEXTURE_2D, m_bloomTextures[0]);
    drawFullscreenTriangle();

    // 3. blur Y
    glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFramebuffers[0]);
    setVec2(
        m_blurProgram,
        "uDirection",
        0.0f,
        1.0f / static_cast<float>(m_bloomHeight)
    );
    glBindTexture(GL_TEXTURE_2D, m_bloomTextures[1]);
    drawFullscreenTriangle();

    // 4. final composite
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_width, m_height);

    glUseProgram(m_compositeProgram);

    setVec2(
        m_compositeProgram,
        "uSceneTexelSize",
        1.0f / static_cast<float>(m_width),
        1.0f / static_cast<float>(m_height)
    );
    setVec2(
        m_compositeProgram,
        "uResolution",
        static_cast<float>(m_width),
        static_cast<float>(m_height)
    );
    setVec4(
        m_compositeProgram,
        "uGameRect",
        static_cast<float>(m_gameViewportX) / static_cast<float>(m_width),
        static_cast<float>(m_gameViewportY) / static_cast<float>(m_height),
        static_cast<float>(m_gameViewportX + m_gameViewportWidth) / static_cast<float>(m_width),
        static_cast<float>(m_gameViewportY + m_gameViewportHeight) / static_cast<float>(m_height)
    );

    setFloat(m_compositeProgram, "uBloomIntensity", std::clamp(m_postProcess.bloomIntensity, 0.0f, 2.0f));
    setFloat(m_compositeProgram, "uSoftening", std::clamp(m_postProcess.softening, 0.0f, 1.0f));
    setFloat(m_compositeProgram, "uSaturation", std::clamp(m_postProcess.saturation, 0.0f, 2.0f));
    setFloat(m_compositeProgram, "uContrast", std::clamp(m_postProcess.contrast, 0.5f, 2.0f));
    setFloat(m_compositeProgram, "uVignette", std::clamp(m_postProcess.vignette, 0.0f, 1.0f));
    setFloat(m_compositeProgram, "uGrain", std::clamp(m_postProcess.grain, 0.0f, 0.10f));
    setFloat(m_compositeProgram, "uHaze", std::clamp(m_postProcess.haze, 0.0f, 1.0f));
    setFloat(m_compositeProgram, "uTime", timeSeconds);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_sceneTexture);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_bloomTextures[0]);

    drawFullscreenTriangle();

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    glUseProgram(0);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_MULTISAMPLE);
}

bool Renderer::ensurePostProcessTargets(int width, int height)
{
    if (m_sceneFramebuffer != 0 &&
        width == m_width &&
        height == m_height)
    {
        return true;
    }

    destroyPostProcessTargets();
    return createPostProcessTargets(width, height);
}

bool Renderer::createPostProcessTargets(int width, int height)
{
    GLint previousFramebuffer = 0;
    GLint previousRenderbuffer = 0;
    GLint previousTexture = 0;

    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFramebuffer);
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &previousRenderbuffer);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);

    m_width = width;
    m_height = height;
    m_bloomWidth = std::max(1, width / 2);
    m_bloomHeight = std::max(1, height / 2);

    bool ok = true;

    // scene FBO (MSAA)
    glGenFramebuffers(1, &m_sceneFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, m_sceneFramebuffer);

    glGenRenderbuffers(1, &m_sceneColorRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, m_sceneColorRenderbuffer);

    if (m_msaaSamples > 1)
    {
        glRenderbufferStorageMultisample(
            GL_RENDERBUFFER,
            m_msaaSamples,
            GL_RGBA8,
            width,
            height
        );
    }
    else
    {
        glRenderbufferStorage(
            GL_RENDERBUFFER,
            GL_RGBA8,
            width,
            height
        );
    }

    glFramebufferRenderbuffer(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_RENDERBUFFER,
        m_sceneColorRenderbuffer
    );

    glGenRenderbuffers(1, &m_sceneDepthStencilRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, m_sceneDepthStencilRenderbuffer);

    if (m_msaaSamples > 1)
    {
        glRenderbufferStorageMultisample(
            GL_RENDERBUFFER,
            m_msaaSamples,
            GL_DEPTH24_STENCIL8,
            width,
            height
        );
    }
    else
    {
        glRenderbufferStorage(
            GL_RENDERBUFFER,
            GL_DEPTH24_STENCIL8,
            width,
            height
        );
    }

    glFramebufferRenderbuffer(
        GL_FRAMEBUFFER,
        GL_DEPTH_STENCIL_ATTACHMENT,
        GL_RENDERBUFFER,
        m_sceneDepthStencilRenderbuffer
    );

    ok = framebufferComplete("scene") && ok;

    // resolve FBO
    glGenFramebuffers(1, &m_resolveFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, m_resolveFramebuffer);

    glGenTextures(1, &m_sceneTexture);
    glBindTexture(GL_TEXTURE_2D, m_sceneTexture);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        width,
        height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        nullptr
    );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        m_sceneTexture,
        0
    );

    ok = framebufferComplete("resolve") && ok;

    // bloom ping-pong
    glGenFramebuffers(2, m_bloomFramebuffers);
    glGenTextures(2, m_bloomTextures);

    for (int i = 0; i < 2; ++i)
    {
        glBindTexture(GL_TEXTURE_2D, m_bloomTextures[i]);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA16F,
            m_bloomWidth,
            m_bloomHeight,
            0,
            GL_RGBA,
            GL_FLOAT,
            nullptr
        );
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFramebuffers[i]);
        glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D,
            m_bloomTextures[i],
            0
        );

        ok = framebufferComplete(i == 0 ? "bloom A" : "bloom B") && ok;
    }

    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
    glBindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(previousRenderbuffer));
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previousFramebuffer));

    if (!ok)
    {
        destroyPostProcessTargets();
        return false;
    }

    std::cout
        << "[PostProcess] targets "
        << width << "x" << height
        << ", bloom "
        << m_bloomWidth << "x" << m_bloomHeight
        << "\n";

    return true;
}

void Renderer::destroyPostProcessTargets()
{
    if (m_bloomTextures[0] != 0 || m_bloomTextures[1] != 0)
    {
        glDeleteTextures(2, m_bloomTextures);
        m_bloomTextures[0] = 0;
        m_bloomTextures[1] = 0;
    }

    if (m_bloomFramebuffers[0] != 0 || m_bloomFramebuffers[1] != 0)
    {
        glDeleteFramebuffers(2, m_bloomFramebuffers);
        m_bloomFramebuffers[0] = 0;
        m_bloomFramebuffers[1] = 0;
    }

    if (m_sceneTexture != 0)
    {
        glDeleteTextures(1, &m_sceneTexture);
        m_sceneTexture = 0;
    }

    if (m_resolveFramebuffer != 0)
    {
        glDeleteFramebuffers(1, &m_resolveFramebuffer);
        m_resolveFramebuffer = 0;
    }

    if (m_sceneDepthStencilRenderbuffer != 0)
    {
        glDeleteRenderbuffers(1, &m_sceneDepthStencilRenderbuffer);
        m_sceneDepthStencilRenderbuffer = 0;
    }

    if (m_sceneColorRenderbuffer != 0)
    {
        glDeleteRenderbuffers(1, &m_sceneColorRenderbuffer);
        m_sceneColorRenderbuffer = 0;
    }

    if (m_sceneFramebuffer != 0)
    {
        glDeleteFramebuffers(1, &m_sceneFramebuffer);
        m_sceneFramebuffer = 0;
    }

    m_width = 0;
    m_height = 0;
    m_bloomWidth = 0;
    m_bloomHeight = 0;
}

void Renderer::drawFullscreenTriangle() const
{
    glBindVertexArray(m_fullscreenVao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void Renderer::drawGrid(int size, float step)
{
    glBegin(GL_LINES);

    glColor3f(0.3f, 0.3f, 0.3f);

    for (int i = -size; i <= size; ++i)
    {
        glVertex3f(i * step, 0.0f, -size * step);
        glVertex3f(i * step, 0.0f,  size * step);

        glVertex3f(-size * step, 0.0f, i * step);
        glVertex3f( size * step, 0.0f, i * step);
    }

    glEnd();
}