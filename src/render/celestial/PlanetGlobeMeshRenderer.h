#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>

namespace render::celestial
{

struct PlanetGlobeLayerDraw
{
    GLuint texture = 0;

    /*
        Координаты относительно текущего viewport,
        начало координат слева сверху.
    */
    glm::dvec2 centerPx {
        0.0,
        0.0
    };

    double radiusPx = 1.0;

    /*
        Локальная сфера -> DetailCamera.
    */
    glm::mat3 bodyToCamera {
        1.0f
    };

    /*
        Дополнительное движение текстуры по долготе.

        Поверхность:
            0.0

        Облака:
            driftU
    */
    float longitudeUvOffset = 0.0f;

    /*
        Старый surface renderer использовал:

            south pole -> V = 0

        Старый cloud renderer:

            south pole -> V = 1
    */
    bool flipV = false;

    glm::vec4 color {
        1.0f,
        1.0f,
        1.0f,
        1.0f
    };

    float opacity = 1.0f;

    /*
        Для поверхности можно оставить blending=true,
        чтобы полностью сохранить старое поведение texture alpha.

        Для облаков blending обязателен.
    */
    bool blending = true;
    /*
        true используется при рендере в прозрачный half-resolution
        overlay target. RGB сохраняется premultiplied, alpha —
        обычным накоплением.
    */
    bool premultipliedTarget = false;

    bool useHorizonFade = false;
    float horizonFadeStart = 0.035f;
    float horizonFadeEnd = 0.30f;

    bool usePolarFade = false;
    float polarFadeStart = 0.82f;
    float polarFadeEnd = 0.995f;
};


class PlanetGlobeMeshRenderer
{
public:
    void render(
        const PlanetGlobeLayerDraw& draw
    );

private:
    void ensureResources();
    void createSphereMesh();

private:
    GLuint m_shader = 0;

    GLuint m_vao = 0;
    GLuint m_vertexBuffer = 0;
    GLuint m_indexBuffer = 0;

    GLsizei m_indexCount = 0;

    GLint m_textureLocation = -1;
    GLint m_bodyToCameraLocation = -1;

    GLint m_viewportSizeLocation = -1;
    GLint m_planetCenterLocation = -1;
    GLint m_planetRadiusLocation = -1;

    GLint m_longitudeUvOffsetLocation = -1;
    GLint m_flipVLocation = -1;

    GLint m_colorLocation = -1;
    GLint m_opacityLocation = -1;

    GLint m_useHorizonFadeLocation = -1;
    GLint m_horizonFadeStartLocation = -1;
    GLint m_horizonFadeEndLocation = -1;

    GLint m_usePolarFadeLocation = -1;
    GLint m_polarFadeStartLocation = -1;
    GLint m_polarFadeEndLocation = -1;

    bool m_missingShaderWarningPrinted = false;
};

} // namespace render::celestial