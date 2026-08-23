#pragma once

#include <chrono>
#include <cstdint>
#include <unordered_map>

#include <glm/glm.hpp>

#include "src/game/navigation/RoutePlan.h"
#include "src/game/system_map/NavigationMapTextProfile.h"
#include "src/render/types/Viewport.h"

namespace game::system_map
{

struct NavigationRouteOverlayPointerResult
{
    bool consumed = false;
    std::uint64_t focusRouteNodeId = 0;
    std::uint64_t selectedRouteNodeId = 0;
};

class NavigationRouteOverlayState
{
public:
    static constexpr double WidthPx = 274.0;
    static constexpr double HeaderHeightPx = 30.0;
    static constexpr double MasterRowHeightPx = 28.0;
    static constexpr double StartRowHeightPx = 48.0;
    static constexpr double WaypointRowHeightPx = 48.0;
    static constexpr double FinishRowHeightPx = 84.0;
    static constexpr double FooterHeightPx = 30.0;

    bool collapsed() const noexcept { return m_collapsed; }
    const glm::dvec2& topLeftPx() const noexcept { return m_topLeftPx; }

    double panelHeight(const game::navigation::RoutePlan& routePlan) const;

    NavigationRouteOverlayPointerResult handlePointer(
        game::navigation::RoutePlan& routePlan,
        const glm::dvec2& viewportSizePx,
        const glm::dvec2& mousePx,
        bool inside,
        bool leftDown
    );

    void clearTransientDrag() noexcept;

    bool deleteRouteArmed() const noexcept { return m_deleteRouteArmed; }
    std::uint64_t deleteNodeArmedId() const noexcept { return m_deleteNodeArmedId; }
    std::uint64_t draggingNodeId() const noexcept { return m_draggingNodeId; }
    std::uint64_t selectedNodeId() const noexcept { return m_selectedNodeId; }
    bool isNodeSelected(std::uint64_t routeNodeId) const noexcept
    {
        return routeNodeId != 0 && routeNodeId == m_selectedNodeId;
    }

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

    std::uint64_t m_pressedRowId = 0;
    glm::dvec2 m_pressedAtPx {0.0};
    std::uint64_t m_draggingNodeId = 0;
    std::uint64_t m_selectedNodeId = 0;
    std::uint64_t m_lastClickedRowId = 0;
    std::chrono::steady_clock::time_point m_lastClickedAt {};

    double m_dragPointerViewportY = 0.0;
    double m_dragGrabOffsetY = 0.0;
    bool m_liveNodeDrag = false;

    std::unordered_map<std::uint64_t, int> m_reorderFromSequence;
    std::chrono::steady_clock::time_point m_reorderAnimationStartedAt {};
    bool m_reorderAnimationActive = false;

    bool m_deleteRouteArmed = false;
    std::uint64_t m_deleteNodeArmedId = 0;
};

class NavigationRouteOverlayRenderer
{
public:
    void render(
        const Viewport& viewport,
        const game::navigation::RoutePlan& routePlan,
        const NavigationRouteOverlayState& state,
        const NavigationMapTextProfile& textProfile
    ) const;
};

} // namespace game::system_map
