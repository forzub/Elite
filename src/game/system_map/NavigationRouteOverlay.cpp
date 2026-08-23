#include "src/game/system_map/NavigationRouteOverlay.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

#include <glad/gl.h>

#include "render/HUD/TextRenderer.h"

namespace game::system_map
{
namespace
{
constexpr glm::vec4 kBackground(0.08f, 0.10f, 0.13f, 0.94f);
constexpr glm::vec4 kHeader(0.15f, 0.17f, 0.22f, 0.97f);
constexpr glm::vec4 kBorder(0.72f, 0.78f, 0.86f, 0.72f);
constexpr glm::vec4 kText(0.93f, 0.95f, 0.98f, 1.00f);
constexpr glm::vec4 kMuted(0.62f, 0.69f, 0.76f, 1.00f);
constexpr glm::vec4 kWaypoint(0.40f, 0.92f, 0.60f, 0.94f);
constexpr glm::vec4 kFinish(1.00f, 0.82f, 0.30f, 0.96f);
constexpr glm::vec4 kDanger(1.00f, 0.34f, 0.30f, 0.96f);

struct ScreenSpaceState
{
    GLint program = 0;
    GLint matrixMode = GL_MODELVIEW;
    GLint blendSrc = GL_ONE;
    GLint blendDst = GL_ZERO;
    GLboolean depthEnabled = GL_FALSE;
    GLboolean blendEnabled = GL_FALSE;
    GLfloat lineWidth = 1.0f;
};

ScreenSpaceState beginScreenSpace(const Viewport& viewport)
{
    ScreenSpaceState previous;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previous.program);
    glGetIntegerv(GL_MATRIX_MODE, &previous.matrixMode);
    glGetIntegerv(GL_BLEND_SRC_RGB, &previous.blendSrc);
    glGetIntegerv(GL_BLEND_DST_RGB, &previous.blendDst);
    previous.depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    previous.blendEnabled = glIsEnabled(GL_BLEND);
    glGetFloatv(GL_LINE_WIDTH, &previous.lineWidth);

    glUseProgram(0);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, viewport.width, viewport.height, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    return previous;
}

void endScreenSpace(const ScreenSpaceState& previous)
{
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glLineWidth(previous.lineWidth);
    glBlendFunc(previous.blendSrc, previous.blendDst);
    if (previous.blendEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (previous.depthEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    glUseProgram(static_cast<GLuint>(previous.program));
    glMatrixMode(previous.matrixMode);
}

void rect(const glm::dvec2& p, double w, double h, const glm::vec4& c)
{
    glColor4f(c.r, c.g, c.b, c.a);
    glBegin(GL_QUADS);
    glVertex2d(p.x, p.y);
    glVertex2d(p.x + w, p.y);
    glVertex2d(p.x + w, p.y + h);
    glVertex2d(p.x, p.y + h);
    glEnd();
}

void outline(const glm::dvec2& p, double w, double h, const glm::vec4& c, float width = 1.0f)
{
    glColor4f(c.r, c.g, c.b, c.a);
    glLineWidth(width);
    glBegin(GL_LINE_LOOP);
    glVertex2d(p.x, p.y);
    glVertex2d(p.x + w, p.y);
    glVertex2d(p.x + w, p.y + h);
    glVertex2d(p.x, p.y + h);
    glEnd();
    glLineWidth(1.0f);
}

void line(const glm::dvec2& a, const glm::dvec2& b, const glm::vec4& c, float width = 1.0f)
{
    glColor4f(c.r, c.g, c.b, c.a);
    glLineWidth(width);
    glBegin(GL_LINES);
    glVertex2d(a.x, a.y);
    glVertex2d(b.x, b.y);
    glEnd();
    glLineWidth(1.0f);
}

bool contains(const glm::dvec2& p, const glm::dvec2& tl, double w, double h)
{
    return p.x >= tl.x && p.x <= tl.x + w && p.y >= tl.y && p.y <= tl.y + h;
}

const std::string& arrivalLabel(
    game::navigation::NavigationArrivalMode mode,
    const NavigationMapTextProfile& textProfile
)
{
    using Mode = game::navigation::NavigationArrivalMode;
    switch (mode)
    {
        case Mode::SafeZone: return textProfile.arrivalSafeZone;
        case Mode::Follow: return textProfile.arrivalFollow;
        case Mode::Formation: return textProfile.arrivalFormation;
        case Mode::ParadeFormation: return textProfile.arrivalParade;
    }
    return textProfile.arrivalSafeZone;
}

void drawArrivalGlyph(
    const glm::dvec2& topLeft,
    double size,
    game::navigation::NavigationArrivalMode mode,
    const glm::vec4& color
)
{
    const glm::dvec2 c = topLeft + glm::dvec2(size * 0.5);
    const double r = size * 0.12;
    auto dot = [&](double x, double y, double scale = 1.0)
    {
        rect(glm::dvec2(x - r * scale, y - r * scale), r * 2.0 * scale, r * 2.0 * scale, color);
    };

    using Mode = game::navigation::NavigationArrivalMode;
    if (mode == Mode::SafeZone)
    {
        outline(topLeft + glm::dvec2(size * 0.20), size * 0.60, size * 0.60, color, 1.2f);
        dot(c.x, c.y, 0.75);
        dot(topLeft.x + size * 0.12, c.y, 0.45);
    }
    else if (mode == Mode::Follow)
    {
        dot(topLeft.x + size * 0.60, topLeft.y + size * 0.42, 0.85);
        dot(topLeft.x + size * 0.33, topLeft.y + size * 0.64, 0.60);
        line(
            glm::dvec2(topLeft.x + size * 0.40, topLeft.y + size * 0.60),
            glm::dvec2(topLeft.x + size * 0.52, topLeft.y + size * 0.48),
            color,
            1.2f
        );
    }
    else if (mode == Mode::Formation)
    {
        dot(c.x, topLeft.y + size * 0.30, 0.75);
        dot(topLeft.x + size * 0.30, topLeft.y + size * 0.62, 0.58);
        dot(topLeft.x + size * 0.70, topLeft.y + size * 0.62, 0.58);
    }
    else
    {
        dot(topLeft.x + size * 0.32, topLeft.y + size * 0.34, 0.55);
        dot(topLeft.x + size * 0.68, topLeft.y + size * 0.34, 0.55);
        dot(topLeft.x + size * 0.32, topLeft.y + size * 0.68, 0.55);
        dot(topLeft.x + size * 0.68, topLeft.y + size * 0.68, 0.55);
    }
}

std::string shortAssetName(const game::navigation::NavigationAssetRef& asset)
{
    using Kind = game::navigation::NavigationAssetKind;
    if (asset.kind == Kind::Ship)
        return "SHIP #" + std::to_string(asset.shipInstanceId);
    if (asset.kind == Kind::Drone)
        return "DRONE #" + std::to_string(asset.droneInstanceId.value);
    return "UNASSIGNED";
}

std::string shortName(const game::navigation::NavigationWaypoint& waypoint)
{
    const std::string source = waypoint.displayName.empty()
        ? waypoint.address
        : waypoint.displayName;
    if (source.size() <= 28)
        return source;
    return source.substr(0, 25) + "...";
}

} // namespace

double NavigationRouteOverlayState::panelHeight(
    const game::navigation::RoutePlan& routePlan
) const
{
    if (m_collapsed)
        return HeaderHeightPx;

    double h = HeaderHeightPx + MasterRowHeightPx + StartRowHeightPx + FooterHeightPx + 8.0;
    for (const auto* waypoint : routePlan.orderedRouteWaypoints())
    {
        h += waypoint->role == game::navigation::NavigationWaypointRole::Finish
            ? FinishRowHeightPx
            : WaypointRowHeightPx;
    }
    return h;
}

void NavigationRouteOverlayState::ensurePlaced(const glm::dvec2& viewportSizePx)
{
    if (!m_placed)
    {
        m_topLeftPx = glm::dvec2(
            std::max(8.0, viewportSizePx.x - WidthPx - 16.0),
            72.0
        );
        m_placed = true;
    }
}

void NavigationRouteOverlayState::clampToViewport(
    const glm::dvec2& viewportSizePx,
    double height
)
{
    m_topLeftPx.x = std::clamp(
        m_topLeftPx.x,
        4.0,
        std::max(4.0, viewportSizePx.x - WidthPx - 4.0)
    );
    m_topLeftPx.y = std::clamp(
        m_topLeftPx.y,
        4.0,
        std::max(4.0, viewportSizePx.y - std::min(height, viewportSizePx.y - 8.0) - 4.0)
    );
}

void NavigationRouteOverlayState::clearTransientDrag() noexcept
{
    m_draggingPanel = false;
    m_draggingNodeId = 0;
    m_pressedRowId = 0;
    m_liveNodeDrag = false;
}

double NavigationRouteOverlayState::reorderOffsetPx(
    const game::navigation::NavigationWaypoint& waypoint
) const
{
    if (m_liveNodeDrag && waypoint.id == m_draggingNodeId)
        return 0.0;

    if (!m_reorderAnimationActive ||
        waypoint.role != game::navigation::NavigationWaypointRole::Intermediate)
    {
        return 0.0;
    }

    const auto found = m_reorderFromSequence.find(waypoint.id);
    if (found == m_reorderFromSequence.end())
        return 0.0;

    constexpr double durationSeconds = 0.28;
    const double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - m_reorderAnimationStartedAt
    ).count();
    if (elapsed >= durationSeconds)
        return 0.0;

    const double linear = std::clamp(elapsed / durationSeconds, 0.0, 1.0);
    const double eased = 1.0 - std::pow(1.0 - linear, 3.0);
    const double fromY = static_cast<double>(found->second - 1) * WaypointRowHeightPx;
    const double toY = static_cast<double>(waypoint.sequence - 1) * WaypointRowHeightPx;
    return (fromY - toY) * (1.0 - eased);
}

double NavigationRouteOverlayState::draggingVisualOffsetPx(
    const game::navigation::NavigationWaypoint& waypoint,
    double nominalTopPx
) const
{
    if (!m_liveNodeDrag || waypoint.id != m_draggingNodeId)
        return 0.0;

    const double draggedTop = m_dragPointerViewportY - m_dragGrabOffsetY;
    return draggedTop - nominalTopPx;
}

NavigationRouteOverlayPointerResult NavigationRouteOverlayState::handlePointer(
    game::navigation::RoutePlan& routePlan,
    const glm::dvec2& viewportSizePx,
    const glm::dvec2& mousePx,
    bool inside,
    bool leftDown
)
{
    NavigationRouteOverlayPointerResult result;
    ensurePlaced(viewportSizePx);
    const double height = panelHeight(routePlan);
    clampToViewport(viewportSizePx, height);

    const bool press = leftDown && !m_leftWasDown;
    const bool release = !leftDown && m_leftWasDown;
    m_leftWasDown = leftDown;

    if (!routePlan.hasRoute())
    {
        clearTransientDrag();
        m_deleteRouteArmed = false;
        m_deleteNodeArmedId = 0;
        m_selectedNodeId = 0;
        return result;
    }

    const bool overPanel = inside && contains(mousePx, m_topLeftPx, WidthPx, height);

    if (m_draggingPanel && leftDown)
    {
        m_topLeftPx = mousePx - m_panelDragOffsetPx;
        clampToViewport(viewportSizePx, height);
        result.consumed = true;
        return result;
    }
    if (m_draggingPanel && release)
    {
        m_draggingPanel = false;
        result.consumed = true;
        return result;
    }

    if (m_draggingNodeId != 0 && leftDown)
    {
        const double distance = glm::length(mousePx - m_pressedAtPx);
        if (distance >= 6.0)
            m_liveNodeDrag = true;

        if (m_liveNodeDrag)
        {
            m_dragPointerViewportY = mousePx.y;
            int targetSequence = 1;
            double y = m_topLeftPx.y + HeaderHeightPx + MasterRowHeightPx + StartRowHeightPx + 4.0;
            for (const auto* waypoint : routePlan.orderedRouteWaypoints())
            {
                if (waypoint->role != game::navigation::NavigationWaypointRole::Intermediate)
                    break;
                if (waypoint->id == m_draggingNodeId)
                    continue;
                const double mid = y + WaypointRowHeightPx * 0.5;
                if (mousePx.y > mid)
                    ++targetSequence;
                y += WaypointRowHeightPx;
            }
            routePlan.moveIntermediateWaypoint(m_draggingNodeId, targetSequence);
            result.consumed = true;
            result.selectedRouteNodeId = m_draggingNodeId;
            return result;
        }
    }

    if (release && m_pressedRowId != 0)
    {
        const std::uint64_t releasedId = m_pressedRowId;
        const bool dragged = m_liveNodeDrag && m_draggingNodeId != 0;

        if (dragged)
        {
            m_reorderFromSequence.clear();
            for (const auto* waypoint : routePlan.orderedRouteWaypoints())
            {
                if (waypoint->role ==
                    game::navigation::NavigationWaypointRole::Intermediate)
                {
                    m_reorderFromSequence.emplace(
                        waypoint->id,
                        waypoint->sequence
                    );
                }
            }
            m_reorderAnimationStartedAt = std::chrono::steady_clock::now();
            m_reorderAnimationActive = true;
            m_lastClickedRowId = 0;
        }
        else
        {
            const auto now = std::chrono::steady_clock::now();
            const double sinceLast = std::chrono::duration<double>(
                now - m_lastClickedAt
            ).count();
            if (m_lastClickedRowId == releasedId && sinceLast <= 0.38)
            {
                result.focusRouteNodeId = releasedId;
                m_lastClickedRowId = 0;
            }
            else
            {
                m_lastClickedRowId = releasedId;
                m_lastClickedAt = now;
            }
        }

        result.selectedRouteNodeId = releasedId;
        m_selectedNodeId = releasedId;
        m_draggingNodeId = 0;
        m_pressedRowId = 0;
        m_liveNodeDrag = false;
        result.consumed = true;
        return result;
    }

    if (!press)
    {
        if (overPanel || m_draggingNodeId != 0)
            result.consumed = overPanel || m_draggingNodeId != 0;
        if (m_selectedNodeId != 0)
            result.selectedRouteNodeId = m_selectedNodeId;
        return result;
    }

    if (!overPanel)
    {
        m_deleteRouteArmed = false;
        m_deleteNodeArmedId = 0;
        if (m_selectedNodeId != 0)
            result.selectedRouteNodeId = m_selectedNodeId;
        return result;
    }

    result.consumed = true;
    const glm::dvec2 local = mousePx - m_topLeftPx;
    const bool collapseHit =
        local.x >= WidthPx - 48.0 && local.x <= WidthPx - 29.0 &&
        local.y >= 5.0 && local.y <= 25.0;
    const bool deleteRouteHit =
        local.x >= WidthPx - 26.0 && local.x <= WidthPx - 6.0 &&
        local.y >= 5.0 && local.y <= 25.0;

    auto footerBounds = [&](double& footerTop, glm::dvec2& yesTop, glm::dvec2& noTop)
    {
        footerTop = height - FooterHeightPx;
        yesTop = glm::dvec2(WidthPx - 78.0, footerTop + 6.0);
        noTop = glm::dvec2(WidthPx - 40.0, footerTop + 6.0);
    };

    if (m_deleteRouteArmed || m_deleteNodeArmedId != 0)
    {
        double footerTop = 0.0;
        glm::dvec2 yesTop{0.0}, noTop{0.0};
        footerBounds(footerTop, yesTop, noTop);
        const bool yesHit = contains(local, yesTop, 28.0, 18.0);
        const bool noHit = contains(local, noTop, 28.0, 18.0);
        if (yesHit)
        {
            if (m_deleteRouteArmed)
            {
                routePlan.clearRoute();
                m_selectedNodeId = 0;
                m_deleteRouteArmed = false;
            }
            else if (m_deleteNodeArmedId != 0)
            {
                if (m_selectedNodeId == m_deleteNodeArmedId)
                    m_selectedNodeId = 0;
                routePlan.removeRouteWaypoint(m_deleteNodeArmedId);
                m_deleteNodeArmedId = 0;
            }
            return result;
        }
        if (noHit || local.y >= footerTop)
        {
            m_deleteRouteArmed = false;
            m_deleteNodeArmedId = 0;
            return result;
        }
    }

    if (collapseHit)
    {
        m_collapsed = !m_collapsed;
        m_deleteRouteArmed = false;
        m_deleteNodeArmedId = 0;
        return result;
    }
    if (deleteRouteHit)
    {
        m_deleteRouteArmed = true;
        m_deleteNodeArmedId = 0;
        return result;
    }
    if (local.y <= HeaderHeightPx)
    {
        m_draggingPanel = true;
        m_panelDragOffsetPx = mousePx - m_topLeftPx;
        m_deleteRouteArmed = false;
        m_deleteNodeArmedId = 0;
        return result;
    }
    if (m_collapsed)
        return result;

    double rowTop = HeaderHeightPx;
    if (local.y >= rowTop && local.y <= rowTop + MasterRowHeightPx)
    {
        routePlan.setRouteVisibleOnHud(!routePlan.routeVisibleOnHud());
        m_deleteRouteArmed = false;
        m_deleteNodeArmedId = 0;
        if (m_selectedNodeId != 0)
            result.selectedRouteNodeId = m_selectedNodeId;
        return result;
    }
    rowTop += MasterRowHeightPx + 4.0;

    // START is immutable and always first. It is route execution identity, not
    // a draggable waypoint, so clicks in this row are consumed without changing
    // waypoint selection/reorder state.
    if (local.y >= rowTop && local.y <= rowTop + StartRowHeightPx)
    {
        m_deleteRouteArmed = false;
        m_deleteNodeArmedId = 0;
        if (m_selectedNodeId != 0)
            result.selectedRouteNodeId = m_selectedNodeId;
        return result;
    }
    rowTop += StartRowHeightPx;

    for (const auto* waypoint : routePlan.orderedRouteWaypoints())
    {
        const double rowHeight =
            waypoint->role == game::navigation::NavigationWaypointRole::Finish
                ? FinishRowHeightPx
                : WaypointRowHeightPx;
        if (local.y < rowTop || local.y > rowTop + rowHeight)
        {
            rowTop += rowHeight;
            continue;
        }

        m_selectedNodeId = waypoint->id;
        result.selectedRouteNodeId = m_selectedNodeId;

        const bool hudHit = local.x >= 8.0 && local.x <= 28.0 &&
                            local.y >= rowTop + 8.0 && local.y <= rowTop + 28.0;
        const bool deleteNodeHit = local.x >= WidthPx - 27.0 && local.x <= WidthPx - 7.0 &&
                                   local.y >= rowTop + 8.0 && local.y <= rowTop + 28.0;
        if (hudHit)
        {
            routePlan.setWaypointHudVisible(waypoint->id, !waypoint->showOnHud);
            m_deleteNodeArmedId = 0;
            m_deleteRouteArmed = false;
            return result;
        }
        if (deleteNodeHit)
        {
            m_deleteNodeArmedId = waypoint->id;
            m_deleteRouteArmed = false;
            return result;
        }

        if (waypoint->role == game::navigation::NavigationWaypointRole::Finish)
        {
            constexpr double iconSize = 28.0;
            constexpr double gap = 6.0;
            const double iconsTop = rowTop + rowHeight - iconSize - 8.0;
            const double iconsLeft = 39.0;
            using Mode = game::navigation::NavigationArrivalMode;
            const Mode modes[4] = {
                Mode::SafeZone,
                Mode::Follow,
                Mode::Formation,
                Mode::ParadeFormation
            };
            for (int i = 0; i < 4; ++i)
            {
                const double x = iconsLeft + i * (iconSize + gap);
                if (local.x >= x && local.x <= x + iconSize &&
                    local.y >= iconsTop && local.y <= iconsTop + iconSize)
                {
                    routePlan.setFinishArrivalMode(modes[i]);
                    m_deleteNodeArmedId = 0;
                    m_deleteRouteArmed = false;
                    return result;
                }
            }
        }

        m_deleteNodeArmedId = 0;
        m_deleteRouteArmed = false;
        m_pressedRowId = waypoint->id;
        m_pressedAtPx = mousePx;
        m_dragPointerViewportY = mousePx.y;
        m_dragGrabOffsetY = local.y - rowTop;
        m_liveNodeDrag = false;
        if (waypoint->role == game::navigation::NavigationWaypointRole::Intermediate)
            m_draggingNodeId = waypoint->id;
        return result;
    }

    m_deleteRouteArmed = false;
    m_deleteNodeArmedId = 0;
    if (m_selectedNodeId != 0)
        result.selectedRouteNodeId = m_selectedNodeId;
    return result;
}

void NavigationRouteOverlayRenderer::render(
    const Viewport& viewport,
    const game::navigation::RoutePlan& routePlan,
    const NavigationRouteOverlayState& state,
    const NavigationMapTextProfile& textProfile
) const
{
    if (!routePlan.hasRoute())
        return;

    const auto& top = state.topLeftPx();
    const double height = state.panelHeight(routePlan);
    const ScreenSpaceState previous = beginScreenSpace(viewport);
    auto& text = TextRenderer::instance();
    text.beginFrameForViewport(viewport.width, viewport.height);

    rect(top, NavigationRouteOverlayState::WidthPx, height, kBackground);
    rect(top, NavigationRouteOverlayState::WidthPx, NavigationRouteOverlayState::HeaderHeightPx, kHeader);
    outline(top, NavigationRouteOverlayState::WidthPx, height, kBorder, 1.2f);

    text.textDrawPx(
        textProfile.routeTitle,
        static_cast<float>(top.x + 10.0),
        static_cast<float>(top.y + 20.0),
        12,
        kText
    );
    const std::string count = std::to_string(routePlan.routeSize());
    text.textDrawPx(
        count,
        static_cast<float>(top.x + 76.0),
        static_cast<float>(top.y + 20.0),
        10,
        kMuted
    );
    text.textDrawPx(
        state.collapsed() ? "+" : "−",
        static_cast<float>(top.x + NavigationRouteOverlayState::WidthPx - 44.0),
        static_cast<float>(top.y + 20.0),
        13,
        kText
    );
    text.textDrawPx(
        "×",
        static_cast<float>(top.x + NavigationRouteOverlayState::WidthPx - 20.0),
        static_cast<float>(top.y + 20.0),
        13,
        state.deleteRouteArmed() ? kDanger : kText
    );

    if (state.collapsed())
    {
        text.endFrame();
        endScreenSpace(previous);
        return;
    }

    double y = top.y + NavigationRouteOverlayState::HeaderHeightPx;
    const glm::dvec2 masterBox(top.x + 9.0, y + 7.0);
    outline(masterBox, 14.0, 14.0, routePlan.routeVisibleOnHud() ? kWaypoint : kMuted, 1.2f);
    if (routePlan.routeVisibleOnHud())
        rect(masterBox + glm::dvec2(3.0), 8.0, 8.0, kWaypoint);
    text.textDrawPx(
        textProfile.showOnHud,
        static_cast<float>(top.x + 31.0),
        static_cast<float>(y + 19.0),
        10,
        routePlan.routeVisibleOnHud() ? kText : kMuted
    );
    y += NavigationRouteOverlayState::MasterRowHeightPx + 4.0;

    // START is a fixed semantic node. It identifies the vehicle executing the
    // route and never participates in waypoint drag/delete ordering.
    const glm::vec4 startAccent(0.40f, 0.72f, 1.00f, 0.94f);
    rect(
        glm::dvec2(top.x + 5.0, y),
        NavigationRouteOverlayState::WidthPx - 10.0,
        NavigationRouteOverlayState::StartRowHeightPx - 2.0,
        glm::vec4(0.10f, 0.13f, 0.17f, 0.94f)
    );
    outline(
        glm::dvec2(top.x + 5.0, y),
        NavigationRouteOverlayState::WidthPx - 10.0,
        NavigationRouteOverlayState::StartRowHeightPx - 2.0,
        startAccent,
        1.0f
    );
    text.textDrawPx(
        "S",
        static_cast<float>(top.x + 31.0),
        static_cast<float>(y + 18.0),
        11,
        startAccent
    );
    text.textDrawPx(
        textProfile.start,
        static_cast<float>(top.x + 54.0),
        static_cast<float>(y + 17.0),
        10,
        startAccent
    );
    text.textDrawPx(
        shortAssetName(routePlan.start().executor),
        static_cast<float>(top.x + 54.0),
        static_cast<float>(y + 34.0),
        10,
        kText
    );
    y += NavigationRouteOverlayState::StartRowHeightPx;

    for (const auto* waypoint : routePlan.orderedRouteWaypoints())
    {
        const bool finish = waypoint->role == game::navigation::NavigationWaypointRole::Finish;
        const double rowHeight = finish
            ? NavigationRouteOverlayState::FinishRowHeightPx
            : NavigationRouteOverlayState::WaypointRowHeightPx;
        const glm::vec4 accent = finish ? kFinish : kWaypoint;
        const bool dragging = state.draggingNodeId() == waypoint->id;
        const bool selected = state.isNodeSelected(waypoint->id);
        const double visualY =
            y +
            state.reorderOffsetPx(*waypoint) +
            state.draggingVisualOffsetPx(*waypoint, y + state.reorderOffsetPx(*waypoint));
        glm::vec4 rowFill = dragging
            ? glm::vec4(accent.r * 0.24f, accent.g * 0.24f, accent.b * 0.24f, 0.98f)
            : selected
                ? glm::vec4(accent.r * 0.14f, accent.g * 0.14f, accent.b * 0.14f, 0.96f)
                : glm::vec4(0.10f, 0.13f, 0.17f, 0.94f);
        rect(glm::dvec2(top.x + 5.0, visualY), NavigationRouteOverlayState::WidthPx - 10.0, rowHeight - 2.0, rowFill);
        outline(glm::dvec2(top.x + 5.0, visualY), NavigationRouteOverlayState::WidthPx - 10.0, rowHeight - 2.0, accent, dragging ? 1.8f : selected ? 1.4f : 1.0f);

        const glm::dvec2 hudBox(top.x + 9.0, visualY + 8.0);
        outline(hudBox, 14.0, 14.0, waypoint->showOnHud ? accent : kMuted, 1.1f);
        if (waypoint->showOnHud)
            rect(hudBox + glm::dvec2(3.0), 8.0, 8.0, accent);

        std::ostringstream index;
        if (finish)
            index << "F";
        else
            index << std::setw(2) << std::setfill('0') << waypoint->sequence;
        text.textDrawPx(
            index.str(),
            static_cast<float>(top.x + 31.0),
            static_cast<float>(visualY + 18.0),
            11,
            accent
        );
        text.textDrawPx(
            finish ? textProfile.finish : textProfile.waypoint,
            static_cast<float>(top.x + 54.0),
            static_cast<float>(visualY + 17.0),
            10,
            accent
        );
        text.textDrawPx(
            shortName(*waypoint),
            static_cast<float>(top.x + 54.0),
            static_cast<float>(visualY + 34.0),
            10,
            kText
        );

        const bool armed = state.deleteNodeArmedId() == waypoint->id;
        text.textDrawPx(
            "×",
            static_cast<float>(top.x + NavigationRouteOverlayState::WidthPx - 21.0),
            static_cast<float>(visualY + 19.0),
            12,
            armed ? kDanger : kMuted
        );

        if (finish)
        {
            constexpr double iconSize = 28.0;
            constexpr double gap = 6.0;
            const double iconsLeft = top.x + 39.0;
            const double iconsTop = visualY + rowHeight - iconSize - 8.0;
            using Mode = game::navigation::NavigationArrivalMode;
            const Mode modes[4] = {
                Mode::SafeZone,
                Mode::Follow,
                Mode::Formation,
                Mode::ParadeFormation
            };
            for (int i = 0; i < 4; ++i)
            {
                const glm::dvec2 iconTop(
                    iconsLeft + i * (iconSize + gap),
                    iconsTop
                );
                const bool active = waypoint->arrival.mode == modes[i];
                outline(iconTop, iconSize, iconSize, active ? accent : kMuted, active ? 1.6f : 1.0f);
                drawArrivalGlyph(iconTop, iconSize, modes[i], active ? accent : kMuted);
            }
            text.textDrawPx(
                arrivalLabel(waypoint->arrival.mode, textProfile),
                static_cast<float>(top.x + 186.0),
                static_cast<float>(iconsTop + 19.0),
                9,
                accent
            );
        }

        y += rowHeight;
    }

    const bool deleteRouteArmed = state.deleteRouteArmed();
    const bool deleteNodeArmed = state.deleteNodeArmedId() != 0;
    const double footerTop = top.y + height - NavigationRouteOverlayState::FooterHeightPx;
    if (deleteRouteArmed || deleteNodeArmed)
    {
        const std::string prompt = deleteRouteArmed
            ? textProfile.deleteRoute
            : textProfile.deleteWaypoint;
        text.textDrawPx(
            prompt,
            static_cast<float>(top.x + 9.0),
            static_cast<float>(footerTop + 18.0),
            9,
            kDanger
        );
        const glm::dvec2 yesTop(top.x + NavigationRouteOverlayState::WidthPx - 78.0, footerTop + 6.0);
        const glm::dvec2 noTop(top.x + NavigationRouteOverlayState::WidthPx - 40.0, footerTop + 6.0);
        outline(yesTop, 28.0, 18.0, kDanger, 1.0f);
        outline(noTop, 28.0, 18.0, kMuted, 1.0f);
        text.textDrawPx(
            textProfile.yes,
            static_cast<float>(yesTop.x + 4.0),
            static_cast<float>(yesTop.y + 13.0),
            8,
            kDanger
        );
        text.textDrawPx(
            textProfile.no,
            static_cast<float>(noTop.x + 5.0),
            static_cast<float>(noTop.y + 13.0),
            8,
            kMuted
        );
    }
    else
    {
        text.textDrawPx(
            textProfile.dragWaypoints,
            static_cast<float>(top.x + 9.0),
            static_cast<float>(top.y + height - 10.0),
            9,
            kMuted
        );
    }

    text.endFrame();
    endScreenSpace(previous);
}

} // namespace game::system_map
