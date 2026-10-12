#include "src/render/cockpit/EngineThrustIndicatorRenderer.h"

#include <algorithm>
#include <cmath>

#include "src/render/HUD/TextRenderer.h"

namespace render::cockpit
{

void EngineThrustIndicatorRenderer::render(
    const game::presentation::EngineThrustHudPresentation& presentation,
    const Viewport& viewport
)
{
    using FillDirection =
        game::presentation::EngineThrustFillDirection;

    if (!presentation.visible ||
        presentation.groups.empty() ||
        viewport.width <= 0 ||
        viewport.height <= 0)
    {
        return;
    }

    auto& text = TextRenderer::instance();

    const float scale = std::clamp(
        static_cast<float>(viewport.height) / 1440.0f,
        0.68f,
        1.35f
    );

    const float barWidth = std::max(4.0f, 6.0f * scale);
    const float barHeight = std::max(48.0f, 72.0f * scale);
    const float barGap = std::max(2.0f, 2.5f * scale);
    const float groupGap = std::max(7.0f, 10.0f * scale);
    const float padding = std::max(5.0f, 7.0f * scale);
    const float labelGap = std::max(4.0f, 5.0f * scale);
    const int labelPx = std::max(
        10,
        static_cast<int>(std::lround(12.0f * scale))
    );

    float barsWidth = 0.0f;
    for (std::size_t g = 0; g < presentation.groups.size(); ++g)
    {
        const auto& group = presentation.groups[g];
        if (!group.bars.empty())
        {
            barsWidth +=
                static_cast<float>(group.bars.size()) * barWidth +
                static_cast<float>(group.bars.size() - 1) * barGap;
        }

        if (g + 1 < presentation.groups.size())
            barsWidth += groupGap;
    }

    const float panelWidth = barsWidth + padding * 2.0f;
    const float panelHeight =
        barHeight + padding * 2.0f +
        static_cast<float>(labelPx) + labelGap;

    const float panelX =
        static_cast<float>(viewport.width) * 0.025f;
    const float panelBottom =
        static_cast<float>(viewport.height) * 0.965f;
    const float panelY = panelBottom - panelHeight;
    const float barsY =
        panelY + padding +
        static_cast<float>(labelPx) + labelGap;

    const glm::vec4 panelColor(0.012f, 0.016f, 0.020f, 0.68f);
    const glm::vec4 trackColor(0.10f, 0.085f, 0.045f, 0.82f);
    const glm::vec4 outlineColor(0.55f, 0.46f, 0.18f, 0.56f);
    const glm::vec4 capacityColor(0.98f, 0.74f, 0.08f, 0.82f);
    const glm::vec4 thrustColor(1.00f, 0.12f, 0.055f, 0.98f);
    const glm::vec4 labelColor(0.88f, 0.77f, 0.36f, 0.92f);

    text.solidRectPx(
        panelX,
        panelY,
        panelWidth,
        panelHeight,
        panelColor
    );

    text.textDrawPx(
        presentation.modelLabel,
        panelX + padding,
        panelY + padding,
        labelPx,
        labelColor
    );

    float x = panelX + padding;

    for (std::size_t g = 0; g < presentation.groups.size(); ++g)
    {
        const auto& group = presentation.groups[g];

        for (std::size_t b = 0; b < group.bars.size(); ++b)
        {
            const auto& bar = group.bars[b];

            text.solidRectPx(
                x - 1.0f,
                barsY - 1.0f,
                barWidth + 2.0f,
                barHeight + 2.0f,
                outlineColor
            );
            text.solidRectPx(
                x,
                barsY,
                barWidth,
                barHeight,
                trackColor
            );

            const float capability =
                std::clamp(bar.capability01, 0.0f, 1.0f);
            const float thrust =
                std::clamp(bar.thrust01, 0.0f, capability);

            const float capabilityHeight =
                barHeight * capability;
            const float thrustHeight =
                barHeight * thrust;

            const bool topDown =
                bar.fillDirection == FillDirection::TopDown;

            const float capabilityY = topDown
                ? barsY
                : barsY + barHeight - capabilityHeight;
            const float thrustY = topDown
                ? barsY
                : barsY + barHeight - thrustHeight;

            if (capabilityHeight > 0.0f)
            {
                text.solidRectPx(
                    x,
                    capabilityY,
                    barWidth,
                    capabilityHeight,
                    capacityColor
                );
            }

            if (thrustHeight > 0.0f)
            {
                const float redWidth =
                    std::max(2.0f, barWidth * 0.56f);
                const float redX =
                    x + (barWidth - redWidth) * 0.5f;

                text.solidRectPx(
                    redX,
                    thrustY,
                    redWidth,
                    thrustHeight,
                    thrustColor
                );
            }

            x += barWidth;
            if (b + 1 < group.bars.size())
                x += barGap;
        }

        if (g + 1 < presentation.groups.size())
            x += groupGap;
    }
}

} // namespace render::cockpit
