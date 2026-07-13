#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>

namespace render::celestial
{
    struct HubSphericalGridStyle
    {
        bool enabled = true;

        // Радиус оболочки сетки относительно радиуса планеты.
        // Чуть выше поверхности и облаков.
        double radiusScale = 1.010;

        // Шаг сетки.
        int latitudeStepDeg = 10;
        int longitudeStepDeg = 10;

        // Каждая N-я линия считается major.
        int majorEvery = 3;

        // Качество аппроксимации линий.
        int samplesPerLine = 180;

        // Цвета.
        glm::vec4 minorColor {
            0.28f,
            0.66f,
            1.00f,
            0.10f
        };

        glm::vec4 majorColor {
            0.46f,
            0.82f,
            1.00f,
            0.18f
        };

        // Плавное затухание к горизонту.
        // 0 = лимб, 1 = центр полусферы.
        float horizonFadeStart = 0.05f;
        float horizonFadeEnd = 0.30f;
    };

    class HubSphericalGridRenderer
    {
    public:
        void render(
            const glm::dvec2& planetCenterPx,
            double planetRadiusPx,
            double longitudeOffsetRad,
            const HubSphericalGridStyle& style
        ) const;

    private:
        static float smoothStep(
            float edge0,
            float edge1,
            float x
        );

        static bool projectFrontHemispherePoint(
            const glm::dvec2& planetCenterPx,
            double shellRadiusPx,
            double lonRad,
            double latRad,
            float fadeStart,
            float fadeEnd,
            glm::dvec2& outScreenPx,
            float& outFade
        );

        static void drawLatitudeLine(
            const glm::dvec2& planetCenterPx,
            double shellRadiusPx,
            double latRad,
            double longitudeOffsetRad,
            const glm::vec4& color,
            const HubSphericalGridStyle& style
        );

        static void drawLongitudeLine(
            const glm::dvec2& planetCenterPx,
            double shellRadiusPx,
            double lonRad,
            double longitudeOffsetRad,
            const glm::vec4& color,
            const HubSphericalGridStyle& style
        );
    };
}