#include "src/render/celestial/PlanetGlobeMeshRenderer.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>
#include <cstddef>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "src/render/ShaderLibrary.h"

namespace render::celestial
{

namespace
{

struct GlobeVertex
{
    glm::vec3 position {
        0.0f
    };

    glm::vec2 uv {
        0.0f
    };
};

} // namespace


void PlanetGlobeMeshRenderer::ensureResources()
{
    if (m_shader == 0)
    {
        m_shader =
            ShaderLibrary::instance().get(
                "planet_globe_mesh"
            );

        if (m_shader == 0)
        {
            if (!m_missingShaderWarningPrinted)
            {
                m_missingShaderWarningPrinted = true;

                std::cerr
                    << "[PlanetGlobeMeshRenderer]"
                    << " shader 'planet_globe_mesh'"
                    << " is unavailable\n";
            }

            return;
        }

        m_textureLocation =
            glGetUniformLocation(
                m_shader,
                "uTexture"
            );

        m_bodyToCameraLocation =
            glGetUniformLocation(
                m_shader,
                "uBodyToCamera"
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

        m_longitudeUvOffsetLocation =
            glGetUniformLocation(
                m_shader,
                "uLongitudeUvOffset"
            );

        m_flipVLocation =
            glGetUniformLocation(
                m_shader,
                "uFlipV"
            );

        m_colorLocation =
            glGetUniformLocation(
                m_shader,
                "uColor"
            );

        m_opacityLocation =
            glGetUniformLocation(
                m_shader,
                "uOpacity"
            );

        m_useHorizonFadeLocation =
            glGetUniformLocation(
                m_shader,
                "uUseHorizonFade"
            );

        m_horizonFadeStartLocation =
            glGetUniformLocation(
                m_shader,
                "uHorizonFadeStart"
            );

        m_horizonFadeEndLocation =
            glGetUniformLocation(
                m_shader,
                "uHorizonFadeEnd"
            );

        m_usePolarFadeLocation =
            glGetUniformLocation(
                m_shader,
                "uUsePolarFade"
            );

        m_polarFadeStartLocation =
            glGetUniformLocation(
                m_shader,
                "uPolarFadeStart"
            );

        m_polarFadeEndLocation =
            glGetUniformLocation(
                m_shader,
                "uPolarFadeEnd"
            );

        glUseProgram(
            m_shader
        );

        if (m_textureLocation >= 0)
        {
            glUniform1i(
                m_textureLocation,
                0
            );
        }

        glUseProgram(
            0
        );
    }

    if (m_vao == 0)
    {
        createSphereMesh();
    }
}


void PlanetGlobeMeshRenderer::createSphereMesh()
{
    /*
        Одна сфера используется всеми планетами и всеми
        облачными слоями.

        64 x 128 выше старого surface LOD 48 x 96
        и немного выше cloud LOD 56 x 112.
    */
    constexpr int latitudeSegments =
        64;

    constexpr int longitudeSegments =
        128;

    std::vector<GlobeVertex> vertices;

    vertices.reserve(
        static_cast<std::size_t>(
            latitudeSegments + 1
        ) *
        static_cast<std::size_t>(
            longitudeSegments + 1
        )
    );

    for (int latitudeIndex = 0;
         latitudeIndex <= latitudeSegments;
         ++latitudeIndex)
    {
        const float v =
            static_cast<float>(
                latitudeIndex
            ) /
            static_cast<float>(
                latitudeSegments
            );

        const float latitude =
            -glm::half_pi<float>() +
            v *
                glm::pi<float>();

        const float cosLatitude =
            std::cos(
                latitude
            );

        const float sinLatitude =
            std::sin(
                latitude
            );

        for (int longitudeIndex = 0;
             longitudeIndex <= longitudeSegments;
             ++longitudeIndex)
        {
            const float u =
                static_cast<float>(
                    longitudeIndex
                ) /
                static_cast<float>(
                    longitudeSegments
                );

            const float longitude =
                -glm::pi<float>() +
                u *
                    glm::two_pi<float>();

            GlobeVertex vertex;

            /*
                Эта система совпадает с
                planetSurfacePointMeters():

                    X -> prime axis
                    Y -> north
                    Z -> east
            */
            vertex.position =
                glm::vec3(
                    cosLatitude *
                        std::cos(longitude),

                    sinLatitude,

                    cosLatitude *
                        std::sin(longitude)
                );

            vertex.uv =
                glm::vec2(
                    u,
                    v
                );

            vertices.push_back(
                vertex
            );
        }
    }

    std::vector<std::uint32_t> indices;

    indices.reserve(
        static_cast<std::size_t>(
            latitudeSegments
        ) *
        static_cast<std::size_t>(
            longitudeSegments
        ) *
        6u
    );

    const int rowStride =
        longitudeSegments +
        1;

    for (int latitudeIndex = 0;
         latitudeIndex < latitudeSegments;
         ++latitudeIndex)
    {
        for (int longitudeIndex = 0;
             longitudeIndex < longitudeSegments;
             ++longitudeIndex)
        {
            const std::uint32_t i00 =
                static_cast<std::uint32_t>(
                    latitudeIndex *
                    rowStride +
                    longitudeIndex
                );

            const std::uint32_t i10 =
                i00 +
                1u;

            const std::uint32_t i01 =
                static_cast<std::uint32_t>(
                    (
                        latitudeIndex +
                        1
                    ) *
                    rowStride +
                    longitudeIndex
                );

            const std::uint32_t i11 =
                i01 +
                1u;

            /*
                Внешнее CCW-направление.

                Fragment shader всё равно дополнительно
                отбрасывает cameraNormal.z <= 0,
                поэтому результат не зависит от режима culling.
            */
            indices.push_back(i00);
            indices.push_back(i11);
            indices.push_back(i10);

            indices.push_back(i00);
            indices.push_back(i01);
            indices.push_back(i11);
        }
    }

    GLint previousVao = 0;
    GLint previousArrayBuffer = 0;

    glGetIntegerv(
        GL_VERTEX_ARRAY_BINDING,
        &previousVao
    );

    glGetIntegerv(
        GL_ARRAY_BUFFER_BINDING,
        &previousArrayBuffer
    );

    glGenVertexArrays(
        1,
        &m_vao
    );

    glGenBuffers(
        1,
        &m_vertexBuffer
    );

    glGenBuffers(
        1,
        &m_indexBuffer
    );

    glBindVertexArray(
        m_vao
    );

    glBindBuffer(
        GL_ARRAY_BUFFER,
        m_vertexBuffer
    );

    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(
            vertices.size() *
            sizeof(GlobeVertex)
        ),
        vertices.data(),
        GL_STATIC_DRAW
    );

    glBindBuffer(
        GL_ELEMENT_ARRAY_BUFFER,
        m_indexBuffer
    );

    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(
            indices.size() *
            sizeof(std::uint32_t)
        ),
        indices.data(),
        GL_STATIC_DRAW
    );

    glEnableVertexAttribArray(
        0
    );

    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(GlobeVertex),
        reinterpret_cast<void*>(
            offsetof(
                GlobeVertex,
                position
            )
        )
    );

    glEnableVertexAttribArray(
        1
    );

    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(GlobeVertex),
        reinterpret_cast<void*>(
            offsetof(
                GlobeVertex,
                uv
            )
        )
    );

    m_indexCount =
        static_cast<GLsizei>(
            indices.size()
        );

    glBindVertexArray(
        static_cast<GLuint>(
            previousVao
        )
    );

    glBindBuffer(
        GL_ARRAY_BUFFER,
        static_cast<GLuint>(
            previousArrayBuffer
        )
    );

    std::cout
        << "[PlanetGlobeMeshRenderer]"
        << " sphere created"
        << " vertices=" << vertices.size()
        << " indices=" << indices.size()
        << "\n";
}


void PlanetGlobeMeshRenderer::render(
    const PlanetGlobeLayerDraw& draw
)
{
    if (draw.texture == 0 ||
        draw.radiusPx <= 1.0)
    {
        return;
    }

    ensureResources();

    if (m_shader == 0 ||
        m_vao == 0 ||
        m_indexCount <= 0)
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

    GLint previousBlendSourceRgb = GL_SRC_ALPHA;
    GLint previousBlendDestinationRgb = GL_ONE_MINUS_SRC_ALPHA;
    GLint previousBlendSourceAlpha = GL_SRC_ALPHA;
    GLint previousBlendDestinationAlpha = GL_ONE_MINUS_SRC_ALPHA;

    GLint previousBlendEquationRgb = GL_FUNC_ADD;
    GLint previousBlendEquationAlpha = GL_FUNC_ADD;

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

    glGetIntegerv(
        GL_BLEND_EQUATION_RGB,
        &previousBlendEquationRgb
    );

    glGetIntegerv(
        GL_BLEND_EQUATION_ALPHA,
        &previousBlendEquationAlpha
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

    GLint previousCullFaceMode = GL_BACK;
    GLint previousFrontFace = GL_CCW;

    glGetIntegerv(
        GL_CULL_FACE_MODE,
        &previousCullFaceMode
    );

    glGetIntegerv(
        GL_FRONT_FACE,
        &previousFrontFace
    );





    glDisable(
        GL_DEPTH_TEST
    );

    /*
        bodyToCamera может менять handedness базиса,
        поэтому winding экранных треугольников не гарантирован.

        Видимую полусферу уже корректно отбрасывает
        planet_globe_mesh.frag через cameraNormal.z.
    */
    glDisable(
        GL_CULL_FACE
    );

    if (draw.blending)
    {
        glEnable(
            GL_BLEND
        );

        glBlendEquation(
            GL_FUNC_ADD
        );



        if (draw.premultipliedTarget)
        {
            glBlendFuncSeparate(
                GL_SRC_ALPHA,
                GL_ONE_MINUS_SRC_ALPHA,
                GL_ONE,
                GL_ONE_MINUS_SRC_ALPHA
            );
        }
        else
        {
            glBlendFunc(
                GL_SRC_ALPHA,
                GL_ONE_MINUS_SRC_ALPHA
            );
        }






    }
    else
    {
        glDisable(
            GL_BLEND
        );
    }

    glUseProgram(
        m_shader
    );

    glActiveTexture(
        GL_TEXTURE0
    );

    glBindTexture(
        GL_TEXTURE_2D,
        draw.texture
    );

    if (m_bodyToCameraLocation >= 0)
    {
        glUniformMatrix3fv(
            m_bodyToCameraLocation,
            1,
            GL_FALSE,
            glm::value_ptr(
                draw.bodyToCamera
            )
        );
    }

    if (m_viewportSizeLocation >= 0)
    {
        glUniform2f(
            m_viewportSizeLocation,
            static_cast<float>(
                viewport[2]
            ),
            static_cast<float>(
                viewport[3]
            )
        );
    }

    if (m_planetCenterLocation >= 0)
    {
        glUniform2f(
            m_planetCenterLocation,
            static_cast<float>(
                draw.centerPx.x
            ),
            static_cast<float>(
                draw.centerPx.y
            )
        );
    }

    if (m_planetRadiusLocation >= 0)
    {
        glUniform1f(
            m_planetRadiusLocation,
            static_cast<float>(
                draw.radiusPx
            )
        );
    }

    if (m_longitudeUvOffsetLocation >= 0)
    {
        glUniform1f(
            m_longitudeUvOffsetLocation,
            draw.longitudeUvOffset
        );
    }

    if (m_flipVLocation >= 0)
    {
        glUniform1i(
            m_flipVLocation,
            draw.flipV
                ? 1
                : 0
        );
    }

    if (m_colorLocation >= 0)
    {
        glUniform4f(
            m_colorLocation,
            draw.color.r,
            draw.color.g,
            draw.color.b,
            draw.color.a
        );
    }

    if (m_opacityLocation >= 0)
    {
        glUniform1f(
            m_opacityLocation,
            std::clamp(
                draw.opacity,
                0.0f,
                1.0f
            )
        );
    }

    if (m_useHorizonFadeLocation >= 0)
    {
        glUniform1i(
            m_useHorizonFadeLocation,
            draw.useHorizonFade
                ? 1
                : 0
        );
    }

    if (m_horizonFadeStartLocation >= 0)
    {
        glUniform1f(
            m_horizonFadeStartLocation,
            draw.horizonFadeStart
        );
    }

    if (m_horizonFadeEndLocation >= 0)
    {
        glUniform1f(
            m_horizonFadeEndLocation,
            draw.horizonFadeEnd
        );
    }

    if (m_usePolarFadeLocation >= 0)
    {
        glUniform1i(
            m_usePolarFadeLocation,
            draw.usePolarFade
                ? 1
                : 0
        );
    }

    if (m_polarFadeStartLocation >= 0)
    {
        glUniform1f(
            m_polarFadeStartLocation,
            draw.polarFadeStart
        );
    }

    if (m_polarFadeEndLocation >= 0)
    {
        glUniform1f(
            m_polarFadeEndLocation,
            draw.polarFadeEnd
        );
    }

    glBindVertexArray(
        m_vao
    );

    glDrawElements(
        GL_TRIANGLES,
        m_indexCount,
        GL_UNSIGNED_INT,
        nullptr
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
        static_cast<GLenum>(
            previousActiveTexture
        )
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

    if (blendWasEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);

    if (depthWasEnabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);


    glCullFace(
        static_cast<GLenum>(
            previousCullFaceMode
        )
    );

    glFrontFace(
        static_cast<GLenum>(
            previousFrontFace
        )
    );



    if (cullWasEnabled)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);
}

} // namespace render::celestial