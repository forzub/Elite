#pragma once

#include <memory>
#include <string>
#include <vector>

#include "src/render/navigation/NavigationOverlayVisualSettings.h"
#include "render/types/Viewport.h"

class Font;

namespace render::navigation
{

enum class NavigationCoordinateRole
{
    Player,
    Selected,
    Hovered
};

struct NavigationCoordinateBlock
{
    NavigationCoordinateRole role =
        NavigationCoordinateRole::Player;

    std::string title;
    std::string regionNames;

    std::vector<std::string> addressLines;
};

struct NavigationOverlayButtonBounds
{
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;

    bool contains(
        double x,
        double y
    ) const
    {
        return
            x >= static_cast<double>(left) &&
            x <= static_cast<double>(right) &&
            y >= static_cast<double>(top) &&
            y <= static_cast<double>(bottom);
    }
};

class NavigationCoordinateOverlay
{
public:
    NavigationCoordinateOverlay();
    ~NavigationCoordinateOverlay();

    void draw(
        const Viewport& viewport,
        const std::vector<NavigationCoordinateBlock>& blocks,
        const std::string& footerText = {},
        const std::string& levelAnnouncement = {},
        float levelAnnouncementAlpha = 0.0f,
        bool showLevelZeroButton = false,
        bool levelZeroButtonHovered = false,
        bool showTrackButton = false,
        bool trackButtonHovered = false,
        bool trackButtonActive = false,
        bool trackButtonEnabled = false
    );

    static NavigationOverlayButtonBounds
        levelZeroButtonBounds(
            const Viewport& viewport
        );

    static NavigationOverlayButtonBounds
        trackButtonBounds(
            const Viewport& viewport
        );

private:
    void ensureFont();

private:
    std::unique_ptr<Font> m_font;
    NavigationOverlayVisualSettings m_visuals;
};

} // namespace render::navigation
