// SystemMapRenderer: map background, starfield backdrop and the atmosphere veil.

#include "src/game/system_map/SystemMapRendererInternal.h"








































void SystemMapRenderer::ensureBackground()
{
    if (m_bgVao && m_bgVbo && m_bgShader)
        return;

    m_bgShader = ShaderLibrary::instance().get("system_map_background");

    if (!m_bgShader)
    {

        return;
    }

    const float verts[] =
    {
        -1.0f, -1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f,

        -1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f,  1.0f
    };

    glGenVertexArrays(1, &m_bgVao);
    glGenBuffers(1, &m_bgVbo);

    glBindVertexArray(m_bgVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_bgVbo);

    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(float) * 2,
        reinterpret_cast<void*>(0)
    );

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}










void SystemMapRenderer::drawBackground()
{
    if (!m_bgShader || !m_bgVao)
        return;

    const GLboolean depthWasEnabled =
        glIsEnabled(GL_DEPTH_TEST);

    const GLboolean blendWasEnabled =
        glIsEnabled(GL_BLEND);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    glUseProgram(m_bgShader);

    /*
        Проход 0 — обычный фон карты.
        Проход 1 используется только для transition snapshot.
    */
    const GLint passLoc =
        glGetUniformLocation(
            m_bgShader,
            "uPass"
        );

    if (passLoc >= 0)
    {
        glUniform1i(
            passLoc,
            0
        );
    }

    glBindVertexArray(m_bgVao);

    glDrawArrays(
        GL_TRIANGLES,
        0,
        6
    );

    glBindVertexArray(0);
    glUseProgram(0);

    if (depthWasEnabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

    if (blendWasEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
}




















void SystemMapRenderer::drawMapAtmosphereVeil(
    float centerAlpha,
    float edgeAlpha,
    float aquaStrength
)
{
    if (!m_bgShader || !m_bgVao)
        return;

    /*
        Теперь это не "alpha тёмной пелены", а сила затемнения
        уже нарисованного starfield.

        0.0 = не затемнять
        1.0 = полностью убить яркость
    */
    centerAlpha =
        std::clamp(
            centerAlpha,
            0.0f,
            0.95f
        );

    edgeAlpha =
        std::clamp(
            edgeAlpha,
            centerAlpha,
            0.95f
        );

    aquaStrength =
        std::clamp(
            aquaStrength,
            0.0f,
            1.0f
        );

    if (edgeAlpha <= 0.0f)
        return;

    const GLboolean depthWasEnabled =
        glIsEnabled(
            GL_DEPTH_TEST
        );

    const GLboolean blendWasEnabled =
        glIsEnabled(
            GL_BLEND
        );

    GLboolean previousDepthWriteMask =
        GL_TRUE;

    glGetBooleanv(
        GL_DEPTH_WRITEMASK,
        &previousDepthWriteMask
    );

    GLint previousBlendEquationRgb =
        GL_FUNC_ADD;

    GLint previousBlendEquationAlpha =
        GL_FUNC_ADD;

    GLint previousBlendSourceRgb =
        GL_ONE;

    GLint previousBlendDestinationRgb =
        GL_ZERO;

    GLint previousBlendSourceAlpha =
        GL_ONE;

    GLint previousBlendDestinationAlpha =
        GL_ZERO;

    glGetIntegerv(
        GL_BLEND_EQUATION_RGB,
        &previousBlendEquationRgb
    );

    glGetIntegerv(
        GL_BLEND_EQUATION_ALPHA,
        &previousBlendEquationAlpha
    );

    glGetIntegerv(
        GL_BLEND_SRC_RGB,
        &previousBlendSourceRgb
    );

    glGetIntegerv(
        GL_BLEND_DST_RGB,
        &previousBlendDestinationRgb
    );

    glGetIntegerv(
        GL_BLEND_SRC_ALPHA,
        &previousBlendSourceAlpha
    );

    glGetIntegerv(
        GL_BLEND_DST_ALPHA,
        &previousBlendDestinationAlpha
    );

    glDisable(
        GL_DEPTH_TEST
    );

    glDepthMask(
        GL_FALSE
    );

    glEnable(
        GL_BLEND
    );

    /*
        Самая важная часть.

        Мы не добавляем поверх тёмный цвет.
        Мы умножаем уже нарисованный starfield
        на вычисленный RGB-множитель из шейдера.
    */
    glBlendEquationSeparate(
        GL_FUNC_ADD,
        GL_FUNC_ADD
    );

    glBlendFuncSeparate(
        GL_ZERO,
        GL_SRC_COLOR,
        GL_ZERO,
        GL_ONE
    );

    glUseProgram(
        m_bgShader
    );

    const GLint passLoc =
        glGetUniformLocation(
            m_bgShader,
            "uPass"
        );

    const GLint centerAlphaLoc =
        glGetUniformLocation(
            m_bgShader,
            "uMapVeilCenterAlpha"
        );

    const GLint edgeAlphaLoc =
        glGetUniformLocation(
            m_bgShader,
            "uMapVeilEdgeAlpha"
        );

    const GLint aquaStrengthLoc =
        glGetUniformLocation(
            m_bgShader,
            "uMapVeilAquaStrength"
        );

    if (passLoc >= 0)
    {
        glUniform1i(
            passLoc,
            2
        );
    }

    if (centerAlphaLoc >= 0)
    {
        glUniform1f(
            centerAlphaLoc,
            centerAlpha
        );
    }

    if (edgeAlphaLoc >= 0)
    {
        glUniform1f(
            edgeAlphaLoc,
            edgeAlpha
        );
    }

    if (aquaStrengthLoc >= 0)
    {
        glUniform1f(
            aquaStrengthLoc,
            aquaStrength
        );
    }

    glBindVertexArray(
        m_bgVao
    );

    glDrawArrays(
        GL_TRIANGLES,
        0,
        6
    );

    glBindVertexArray(
        0
    );

    if (passLoc >= 0)
    {
        glUniform1i(
            passLoc,
            0
        );
    }

    glUseProgram(
        0
    );

    glBlendEquationSeparate(
        static_cast<GLenum>(
            previousBlendEquationRgb
        ),
        static_cast<GLenum>(
            previousBlendEquationAlpha
        )
    );

    glBlendFuncSeparate(
        static_cast<GLenum>(
            previousBlendSourceRgb
        ),
        static_cast<GLenum>(
            previousBlendDestinationRgb
        ),
        static_cast<GLenum>(
            previousBlendSourceAlpha
        ),
        static_cast<GLenum>(
            previousBlendDestinationAlpha
        )
    );

    glDepthMask(
        previousDepthWriteMask
    );

    if (depthWasEnabled)
    {
        glEnable(
            GL_DEPTH_TEST
        );
    }
    else
    {
        glDisable(
            GL_DEPTH_TEST
        );
    }

    if (blendWasEnabled)
    {
        glEnable(
            GL_BLEND
        );
    }
    else
    {
        glDisable(
            GL_BLEND
        );
    }
}
