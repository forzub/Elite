#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

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
    Infrastructure
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
};

struct MapObjectTrajectory
{
    std::string objectId;
    MapTrajectoryKind kind = MapTrajectoryKind::History;
    std::vector<MapTrajectoryPoint> points;
};

struct MapObjectInfoField
{
    // Localization key where available; a future producer may also provide
    // an already-localized/custom label without changing panel ownership.
    std::string labelKey;
    std::string value;
    std::string unit;
};

struct MapObjectOverlayItem
{
    std::string objectId;
    std::string name;
    std::string typeName;
    std::string owner;
    std::vector<MapObjectInfoField> extraFields;

    MapObjectGlyphKind kind = MapObjectGlyphKind::Ship;
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
    std::string toggledObjectId;
    std::string closedObjectId;
};

struct MapObjectInfoPanelState
{
    std::string objectId;
    glm::dvec2 topLeftPx {0.0};
    glm::dvec2 dragOffsetPx {0.0};
    std::uint64_t zOrder = 0;
    bool dragging = false;
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
    static constexpr double PanelHeightPx = 170.0;
    static constexpr double PanelHeaderHeightPx = 26.0;

    std::string trackLabelFor(const std::string& objectId)
    {
        if (objectId.empty())
            return "?";

        const auto found = m_trackNumbers.find(objectId);
        if (found != m_trackNumbers.end())
            return std::to_string(found->second);

        const int assigned = m_nextTrackNumber++;
        m_trackNumbers.emplace(objectId, assigned);
        return std::to_string(assigned);
    }

    bool isOpen(const std::string& objectId) const
    {
        return m_panels.find(objectId) != m_panels.end();
    }

    void close(const std::string& objectId)
    {
        m_panels.erase(objectId);
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

        MapObjectInfoPanelState panel;
        panel.objectId = item.objectId;
        panel.zOrder = ++m_zCounter;
        panel.topLeftPx = glm::dvec2(
            item.screenPx.x + 36.0,
            item.screenPx.y - PanelHeightPx * 0.5
        );
        clampPanel(panel, viewportSizePx);
        m_panels.emplace(panel.objectId, std::move(panel));
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

                if (closeHit)
                {
                    const std::string id = panel->objectId;
                    close(id);
                    result.closedObjectId = id;
                }
                else if (local.y >= 0.0 &&
                         local.y <= PanelHeaderHeightPx)
                {
                    panel->dragging = true;
                    panel->dragOffsetPx = mousePx - panel->topLeftPx;
                }
            }
            else
            {
                const MapObjectOverlayItem* picked = nullptr;
                double bestPhysicalSizeMeters = -1.0;
                double bestDistance = 1.0e30;

                for (const auto& item : frame.items)
                {
                    if (!item.visible || item.objectId.empty())
                        continue;

                    const double distance =
                        glm::length(mousePx - item.screenPx);
                    if (distance > item.hitRadiusPx)
                        continue;

                    const double physicalSizeMeters =
                        std::max(0.0, item.physicalSizeMeters);
                    const bool larger =
                        physicalSizeMeters >
                        bestPhysicalSizeMeters + 1.0e-6;
                    const bool sameSize =
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

                    if (!picked || larger || nearer || deterministicTie)
                    {
                        picked = &item;
                        bestPhysicalSizeMeters = physicalSizeMeters;
                        bestDistance = distance;
                    }
                }

                if (picked &&
                    bestPhysicalSizeMeters + 1.0e-6 >=
                        std::max(
                            0.0,
                            dominantExternalPhysicalSizeMeters
                        ))
                {
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
            std::max(4.0, viewportSizePx.y - PanelHeightPx - 4.0)
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
                mousePx.y <= panel.topLeftPx.y + PanelHeightPx;
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
    int m_nextTrackNumber = 1;
    std::uint64_t m_zCounter = 0;
    bool m_leftWasDown = false;
    bool m_pointerCaptured = false;
};

} // namespace game::system_map
