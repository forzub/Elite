#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>

namespace render::celestial
{

class HubPlanetSurfaceRenderer
{
public:
        void render(
            GLuint albedoTexture,
            GLuint normalTexture,
            const glm::dvec2& planetCenterPx,
            double planetRadiusPx,

            /*
                Преобразование нормали из экранной системы
                cinematic globe в body-fixed систему планеты.
            */
            const glm::mat3& cameraToPlanetBody,

            const glm::vec3& lightDirection
        );

private:
    void ensureResources();

private:
    GLuint m_shader = 0;
    GLuint m_fullscreenVao = 0;

    GLint m_albedoTextureLocation = -1;
    GLint m_normalTextureLocation = -1;
    GLint m_hasNormalTextureLocation = -1;

    GLint m_viewportOriginLocation = -1;
    GLint m_viewportSizeLocation = -1;
    GLint m_planetCenterLocation = -1;

    GLint m_planetRadiusLocation = -1;
    GLint m_cameraToPlanetBodyLocation = -1;

    GLint m_lightDirectionLocation = -1;

    GLint m_ambientStrengthLocation = -1;
    GLint m_diffuseStrengthLocation = -1;
    GLint m_normalStrengthLocation = -1;

    GLint m_horizonDarkeningLocation = -1;
    GLint m_surfaceContrastLocation = -1;
    GLint m_surfaceSaturationLocation = -1;

    GLint m_aerialPerspectiveColorLocation = -1;
    GLint m_aerialPerspectiveStrengthLocation = -1;
    GLint m_aerialPerspectiveStartLocation = -1;
    GLint m_aerialPerspectiveEndLocation = -1;
    GLint m_aerialPerspectiveBlurLodLocation = -1;

    bool m_missingShaderWarningPrinted = false;
};

} // namespace render::celestial