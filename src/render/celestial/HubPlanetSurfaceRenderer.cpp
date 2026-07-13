#include "src/render/celestial/HubPlanetSurfaceRenderer.h"

#include <algorithm>
#include <iostream>

#include "src/render/ShaderLibrary.h"

namespace render::celestial
{

void HubPlanetSurfaceRenderer::ensureResources()
{
    if (m_shader == 0)
    {
        m_shader =
            ShaderLibrary::instance().get(
                "hub_planet_surface"
            );

        if (m_shader == 0)
        {
            if (!m_missingShaderWarningPrinted)
            {
                m_missingShaderWarningPrinted = true;

                std::cerr
                    << "[HubPlanetSurfaceRenderer]"
                    << " shader 'hub_planet_surface' is unavailable"
                    << "\n";
            }

            return;
        }

        m_albedoTextureLocation =
            glGetUniformLocation(
                m_shader,
                "uAlbedoTexture"
            );

        m_normalTextureLocation =
            glGetUniformLocation(
                m_shader,
                "uNormalTexture"
            );

        m_hasNormalTextureLocation =
            glGetUniformLocation(
                m_shader,
                "uHasNormalTexture"
            );

        m_viewportOriginLocation =
            glGetUniformLocation(
                m_shader,
                "uViewportOriginPx"
            );

        m_viewportSizeLocation =
            glGetUniformLocation(
                m_shader,
                "uViewportSize"
            );

        m_planetCenterLocation =
            glGetUniformLocation(
                m_shader,
                "uPlanetCenterPx"
            );

        m_planetRadiusLocation =
            glGetUniformLocation(
                m_shader,
                "uPlanetRadiusPx"
            );

        m_longitudeOffsetLocation =
            glGetUniformLocation(
                m_shader,
                "uLongitudeOffset"
            );

        m_lightDirectionLocation =
            glGetUniformLocation(
                m_shader,
                "uLightDirection"
            );

        m_ambientStrengthLocation =
            glGetUniformLocation(
                m_shader,
                "uAmbientStrength"
            );

        m_diffuseStrengthLocation =
            glGetUniformLocation(
                m_shader,
                "uDiffuseStrength"
            );

        m_normalStrengthLocation =
            glGetUniformLocation(
                m_shader,
                "uNormalStrength"
            );

        m_horizonDarkeningLocation =
            glGetUniformLocation(
                m_shader,
                "uHorizonDarkening"
            );

        m_surfaceContrastLocation =
            glGetUniformLocation(
                m_shader,
                "uSurfaceContrast"
            );

        m_surfaceSaturationLocation =
            glGetUniformLocation(
                m_shader,
                "uSurfaceSaturation"
            );
    }

    if (m_fullscreenVao == 0)
    {
        glGenVertexArrays(
            1,
            &m_fullscreenVao
        );
    }
}

void HubPlanetSurfaceRenderer::render(
    GLuint albedoTexture,
    GLuint normalTexture,
    const glm::dvec2& planetCenterPx,
    double planetRadiusPx,
    double longitudeOffset,
    const glm::vec3& lightDirection
)
{
    if (albedoTexture == 0 ||
        planetRadiusPx <= 1.0)
    {
        return;
    }

    ensureResources();

    if (m_shader == 0 ||
        m_fullscreenVao == 0)
    {
        return;
    }

    GLint viewport[4] =
    {
        0,
        0,
        1,
        1
    };

    glGetIntegerv(
        GL_VIEWPORT,
        viewport
    );

    GLint previousProgram = 0;
    GLint previousVao = 0;
    GLint previousActiveTexture = 0;

    GLint previousTexture0 = 0;
    GLint previousTexture1 = 0;

    glGetIntegerv(
        GL_CURRENT_PROGRAM,
        &previousProgram
    );

    glGetIntegerv(
        GL_VERTEX_ARRAY_BINDING,
        &previousVao
    );

    glGetIntegerv(
        GL_ACTIVE_TEXTURE,
        &previousActiveTexture
    );

    glActiveTexture(
        GL_TEXTURE0
    );

    glGetIntegerv(
        GL_TEXTURE_BINDING_2D,
        &previousTexture0
    );

    glActiveTexture(
        GL_TEXTURE1
    );

    glGetIntegerv(
        GL_TEXTURE_BINDING_2D,
        &previousTexture1
    );

    const GLboolean blendWasEnabled =
        glIsEnabled(
            GL_BLEND
        );

    const GLboolean depthWasEnabled =
        glIsEnabled(
            GL_DEPTH_TEST
        );

    const GLboolean cullWasEnabled =
        glIsEnabled(
            GL_CULL_FACE
        );

    const GLboolean scissorWasEnabled =
        glIsEnabled(
            GL_SCISSOR_TEST
        );

    glDisable(
        GL_BLEND
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

    glUseProgram(
        m_shader
    );

    glActiveTexture(
        GL_TEXTURE0
    );

    glBindTexture(
        GL_TEXTURE_2D,
        albedoTexture
    );

    glUniform1i(
        m_albedoTextureLocation,
        0
    );

    glActiveTexture(
        GL_TEXTURE1
    );

    glBindTexture(
        GL_TEXTURE_2D,
        normalTexture
    );

    glUniform1i(
        m_normalTextureLocation,
        1
    );

    glUniform1i(
        m_hasNormalTextureLocation,
        normalTexture != 0
            ? 1
            : 0
    );

    glUniform2f(
        m_viewportOriginLocation,
        static_cast<float>(
            viewport[0]
        ),
        static_cast<float>(
            viewport[1]
        )
    );

    glUniform2f(
        m_viewportSizeLocation,
        static_cast<float>(
            viewport[2]
        ),
        static_cast<float>(
            viewport[3]
        )
    );

    glUniform2f(
        m_planetCenterLocation,
        static_cast<float>(
            planetCenterPx.x
        ),
        static_cast<float>(
            planetCenterPx.y
        )
    );

    glUniform1f(
        m_planetRadiusLocation,
        static_cast<float>(
            planetRadiusPx
        )
    );

    glUniform1f(
        m_longitudeOffsetLocation,
        static_cast<float>(
            longitudeOffset
        )
    );

    glUniform3f(
        m_lightDirectionLocation,
        lightDirection.x,
        lightDirection.y,
        lightDirection.z
    );

    glUniform1f(
        m_ambientStrengthLocation,
        0.48f
    );

    glUniform1f(
        m_diffuseStrengthLocation,
        0.72f
    );

    glUniform1f(
        m_normalStrengthLocation,
        normalTexture != 0
            ? 0.85f
            : 4.0f
    );

    glUniform1f(
        m_horizonDarkeningLocation,
        0.58f
    );

    glUniform1f(
        m_surfaceContrastLocation,
        1.04f
    );

    glUniform1f(
        m_surfaceSaturationLocation,
        0.92f
    );

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
            previousVao
        )
    );

    glUseProgram(
        static_cast<GLuint>(
            previousProgram
        )
    );

    glActiveTexture(
        GL_TEXTURE0
    );

    glBindTexture(
        GL_TEXTURE_2D,
        static_cast<GLuint>(
            previousTexture0
        )
    );

    glActiveTexture(
        GL_TEXTURE1
    );

    glBindTexture(
        GL_TEXTURE_2D,
        static_cast<GLuint>(
            previousTexture1
        )
    );

    glActiveTexture(
        static_cast<GLenum>(
            previousActiveTexture
        )
    );

    if (blendWasEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);

    if (depthWasEnabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

    if (cullWasEnabled)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);

    if (scissorWasEnabled)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);
}

} // namespace render::celestial