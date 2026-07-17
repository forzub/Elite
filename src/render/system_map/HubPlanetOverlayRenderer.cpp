#include "src/render/system_map/HubPlanetOverlayRenderer.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include "src/render/ShaderLibrary.h"

namespace render::system_map
{

void HubPlanetOverlayRenderer::ensureResources(
    int targetWidth,
    int targetHeight
)
{
    targetWidth =
        std::max(
            1,
            targetWidth
        );

    targetHeight =
        std::max(
            1,
            targetHeight
        );

    if (m_compositeShader == 0)
    {
        m_compositeShader =
            ShaderLibrary::instance().get(
                "hub_planet_overlay_composite"
            );

        if (m_compositeShader != 0)
        {
            m_textureLocation =
                glGetUniformLocation(
                    m_compositeShader,
                    "uOverlayTexture"
                );
        }
    }

    if (m_fullscreenVao == 0)
    {
        glGenVertexArrays(
            1,
            &m_fullscreenVao
        );
    }

    if (m_framebuffer == 0)
    {
        glGenFramebuffers(
            1,
            &m_framebuffer
        );
    }

    if (m_colorTexture == 0)
    {
        glGenTextures(
            1,
            &m_colorTexture
        );
    }

    if (m_targetWidth == targetWidth &&
        m_targetHeight == targetHeight)
    {
        return;
    }

    GLint previousTexture = 0;

    glGetIntegerv(
        GL_TEXTURE_BINDING_2D,
        &previousTexture
    );

    glBindTexture(
        GL_TEXTURE_2D,
        m_colorTexture
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MIN_FILTER,
        GL_LINEAR
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MAG_FILTER,
        GL_LINEAR
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_S,
        GL_CLAMP_TO_EDGE
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_T,
        GL_CLAMP_TO_EDGE
    );

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        targetWidth,
        targetHeight,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        nullptr
    );

    glBindTexture(
        GL_TEXTURE_2D,
        static_cast<GLuint>(
            previousTexture
        )
    );

    m_targetWidth = targetWidth;
    m_targetHeight = targetHeight;
}


bool HubPlanetOverlayRenderer::begin(
    int viewportWidth,
    int viewportHeight,
    float resolutionScale
)
{
    if (m_active ||
        viewportWidth <= 0 ||
        viewportHeight <= 0)
    {
        return false;
    }

    m_resolutionScale =
        std::clamp(
            resolutionScale,
            0.25f,
            1.0f
        );

    const int targetWidth =
        std::max(
            1,
            static_cast<int>(
                std::ceil(
                    static_cast<double>(viewportWidth) *
                    static_cast<double>(m_resolutionScale)
                )
            )
        );

    const int targetHeight =
        std::max(
            1,
            static_cast<int>(
                std::ceil(
                    static_cast<double>(viewportHeight) *
                    static_cast<double>(m_resolutionScale)
                )
            )
        );

    ensureResources(
        targetWidth,
        targetHeight
    );

    if (m_framebuffer == 0 ||
        m_colorTexture == 0 ||
        m_fullscreenVao == 0 ||
        m_compositeShader == 0)
    {
        if (!m_warningPrinted)
        {
            m_warningPrinted = true;

            std::cerr
                << "[HubPlanetOverlayRenderer]"
                << " required resources are unavailable"
                << "\n";
        }

        return false;
    }

    glGetIntegerv(
        GL_FRAMEBUFFER_BINDING,
        &m_previousFramebuffer
    );

    glGetIntegerv(
        GL_VIEWPORT,
        m_previousViewport
    );

    glGetIntegerv(
        GL_SCISSOR_BOX,
        m_previousScissorBox
    );

    glGetFloatv(
        GL_COLOR_CLEAR_VALUE,
        m_previousClearColor
    );

    glGetIntegerv(
        GL_CURRENT_PROGRAM,
        &m_previousProgram
    );

    glGetIntegerv(
        GL_VERTEX_ARRAY_BINDING,
        &m_previousVao
    );

    glGetIntegerv(
        GL_ACTIVE_TEXTURE,
        &m_previousActiveTexture
    );

    glActiveTexture(
        GL_TEXTURE0
    );

    glGetIntegerv(
        GL_TEXTURE_BINDING_2D,
        &m_previousTexture0
    );

    glGetIntegerv(
        GL_BLEND_SRC_RGB,
        &m_previousBlendSourceRgb
    );

    glGetIntegerv(
        GL_BLEND_DST_RGB,
        &m_previousBlendDestinationRgb
    );

    glGetIntegerv(
        GL_BLEND_SRC_ALPHA,
        &m_previousBlendSourceAlpha
    );

    glGetIntegerv(
        GL_BLEND_DST_ALPHA,
        &m_previousBlendDestinationAlpha
    );

    glGetIntegerv(
        GL_BLEND_EQUATION_RGB,
        &m_previousBlendEquationRgb
    );

    glGetIntegerv(
        GL_BLEND_EQUATION_ALPHA,
        &m_previousBlendEquationAlpha
    );

    m_blendWasEnabled =
        glIsEnabled(
            GL_BLEND
        );

    m_depthWasEnabled =
        glIsEnabled(
            GL_DEPTH_TEST
        );

    m_cullWasEnabled =
        glIsEnabled(
            GL_CULL_FACE
        );

    m_scissorWasEnabled =
        glIsEnabled(
            GL_SCISSOR_TEST
        );

    glBindFramebuffer(
        GL_FRAMEBUFFER,
        m_framebuffer
    );

    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        m_colorTexture,
        0
    );

    if (glCheckFramebufferStatus(
            GL_FRAMEBUFFER
        ) != GL_FRAMEBUFFER_COMPLETE)
    {
        glBindFramebuffer(
            GL_FRAMEBUFFER,
            static_cast<GLuint>(
                m_previousFramebuffer
            )
        );

        if (!m_warningPrinted)
        {
            m_warningPrinted = true;

            std::cerr
                << "[HubPlanetOverlayRenderer]"
                << " framebuffer is incomplete"
                << "\n";
        }

        return false;
    }

    glViewport(
        0,
        0,
        m_targetWidth,
        m_targetHeight
    );

    glDisable(
        GL_DEPTH_TEST
    );

    glDisable(
        GL_CULL_FACE
    );

    glDisable(
        GL_SCISSOR_TEST
    );

    glDisable(
        GL_BLEND
    );

    glClearColor(
        0.0f,
        0.0f,
        0.0f,
        0.0f
    );

    glClear(
        GL_COLOR_BUFFER_BIT
    );

    m_active = true;
    return true;
}


void HubPlanetOverlayRenderer::endAndComposite()
{
    if (!m_active)
        return;

    glBindFramebuffer(
        GL_FRAMEBUFFER,
        static_cast<GLuint>(
            m_previousFramebuffer
        )
    );

    glViewport(
        m_previousViewport[0],
        m_previousViewport[1],
        m_previousViewport[2],
        m_previousViewport[3]
    );

    glDisable(
        GL_DEPTH_TEST
    );

    glDisable(
        GL_CULL_FACE
    );

    glDisable(
        GL_SCISSOR_TEST
    );

    glEnable(
        GL_BLEND
    );

    /*
        Overlay texture содержит premultiplied RGB.
    */
    glBlendEquation(
        GL_FUNC_ADD
    );

    glBlendFuncSeparate(
        GL_ONE,
        GL_ONE_MINUS_SRC_ALPHA,
        GL_ONE,
        GL_ONE_MINUS_SRC_ALPHA
    );

    glUseProgram(
        m_compositeShader
    );

    glActiveTexture(
        GL_TEXTURE0
    );

    glBindTexture(
        GL_TEXTURE_2D,
        m_colorTexture
    );

    if (m_textureLocation >= 0)
    {
        glUniform1i(
            m_textureLocation,
            0
        );
    }

    glBindVertexArray(
        m_fullscreenVao
    );

    glDrawArrays(
        GL_TRIANGLES,
        0,
        3
    );

    glBindVertexArray(
        static_cast<GLuint>(
            m_previousVao
        )
    );

    glUseProgram(
        static_cast<GLuint>(
            m_previousProgram
        )
    );

    glActiveTexture(
        GL_TEXTURE0
    );

    glBindTexture(
        GL_TEXTURE_2D,
        static_cast<GLuint>(
            m_previousTexture0
        )
    );

    glActiveTexture(
        static_cast<GLenum>(
            m_previousActiveTexture
        )
    );

    glBlendEquationSeparate(
        static_cast<GLenum>(
            m_previousBlendEquationRgb
        ),
        static_cast<GLenum>(
            m_previousBlendEquationAlpha
        )
    );

    glBlendFuncSeparate(
        static_cast<GLenum>(
            m_previousBlendSourceRgb
        ),
        static_cast<GLenum>(
            m_previousBlendDestinationRgb
        ),
        static_cast<GLenum>(
            m_previousBlendSourceAlpha
        ),
        static_cast<GLenum>(
            m_previousBlendDestinationAlpha
        )
    );

    if (m_blendWasEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);

    if (m_depthWasEnabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

    if (m_cullWasEnabled)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);

    glScissor(
        m_previousScissorBox[0],
        m_previousScissorBox[1],
        m_previousScissorBox[2],
        m_previousScissorBox[3]
    );

    if (m_scissorWasEnabled)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);

    glClearColor(
        m_previousClearColor[0],
        m_previousClearColor[1],
        m_previousClearColor[2],
        m_previousClearColor[3]
    );

    m_active = false;
}

} // namespace render::system_map
