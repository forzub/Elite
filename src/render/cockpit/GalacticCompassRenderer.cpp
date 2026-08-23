#include "src/render/cockpit/GalacticCompassRenderer.h"

#include <algorithm>
#include <cmath>

#include "src/render/HUD/TextRenderer.h"

namespace render::cockpit
{

void GalacticCompassRenderer::init()
{
    m_batch.init();
}

void GalacticCompassRenderer::ensureFont(int pixelSize)
{
    pixelSize = std::max(11, pixelSize);
    if (!m_font || m_fontPixelSize != pixelSize)
    {
        m_font = std::make_unique<Font>(
            "assets/fonts/Roboto-Light.ttf",
            pixelSize
        );
        m_fontPixelSize = pixelSize;
    }
}

double GalacticCompassRenderer::wrapLongitude(double degrees)
{
    degrees = std::fmod(degrees, 360.0);
    if (degrees < 0.0)
        degrees += 360.0;
    return degrees;
}

double GalacticCompassRenderer::signedLongitudeDelta(double a, double b)
{
    double delta = wrapLongitude(a) - wrapLongitude(b);
    if (delta > 180.0) delta -= 360.0;
    if (delta < -180.0) delta += 360.0;
    return delta;
}

std::string GalacticCompassRenderer::longitudeLabel(
    int roundedDegrees,
    const game::presentation::GalacticCompassVocabulary& vocabulary
) const
{
    const int normalized = ((roundedDegrees % 360) + 360) % 360;
    if (normalized == 0) return vocabulary.galacticCenter;
    if (normalized == 90) return vocabulary.longitude90;
    if (normalized == 180) return vocabulary.galacticAnticenter;
    if (normalized == 270) return vocabulary.longitude270;
    return std::to_string(normalized);
}

void GalacticCompassRenderer::render(
    const game::presentation::GalacticCompassPresentation& presentation,
    const Viewport& viewport
)
{
    if (!presentation.visible || viewport.width <= 0 || viewport.height <= 0)
        return;

    const float w = static_cast<float>(viewport.width);
    const float h = static_cast<float>(viewport.height);
    const glm::vec4 dim(0.28f, 0.78f, 0.96f, 0.18f);
    const glm::vec4 bright(0.55f, 0.94f, 1.0f, 0.48f);

    const float topY = std::max(38.0f, h * 0.085f);
    const float leftX = w * 0.20f;
    const float rightX = w * 0.80f;
    const float centerX = (leftX + rightX) * 0.5f;
    const double halfSpanLongitudeDeg = 45.0;

    const float verticalX = w * 0.955f;
    const float verticalTop = h * 0.24f;
    const float verticalBottom = h * 0.72f;
    const float verticalCenter = (verticalTop + verticalBottom) * 0.5f;
    const double halfSpanLatitudeDeg = 35.0;

    m_batch.begin(viewport.width, viewport.height);
    m_batch.line({leftX, topY}, {rightX, topY}, 1.0f, dim);
    m_batch.line(
        {centerX, topY - 9.0f},
        {centerX, topY + 12.0f},
        2.0f,
        bright
    );

    const int longitudeStart =
        static_cast<int>(std::floor((presentation.longitudeDeg - 50.0) / 10.0)) * 10;
    const int longitudeEnd =
        static_cast<int>(std::ceil((presentation.longitudeDeg + 50.0) / 10.0)) * 10;

    for (int tick = longitudeStart; tick <= longitudeEnd; tick += 10)
    {
        const double delta = signedLongitudeDelta(
            static_cast<double>(tick),
            presentation.longitudeDeg
        );
        if (std::abs(delta) > halfSpanLongitudeDeg + 0.01)
            continue;

        const float x = centerX + static_cast<float>(
            delta / halfSpanLongitudeDeg
        ) * (rightX - leftX) * 0.5f;
        m_batch.line(
            {x, topY - 6.0f},
            {x, topY + 6.0f},
            1.0f,
            bright
        );
    }

    m_batch.line(
        {verticalX, verticalTop},
        {verticalX, verticalBottom},
        1.0f,
        dim
    );
    m_batch.line(
        {verticalX - 11.0f, verticalCenter},
        {verticalX + 8.0f, verticalCenter},
        2.0f,
        bright
    );

    const int latStart = std::max(
        -90,
        static_cast<int>(std::floor((presentation.latitudeDeg - 40.0) / 10.0)) * 10
    );
    const int latEnd = std::min(
        90,
        static_cast<int>(std::ceil((presentation.latitudeDeg + 40.0) / 10.0)) * 10
    );

    for (int tick = latStart; tick <= latEnd; tick += 10)
    {
        const double delta = static_cast<double>(tick) - presentation.latitudeDeg;
        if (std::abs(delta) > halfSpanLatitudeDeg + 0.01)
            continue;

        const float y = verticalCenter - static_cast<float>(
            delta / halfSpanLatitudeDeg
        ) * (verticalBottom - verticalTop) * 0.5f;
        m_batch.line(
            {verticalX - 7.0f, y},
            {verticalX + 4.0f, y},
            1.0f,
            bright
        );
    }

    m_batch.flush();

    ensureFont(static_cast<int>(std::clamp(h * 0.015f, 12.0f, 19.0f)));
    if (!m_font)
        return;

    const glm::vec4 textColor(0.48f, 0.88f, 1.0f, 0.34f);

    for (int tick = longitudeStart; tick <= longitudeEnd; tick += 10)
    {
        const double delta = signedLongitudeDelta(
            static_cast<double>(tick),
            presentation.longitudeDeg
        );
        if (std::abs(delta) > halfSpanLongitudeDeg + 0.01)
            continue;

        const float x = centerX + static_cast<float>(
            delta / halfSpanLongitudeDeg
        ) * (rightX - leftX) * 0.5f;
        const std::string label = longitudeLabel(tick, presentation.vocabulary);
        const float width = m_font->measureText(label);
        TextRenderer::instance().textDraw(
            *m_font,
            label,
            x - width * 0.5f,
            topY + m_fontPixelSize + 4.0f,
            textColor
        );
    }

    for (int tick = latStart; tick <= latEnd; tick += 10)
    {
        const double delta = static_cast<double>(tick) - presentation.latitudeDeg;
        if (std::abs(delta) > halfSpanLatitudeDeg + 0.01)
            continue;

        const float y = verticalCenter - static_cast<float>(
            delta / halfSpanLatitudeDeg
        ) * (verticalBottom - verticalTop) * 0.5f;

        std::string label;
        if (tick == 90) label = presentation.vocabulary.northGalacticPole;
        else if (tick == -90) label = presentation.vocabulary.southGalacticPole;
        else label = (tick > 0 ? "+" : "") + std::to_string(tick);

        TextRenderer::instance().textDraw(
            *m_font,
            label,
            verticalX - 11.0f - m_font->measureText(label),
            y + m_fontPixelSize * 0.35f,
            textColor
        );
    }


}

} // namespace render::cockpit
