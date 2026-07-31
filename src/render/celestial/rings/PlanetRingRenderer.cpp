#include "src/render/celestial/rings/PlanetRingRenderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>

#include <glm/gtc/type_ptr.hpp>

#include "src/render/ShaderLibrary.h"

namespace render::celestial::rings
{

void PlanetRingRenderer::ensureResources()
{
    if (m_shader == 0)
    {
        m_shader =
            ShaderLibrary::instance().get(
                "planet_rings"
            );

        if (m_shader == 0)
        {
            if (!m_missingShaderWarningPrinted)
            {
                m_missingShaderWarningPrinted =
                    true;

                std::cerr
                    << "[PlanetRingRenderer]"
                    << " shader 'planet_rings'"
                    << " is unavailable\n";
            }

            return;
        }

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

        m_ringAxisXLocation =
            glGetUniformLocation(
                m_shader,
                "uRingAxisXPx"
            );

        m_ringAxisYLocation =
            glGetUniformLocation(
                m_shader,
                "uRingAxisYPx"
            );

        m_ringDepthCoefficientsLocation =
            glGetUniformLocation(
                m_shader,
                "uRingDepthCoefficients"
            );

        m_renderPartLocation =
            glGetUniformLocation(
                m_shader,
                "uRenderPart"
            );

        m_patternPhaseLocation =
            glGetUniformLocation(
                m_shader,
                "uPatternPhase"
            );

        m_opacityScaleLocation =
            glGetUniformLocation(
                m_shader,
                "uOpacityScale"
            );

        m_visualModeLocation =
            glGetUniformLocation(
                m_shader,
                "uVisualMode"
            );

        m_visualBandEmphasisLocation =
            glGetUniformLocation(
                m_shader,
                "uVisualBandEmphasis"
            );

        m_visualStructureLocation =
            glGetUniformLocation(
                m_shader,
                "uVisualStructure"
            );

        m_visualParticleLocation =
            glGetUniformLocation(
                m_shader,
                "uVisualParticle"
            );

        m_visualParticleShapeLocation =
            glGetUniformLocation(
                m_shader,
                "uVisualParticleShape"
            );

        m_visualOcclusionLocation =
            glGetUniformLocation(
                m_shader,
                "uVisualOcclusion"
            );


        m_visualLodLocation =
            glGetUniformLocation(
                m_shader,
                "uVisualLod"
            );




        m_bandCountLocation =
            glGetUniformLocation(
                m_shader,
                "uBandCount"
            );

        m_bandGeometryLocation =
            glGetUniformLocation(
                m_shader,
                "uBandGeometry[0]"
            );

        m_bandColorOpticalLocation =
            glGetUniformLocation(
                m_shader,
                "uBandColorOptical[0]"
            );

        m_bandAppearanceLocation =
            glGetUniformLocation(
                m_shader,
                "uBandAppearance[0]"
            );

        m_bandExtraLocation =
            glGetUniformLocation(
                m_shader,
                "uBandExtra[0]"
            );

        m_bandParticleLocation =
            glGetUniformLocation(
                m_shader,
                "uBandParticle[0]"
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

void PlanetRingRenderer::render(
    const PlanetRingRenderContext& context,
    PlanetRingRenderPart part
)
{
    if (!context.bands ||
        context.bands->empty())
    {
        return;
    }



    glm::dvec2 ringAxisX =
        context.ringAxisXPx;

    glm::dvec2 ringAxisY =
        context.ringAxisYPx;

    const double axisXLength =
        glm::length(ringAxisX);

    const double axisYLength =
        glm::length(ringAxisY);

    const double maximumAxisLength =
        std::max(
            axisXLength,
            axisYLength
        );

    if (maximumAxisLength <= 0.000001)
    {
        return;
    }

    double determinant =
        ringAxisX.x * ringAxisY.y -
        ringAxisX.y * ringAxisY.x;

    const double minimumMinorAxisPx =
        std::max(
            0.0,
            context.minimumProjectedMinorAxisPx
        );

    const double minimumDeterminantMagnitude =
        maximumAxisLength *
        minimumMinorAxisPx;

    if (minimumMinorAxisPx > 0.0 &&
        std::abs(determinant) <
            minimumDeterminantMagnitude)
    {
        const bool xIsMajor =
            axisXLength >= axisYLength;

        const glm::dvec2 majorAxis =
            xIsMajor
                ? ringAxisX
                : ringAxisY;

        glm::dvec2& minorAxis =
            xIsMajor
                ? ringAxisY
                : ringAxisX;

        const glm::dvec2 majorPerpendicular =
            glm::normalize(
                glm::dvec2(
                    -majorAxis.y,
                    majorAxis.x
                )
            );

        const double perpendicularComponent =
            glm::dot(
                minorAxis,
                majorPerpendicular
            );

        double handedness = 1.0;

        if (perpendicularComponent < -0.000000001)
        {
            handedness = -1.0;
        }
        else if (perpendicularComponent > 0.000000001)
        {
            handedness = 1.0;
        }
        else if (determinant < 0.0)
        {
            handedness = -1.0;
        }

        const double stablePerpendicularComponent =
            handedness *
            std::max(
                std::abs(perpendicularComponent),
                minimumMinorAxisPx
            );

        minorAxis +=
            majorPerpendicular *
            (
                stablePerpendicularComponent -
                perpendicularComponent
            );

        determinant =
            ringAxisX.x * ringAxisY.y -
            ringAxisX.y * ringAxisY.x;
    }

    if (std::abs(determinant) < 0.00000001)
        return;

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

    GLint previousBlendSourceRgb = 0;
    GLint previousBlendDestinationRgb = 0;
    GLint previousBlendSourceAlpha = 0;
    GLint previousBlendDestinationAlpha = 0;

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

    std::array<
        glm::vec4,
        MaxRingBands
    > geometry {};

    std::array<
        glm::vec4,
        MaxRingBands
    > colorOptical {};

    std::array<
        glm::vec4,
        MaxRingBands
    > appearance {};

    std::array<
        glm::vec4,
        MaxRingBands
    > extra {};

    std::array<
        glm::vec4,
        MaxRingBands
    > particle {};

    const int bandCount =
        std::min(
            static_cast<int>(
                context.bands->size()
            ),
            MaxRingBands
        );

    /*
        Ring radii передаются в planet-radius units.

        Snapshot хранит километры. Контекст axis vectors
        уже соответствуют одному радиусу планеты, поэтому
        здесь значения bands должны быть заранее нормализованы.

        В SystemMapRenderer мы передадим уже нормализованную
        временную копию.
    */
    for (int index = 0;
         index < bandCount;
         ++index)
    {
        const auto& band =
            (*context.bands)[
                static_cast<std::size_t>(
                    index
                )
            ];

        geometry[
            static_cast<std::size_t>(
                index
            )
        ] =
            glm::vec4(
                static_cast<float>(
                    band.innerRadiusKm
                ),
                static_cast<float>(
                    band.outerRadiusKm
                ),
                band.edgeSoftness,
                static_cast<float>(
                    band.visibilityClass
                )
            );

        colorOptical[
            static_cast<std::size_t>(
                index
            )
        ] =
            glm::vec4(
                band.tint,
                band.opticalDepth
            );

        appearance[
            static_cast<std::size_t>(
                index
            )
        ] =
            glm::vec4(
                band.opacity,
                band.radialNoiseStrength,
                band.radialBrightnessVariation,
                band.azimuthalAsymmetry
            );

        /*
            Детерминированный seed по индексу.
        */
        extra[
            static_cast<std::size_t>(
                index
            )
        ] =
            glm::vec4(
                static_cast<float>(
                    index + 1
                ) *
                    1.61803398875f,
                band.visualOpacityScale,
                band.radialStructureScale,
                band.particleDensityScale
            );

        particle[
            static_cast<std::size_t>(
                index
            )
        ] =
            glm::vec4(
                band.particleClumpiness,
                band.particleRadialJitter,
                band.particleSizePxRange.x,
                band.particleSizePxRange.y
            );
    }

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

    glBlendFunc(
        GL_SRC_ALPHA,
        GL_ONE_MINUS_SRC_ALPHA
    );

    glUseProgram(
        m_shader
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
            context.planetCenterPx.x
        ),
        static_cast<float>(
            context.planetCenterPx.y
        )
    );

    glUniform2f(
        m_ringAxisXLocation,
        static_cast<float>(
            ringAxisX.x
        ),
        static_cast<float>(
            ringAxisX.y
        )
    );

    glUniform2f(
        m_ringAxisYLocation,
        static_cast<float>(
            ringAxisY.x
        ),
        static_cast<float>(
            ringAxisY.y
        )
    );

    glUniform2f(
        m_ringDepthCoefficientsLocation,
        static_cast<float>(
            context.ringDepthCoefficients.x
        ),
        static_cast<float>(
            context.ringDepthCoefficients.y
        )
    );

    glUniform1i(
        m_renderPartLocation,
        part ==
            PlanetRingRenderPart::Front
            ? 1
            : 0
    );

    glUniform1f(
        m_patternPhaseLocation,
        static_cast<float>(
            context.patternPhaseRad
        )
    );

    glUniform1f(
        m_opacityScaleLocation,
        std::clamp(
            context.opacityScale,
            0.0f,
            1.0f
        )
    );

    glUniform1i(
        m_visualModeLocation,
        context.visual.renderMode ==
            world::celestial::
                SystemMapRingDisplayMode::
                    ParticleCloud
            ? 1
            : 0
    );

    glUniform4f(
        m_visualBandEmphasisLocation,
        context.visual.mainBandEmphasis,
        context.visual.secondaryBandEmphasis,
        context.visual.faintBandEmphasis,
        context.visual.diffuseBandEmphasis
    );

    glUniform4f(
        m_visualStructureLocation,
        context.visual.artisticWidthScale,
        context.visual.gapContrast,
        context.visual.radialStructureStrength,
        context.visual.fineStructureStrength
    );

    glUniform4f(
        m_visualParticleLocation,
        context.visual.particleDensity,
        context.visual.particleOpacityScale,
        context.visual.particleSizePxRange.x,
        context.visual.particleSizePxRange.y
    );

    glUniform4f(
        m_visualParticleShapeLocation,
        context.visual.radialJitter,
        context.visual.azimuthalClumping,
        context.visual.clusterScale,
        context.visual.softness
    );

    glUniform4f(
        m_visualOcclusionLocation,
        context.visual.artisticOcclusionEnabled
            ? 1.0f
            : 0.0f,
        context.visual.backHalfBrightness,
        context.visual.innerEdgeDarkening,
        context.visual.brightnessVariation
    );

    glUniform4f(
        m_visualLodLocation,
        context.visual.edgeSoftnessScale,
        context.visual.minimumVisibleWidthPx,
        context.visual.minimumMainBandWidthPx,
        context.visual.continuousFill
    );

    glUniform1i(
        m_bandCountLocation,
        bandCount
    );

    glUniform4fv(
        m_bandGeometryLocation,
        MaxRingBands,
        glm::value_ptr(
            geometry[0]
        )
    );

    glUniform4fv(
        m_bandColorOpticalLocation,
        MaxRingBands,
        glm::value_ptr(
            colorOptical[0]
        )
    );

    glUniform4fv(
        m_bandAppearanceLocation,
        MaxRingBands,
        glm::value_ptr(
            appearance[0]
        )
    );

    glUniform4fv(
        m_bandExtraLocation,
        MaxRingBands,
        glm::value_ptr(
            extra[0]
        )
    );

    glUniform4fv(
        m_bandParticleLocation,
        MaxRingBands,
        glm::value_ptr(
            particle[0]
        )
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

    if (cullWasEnabled)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);

    if (scissorWasEnabled)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);
}

} // namespace render::celestial::rings
