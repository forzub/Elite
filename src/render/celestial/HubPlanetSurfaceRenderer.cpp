#include "src/render/celestial/HubPlanetSurfaceRenderer.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include <glm/gtc/type_ptr.hpp>

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

        m_cameraToPlanetBodyLocation =
            glGetUniformLocation(
                m_shader,
                "uCameraToPlanetBody"
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

        m_aerialPerspectiveColorLocation =
            glGetUniformLocation(
                m_shader,
                "uAerialPerspectiveColor"
            );

        m_aerialPerspectiveStrengthLocation =
            glGetUniformLocation(
                m_shader,
                "uAerialPerspectiveStrength"
            );

        m_aerialPerspectiveStartLocation =
            glGetUniformLocation(
                m_shader,
                "uAerialPerspectiveStart"
            );

        m_aerialPerspectiveEndLocation =
            glGetUniformLocation(
                m_shader,
                "uAerialPerspectiveEnd"
            );

        m_aerialPerspectiveBlurLodLocation =
            glGetUniformLocation(
                m_shader,
                "uAerialPerspectiveBlurLod"
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
    const glm::mat3& cameraToPlanetBody,
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


        GLint previousScissorBox[4] =
        {
            0,
            0,
            1,
            1
        };

        glGetIntegerv(
            GL_SCISSOR_BOX,
            previousScissorBox
        );

        /*
            Fullscreen shader реально нужен только в квадрате,
            содержащем диск планеты.
        */
        const int localLeft =
            static_cast<int>(
                std::floor(
                    planetCenterPx.x -
                    planetRadiusPx -
                    2.0
                )
            );

        const int localRight =
            static_cast<int>(
                std::ceil(
                    planetCenterPx.x +
                    planetRadiusPx +
                    2.0
                )
            );

        const int localTop =
            static_cast<int>(
                std::floor(
                    planetCenterPx.y -
                    planetRadiusPx -
                    2.0
                )
            );

        const int localBottom =
            static_cast<int>(
                std::ceil(
                    planetCenterPx.y +
                    planetRadiusPx +
                    2.0
                )
            );

        const int clippedLeft =
            std::clamp(
                localLeft,
                0,
                viewport[2]
            );

        const int clippedRight =
            std::clamp(
                localRight,
                0,
                viewport[2]
            );

        const int clippedTop =
            std::clamp(
                localTop,
                0,
                viewport[3]
            );

        const int clippedBottom =
            std::clamp(
                localBottom,
                0,
                viewport[3]
            );

        const int scissorWidth =
            clippedRight -
            clippedLeft;

        const int scissorHeight =
            clippedBottom -
            clippedTop;

        if (scissorWidth <= 0 ||
            scissorHeight <= 0)
        {
            return;
        }

        const int scissorX =
            viewport[0] +
            clippedLeft;

        const int scissorY =
            viewport[1] +
            viewport[3] -
            clippedBottom;


    glDisable(
        GL_BLEND
    );

    glDisable(
        GL_DEPTH_TEST
    );

    glDisable(
        GL_CULL_FACE
    );

    glEnable(
        GL_SCISSOR_TEST
    );

    glScissor(
        scissorX,
        scissorY,
        scissorWidth,
        scissorHeight
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

    glUniformMatrix3fv(
        m_cameraToPlanetBodyLocation,
        1,
        GL_FALSE,
        glm::value_ptr(
            cameraToPlanetBody
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

    glUniform3f(
        m_aerialPerspectiveColorLocation,
        0.78f,
        0.84f,
        0.92f
    );

    glUniform1f(
        m_aerialPerspectiveStrengthLocation,
        0.62f
    );

    glUniform1f(
        m_aerialPerspectiveStartLocation,
        0.18f
    );

    glUniform1f(
        m_aerialPerspectiveEndLocation,
        0.92f
    );

    glUniform1f(
        m_aerialPerspectiveBlurLodLocation,
        2.6f
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



    glScissor(
        previousScissorBox[0],
        previousScissorBox[1],
        previousScissorBox[2],
        previousScissorBox[3]
    );




    if (scissorWasEnabled)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);
}

} // namespace render::celestial