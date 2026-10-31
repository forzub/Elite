#pragma once

#include <memory>
#include <string>
#include <vector>

#include "src/render/navigation/NavigationOverlayVisualSettings.h"
#include "render/types/Viewport.h"

class Font;

namespace render::hud
{
class HudPrimitiveBatch;
}

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

struct NavigationOverlayTextProfile
{
    std::string player = "PLAYER";
    std::string selected = "SELECTED";
    std::string cursor = "CURSOR";

    std::string galaxy = "GALAXY";
    std::string system = "SYSTEM";
    std::string edge = "EDGE";
    std::string format = "FORMAT";
    std::string level = "LEVEL";

    std::string track = "TRACK";
    std::string trackOn = "TRACK ON";

    std::string hierarchicalFormat = "STRAIGHT THERE";
    std::string axisFormat = "THREE AXES";
    std::string packedFormat = "VERY SECRET CODE";
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

// One map button: drawn as a rounded wire bar with the label inside.
struct NavigationOverlayButton
{
    std::string label;
    NavigationOverlayButtonBounds bounds;
    bool hovered = false;
    bool enabled = true;
    bool active = false;
};

// A context menu as laid out by its owner: title + command rows.
struct NavigationContextMenuView
{
    NavigationOverlayButtonBounds bounds;
    float headerHeightPx = 26.0f;
    float rowHeightPx = 24.0f;
    std::string title;
    std::vector<std::string> rows;
    std::vector<bool> enabled;
    int hovered = -1;
};

class NavigationCoordinateOverlay
{
public:
    NavigationCoordinateOverlay();
    ~NavigationCoordinateOverlay();

    void setTextProfile(const NavigationOverlayTextProfile& profile)
    {
        m_textProfile = profile;
    }

    const NavigationOverlayTextProfile& textProfile() const noexcept
    {
        return m_textProfile;
    }

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

    // Galaxy Map: one level up (left of LEVEL 0, where System has TRACK).
    // besideTrack: the System map keeps TRACK left of LEVEL 0; up goes
    // left of TRACK.
    static NavigationOverlayButtonBounds
        upButtonBounds(
            const Viewport& viewport,
            bool besideTrack = false
        );

    // Back to the previous map: left of `rightNeighbour`, or the rightmost
    // slot when there is no neighbour (Details / Hub).
    static NavigationOverlayButtonBounds
        backButtonBounds(
            const Viewport& viewport,
            const NavigationOverlayButtonBounds* rightNeighbour
        );

    void drawButtons(
        const Viewport& viewport,
        const std::vector<NavigationOverlayButton>& buttons
    );

    void drawContextMenu(
        const Viewport& viewport,
        const NavigationContextMenuView& menu
    );

    // Galaxy Map breadcrumbs (GALAXY > G0 [..] > G1 [..]): one bounds per
    // crumb, laid out above the footer; separators are not clickable.
    std::vector<NavigationOverlayButtonBounds> breadcrumbBounds(
        const Viewport& viewport,
        const std::vector<std::string>& crumbs
    );

    void drawBreadcrumbs(
        const Viewport& viewport,
        const std::vector<std::string>& crumbs,
        int hoveredCrumb
    );

private:
    void ensureFont();
    void ensureBatch();
    float screenScaleFor(const Viewport& viewport) const;

private:
    std::unique_ptr<Font> m_font;
    std::unique_ptr<render::hud::HudPrimitiveBatch> m_batch;
    NavigationOverlayVisualSettings m_visuals;
    NavigationOverlayTextProfile m_textProfile;
};

} // namespace render::navigation
