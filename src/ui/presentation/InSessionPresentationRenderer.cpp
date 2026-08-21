#include "src/ui/presentation/InSessionPresentationRenderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string>

#include <glad/gl.h>

#include "render/HUD/TextRenderer.h"
#include "src/game/localization/LocalizationService.h"

namespace ui::presentation
{
namespace
{
constexpr float PanelRatio = 0.28f;
constexpr float DropdownRowHeight = 27.0f;

struct Rect
{
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;

    bool contains(double px, double py) const
    {
        return px >= x && py >= y && px <= x + w && py <= y + h;
    }
};

struct PanelLayout
{
    float panelX = 0.0f;
    float panelW = 0.0f;
    float pad = 0.0f;
    Rect dropdownButton;
    Rect dropdownMenu;
    std::array<Rect, 3> navigationButtons;
    int visibleRows = 1;
};

PanelLayout makePanelLayout(const Viewport& viewport)
{
    PanelLayout layout;
    const float w = static_cast<float>(viewport.width);
    const float h = static_cast<float>(viewport.height);
    layout.panelX = std::floor(w * (1.0f - PanelRatio));
    layout.panelW = w - layout.panelX;
    layout.pad = std::max(14.0f, layout.panelW * 0.055f);

    const float contentW = std::max(0.0f, layout.panelW - layout.pad * 2.0f);

    // Minimize/restore and very small Windows resize states may temporarily
    // expose a viewport shorter than the normal STAR ATLAS layout. Never feed
    // std::clamp an inverted [lo, hi] interval: MinGW's debug STL correctly
    // asserts on that contract violation.
    const float dropdownMaxY = std::max(0.0f, h - 220.0f);
    const float dropdownMinY = std::min(300.0f, dropdownMaxY);
    const float dropdownY = std::clamp(
        h * 0.49f,
        dropdownMinY,
        dropdownMaxY
    );
    layout.dropdownButton = {
        layout.panelX + layout.pad,
        dropdownY,
        contentW,
        34.0f
    };

    const float actionY = h - 62.0f;
    const float gap = 5.0f;
    const float actionW = std::max(52.0f, (contentW - gap * 2.0f) / 3.0f);
    for (std::size_t i = 0; i < layout.navigationButtons.size(); ++i)
    {
        layout.navigationButtons[i] = {
            layout.panelX + layout.pad +
                static_cast<float>(i) * (actionW + gap),
            actionY,
            actionW,
            34.0f
        };
    }

    const float menuY = layout.dropdownButton.y + layout.dropdownButton.h + 2.0f;
    const float menuBottom = actionY - 34.0f;
    const float menuH = std::max(DropdownRowHeight, menuBottom - menuY);
    layout.visibleRows = std::max(1, static_cast<int>(std::floor(menuH / DropdownRowHeight)));
    layout.dropdownMenu = {
        layout.dropdownButton.x,
        menuY,
        layout.dropdownButton.w,
        static_cast<float>(layout.visibleRows) * DropdownRowHeight
    };
    return layout;
}

std::string formatDistanceLy(double value)
{
    std::ostringstream out;
    if (value < 0.01)
        out << std::fixed << std::setprecision(4);
    else if (value < 10.0)
        out << std::fixed << std::setprecision(2);
    else
        out << std::fixed << std::setprecision(1);
    out << value << " ly";
    return out.str();
}

std::string modeLabel(
    game::system_map::MapMode mode,
    bool systemLayerIsSpace,
    const game::localization::LocalizationService& loc)
{
    using game::system_map::MapMode;
    switch (mode)
    {
        case MapMode::Galaxy: return loc.text("map.galaxy", "GALAXY");
        case MapMode::System:
            return systemLayerIsSpace
                ? loc.text("map.space", "SPACE")
                : loc.text("map.system", "SYSTEM");
        case MapMode::Detail: return loc.text("map.detail", "DETAIL");
        case MapMode::Hub: return loc.text("map.hub", "HUB");
    }
    return loc.text("map.unknown", "Unknown");
}

std::string navigationButtonLabel(
    game::presentation::SystemMapPanelActionType action,
    const game::presentation::SystemMapPanelPresentation& panel,
    const game::localization::LocalizationService& loc)
{
    using Action = game::presentation::SystemMapPanelActionType;

    switch (action)
    {
        case Action::OpenGalaxy:
            return loc.text("map.galaxy", "GALAXY");
        case Action::OpenSystem:
            return panel.systemLayerIsSpace
                ? loc.text("map.space", "SPACE")
                : loc.text("map.system", "SYSTEM");
        case Action::OpenDetail:
            return loc.text("map.detail", "DETAIL");
        case Action::OpenHub:
            return loc.text("map.hub", "HUB");
        case Action::SelectSystem:
            break;
    }

    return {};
}

void drawLabelValue(
    TextRenderer& text,
    float x,
    float& y,
    float valueX,
    int fontPx,
    const std::string& label,
    const std::string& value)
{
    text.textDrawPx(label, x, y, fontPx, glm::vec4(0.45f, 0.60f, 0.73f, 0.92f));
    text.textDrawPx(value, valueX, y, fontPx, glm::vec4(0.86f, 0.92f, 0.98f, 0.98f));
    y += static_cast<float>(fontPx + 9);
}

void drawButton(
    TextRenderer& text,
    const Rect& bounds,
    const std::string& label,
    int fontPx,
    bool enabled)
{
    const glm::vec4 fill = enabled
        ? glm::vec4(0.035f, 0.095f, 0.150f, 1.0f)
        : glm::vec4(0.020f, 0.035f, 0.052f, 1.0f);
    const glm::vec4 color = enabled
        ? glm::vec4(0.78f, 0.90f, 0.98f, 0.98f)
        : glm::vec4(0.34f, 0.42f, 0.50f, 0.72f);
    text.solidRectPx(bounds.x, bounds.y, bounds.w, bounds.h, fill);
    text.solidRectPx(bounds.x, bounds.y, bounds.w, 1.0f,
        enabled ? glm::vec4(0.32f, 0.62f, 0.86f, 0.75f)
                : glm::vec4(0.20f, 0.28f, 0.34f, 0.45f));
    const float labelW = text.measureTextPx(label, fontPx);
    text.textDrawPx(
        label,
        bounds.x + std::max(5.0f, (bounds.w - labelW) * 0.5f),
        bounds.y + (bounds.h - static_cast<float>(fontPx)) * 0.5f - 1.0f,
        fontPx,
        color);
}

const game::presentation::SystemMapPanelSystemItem* selectedSystem(
    const game::presentation::SystemMapPanelPresentation& panel)
{
    const auto it = std::find_if(
        panel.systems.begin(),
        panel.systems.end(),
        [](const auto& item) { return item.selected; });
    return it == panel.systems.end() ? nullptr : &*it;
}

std::string systemRowLabel(
    const game::presentation::SystemMapPanelSystemItem& item)
{
    std::string row;
    if (item.current) row += "C ";
    if (item.selected) row += "> ";
    row += item.name;
    if (!item.starType.empty()) row += " [" + item.starType + "]";
    return row;
}
}

void InSessionPresentationRenderer::renderServicePanel(
    const Viewport& viewport,
    const game::localization::LocalizationService& localization,
    ui::services::ServiceUiId service) const
{
    if (viewport.width <= 0 || viewport.height <= 0)
        return;

    const auto* definition = ui::services::findServiceUiDefinition(service);
    if (!definition)
        return;

    glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
    glScissor(viewport.x, viewport.y, viewport.width, viewport.height);
    glDisable(GL_DEPTH_TEST);

    auto& text = TextRenderer::instance();
    text.beginFrameForViewport(viewport.width, viewport.height);

    const float w = static_cast<float>(viewport.width);
    const float h = static_cast<float>(viewport.height);
    text.solidRectPx(0.0f, 0.0f, w, h, glm::vec4(0.008f, 0.018f, 0.035f, 1.0f));
    text.solidRectPx(0.0f, 0.0f, w, 2.0f, glm::vec4(0.32f, 0.62f, 0.86f, 0.70f));

    const int titlePx = std::clamp(static_cast<int>(h * 0.055f), 28, 48);
    const std::string title = localization.text(
        definition->titleKey,
        definition->englishTitle);
    const float titleW = text.measureTextPx(title, titlePx);
    text.textDrawPx(
        title,
        std::max(32.0f, (w - titleW) * 0.5f),
        h * 0.22f,
        titlePx,
        glm::vec4(0.94f, 0.97f, 1.0f, 0.98f));

    const std::string route =
        "F" + std::to_string(definition->functionKey) + "  /  " + definition->stableId;
    const int smallPx = std::clamp(static_cast<int>(h * 0.021f), 13, 18);
    const float routeW = text.measureTextPx(route, smallPx);
    text.textDrawPx(
        route,
        std::max(32.0f, (w - routeW) * 0.5f),
        h * 0.22f + static_cast<float>(titlePx + 26),
        smallPx,
        glm::vec4(0.45f, 0.64f, 0.80f, 0.82f));

    text.endFrame();
    glEnable(GL_DEPTH_TEST);
}

bool InSessionPresentationRenderer::systemMapPanelContains(
    const Viewport& viewport,
    double mouseX,
    double mouseY) const
{
    if (viewport.width <= 0 || viewport.height <= 0)
        return false;

    const auto layout = makePanelLayout(viewport);
    const double localX = mouseX - static_cast<double>(viewport.x);
    const double localY = mouseY - static_cast<double>(viewport.y);
    return localX >= layout.panelX &&
           localX <= static_cast<double>(viewport.width) &&
           localY >= 0.0 && localY <= static_cast<double>(viewport.height);
}

std::optional<game::presentation::SystemMapPanelAction>
InSessionPresentationRenderer::handleSystemMapPanelInput(
    const Viewport& viewport,
    const game::presentation::SystemMapPanelPresentation& panel,
    double mouseX,
    double mouseY,
    bool leftDown,
    double scrollY)
{
    using game::presentation::SystemMapPanelAction;
    using game::presentation::SystemMapPanelActionType;

    if (viewport.width <= 0 || viewport.height <= 0)
    {
        m_systemPanelLeftWasDown = leftDown;
        return std::nullopt;
    }

    const auto layout = makePanelLayout(viewport);
    const double localX = mouseX - static_cast<double>(viewport.x);
    const double localY = mouseY - static_cast<double>(viewport.y);
    const bool inside = systemMapPanelContains(viewport, mouseX, mouseY);
    const bool pressed = leftDown && !m_systemPanelLeftWasDown;
    m_systemPanelLeftWasDown = leftDown;

    if (!inside)
    {
        if (pressed)
            m_systemDropdownOpen = false;
        return std::nullopt;
    }

    const int rowCount = static_cast<int>(panel.systems.size());
    const int maxFirst = std::max(0, rowCount - layout.visibleRows);
    m_systemDropdownFirstRow = std::clamp(m_systemDropdownFirstRow, 0, maxFirst);

    if (m_systemDropdownOpen && scrollY != 0.0)
    {
        const int direction = scrollY > 0.0 ? -1 : 1;
        m_systemDropdownFirstRow = std::clamp(
            m_systemDropdownFirstRow + direction,
            0,
            maxFirst);
    }

    if (!pressed)
        return std::nullopt;

    if (layout.dropdownButton.contains(localX, localY))
    {
        m_systemDropdownOpen = !m_systemDropdownOpen;
        if (m_systemDropdownOpen)
        {
            const auto selected = std::find_if(
                panel.systems.begin(),
                panel.systems.end(),
                [](const auto& item) { return item.selected; });
            if (selected != panel.systems.end())
            {
                const int selectedIndex = static_cast<int>(
                    std::distance(panel.systems.begin(), selected));
                m_systemDropdownFirstRow = std::clamp(
                    selectedIndex - layout.visibleRows / 2,
                    0,
                    maxFirst);
            }
        }
        return std::nullopt;
    }

    if (m_systemDropdownOpen && layout.dropdownMenu.contains(localX, localY))
    {
        const int visibleRow = static_cast<int>(
            (localY - layout.dropdownMenu.y) / DropdownRowHeight);
        const int itemIndex = m_systemDropdownFirstRow + visibleRow;
        if (itemIndex >= 0 && itemIndex < rowCount)
        {
            m_systemDropdownOpen = false;
            return SystemMapPanelAction{
                SystemMapPanelActionType::SelectSystem,
                panel.systems[static_cast<std::size_t>(itemIndex)].id
            };
        }
        return std::nullopt;
    }

    if (m_systemDropdownOpen)
        m_systemDropdownOpen = false;

    const auto buttons = game::presentation::buildSystemMapPanelNavigationActions(panel);
    for (std::size_t i = 0; i < buttons.size(); ++i)
    {
        if (!buttons[i].enabled ||
            !layout.navigationButtons[i].contains(localX, localY))
        {
            continue;
        }

        return SystemMapPanelAction{buttons[i].action, -1};
    }

    return std::nullopt;
}

void InSessionPresentationRenderer::renderSystemMapPanel(
    const Viewport& viewport,
    const game::localization::LocalizationService& localization,
    const game::presentation::SystemMapPanelPresentation& panel)
{
    if (viewport.width <= 0 || viewport.height <= 0)
        return;

    glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
    glScissor(viewport.x, viewport.y, viewport.width, viewport.height);
    glDisable(GL_DEPTH_TEST);

    auto& text = TextRenderer::instance();
    text.beginFrameForViewport(viewport.width, viewport.height);

    const auto layout = makePanelLayout(viewport);
    const float w = static_cast<float>(viewport.width);
    const float h = static_cast<float>(viewport.height);
    const float panelX = layout.panelX;
    const float panelW = layout.panelW;
    const float pad = layout.pad;

    text.solidRectPx(panelX, 0.0f, panelW, h, glm::vec4(0.012f, 0.030f, 0.055f, 1.0f));
    text.solidRectPx(panelX, 0.0f, 1.0f, h, glm::vec4(0.36f, 0.66f, 0.86f, 0.55f));

    const int titlePx = std::clamp(static_cast<int>(h * 0.040f), 24, 40);
    const int bodyPx = std::clamp(static_cast<int>(h * 0.019f), 12, 17);
    const int smallPx = std::clamp(static_cast<int>(h * 0.016f), 11, 14);

    float y = std::max(42.0f, h * 0.075f);
    text.textDrawPx(
        localization.text("map.title", "STAR ATLAS"),
        panelX + pad,
        y,
        titlePx,
        glm::vec4(1.0f, 0.70f, 0.42f, 0.98f));
    y += static_cast<float>(titlePx + 10);
    text.textDrawPx(
        localization.text("map.subtitle", "Navigation interface"),
        panelX + pad,
        y,
        smallPx,
        glm::vec4(0.55f, 0.70f, 0.83f, 0.92f));
    y += static_cast<float>(smallPx + 25);

    const float valueX = panelX + panelW * 0.43f;
    drawLabelValue(text, panelX + pad, y, valueX, bodyPx,
        localization.text("map.mode", "Mode"),
        modeLabel(panel.mode, panel.systemLayerIsSpace, localization));
    drawLabelValue(text, panelX + pad, y, valueX, bodyPx,
        localization.text("map.universe_time", "Universe time"),
        panel.universeDate.empty() ? "—" : panel.universeDate);
    drawLabelValue(text, panelX + pad, y, valueX, bodyPx,
        localization.text("map.current_system", "Current system"),
        panel.currentSystemName.empty() ? "—" : panel.currentSystemName);

    y += 7.0f;
    text.solidRectPx(panelX + pad, y, panelW - pad * 2.0f, 1.0f,
        glm::vec4(0.36f, 0.56f, 0.74f, 0.28f));
    y += 22.0f;

    text.textDrawPx(
        localization.text("map.selected_system", "Selected system"),
        panelX + pad,
        y,
        bodyPx,
        glm::vec4(1.0f, 0.70f, 0.42f, 0.96f));
    y += static_cast<float>(bodyPx + 13);

    const auto* selected = selectedSystem(panel);
    const std::string dash = "—";
    drawLabelValue(text, panelX + pad, y, valueX, bodyPx,
        localization.text("map.name", "Name"), selected ? selected->name : dash);
    drawLabelValue(text, panelX + pad, y, valueX, bodyPx,
        localization.text("map.star", "Star"),
        selected && !selected->starType.empty() ? selected->starType : dash);
    drawLabelValue(text, panelX + pad, y, valueX, bodyPx,
        localization.text("map.jurisdiction", "Jurisdiction"),
        selected
            ? (selected->jurisdiction.empty()
                ? localization.text("map.unregistered", "Unregistered")
                : selected->jurisdiction)
            : dash);
    drawLabelValue(text, panelX + pad, y, valueX, bodyPx,
        localization.text("map.distance", "Distance"),
        selected ? formatDistanceLy(selected->distanceFromPlayerLy) : dash);

    text.textDrawPx(
        localization.text("map.system_list", "System list"),
        layout.dropdownButton.x,
        layout.dropdownButton.y - static_cast<float>(bodyPx + 7),
        bodyPx,
        glm::vec4(1.0f, 0.70f, 0.42f, 0.94f));

    text.solidRectPx(
        layout.dropdownButton.x,
        layout.dropdownButton.y,
        layout.dropdownButton.w,
        layout.dropdownButton.h,
        glm::vec4(0.025f, 0.065f, 0.105f, 1.0f));
    text.solidRectPx(
        layout.dropdownButton.x,
        layout.dropdownButton.y,
        layout.dropdownButton.w,
        1.0f,
        glm::vec4(0.32f, 0.62f, 0.86f, 0.68f));
    const std::string dropdownLabel = selected
        ? systemRowLabel(*selected)
        : localization.text("map.select_system", "Select system");
    text.textDrawPx(
        dropdownLabel,
        layout.dropdownButton.x + 9.0f,
        layout.dropdownButton.y + 8.0f,
        smallPx,
        glm::vec4(0.76f, 0.88f, 0.96f, 0.96f));
    text.textDrawPx(
        m_systemDropdownOpen ? "^" : "v",
        layout.dropdownButton.x + layout.dropdownButton.w - 18.0f,
        layout.dropdownButton.y + 8.0f,
        smallPx,
        glm::vec4(0.65f, 0.80f, 0.91f, 0.90f));

    if (m_systemDropdownOpen)
    {
        text.solidRectPx(
            layout.dropdownMenu.x,
            layout.dropdownMenu.y,
            layout.dropdownMenu.w,
            layout.dropdownMenu.h,
            glm::vec4(0.008f, 0.023f, 0.040f, 1.0f));

        const int rowCount = static_cast<int>(panel.systems.size());
        const int maxFirst = std::max(0, rowCount - layout.visibleRows);
        m_systemDropdownFirstRow = std::clamp(m_systemDropdownFirstRow, 0, maxFirst);
        const int end = std::min(rowCount, m_systemDropdownFirstRow + layout.visibleRows);
        for (int i = m_systemDropdownFirstRow; i < end; ++i)
        {
            const auto& item = panel.systems[static_cast<std::size_t>(i)];
            const float rowY = layout.dropdownMenu.y +
                static_cast<float>(i - m_systemDropdownFirstRow) * DropdownRowHeight;
            if (item.selected)
            {
                text.solidRectPx(
                    layout.dropdownMenu.x,
                    rowY,
                    layout.dropdownMenu.w,
                    DropdownRowHeight,
                    glm::vec4(0.055f, 0.120f, 0.175f, 1.0f));
            }
            text.textDrawPx(
                systemRowLabel(item),
                layout.dropdownMenu.x + 8.0f,
                rowY + 6.0f,
                smallPx,
                item.selected
                    ? glm::vec4(0.68f, 0.90f, 1.0f, 0.98f)
                    : glm::vec4(0.67f, 0.78f, 0.87f, 0.92f));
        }
    }

    const auto buttons = game::presentation::buildSystemMapPanelNavigationActions(panel);
    for (std::size_t i = 0; i < buttons.size(); ++i)
    {
        drawButton(
            text,
            layout.navigationButtons[i],
            navigationButtonLabel(buttons[i].action, panel, localization),
            smallPx,
            buttons[i].enabled
        );
    }

    const std::string hint = localization.text(
        "map.hint",
        "F9 Galaxy · F10 System · F11 Details · F12 Local / Hub");
    text.textDrawPx(
        hint,
        panelX + pad,
        h - 90.0f,
        smallPx,
        glm::vec4(0.48f, 0.61f, 0.74f, 0.88f));

    text.endFrame();
    glEnable(GL_DEPTH_TEST);
}
}
