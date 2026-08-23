#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "src/world/coordinates/WorldPosition.h"
#include "src/game/identity/ShipInstanceId.h"
#include "src/game/system_map/ScreenHitTest.h"

namespace game::system_map
{

enum class MapObjectVelocityMode
{
    Global = 0,
    Local
};

enum class MapObjectGlyphKind
{
    Ship = 0,
    Hub,
    Infrastructure,
    DockingPort
};

enum class MapObjectInfoKind
{
    Tactical = 0,
    Celestial,
    WaypointCandidate,
    Infrastructure,
    DockingPort
};

enum class MapTrajectoryKind
{
    History = 0,
    Prediction,
    Planned
};

struct MapTrajectoryPoint
{
    double universeTimeSeconds = 0.0;
    glm::dvec3 position {0.0};

    // Projection is supplied by the current map presentation because the same
    // world-space trajectory may be shown on System, Detail or Hub with very
    // different coordinate frames. The trajectory producer never owns camera
    // state; it only supplies physical samples.
    glm::dvec2 screenPx {0.0};
    bool screenProjected = false;
};

struct MapObjectTrajectory
{
    std::string objectId;
    MapTrajectoryKind kind = MapTrajectoryKind::History;
    bool noSafePrimarySolution = false;
    std::vector<MapTrajectoryPoint> points;
};

struct MapObjectInfoField
{
    // Localization key where available; a future producer may also provide
    // an already-localized/custom label without changing panel ownership.
    std::string labelKey;
    std::string value;
    std::string unit;
    glm::vec4 valueColor {0.0f};
    bool hasValueColor = false;
};

struct MapObjectPanelAction
{
    std::string key;
    std::string labelKey;
    bool visible = true;
    bool active = false;
    bool enabled = true;
};

struct MapObjectOverlayItem
{
    std::string objectId;
    ShipInstanceId shipInstanceId = 0;
    std::string name;
    std::string typeName;
    std::string owner;
    std::vector<MapObjectInfoField> extraFields;
    std::vector<MapObjectPanelAction> panelActions;

    MapObjectGlyphKind kind = MapObjectGlyphKind::Ship;
    MapObjectInfoKind infoKind = MapObjectInfoKind::Tactical;
    // Semantic target behind a non-tactical card, for example the System body
    // id represented by a client-only celestial information panel.
    std::string semanticTargetId;
    std::string semanticAnchorId;
    int trackingSystemId = -1;
    // Local-neighborhood semantic binding for tactical targets.  A ship or
    // infrastructure object may live inside a Hub even though the clicked
    // object itself is not that Hub.  System/Details navigation can therefore
    // keep the object active while enabling the HUB drill to its parent.
    std::string navigationHubId;
    std::string navigationHubParentBodyId;
    glm::dvec3 navigationSystemPositionAu {0.0};
    bool hasNavigationSystemPositionAu = false;
    world::coordinates::WorldPosition trackingWorldPosition;
    bool hasTrackingWorldPosition = false;
    // Route markers use route order rather than the unrelated tactical track
    // allocator. Zero means no route number is rendered.
    int routeDisplayIndex = 0;
    bool pointerInteractive = true;
    // Screen-space UI affordances (currently the selected-empty-cube info
    // triangle) must win their own small hit area before semantic world-object
    // size arbitration. They are explicit controls, not physical targets.
    bool screenAffordance = false;
    std::string actionKey;

    // Card speed semantics and arrow-color semantics are separate because the
    // Hub reference object reports zero local speed while its broad arrow
    // still visualizes the hub's global motion.
    MapObjectVelocityMode velocityMode = MapObjectVelocityMode::Global;
    MapObjectVelocityMode arrowVelocityMode = MapObjectVelocityMode::Global;

    glm::dvec2 screenPx {0.0};
    glm::dvec2 facingScreenDirection {0.0, -1.0};
    glm::dvec2 velocityScreenDirection {0.0, -1.0};

    glm::dvec3 displayedVelocityMps {0.0};
    glm::dvec3 velocityArrowMps {0.0};
    glm::dvec3 stellarVelocityMps {0.0};

    glm::vec4 factionColor {0.80f, 0.86f, 0.92f, 0.96f};

    double physicalSizeMeters = 1.0;
    double glyphScale = 1.0;
    double hitRadiusPx = 14.0;
    // Optional precise screen-space footprint. When present, pointer picking
    // uses the projected physical footprint instead of the broad circular
    // glyph radius. This is used by Hub infrastructure whose visual mesh can
    // be long/thin and must not be selectable from empty surrounding space.
    std::vector<glm::dvec2> hitPolygonPx;
    // Semantic subtargets such as docking ports must win over their much
    // larger parent module when their screen-space hit areas overlap.
    int pickPriority = 0;

    bool visible = false;
    bool drawGlyph = true;
    bool wideVelocityArrow = false;
};

struct MapObjectOverlayFrame
{
    std::vector<MapObjectOverlayItem> items;
    std::vector<MapObjectTrajectory> trajectories;
};

struct MapObjectOverlayPointerResult
{
    bool consumed = false;
    // True only on the press edge inside the map. Hub interaction uses this
    // to distinguish an empty-space orbit gesture from a held mouse button.
    bool primaryPressStarted = false;
    // A card/glyph click activates the object independently from whether the
    // information card was just opened or closed. Navigation consumes this
    // as the canonical tactical selection signal.
    std::string activatedObjectId;
    MapObjectInfoKind activatedInfoKind = MapObjectInfoKind::Tactical;
    std::string activatedSemanticTargetId;
    std::string toggledObjectId;
    std::string closedObjectId;
    std::string actionObjectId;
    std::string actionKey;
};

struct MapObjectInfoPanelState
{
    std::string objectId;
    glm::dvec2 topLeftPx {0.0};
    glm::dvec2 dragOffsetPx {0.0};
    std::uint64_t zOrder = 0;
    double expandedHeightPx = 170.0;
    bool dragging = false;
    bool collapsed = false;
};

inline glm::dvec2 normalizedScreenDirection(
    const glm::dvec2& direction,
    const glm::dvec2& fallback = glm::dvec2(0.0, -1.0)
)
{
    const double len2 = glm::dot(direction, direction);
    if (len2 <= 1.0e-12)
        return fallback;
    return direction / std::sqrt(len2);
}

inline double mapObjectGlyphScale(
    double physicalSizeMeters,
    double pixelsPerMeter
)
{
    // Far objects stay readable in screen space. Once their real projected
    // size becomes relevant, the tactical symbol grows with the object, but
    // never turns into a screen-filling spearhead.
    const double physicalPx =
        std::max(0.0, physicalSizeMeters) *
        std::max(0.0, pixelsPerMeter);

    if (physicalPx <= 12.0)
        return 1.0;

    return std::clamp(
        1.0 + (physicalPx - 12.0) / 28.0,
        1.0,
        4.0
    );
}

inline double mapObjectVelocityArrowLengthScale(
    double speedMps,
    MapObjectVelocityMode mode
)
{
    if (!std::isfinite(speedMps) || speedMps <= 1.0e-9)
        return 0.0;

    // Navigation arrows deliberately use a linear scale.  A target moving
    // twice as fast should read as approximately twice the arrow length until
    // the protected screen-space maximum is reached.  Local and global motion
    // keep separate reference ranges because their physical regimes differ by
    // orders of magnitude.
    const double maxReferenceSpeedMps =
        mode == MapObjectVelocityMode::Local ? 250.0 : 100000.0;

    return std::clamp(speedMps / maxReferenceSpeedMps, 0.0, 1.0);
}

inline std::pair<double, double> stellarAzimuthElevationDeg(
    const glm::dvec3& velocityMps
)
{
    const double speed = glm::length(velocityMps);
    if (speed <= 1.0e-9)
        return {0.0, 0.0};

    const glm::dvec3 n = velocityMps / speed;
    const double azimuth =
        glm::degrees(std::atan2(n.x, n.z));
    const double elevation =
        glm::degrees(std::asin(std::clamp(n.y, -1.0, 1.0)));

    return {azimuth, elevation};
}

class MapObjectOverlayState
{
public:
    static constexpr double PanelWidthPx = 238.0;
    static constexpr double PanelHeightPx = 170.0; // legacy/default seed only
    static constexpr double PanelHeaderHeightPx = 26.0;
    static constexpr double PanelCollapsedHeightPx = 30.0;
    static constexpr double PanelMinimumExpandedHeightPx = 104.0;

    static double expandedPanelHeight(const MapObjectOverlayItem& item)
    {
        int fieldRows = 1; // type
        if (item.infoKind == MapObjectInfoKind::Tactical)
        {
            fieldRows += 3; // speed, azimuth, elevation
            if (!item.owner.empty())
                ++fieldRows;
        }

        const int extraRows = static_cast<int>(item.extraFields.size());
        fieldRows += extraRows;

        int visibleActions = 0;
        for (const auto& action : item.panelActions)
        {
            if (action.visible)
                ++visibleActions;
        }

        // Field labels may wrap (Russian "global speed" is the common case),
        // so reserve one extra wrapped line for tactical cards and a little
        // breathing room for custom fields. The result is content-driven, not
        // a fixed 170 px box.
        double height =
            43.0 +
            static_cast<double>(fieldRows) * 18.0 +
            (item.infoKind == MapObjectInfoKind::Tactical ? 12.0 : 0.0) +
            static_cast<double>(extraRows) * 4.0 +
            10.0;

        if (visibleActions > 0)
        {
            constexpr double actionHeight = 23.0;
            constexpr double actionGap = 5.0;
            height +=
                8.0 +
                static_cast<double>(visibleActions) * actionHeight +
                static_cast<double>(visibleActions - 1) * actionGap +
                8.0;
        }

        return std::max(PanelMinimumExpandedHeightPx, height);
    }

    int shipTargetNumberFor(const std::string& objectId)
    {
        if (objectId.empty())
            return -1;
        if (objectId == "player")
            return 0;

        const auto found = m_trackNumbers.find(objectId);
        if (found != m_trackNumbers.end())
            return found->second;

        const int assigned = m_nextTrackNumber++;
        m_trackNumbers.emplace(objectId, assigned);
        return assigned;
    }

    std::string shipTargetLabelFor(const std::string& objectId)
    {
        const int number = shipTargetNumberFor(objectId);
        return number < 0 ? "?" : std::to_string(number);
    }

    const std::string& activeObjectId() const noexcept
    {
        return m_activeObjectId;
    }

    bool isActive(const std::string& objectId) const
    {
        return !objectId.empty() && objectId == m_activeObjectId;
    }

    void activate(const std::string& objectId)
    {
        if (!objectId.empty())
            m_activeObjectId = objectId;
    }

    void clearActive()
    {
        m_activeObjectId.clear();
    }

    bool isOpen(const std::string& objectId) const
    {
        return m_panels.find(objectId) != m_panels.end();
    }

    std::vector<std::string> openObjectIds() const
    {
        std::vector<std::string> ids;
        ids.reserve(m_panels.size());
        for (const auto& [id, panel] : m_panels)
        {
            (void)panel;
            ids.push_back(id);
        }
        return ids;
    }

    void ensureOpen(
        const MapObjectOverlayItem& item,
        const glm::dvec2& viewportSizePx
    )
    {
        if (!isOpen(item.objectId))
            open(item, viewportSizePx);
    }

    void close(const std::string& objectId)
    {
        m_panels.erase(objectId);
        if (m_activeObjectId == objectId)
            m_activeObjectId.clear();
    }

    void toggle(
        const MapObjectOverlayItem& item,
        const glm::dvec2& viewportSizePx
    )
    {
        if (isOpen(item.objectId))
        {
            close(item.objectId);
            return;
        }

        open(item, viewportSizePx);
    }

    void open(
        const MapObjectOverlayItem& item,
        const glm::dvec2& viewportSizePx
    )
    {
        MapObjectInfoPanelState panel;
        panel.objectId = item.objectId;
        panel.zOrder = ++m_zCounter;
        panel.expandedHeightPx = expandedPanelHeight(item);
        panel.topLeftPx = glm::dvec2(
            item.screenPx.x + 36.0,
            item.screenPx.y - panel.expandedHeightPx * 0.5
        );
        clampPanel(panel, viewportSizePx);
        m_panels[panel.objectId] = std::move(panel);
    }

    MapObjectOverlayPointerResult handlePointer(
        const MapObjectOverlayFrame& frame,
        const glm::dvec2& viewportSizePx,
        const glm::dvec2& mousePx,
        bool inside,
        bool leftDown,
        double dominantExternalPhysicalSizeMeters = 0.0
    )
    {
        MapObjectOverlayPointerResult result;
        result.primaryPressStarted =
            inside && leftDown && !m_leftWasDown;

        // Panel actions/fields are presentation data and may change while a
        // card stays open (for example when a route FINISH already exists).
        // Refresh geometry from current content before hit-testing/rendering.
        for (auto& [id, panel] : m_panels)
        {
            const auto itemIt = std::find_if(
                frame.items.begin(),
                frame.items.end(),
                [&](const MapObjectOverlayItem& item)
                {
                    return item.objectId == id;
                }
            );
            if (itemIt == frame.items.end())
                continue;
            panel.expandedHeightPx = expandedPanelHeight(*itemIt);
            clampPanel(panel, viewportSizePx);
        }

        if (leftDown && !m_leftWasDown && inside)
        {
            if (auto* panel = topPanelAt(mousePx))
            {
                panel->zOrder = ++m_zCounter;
                m_pointerCaptured = true;
                result.consumed = true;

                const glm::dvec2 local = mousePx - panel->topLeftPx;
                const bool closeHit =
                    local.x >= PanelWidthPx - 25.0 &&
                    local.x <= PanelWidthPx - 5.0 &&
                    local.y >= 4.0 &&
                    local.y <= 24.0;
                const bool collapseHit =
                    local.x >= PanelWidthPx - 45.0 &&
                    local.x <= PanelWidthPx - 27.0 &&
                    local.y >= 4.0 &&
                    local.y <= 24.0;

                const auto panelItem = std::find_if(
                    frame.items.begin(),
                    frame.items.end(),
                    [&](const MapObjectOverlayItem& item)
                    {
                        return item.objectId == panel->objectId;
                    }
                );

                auto hitAction = [&]() -> const MapObjectPanelAction*
                {
                    if (panel->collapsed || panelItem == frame.items.end())
                        return nullptr;

                    constexpr double buttonHeight = 22.0;
                    constexpr double buttonGap = 5.0;
                    double buttonTop =
                        panelHeight(*panel) - 8.0 - buttonHeight;

                    for (auto it = panelItem->panelActions.rbegin();
                         it != panelItem->panelActions.rend();
                         ++it)
                    {
                        if (!it->visible)
                            continue;

                        const bool hit =
                            local.x >= 8.0 &&
                            local.x <= PanelWidthPx - 8.0 &&
                            local.y >= buttonTop &&
                            local.y <= buttonTop + buttonHeight;
                        if (hit)
                            return &(*it);

                        buttonTop -= buttonHeight + buttonGap;
                    }
                    return nullptr;
                };

                if (closeHit)
                {
                    const std::string id = panel->objectId;
                    close(id);
                    result.closedObjectId = id;
                }
                else if (collapseHit)
                {
                    panel->collapsed = !panel->collapsed;
                    clampPanel(*panel, viewportSizePx);
                }
                else if (const auto* action = hitAction())
                {
                    if (action->enabled)
                    {
                        panel->zOrder = ++m_zCounter;
                        result.actionObjectId = panel->objectId;
                        result.actionKey = action->key;
                    }
                }
                else
                {
                    if (panelItem != frame.items.end())
                    {
                        result.activatedInfoKind = panelItem->infoKind;
                        result.activatedSemanticTargetId =
                            panelItem->semanticTargetId;
                    }
                    activate(panel->objectId);
                    result.activatedObjectId = panel->objectId;

                    if (local.y >= 0.0 &&
                        local.y <= PanelHeaderHeightPx)
                    {
                        panel->dragging = true;
                        panel->dragOffsetPx = mousePx - panel->topLeftPx;
                    }
                }
            }
            else
            {
                const MapObjectOverlayItem* picked = nullptr;
                int bestPickPriority = std::numeric_limits<int>::min();
                double bestPhysicalSizeMeters = -1.0;
                double bestDistance = 1.0e30;

                // Explicit screen controls get first refusal inside their own
                // compact hit circle. This keeps the selected-cube info
                // triangle clickable even when a large projected body sits
                // behind it, without weakening physical-size arbitration for
                // normal map objects.
                for (const auto& item : frame.items)
                {
                    if (!item.visible || item.objectId.empty() ||
                        !item.pointerInteractive || !item.screenAffordance)
                    {
                        continue;
                    }

                    const double distance =
                        glm::length(mousePx - item.screenPx);
                    if (distance > item.hitRadiusPx)
                        continue;

                    if (!picked || distance < bestDistance)
                    {
                        picked = &item;
                        bestDistance = distance;
                    }
                }

                if (!picked)
                {
                    for (const auto& item : frame.items)
                    {
                        if (!item.visible || item.objectId.empty() ||
                            !item.pointerInteractive || item.screenAffordance)
                        {
                            continue;
                        }

                        const double distance =
                            glm::length(mousePx - item.screenPx);
                        const bool preciseHit = !item.hitPolygonPx.empty();
                        if (preciseHit)
                        {
                            if (!screenPointInsideConvexPolygon(
                                    mousePx,
                                    item.hitPolygonPx))
                            {
                                continue;
                            }
                        }
                        else if (distance > item.hitRadiusPx)
                        {
                            continue;
                        }

                        const double physicalSizeMeters =
                            std::max(0.0, item.physicalSizeMeters);
                        const bool higherPriority =
                            item.pickPriority > bestPickPriority;
                        const bool samePriority =
                            item.pickPriority == bestPickPriority;
                        const bool larger =
                            samePriority &&
                            physicalSizeMeters >
                                bestPhysicalSizeMeters + 1.0e-6;
                        const bool sameSize =
                            samePriority &&
                            std::abs(
                                physicalSizeMeters -
                                bestPhysicalSizeMeters
                            ) <= 1.0e-6;
                        const bool nearer =
                            sameSize && distance < bestDistance - 1.0e-6;
                        const bool deterministicTie =
                            sameSize &&
                            std::abs(distance - bestDistance) <= 1.0e-6 &&
                            picked &&
                            item.objectId < picked->objectId;

                        if (!picked || higherPriority || larger || nearer || deterministicTie)
                        {
                            picked = &item;
                            bestPickPriority = item.pickPriority;
                            bestPhysicalSizeMeters = physicalSizeMeters;
                            bestDistance = distance;
                        }
                    }
                }

                const bool pickedScreenAffordance =
                    picked && picked->screenAffordance;
                if (picked &&
                    (pickedScreenAffordance ||
                     bestPhysicalSizeMeters + 1.0e-6 >=
                        std::max(
                            0.0,
                            dominantExternalPhysicalSizeMeters
                        )))
                {
                    activate(picked->objectId);
                    result.activatedObjectId = picked->objectId;
                    result.activatedInfoKind = picked->infoKind;
                    result.activatedSemanticTargetId =
                        picked->semanticTargetId;
                    toggle(*picked, viewportSizePx);
                    result.toggledObjectId = picked->objectId;
                    m_pointerCaptured = true;
                    result.consumed = true;
                }
            }
        }

        if (leftDown)
        {
            if (m_pointerCaptured)
                result.consumed = true;

            for (auto& [id, panel] : m_panels)
            {
                (void)id;
                if (!panel.dragging)
                    continue;

                panel.topLeftPx = mousePx - panel.dragOffsetPx;
                clampPanel(panel, viewportSizePx);
                result.consumed = true;
            }
        }
        else
        {
            for (auto& [id, panel] : m_panels)
            {
                (void)id;
                panel.dragging = false;
            }
            m_pointerCaptured = false;
        }

        m_leftWasDown = leftDown;
        return result;
    }

    std::vector<MapObjectInfoPanelState> orderedPanels() const
    {
        std::vector<MapObjectInfoPanelState> result;
        result.reserve(m_panels.size());
        for (const auto& [id, panel] : m_panels)
        {
            (void)id;
            result.push_back(panel);
        }
        std::sort(
            result.begin(),
            result.end(),
            [](const auto& a, const auto& b)
            {
                return a.zOrder < b.zOrder;
            }
        );
        return result;
    }

    void clearTransientDrag()
    {
        m_leftWasDown = false;
        m_pointerCaptured = false;
        for (auto& [id, panel] : m_panels)
        {
            (void)id;
            panel.dragging = false;
        }
    }

private:
    static double panelHeight(const MapObjectInfoPanelState& panel)
    {
        return panel.collapsed
            ? PanelCollapsedHeightPx
            : panel.expandedHeightPx;
    }

    void clampPanel(
        MapObjectInfoPanelState& panel,
        const glm::dvec2& viewportSizePx
    ) const
    {
        panel.topLeftPx.x = std::clamp(
            panel.topLeftPx.x,
            4.0,
            std::max(4.0, viewportSizePx.x - PanelWidthPx - 4.0)
        );
        panel.topLeftPx.y = std::clamp(
            panel.topLeftPx.y,
            4.0,
            std::max(4.0, viewportSizePx.y - panelHeight(panel) - 4.0)
        );
    }

    MapObjectInfoPanelState* topPanelAt(const glm::dvec2& mousePx)
    {
        MapObjectInfoPanelState* best = nullptr;
        for (auto& [id, panel] : m_panels)
        {
            (void)id;
            const bool hit =
                mousePx.x >= panel.topLeftPx.x &&
                mousePx.x <= panel.topLeftPx.x + PanelWidthPx &&
                mousePx.y >= panel.topLeftPx.y &&
                mousePx.y <= panel.topLeftPx.y + panelHeight(panel);
            if (!hit)
                continue;
            if (!best || panel.zOrder > best->zOrder)
                best = &panel;
        }
        return best;
    }

private:
    std::unordered_map<std::string, int> m_trackNumbers;
    std::unordered_map<std::string, MapObjectInfoPanelState> m_panels;
    std::string m_activeObjectId;
    int m_nextTrackNumber = 1;
    std::uint64_t m_zCounter = 0;
    bool m_leftWasDown = false;
    bool m_pointerCaptured = false;
};

} // namespace game::system_map
