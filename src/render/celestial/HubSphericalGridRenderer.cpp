#include "src/render/celestial/HubSphericalGridRenderer.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/constants.hpp>

namespace render::celestial
{
    float HubSphericalGridRenderer::smoothStep(
        float edge0,
        float edge1,
        float x
    )
    {
        const float t =
            std::clamp(
                (x - edge0) /
                std::max(
                    0.000001f,
                    edge1 - edge0
                ),
                0.0f,
                1.0f
            );

        return
            t * t *
            (3.0f - 2.0f * t);
    }

    bool HubSphericalGridRenderer::projectFrontHemispherePoint(
        const glm::dvec2& planetCenterPx,
        double shellRadiusPx,
        double lonRad,
        double latRad,
        float fadeStart,
        float fadeEnd,
        glm::dvec2& outScreenPx,
        float& outFade
    )
    {
        const double cosLat =
            std::cos(latRad);

        const double sinLat =
            std::sin(latRad);

        const double sinLon =
            std::sin(lonRad);

        const double cosLon =
            std::cos(lonRad);

        // Камера смотрит на переднюю полусферу (+Z).
        const double x =
            sinLon * cosLat;

        const double y =
            sinLat;

        const double z =
            cosLon * cosLat;

        if (z <= 0.0)
            return false;

        outScreenPx =
            glm::dvec2(
                planetCenterPx.x + x * shellRadiusPx,
                planetCenterPx.y - y * shellRadiusPx
            );

        outFade =
            smoothStep(
                fadeStart,
                fadeEnd,
                static_cast<float>(z)
            );

        return outFade > 0.001f;
    }

    void HubSphericalGridRenderer::drawLatitudeLine(
        const glm::dvec2& planetCenterPx,
        double shellRadiusPx,
        double latRad,
        double longitudeOffsetRad,
        const glm::vec4& color,
        const HubSphericalGridStyle& style
    )
    {
        const int samples =
            std::max(
                48,
                style.samplesPerLine
            );

        bool havePrev = false;
        glm::dvec2 prevP {0.0, 0.0};
        float prevFade = 0.0f;

        for (int i = 0; i <= samples; ++i)
        {
            const double t =
                static_cast<double>(i) /
                static_cast<double>(samples);

            const double lon =
                -glm::pi<double>() +
                glm::two_pi<double>() * t +
                longitudeOffsetRad;

            glm::dvec2 p {0.0, 0.0};
            float fade = 0.0f;

            const bool visible =
                projectFrontHemispherePoint(
                    planetCenterPx,
                    shellRadiusPx,
                    lon,
                    latRad,
                    style.horizonFadeStart,
                    style.horizonFadeEnd,
                    p,
                    fade
                );

            if (visible && havePrev)
            {
                glBegin(GL_LINES);

                glColor4f(
                    color.r,
                    color.g,
                    color.b,
                    color.a * prevFade
                );

                glVertex2d(
                    prevP.x,
                    prevP.y
                );

                glColor4f(
                    color.r,
                    color.g,
                    color.b,
                    color.a * fade
                );

                glVertex2d(
                    p.x,
                    p.y
                );

                glEnd();
            }

            if (visible)
            {
                prevP = p;
                prevFade = fade;
                havePrev = true;
            }
            else
            {
                havePrev = false;
            }
        }
    }

    void HubSphericalGridRenderer::drawLongitudeLine(
        const glm::dvec2& planetCenterPx,
        double shellRadiusPx,
        double lonRad,
        double longitudeOffsetRad,
        const glm::vec4& color,
        const HubSphericalGridStyle& style
    )
    {
        const int samples =
            std::max(
                48,
                style.samplesPerLine
            );

        bool havePrev = false;
        glm::dvec2 prevP {0.0, 0.0};
        float prevFade = 0.0f;

        for (int i = 0; i <= samples; ++i)
        {
            const double t =
                static_cast<double>(i) /
                static_cast<double>(samples);

            const double lat =
                -glm::half_pi<double>() +
                glm::pi<double>() * t;

            glm::dvec2 p {0.0, 0.0};
            float fade = 0.0f;

            const bool visible =
                projectFrontHemispherePoint(
                    planetCenterPx,
                    shellRadiusPx,
                    lonRad + longitudeOffsetRad,
                    lat,
                    style.horizonFadeStart,
                    style.horizonFadeEnd,
                    p,
                    fade
                );

            if (visible && havePrev)
            {
                glBegin(GL_LINES);

                glColor4f(
                    color.r,
                    color.g,
                    color.b,
                    color.a * prevFade
                );

                glVertex2d(
                    prevP.x,
                    prevP.y
                );

                glColor4f(
                    color.r,
                    color.g,
                    color.b,
                    color.a * fade
                );

                glVertex2d(
                    p.x,
                    p.y
                );

                glEnd();
            }

            if (visible)
            {
                prevP = p;
                prevFade = fade;
                havePrev = true;
            }
            else
            {
                havePrev = false;
            }
        }
    }

    void HubSphericalGridRenderer::render(
        const glm::dvec2& planetCenterPx,
        double planetRadiusPx,
        double longitudeOffsetRad,
        const HubSphericalGridStyle& style
    ) const
    {
        if (!style.enabled || planetRadiusPx <= 1.0)
            return;

        const double shellRadiusPx =
            planetRadiusPx * style.radiusScale;

        const int latStep =
            std::max(
                1,
                style.latitudeStepDeg
            );

        const int lonStep =
            std::max(
                1,
                style.longitudeStepDeg
            );

        const int majorEvery =
            std::max(
                1,
                style.majorEvery
            );

        // Latitude lines.
        int latIndex = 0;
        for (int latDeg = -80; latDeg <= 80; latDeg += latStep, ++latIndex)
        {
            const bool major =
                (latIndex % majorEvery) == 0 ||
                latDeg == 0;

            const glm::vec4 color =
                major
                    ? style.majorColor
                    : style.minorColor;

            drawLatitudeLine(
                planetCenterPx,
                shellRadiusPx,
                glm::radians(
                    static_cast<double>(latDeg)
                ),
                longitudeOffsetRad,
                color,
                style
            );
        }

        // Longitude lines.
        int lonIndex = 0;
        for (int lonDeg = -180; lonDeg < 180; lonDeg += lonStep, ++lonIndex)
        {
            const bool major =
                (lonIndex % majorEvery) == 0 ||
                lonDeg == 0;

            const glm::vec4 color =
                major
                    ? style.majorColor
                    : style.minorColor;

            drawLongitudeLine(
                planetCenterPx,
                shellRadiusPx,
                glm::radians(
                    static_cast<double>(lonDeg)
                ),
                longitudeOffsetRad,
                color,
                style
            );
        }
    }
}