#pragma once

#include <chrono>
#include <string>
#include <unordered_map>

#include <glm/glm.hpp>

#include "src/game/navigation/NavigationTrackingState.h"
#include "src/render/types/Viewport.h"

namespace game::system_map
{

struct NavigationRouteOverlayPointerResult
{
    bool consumed = false;
    std::string focusSourceObjectId;
    std::string selectedSourceObjectId;
};

class NavigationRouteOverlayState
{
public:
    static constexpr double WidthPx = 274.0;
    static constexpr double HeaderHeightPx = 30.0;
    static constexpr double MasterRowHeightPx = 28.0;
    static constexpr double WaypointRowHeightPx = 48.0;
    static constexpr double FinishRowHeightPx = 84.0;
    static constexpr double FooterHeightPx = 30.0;

    bool collapsed() const noexcept { return m_collapsed; }
    const glm::dvec2& topLeftPx() const noexcept { return m_topLeftPx; }

    double panelHeight(const game::navigation::NavigationTrackingState& tracking) const;

    NavigationRouteOverlayPointerResult handlePointer(
        game::navigation::NavigationTrackingState& tracking,
        const glm::dvec2& viewportSizePx,
        const glm::dvec2& mousePx,
        bool inside,
        bool leftDown
    );

    void clearTransientDrag() noexcept;

    bool deleteRouteArmed() const noexcept { return m_deleteRouteArmed; }
    const std::string& deleteNodeArmedId() const noexcept { return m_deleteNodeArmedId; }
    const std::string& draggingNodeId() const noexcept { return m_draggingNodeId; }
    const std::string& selectedNodeId() const noexcept { return m_selectedNodeId; }
    bool isNodeSelected(const std::string& sourceObjectId) const noexcept
    {
        return !sourceObjectId.empty() && sourceObjectId == m_selectedNodeId;
    }

    // After a drop, rows glide from their previous order into the new order so
    // reordering has unmistakable visual feedback without adding UI chrome.
    double reorderOffsetPx(
        const game::navigation::NavigationWaypoint& waypoint
    ) const;

    double draggingVisualOffsetPx(
        const game::navigation::NavigationWaypoint& waypoint,
        double nominalTopPx
    ) const;

private:
    void ensurePlaced(const glm::dvec2& viewportSizePx);
    void clampToViewport(const glm::dvec2& viewportSizePx, double height);

private:
    glm::dvec2 m_topLeftPx {0.0};
    bool m_placed = false;
    bool m_collapsed = false;
    bool m_leftWasDown = false;
    bool m_draggingPanel = false;
    glm::dvec2 m_panelDragOffsetPx {0.0};

    std::string m_pressedRowId;
    glm::dvec2 m_pressedAtPx {0.0};
    std::string m_draggingNodeId;
    std::string m_selectedNodeId;
    std::string m_lastClickedRowId;
    std::chrono::steady_clock::time_point m_lastClickedAt {};

    double m_dragPointerViewportY = 0.0;
    double m_dragGrabOffsetY = 0.0;
    bool m_liveNodeDrag = false;

    std::unordered_map<std::string, int> m_reorderFromSequence;
    std::chrono::steady_clock::time_point m_reorderAnimationStartedAt {};
    bool m_reorderAnimationActive = false;

    bool m_deleteRouteArmed = false;
    std::string m_deleteNodeArmedId;
};

class NavigationRouteOverlayRenderer
{
public:
    void render(
        const Viewport& viewport,
        const game::navigation::NavigationTrackingState& tracking,
        const NavigationRouteOverlayState& state,
        const std::string& locale
    ) const;
};

} // namespace game::system_map
