#pragma once

#include <memory>
#include <string>
#include <vector>

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
        float levelAnnouncementAlpha = 0.0f
    );

private:
    void ensureFont();

private:
    std::unique_ptr<Font> m_font;
};

} // namespace render::navigation
