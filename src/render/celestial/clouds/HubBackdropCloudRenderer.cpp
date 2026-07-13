#include "src/render/celestial/clouds/HubBackdropCloudRenderer.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include <glad/gl.h>
#include <glm/gtc/constants.hpp>











namespace
{
    float smoothStep(
        float edge0,
        float edge1,
        float value
    )
    {
        const float denominator =
            std::max(
                0.000001f,
                edge1 - edge0
            );

        const float t =
            std::clamp(
                (value - edge0) /
                denominator,
                0.0f,
                1.0f
            );

        return
            t *
            t *
            (3.0f - 2.0f * t);
    }
}









namespace render::celestial
{
    void HubBackdropCloudRenderer::render(
        ProceduralCloudLayer& textureSource,
        const glm::dvec2& planetCenterPx,
        double planetRadiusPx,
        double cloudRadiusScale,
        double longitudeOffset,
        double timeSeconds,
        const ProceduralCloudStyle& style
    )
    {
        if (!style.enabled ||
            planetRadiusPx <= 1.0||
            cloudRadiusScale <= 0.0)
        {
            return;
        }

        const GLuint texture =
        textureSource.textureForStyle(
            style,
            timeSeconds
        );

        if (texture == 0)
            return;






        static GLuint lastLoggedHubTexture = 0;

        if (lastLoggedHubTexture != texture)
        {
            lastLoggedHubTexture = texture;

            std::cout
                << "[HubBackdropCloudRenderer]"
                << " texture=" << texture
                << " enabled=" << style.enabled
                << " opacity=" << style.opacity
                << " radiusPx=" << planetRadiusPx
                << " cloudRadiusScale=" << cloudRadiusScale
                << "\n";
        }












        const double cloudRadiusPx =
            planetRadiusPx *
            cloudRadiusScale;

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

        const double viewW =
            static_cast<double>(
                std::max(
                    viewport[2],
                    1
                )
            );

        const double viewH =
            static_cast<double>(
                std::max(
                    viewport[3],
                    1
                )
            );

        GLboolean textureWasEnabled =
            glIsEnabled(
                GL_TEXTURE_2D
            );

        GLboolean blendWasEnabled =
            glIsEnabled(
                GL_BLEND
            );

        GLint oldTextureBinding =
            0;

        glGetIntegerv(
            GL_TEXTURE_BINDING_2D,
            &oldTextureBinding
        );

        glUseProgram(0);

        glEnable(
            GL_TEXTURE_2D
        );

        glBindTexture(
            GL_TEXTURE_2D,
            texture
        );

        GLint oldTextureEnvMode =
            GL_MODULATE;

        glGetTexEnviv(
            GL_TEXTURE_ENV,
            GL_TEXTURE_ENV_MODE,
            &oldTextureEnvMode
        );

        glTexEnvi(
            GL_TEXTURE_ENV,
            GL_TEXTURE_ENV_MODE,
            GL_MODULATE
        );

        glEnable(
            GL_BLEND
        );

        glBlendFunc(
            GL_SRC_ALPHA,
            GL_ONE_MINUS_SRC_ALPHA
        );

        const int gridX = 144;
        const int gridY = 96;

        const double cellW =
            viewW /
            static_cast<double>(gridX);

        const double cellH =
            viewH /
            static_cast<double>(gridY);

        auto emit =
            [&](double sx, double sy)
            {
                const double nx =
                    (
                        sx -
                        planetCenterPx.x
                    ) /
                    cloudRadiusPx;

                const double ny =
                    -(
                        sy -
                        planetCenterPx.y
                    ) /
                    cloudRadiusPx;

                const double rr =
                    nx * nx +
                    ny * ny;

                if (rr > 1.0)
                    return;

                const double nz =
                    std::sqrt(
                        std::max(
                            0.0,
                            1.0 - rr
                        )
                    );

                const double lon =
                    std::atan2(
                        nx,
                        std::max(
                            0.000001,
                            nz
                        )
                    );

                const double lat =
                    std::asin(
                        std::clamp(
                            ny,
                            -0.98,
                            0.98
                        )
                    );

                




                const double baseU =
                0.5 +
                lon / glm::two_pi<double>();

            // Держим drift в ограниченном диапазоне,
            // но НЕ clamp-им итоговый u.
            // GL_REPEAT должен повторять текстуру по долготе.
            const double driftU =
                std::fmod(
                    timeSeconds *
                        static_cast<double>(style.driftSpeed),
                    1.0
                );

            double u =
                baseU +
                longitudeOffset +
                driftU;

            double v =
                std::clamp(
                    0.5 -
                        lat / glm::pi<double>(),
                    0.02,
                    0.98
                );

                







                const double limb =
                    std::sqrt(
                        std::max(
                            0.0,
                            1.0 - rr
                        )
                    );

                // К самому горизонту облака должны растворяться
                // в атмосферном haze, а не резаться жёсткой линией.
                const double horizonFade =
                    smoothStep(
                        0.05f,
                        0.32f,
                        static_cast<float>(limb)
                    );

                const float renderOpacity =
                    std::clamp(
                        style.opacity * 2.6f,
                        0.0f,
                        0.72f
                    );

                const float alpha =
                    static_cast<float>(
                        renderOpacity *
                        horizonFade
                    );

                glColor4f(
                    1.0f,
                    1.0f,
                    1.0f,
                    alpha
                );

                glTexCoord2d(
                    u,
                    v
                );

                glVertex2d(
                    sx,
                    sy
                );
            };

        glBegin(
            GL_TRIANGLES
        );

        for (int iy = 0; iy < gridY; ++iy)
        {
            const double y0 =
                static_cast<double>(iy) *
                cellH;

            const double y1 =
                static_cast<double>(iy + 1) *
                cellH;

            for (int ix = 0; ix < gridX; ++ix)
            {
                const double x0 =
                    static_cast<double>(ix) *
                    cellW;

                const double x1 =
                    static_cast<double>(ix + 1) *
                    cellW;

                const double nx00 =
                    (x0 - planetCenterPx.x) /
                    cloudRadiusPx;

                const double ny00 =
                    -(y0 - planetCenterPx.y) /
                    cloudRadiusPx;

                const double nx10 =
                    (x1 - planetCenterPx.x) /
                    cloudRadiusPx;

                const double ny10 =
                    -(y0 - planetCenterPx.y) /
                    cloudRadiusPx;

                const double nx11 =
                    (x1 - planetCenterPx.x) /
                    cloudRadiusPx;

                const double ny11 =
                    -(y1 - planetCenterPx.y) /
                    cloudRadiusPx;

                const double nx01 =
                    (x0 - planetCenterPx.x) /
                    cloudRadiusPx;

                const double ny01 =
                    -(y1 - planetCenterPx.y) /
                    cloudRadiusPx;

                const bool q00 =
                    nx00 * nx00 +
                    ny00 * ny00 <= 1.0;

                const bool q10 =
                    nx10 * nx10 +
                    ny10 * ny10 <= 1.0;

                const bool q11 =
                    nx11 * nx11 +
                    ny11 * ny11 <= 1.0;

                const bool q01 =
                    nx01 * nx01 +
                    ny01 * ny01 <= 1.0;

                if (q00 && q10 && q11)
                {
                    emit(x0, y0);
                    emit(x1, y0);
                    emit(x1, y1);
                }

                if (q00 && q11 && q01)
                {
                    emit(x0, y0);
                    emit(x1, y1);
                    emit(x0, y1);
                }
            }
        }

        

        glEnd();

        glTexEnvi(
            GL_TEXTURE_ENV,
            GL_TEXTURE_ENV_MODE,
            oldTextureEnvMode
        );

        glBindTexture(
            GL_TEXTURE_2D,
            static_cast<GLuint>(oldTextureBinding)
        );

        if (textureWasEnabled)
        {
            glEnable(
                GL_TEXTURE_2D
            );
        }
        else
        {
            glDisable(
                GL_TEXTURE_2D
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
}