#include "src/game/system_map/LocalMapAtmosphereRenderer.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include <glad/gl.h>
#include <glm/gtc/constants.hpp>

#include "src/render/ShaderLibrary.h"

namespace game::system_map
{
void drawLocalMapAtmosphereSoftBand(
    const glm::dvec2& planetCenterPx,
    double planetRadiusPx,
    const glm::vec4& peakColor,
    double startRadiusFactor,
    double peakRadiusFactor,
    double endRadiusFactor,
    int radialSteps,
    int segments
)
{
    if (planetRadiusPx <= 1.0)
        return;

    if (endRadiusFactor <= startRadiusFactor)
        return;

    if (peakColor.a <= 0.0001f)
        return;

    radialSteps =
        std::max(
            8,
            radialSteps
        );

    segments =
        std::max(
            96,
            segments
        );

    GLboolean textureWasEnabled =
        glIsEnabled(
            GL_TEXTURE_2D
        );

    GLboolean blendWasEnabled =
        glIsEnabled(
            GL_BLEND
        );

    GLboolean depthWasEnabled =
        glIsEnabled(
            GL_DEPTH_TEST
        );

    GLboolean depthMaskWasEnabled =
        GL_TRUE;

    glGetBooleanv(
        GL_DEPTH_WRITEMASK,
        &depthMaskWasEnabled
    );

    GLint oldTextureBinding = 0;

    glGetIntegerv(
        GL_TEXTURE_BINDING_2D,
        &oldTextureBinding
    );

    glUseProgram(
        0
    );

    // ВАЖНО:
    // Atmosphere band — это чистая цветная геометрия.
    // Если оставить GL_TEXTURE_2D включённым, fixed pipeline будет
    // умножать цвет на текущую текстуру и текущие texture coords.
    // В результате band может стать полностью невидимым.
    glDisable(
        GL_TEXTURE_2D
    );

    glBindTexture(
        GL_TEXTURE_2D,
        0
    );

    glEnable(
        GL_BLEND
    );

    glBlendFunc(
        GL_SRC_ALPHA,
        GL_ONE_MINUS_SRC_ALPHA
    );

    glDisable(
        GL_DEPTH_TEST
    );

    glDepthMask(
        GL_FALSE
    );

    const double startR =
        planetRadiusPx *
        startRadiusFactor;

    const double peakR =
        planetRadiusPx *
        peakRadiusFactor;

    const double endR =
        planetRadiusPx *
        endRadiusFactor;

    const double totalSpan =
        std::max(
            0.000001,
            endR - startR
        );

    const double peakT =
        std::clamp(
            (peakR - startR) / totalSpan,
            0.0,
            1.0
        );

    auto smoothStep =
        [](double edge0, double edge1, double x) -> double
        {
            const double t =
                std::clamp(
                    (x - edge0) /
                    std::max(
                        0.000001,
                        edge1 - edge0
                    ),
                    0.0,
                    1.0
                );

            return
                t * t *
                (3.0 - 2.0 * t);
        };

    auto alphaAt =
        [&](double t) -> float
        {
            double a = 0.0;

            if (t <= peakT)
            {
                a =
                    smoothStep(
                        0.0,
                        std::max(
                            0.000001,
                            peakT
                        ),
                        t
                    );
            }
            else
            {
                a =
                    1.0 -
                    smoothStep(
                        peakT,
                        1.0,
                        t
                    );
            }

            return static_cast<float>(
                a * peakColor.a
            );
        };

    for (int ring = 0; ring < radialSteps; ++ring)
    {
        const double t0 =
            static_cast<double>(ring) /
            static_cast<double>(radialSteps);

        const double t1 =
            static_cast<double>(ring + 1) /
            static_cast<double>(radialSteps);

        const double r0 =
            startR +
            (endR - startR) * t0;

        const double r1 =
            startR +
            (endR - startR) * t1;

        const float a0 =
            alphaAt(
                t0
            );

        const float a1 =
            alphaAt(
                t1
            );

        glBegin(
            GL_TRIANGLE_STRIP
        );

        for (int i = 0; i <= segments; ++i)
        {
            const double ang =
                glm::two_pi<double>() *
                static_cast<double>(i) /
                static_cast<double>(segments);

            const double ca =
                std::cos(
                    ang
                );

            const double sa =
                std::sin(
                    ang
                );

            glColor4f(
                peakColor.r,
                peakColor.g,
                peakColor.b,
                a0
            );

            glVertex2d(
                planetCenterPx.x + ca * r0,
                planetCenterPx.y + sa * r0
            );

            glColor4f(
                peakColor.r,
                peakColor.g,
                peakColor.b,
                a1
            );

            glVertex2d(
                planetCenterPx.x + ca * r1,
                planetCenterPx.y + sa * r1
            );
        }

        glEnd();
    }

    glDepthMask(
        depthMaskWasEnabled
    );

    if (depthWasEnabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

    glBindTexture(
        GL_TEXTURE_2D,
        static_cast<GLuint>(oldTextureBinding)
    );

    if (textureWasEnabled)
        glEnable(GL_TEXTURE_2D);
    else
        glDisable(GL_TEXTURE_2D);

    if (blendWasEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
}



void drawLocalMapAtmosphereStack(
    const glm::dvec2& planetCenterPx,
    double planetRadiusPx,
    const LocalMapAtmosphereStyle& style,
    bool premultipliedTarget
)
{
    if (!style.enabled ||
        planetRadiusPx <= 1.0)
    {
        return;
    }

    static GLuint atmosphereShader = 0;
    static GLuint fullscreenVao = 0;

    static GLint viewportOriginLocation = -1;
    static GLint viewportSizeLocation = -1;
    static GLint planetCenterLocation = -1;
    static GLint planetRadiusLocation = -1;

    static GLint radiusScaleLocation = -1;
    static GLint visualIntensityLocation = -1;

    static GLint surfaceHazeLocation = -1;
    static GLint limbCoreLocation = -1;
    static GLint nearAtmosphereLocation = -1;
    static GLint outerAtmosphereLocation = -1;

    if (atmosphereShader == 0)
    {
        atmosphereShader =
            ShaderLibrary::instance().get(
                "hub_planet_atmosphere"
            );

        if (atmosphereShader == 0)
        {
            static bool warned = false;

            if (!warned)
            {
                warned = true;

                std::cerr
                    << "[HubAtmosphere]"
                    << " shader not available"
                    << "\n";
            }

            return;
        }

        viewportOriginLocation =
            glGetUniformLocation(
                atmosphereShader,
                "uViewportOriginPx"
            );

        viewportSizeLocation =
            glGetUniformLocation(
                atmosphereShader,
                "uViewportSize"
            );

        planetCenterLocation =
            glGetUniformLocation(
                atmosphereShader,
                "uPlanetCenterPx"
            );

        planetRadiusLocation =
            glGetUniformLocation(
                atmosphereShader,
                "uPlanetRadiusPx"
            );

        radiusScaleLocation =
            glGetUniformLocation(
                atmosphereShader,
                "uRadiusScale"
            );

        visualIntensityLocation =
            glGetUniformLocation(
                atmosphereShader,
                "uVisualIntensity"
            );

        surfaceHazeLocation =
            glGetUniformLocation(
                atmosphereShader,
                "uSurfaceHaze"
            );

        limbCoreLocation =
            glGetUniformLocation(
                atmosphereShader,
                "uLimbCore"
            );

        nearAtmosphereLocation =
            glGetUniformLocation(
                atmosphereShader,
                "uNearAtmosphere"
            );

        outerAtmosphereLocation =
            glGetUniformLocation(
                atmosphereShader,
                "uOuterAtmosphere"
            );
    }

    if (fullscreenVao == 0)
    {
        glGenVertexArrays(
            1,
            &fullscreenVao
        );
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

    glGetIntegerv(
        GL_CURRENT_PROGRAM,
        &previousProgram
    );

    glGetIntegerv(
        GL_VERTEX_ARRAY_BINDING,
        &previousVao
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


    GLint previousBlendSourceRgb =
        GL_SRC_ALPHA;

    GLint previousBlendDestinationRgb =
        GL_ONE_MINUS_SRC_ALPHA;

    GLint previousBlendSourceAlpha =
        GL_SRC_ALPHA;

    GLint previousBlendDestinationAlpha =
        GL_ONE_MINUS_SRC_ALPHA;


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



    /*
        Atmosphere shader нужен только в прямоугольнике,
        содержащем внешнюю атмосферную оболочку.
    */
    const double atmosphereOuterRadiusScale =
        std::max(
            1.001,
            static_cast<double>(
                style.radiusScale
            ) +
            0.075
        );

    const double atmosphereRadiusPx =
        planetRadiusPx *
        atmosphereOuterRadiusScale;

    const int localLeft =
        static_cast<int>(
            std::floor(
                planetCenterPx.x -
                atmosphereRadiusPx -
                2.0
            )
        );

    const int localRight =
        static_cast<int>(
            std::ceil(
                planetCenterPx.x +
                atmosphereRadiusPx +
                2.0
            )
        );

    const int localTop =
        static_cast<int>(
            std::floor(
                planetCenterPx.y -
                atmosphereRadiusPx -
                2.0
            )
        );

    const int localBottom =
        static_cast<int>(
            std::ceil(
                planetCenterPx.y +
                atmosphereRadiusPx +
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

    /*
        planetCenterPx использует начало координат сверху,
        glScissor — снизу.
    */
    const int scissorX =
        viewport[0] +
        clippedLeft;

    const int scissorY =
        viewport[1] +
        viewport[3] -
        clippedBottom;




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



    glEnable(
        GL_BLEND
    );

    if (premultipliedTarget)
    {
        /*
            RGB остаётся premultiplied внутри прозрачного
            half-resolution overlay.

            Alpha накапливается отдельно.
        */
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





    glUseProgram(
        atmosphereShader
    );

    glUniform2f(
        viewportOriginLocation,
        static_cast<float>(viewport[0]),
        static_cast<float>(viewport[1])
    );

    glUniform2f(
        viewportSizeLocation,
        static_cast<float>(
            viewport[2]
        ),
        static_cast<float>(
            viewport[3]
        )
    );

    glUniform2f(
        planetCenterLocation,
        static_cast<float>(
            planetCenterPx.x
        ),
        static_cast<float>(
            planetCenterPx.y
        )
    );

    glUniform1f(
        planetRadiusLocation,
        static_cast<float>(
            planetRadiusPx
        )
    );

    glUniform1f(
        radiusScaleLocation,
        style.radiusScale
    );

    glUniform1f(
        visualIntensityLocation,
        style.visualIntensity
    );

    glUniform4f(
        surfaceHazeLocation,
        style.surfaceHaze.r,
        style.surfaceHaze.g,
        style.surfaceHaze.b,
        style.surfaceHaze.a
    );

    glUniform4f(
        limbCoreLocation,
        style.limbCore.r,
        style.limbCore.g,
        style.limbCore.b,
        style.limbCore.a
    );

    glUniform4f(
        nearAtmosphereLocation,
        style.nearAtmosphere.r,
        style.nearAtmosphere.g,
        style.nearAtmosphere.b,
        style.nearAtmosphere.a
    );

    glUniform4f(
        outerAtmosphereLocation,
        style.outerAtmosphere.r,
        style.outerAtmosphere.g,
        style.outerAtmosphere.b,
        style.outerAtmosphere.a
    );

    glBindVertexArray(
        fullscreenVao
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

    glScissor(
        previousScissorBox[0],
        previousScissorBox[1],
        previousScissorBox[2],
        previousScissorBox[3]
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



}
