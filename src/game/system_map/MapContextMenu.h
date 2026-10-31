#pragma once

#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/CubicNavigationGrid.h"
#include "src/game/system_map/MapMode.h"

namespace game::system_map
{
    /*
        Context menu of a map object (right click or Ctrl + click, every
        map): the object's name and the commands available for it - info
        card, route commands, details (the next map of the object),
        surroundings (a navigation cube: enter it). A menu with one command
        is still a menu. It owns the pointer while open; any press outside
        closes it.
    */
    struct MapContextMenu
    {
        enum class TargetKind
        {
            None,
            OverlayItem, // a card / glyph object of the overlay
            Star,        // Galaxy star system
            Medium,      // Galaxy cloud / icy field
            Body,        // System body
            Hub,         // System hub
            Cube         // navigation cube (Galaxy or System)
        };

        struct Row
        {
            std::string label;
            // "info", "details", "surroundings" or "action:<panel key>".
            std::string command;
            bool enabled = true;
        };

        bool open = false;
        MapMode mode = MapMode::Galaxy;
        glm::vec2 originPx {0.0f};
        std::string title;
        std::vector<Row> rows;
        int hovered = -1;

        TargetKind targetKind = TargetKind::None;
        std::string objectId; // overlay item of the target (card / route)
        int systemId = -1;
        int mediumObject = -1;
        std::string bodyId;
        std::string hubId;
        std::string hubParentBodyId;
        std::optional<navigation::CubicNavigationCell> cell;
        glm::dvec3 pointLy {0.0}; // Galaxy point / System point (AU)

        static constexpr float WidthPx = 270.0f;
        static constexpr float HeaderHeightPx = 28.0f;
        static constexpr float RowHeightPx = 25.0f;

        float heightPx() const
        {
            return HeaderHeightPx + 6.0f + RowHeightPx * static_cast<float>(rows.size());
        }

        // (left, top, right, bottom) in map viewport pixels.
        glm::vec4 bounds() const
        {
            return glm::vec4(originPx.x, originPx.y, originPx.x + WidthPx, originPx.y + heightPx());
        }

        int rowAt(double x, double y) const
        {
            const glm::vec4 b = bounds();
            if (x < b.x || x > b.z)
                return -1;
            const double top = b.y + HeaderHeightPx + 3.0;
            for (int i = 0; i < static_cast<int>(rows.size()); ++i)
            {
                const double rowTop = top + RowHeightPx * static_cast<double>(i);
                if (y >= rowTop && y < rowTop + RowHeightPx)
                    return i;
            }
            return -1;
        }

        bool contains(double x, double y) const
        {
            const glm::vec4 b = bounds();
            return x >= b.x && x <= b.z && y >= b.y && y <= b.w;
        }

        // Keep the menu on the screen (opened at the pointer).
        void placeAt(const glm::vec2& pointer, int viewportWidth, int viewportHeight)
        {
            originPx = pointer + glm::vec2(10.0f, 4.0f);
            originPx.x = std::min(originPx.x, static_cast<float>(viewportWidth) - WidthPx - 8.0f);
            originPx.y = std::min(originPx.y, static_cast<float>(viewportHeight) - heightPx() - 8.0f);
            originPx = glm::max(originPx, glm::vec2(8.0f));
        }

        void close()
        {
            *this = MapContextMenu {};
        }
    };
}
