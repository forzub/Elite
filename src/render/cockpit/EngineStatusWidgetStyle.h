#pragma once

#include <string_view>

#include <glm/glm.hpp>

namespace render::cockpit
{

struct EngineStatusWidgetStyle
{
    const char* id = "human_amber";

    glm::vec4 panelBackground {0.005f, 0.006f, 0.008f, 0.92f};
    glm::vec4 panelBorder {0.98f, 0.63f, 0.05f, 0.96f};
    glm::vec4 panelBorderGlow {0.98f, 0.55f, 0.02f, 0.20f};

    glm::vec4 track {0.075f, 0.080f, 0.095f, 0.92f};
    glm::vec4 barOutline {0.36f, 0.31f, 0.14f, 0.70f};
    glm::vec4 capacity {0.98f, 0.72f, 0.06f, 0.94f};
    glm::vec4 thrust {1.00f, 0.13f, 0.045f, 0.98f};
    glm::vec4 label {0.96f, 0.73f, 0.18f, 0.98f};
    glm::vec4 idleWarningIcon {0.42f, 0.44f, 0.48f, 0.24f};
    glm::vec4 warning {1.00f, 0.58f, 0.02f, 1.00f};

    float cornerRadiusPx = 7.0f;
    float borderWidthPx = 2.0f;
    float paddingPx = 11.0f;

    float iconSizePx = 24.0f;
    float iconToLabelGapPx = 18.0f;
    float labelToBarsGapPx = 3.0f;

    float barWidthPx = 11.0f;
    float barHeightPx = 80.0f;
    float barGapPx = 4.5f;
    float groupGapPx = 12.0f;

    float warningPeriodSeconds = 1.6f;
    float warningMinimumAlpha = 0.20f;
};

inline const EngineStatusWidgetStyle& resolveEngineStatusWidgetStyle(
    std::string_view id
) noexcept
{
    static const EngineStatusWidgetStyle HumanAmber {};

    // Style registry intentionally starts with one skin. Future race/faction
    // themes add entries here while consuming exactly the same view-model.
    if (id == HumanAmber.id)
        return HumanAmber;

    return HumanAmber;
}

} // namespace render::cockpit
