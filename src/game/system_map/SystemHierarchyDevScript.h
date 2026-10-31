#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "src/core/DevFrameCapture.h"
#include "src/game/system_map/MapContextMenu.h"
#include "src/game/system_map/SystemMapView.h"
#include "src/render/navigation/NavigationCoordinateOverlay.h"
#include "src/render/types/Viewport.h"

#include <GLFW/glfw3.h>

namespace game::system_map
{
/*
    DEV only (ELITE_DEV_SYSTEM_HIERARCHY_SCRIPT=1, with ELITE_DEV_SYSTEM_MAP_AT):
    the System Map cube navigation through the PRODUCTION input path - the
    same model as the Galaxy Map. The script substitutes the cursor /
    buttons / wheel SystemMapRenderer::handleInput reads from GLFW; bodies,
    cards and buttons come from what the frame presented (locators set by
    SystemMapRenderer). Frames: D:/tmp/elite_sh_NN_name.png, result:
    D:/tmp/elite_system_hierarchy_script.txt. Inert without the env var.
*/
class SystemHierarchyDevScript
{
public:
    static bool enabled()
    {
        static const bool on = [] {
            const char* v = std::getenv("ELITE_DEV_SYSTEM_HIERARCHY_SCRIPT");
            return v && *v && *v != '0';
        }();
        return on;
    }

    // Visible bodies of the presented frame: id -> screen point (map px).
    std::function<std::vector<std::pair<std::string, glm::vec2>>()> bodyLocator;
    // Is the info card of this body open?
    std::function<bool(const std::string&)> cardOpen;
    // Current position (AU) of a body.
    std::function<std::optional<glm::dvec3>(const std::string&)> bodyPositionAu;
    // The open context menu (nullptr: closed), the map mode, the back
    // button centre (nullopt: no back button).
    std::function<const MapContextMenu*()> contextMenu;
    std::function<MapMode()> mapMode;
    std::function<std::optional<glm::vec2>()> backButton;

    void drive(SystemMapView& view, const Viewport& vp, double now, double& mx, double& my, bool& leftDown,
               bool& rightDown, double& scrollY)
    {
        m_view = &view;
        m_vp = vp;
        if (m_start < 0.0)
            m_start = now;
        if (!m_planned && now - m_start > 3.0)
        {
            m_planned = true;
            plan();
        }
        while (!m_ops.empty() && m_ops.front().kind != Op::Frame)
        {
            auto op = std::move(m_ops.front());
            m_ops.pop_front();
            if (op.kind == Op::Wait)
            {
                if (now < op.until)
                {
                    m_ops.push_front(std::move(op));
                    break;
                }
            }
            else if (op.kind == Op::StartWait)
            {
                Op w;
                w.kind = Op::Wait;
                w.until = now + op.seconds;
                m_ops.push_front(std::move(w));
            }
            else if (op.kind == Op::WaitCalm)
            {
                if (view.state().cameraFlight.active)
                {
                    m_ops.push_front(std::move(op));
                    break;
                }
            }
            else if (op.call)
            {
                op.call();
            }
        }
        double scroll = 0.0;
        if (!m_ops.empty() && m_ops.front().kind == Op::Frame)
        {
            auto op = std::move(m_ops.front());
            m_ops.pop_front();
            if (op.point)
                m_pointer = op.point();
            m_left = op.left;
            m_right = op.right;
            scroll = op.scroll;
            if (op.scrollWhile && !op.scrollWhile())
                scroll = 0.0;
        }
        mx = vp.x + m_pointer.x;
        my = vp.y + m_pointer.y;
        leftDown = m_left;
        rightDown = m_right;
        if (scroll != 0.0)
            scrollY += scroll;
    }

private:
    struct Op
    {
        enum Kind { Frame, Wait, StartWait, WaitCalm, Call } kind = Frame;
        std::function<glm::vec2()> point;
        bool left = false;
        bool right = false;
        double scroll = 0.0;
        std::function<bool()> scrollWhile;
        double seconds = 0.0;
        double until = 0.0;
        std::function<void()> call;
    };

    SystemMapView* m_view = nullptr;
    Viewport m_vp;
    double m_start = -1.0;
    bool m_planned = false;
    std::deque<Op> m_ops;
    glm::vec2 m_pointer {20.0f, 20.0f};
    bool m_left = false;
    bool m_right = false;
    glm::vec2 m_bodyAt {0.0f};
    SystemMapBookmark m_saved;
    int m_shot = 0;
    std::vector<std::pair<std::string, bool>> m_results;
    GalaxyHierarchyParent m_before;
    SystemCameraState m_camera;
    std::optional<navigation::CubicNavigationCell> m_cell;
    std::string m_body;

    void frame(std::function<glm::vec2()> point, bool left, bool right = false)
    {
        Op op;
        op.point = std::move(point);
        op.left = left;
        op.right = right;
        m_ops.push_back(std::move(op));
    }
    void rightClick(std::function<glm::vec2()> target)
    {
        auto p = std::make_shared<glm::vec2>(-50.0f);
        call([p, target] { *p = target(); });
        const auto at = [p] { return *p; };
        frame(at, false);
        frame(at, false, true);
        frame(at, false, true);
        frame(at, false);
        frame(at, false);
    }
    // A left drag from a point by a screen offset (a rotation).
    void drag(std::function<glm::vec2()> from, glm::vec2 by, int steps)
    {
        auto p = std::make_shared<glm::vec2>(-50.0f);
        call([p, from] { *p = from(); });
        frame([p] { return *p; }, false);
        frame([p] { return *p; }, true);
        for (int k = 1; k <= steps; ++k)
        {
            const float t = static_cast<float>(k) / static_cast<float>(steps);
            frame([p, by, t] { return *p + by * t; }, true);
        }
        frame([p, by] { return *p + by; }, false);
        frame([p, by] { return *p + by; }, false);
    }
    // Centre of the context-menu row with this command (off-screen if none).
    glm::vec2 menuRow(const std::string& command) const
    {
        const MapContextMenu* menu = contextMenu ? contextMenu() : nullptr;
        if (!menu)
            return glm::vec2(-50.0f);
        for (std::size_t i = 0; i < menu->rows.size(); ++i)
            if (menu->rows[i].command == command)
                return glm::vec2(menu->originPx.x + MapContextMenu::WidthPx * 0.5f,
                                 menu->originPx.y + MapContextMenu::HeaderHeightPx + 3.0f +
                                     MapContextMenu::RowHeightPx * (static_cast<float>(i) + 0.5f));
        return glm::vec2(-50.0f);
    }
    bool menuHas(const std::string& command) const
    {
        return menuRow(command).x >= 0.0f;
    }
    float bodyFromCentre(const std::string& id) const
    {
        return glm::length(bodyScreen(id) - glm::vec2(m_vp.width * 0.5f, m_vp.height * 0.5f));
    }
    void wait(double s)
    {
        Op op;
        op.kind = Op::StartWait;
        op.seconds = s;
        m_ops.push_back(std::move(op));
    }
    void calm()
    {
        Op op;
        op.kind = Op::WaitCalm;
        m_ops.push_back(std::move(op));
        wait(0.3);
    }
    void call(std::function<void()> fn)
    {
        Op op;
        op.kind = Op::Call;
        op.call = std::move(fn);
        m_ops.push_back(std::move(op));
    }
    void scrollWhile(double amount, int maxSteps, std::function<bool()> condition)
    {
        for (int k = 0; k < maxSteps; ++k)
        {
            Op op;
            op.scroll = amount;
            op.scrollWhile = condition;
            m_ops.push_back(op);
        }
    }
    void record(const std::string& name, bool pass, const std::string& detail = std::string())
    {
        m_results.push_back({name, pass});
        std::cerr << "[SystemHierarchyScript] " << (pass ? "PASS " : "FAIL ") << name << " " << detail << std::endl;
    }
    void shot(const std::string& name)
    {
        call([this, name] {
            char path[160];
            std::snprintf(path, sizeof path, "D:/tmp/elite_sh_%02d_%s.png", m_shot++, name.c_str());
            core::dev::frameCaptureRequests().push_back({glfwGetTime(), path, false});
        });
        wait(0.25);
    }
    void click(std::function<glm::vec2()> target)
    {
        auto p = std::make_shared<glm::vec2>(-50.0f);
        call([p, target] { *p = target(); });
        const auto at = [p] { return *p; };
        frame(at, false);
        frame(at, false);
        frame(at, true);
        frame(at, true);
        frame(at, false);
        frame(at, false);
    }
    void doubleClick(std::function<glm::vec2()> target)
    {
        auto p = std::make_shared<glm::vec2>(-50.0f);
        call([p, target] { *p = target(); });
        click([p] { return *p; });
        click([p] { return *p; });
    }
    void hoverAt(std::function<glm::vec2()> target)
    {
        auto p = std::make_shared<glm::vec2>(-50.0f);
        call([p, target] { *p = target(); });
        for (int f = 0; f < 4; ++f)
            frame([p] { return *p; }, false);
        wait(0.35);
    }

    std::string parentText() const
    {
        const auto& p = m_view->state().hierarchyParent;
        std::ostringstream s;
        s << "S" << p.level << "[" << p.index.x << "," << p.index.y << "," << p.index.z << "]";
        return s.str();
    }
    bool fills() const
    {
        const auto box = m_view->hierarchyBox();
        const auto& c = m_view->state().camera;
        const float fill = m_view->cubeScreenRatioAt(box.centerLy, box.edgeLy, c.target, c.distance, m_vp, true);
        return fill >= 0.95f && fill <= 1.0f;
    }
    void checkFills(const std::string& tag)
    {
        call([this, tag] {
            const auto box = m_view->hierarchyBox();
            const auto& c = m_view->state().camera;
            record(tag + ": the parent fills the screen", fills(),
                   std::to_string(m_view->cubeScreenRatioAt(box.centerLy, box.edgeLy, c.target, c.distance, m_vp, true)) +
                       " " + parentText());
        });
    }
    // A screen point on an empty-space child (no body there), ring search.
    glm::vec2 emptyChildPoint(int salt) const
    {
        const auto bodies = bodyLocator ? bodyLocator() : std::vector<std::pair<std::string, glm::vec2>>();
        for (int k = 0; k < 160; ++k)
        {
            const float angle = 0.61f * static_cast<float>(k + salt * 7);
            const float radius = (0.08f + 0.003f * static_cast<float>(k)) * static_cast<float>(m_vp.height);
            const glm::vec2 p(m_vp.width * 0.5f + radius * std::cos(angle), m_vp.height * 0.5f + radius * std::sin(angle));
            bool nearBody = false;
            for (const auto& [id, s] : bodies)
                nearBody = nearBody || glm::length(s - p) < 60.0f;
            if (!nearBody)
                return p;
        }
        return glm::vec2(m_vp.width * 0.5f, m_vp.height * 0.5f);
    }
    glm::vec2 bodyScreen(const std::string& id) const
    {
        if (bodyLocator)
            for (const auto& [b, s] : bodyLocator())
                if (b == id)
                    return s;
        return glm::vec2(-50.0f);
    }
    bool selectedCellHoldsBody() const
    {
        const auto& st = m_view->state();
        if (!bodyPositionAu || st.selectedBodyId.empty())
            return false;
        const auto position = bodyPositionAu(st.selectedBodyId);
        const auto child = position ? m_view->hierarchyChildAt(*position) : std::nullopt;
        return child && st.navigationGrid.hasSelectedCell() && st.navigationGrid.selectedCell().index == child->index &&
               st.navigationGrid.selectedCell().level == child->level;
    }

    void plan()
    {
        calm();
        shot("open");
        call([this] {
            record("open: the S0 root is the parent", m_view->state().hierarchyParent.level == 0, parentText());
        });
        checkFills("open");

        // Cube: hover (camera still), click selects, double click enters.
        for (int depth = 0; depth < 2; ++depth)
        {
            const std::string tag = "cube" + std::to_string(depth);
            call([this] { m_camera = m_view->state().camera; });
            hoverAt([this, depth] { return emptyChildPoint(depth * 5); });
            call([this, tag] {
                const auto& st = m_view->state();
                const bool still = st.camera.yaw == m_camera.yaw && st.camera.pitch == m_camera.pitch &&
                                   st.camera.distance == m_camera.distance && st.camera.target == m_camera.target;
                record(tag + " hover: a child highlighted, camera still",
                       st.navigationGrid.hasHoveredCell() &&
                           galaxyHierarchyIsChild(m_view->hierarchyBox(), st.navigationGrid.hoveredCell()) && still);
            });
            shot(tag + "_hover");
            call([this] { m_before = m_view->state().hierarchyParent; });
            click([this, depth] { return emptyChildPoint(depth * 5); });
            call([this, tag] {
                const auto& st = m_view->state();
                const bool ok = st.navigationGrid.hasSelectedCell() &&
                                galaxyHierarchyIsChild(m_view->hierarchyBox(), st.navigationGrid.selectedCell()) &&
                                st.hierarchyParent == m_before;
                if (ok)
                    m_cell = st.navigationGrid.selectedCell();
                record(tag + " click: a child selected, parent unchanged", ok);
            });
            wait(0.6);
            doubleClick([this, depth] { return emptyChildPoint(depth * 5); });
            calm();
            call([this, tag] {
                const auto& p = m_view->state().hierarchyParent;
                record(tag + " double click: the child becomes the parent",
                       m_cell && p.level == m_cell->level && p.index == m_cell->index, parentText());
            });
            checkFills(tag);
            shot(tag + "_entered");
        }

        // Up by the button (left of TRACK on the System Map), twice.
        for (int k = 0; k < 2; ++k)
        {
            call([this] { m_before = m_view->state().hierarchyParent; });
            click([this] {
                const auto b = render::navigation::NavigationCoordinateOverlay::upButtonBounds(m_vp, true);
                return glm::vec2((b.left + b.right) * 0.5f, (b.top + b.bottom) * 0.5f);
            });
            calm();
            call([this, k] {
                const auto& st = m_view->state();
                record("up " + std::to_string(k) + ": the parent of the parent, the cube left selected",
                       st.hierarchyParent.level == m_before.level - 1 && st.navigationGrid.hasSelectedCell() &&
                           st.navigationGrid.selectedCell().index == m_before.index,
                       parentText());
            });
            checkFills("up " + std::to_string(k));
        }
        shot("up");

        // Planet: single click selects it and its cube (no card); the
        // context menu (right click) opens the card; the selection follows
        // the planet; double click approaches it and the camera follows it.
        call([this] {
            m_body.clear();
            float best = 1.0e9f;
            const auto bodies = bodyLocator ? bodyLocator() : std::vector<std::pair<std::string, glm::vec2>>();
            for (const auto& [id, s] : bodies)
                {
                    // A body standing alone on screen (a moon over its planet
                    // is one point at this scale).
                    bool crowded = false;
                    for (const auto& [other, o] : bodies)
                        crowded = crowded || (other != id && glm::length(o - s) < 25.0f);
                    if (crowded)
                        continue;
                    const float d = glm::length(s - glm::vec2(m_vp.width * 0.5f, m_vp.height * 0.5f));
                    if (d < best && d > 30.0f && s.x > 80 && s.y > 80 && s.x < m_vp.width - 80 &&
                        s.y < m_vp.height - 80)
                    {
                        best = d;
                        m_body = id;
                    }
                }
            std::cerr << "[SystemHierarchyScript] body " << m_body << std::endl;
        });
        click([this] { return bodyScreen(m_body); });
        call([this] {
            const auto& st = m_view->state();
            record("planet click: selected with its cube, no card",
                   !m_body.empty() && st.selectedBodyId == m_body && selectedCellHoldsBody() &&
                       !(cardOpen && cardOpen(m_body)),
                   m_body);
        });
        shot("planet_selected");
        wait(0.6);
        rightClick([this] { return bodyScreen(m_body); });
        call([this] {
            const MapContextMenu* menu = contextMenu ? contextMenu() : nullptr;
            record("planet right click: a context menu with the name, info and details",
                   menu && !menu->title.empty() && menuHas("info") && menuHas("details"),
                   menu ? menu->title : std::string("closed"));
        });
        shot("planet_menu");
        click([this] { return menuRow("info"); });
        wait(0.3);
        call([this] {
            record("planet menu INFO: the info card opens, the menu closes",
                   cardOpen && cardOpen(m_body) && !(contextMenu && contextMenu()));
        });
        shot("planet_card");
        for (int k = 0; k < 3; ++k)
        {
            wait(1.5);
            call([this, k] {
                record("planet " + std::to_string(k) + ": the selected cube is the one the planet is in",
                       selectedCellHoldsBody());
            });
        }

        // Double click: approach (its cube is the parent, it is centred) and
        // the camera follows it.
        call([this] { m_before = m_view->state().hierarchyParent; });
        doubleClick([this] { return bodyScreen(m_body); });
        calm();
        call([this] {
            const auto& st = m_view->state();
            record("planet double click: approached - deeper parent, the planet centred, followed",
                   st.hierarchyParent.level == m_before.level + 1 && bodyFromCentre(m_body) < 40.0f &&
                       st.cameraFollowBodyId == m_body,
                   parentText() + " d=" + std::to_string(bodyFromCentre(m_body)));
        });
        shot("planet_approached");
        wait(3.0);
        call([this] {
            record("planet followed: still in the view centre 3 s later", bodyFromCentre(m_body) < 40.0f,
                   std::to_string(bodyFromCentre(m_body)));
        });

        // Rotation near the planet turns about the planet.
        call([this] {
            m_camera = m_view->state().camera;
            m_bodyAt = bodyScreen(m_body);
        });
        drag([this] { return bodyScreen(m_body) + glm::vec2(14.0f, 10.0f); }, glm::vec2(90.0f, 40.0f), 12);
        call([this] {
            const auto& st = m_view->state();
            const float moved = glm::length(bodyScreen(m_body) - m_bodyAt);
            record("rotation near the planet: the camera turned about it",
                   st.camera.yaw != m_camera.yaw && moved < 12.0f, "moved_px=" + std::to_string(moved));
        });

        // The wheel on the planet keeps it under the cursor.
        call([this] { m_bodyAt = bodyScreen(m_body); });
        frame([this] { return m_bodyAt; }, false);
        for (int k = 0; k < 4; ++k)
        {
            Op op;
            op.point = [this] { return m_bodyAt; };
            op.scroll = 1.0;
            m_ops.push_back(op);
            frame([this] { return m_bodyAt; }, false);
        }
        wait(0.3);
        call([this] {
            const float moved = glm::length(bodyScreen(m_body) - m_bodyAt);
            record("wheel on the planet: it stays under the cursor", moved < 12.0f,
                   "moved_px=" + std::to_string(moved));
        });

        // Details from the menu, then back: the System view comes back.
        call([this] { m_saved = m_view->bookmark(0); });
        rightClick([this] { return bodyScreen(m_body); });
        click([this] { return menuRow("details"); });
        wait(1.5);
        call([this] {
            record("planet menu DETAILS: the Details map opens",
                   mapMode && mapMode() == MapMode::Detail);
        });
        shot("details");
        call([this] {
            record("Details: a back arrow is shown", backButton && backButton().has_value());
        });
        click([this] { return backButton && backButton() ? *backButton() : glm::vec2(-50.0f); });
        wait(1.5);
        calm();
        call([this] {
            const auto& st = m_view->state();
            const bool sameParent = st.hierarchyParent == m_saved.parent;
            const bool sameCamera = std::abs(st.camera.yaw - m_saved.camera.yaw) < 1.0e-4f &&
                                    std::abs(st.camera.distance - m_saved.camera.distance) <
                                        1.0e-3f * std::max(1.0f, m_saved.camera.distance);
            record("back: the System map with the same parent and camera",
                   mapMode && mapMode() == MapMode::System && sameParent && sameCamera, parentText());
        });
        shot("back");

        // Wheel: in until a child fills the screen (one hand-over), out to S0.
        call([this] { m_before = m_view->state().hierarchyParent; });
        frame([this] { return emptyChildPoint(13); }, false);
        scrollWhile(1.0, 260, [this] { return m_view->state().hierarchyParent.level == m_before.level; });
        wait(0.3);
        call([this] {
            record("wheel in: a child filling the screen became the parent",
                   m_view->state().hierarchyParent.level == m_before.level + 1, parentText());
        });
        shot("wheel_in");
        scrollWhile(-1.0, 300, [this] { return m_view->state().hierarchyParent.level > 0; });
        // S0 is ~25x the screen right after the hand-over: keep zooming out
        // until it fills the screen (it never gets smaller).
        scrollWhile(-1.0, 200, [this] { return !fills(); });
        for (int k = 0; k < 20; ++k)
        {
            Op op;
            op.scroll = -1.0;
            m_ops.push_back(op);
        }
        wait(0.3);
        call([this] {
            record("wheel out: back to S0, which never gets smaller than the screen",
                   m_view->state().hierarchyParent.level == 0 && fills(), parentText());
        });
        shot("wheel_out");
        call([this] { finish(); });
    }

    void finish()
    {
        std::FILE* f = std::fopen("D:/tmp/elite_system_hierarchy_script.txt", "w");
        int failures = 0;
        for (const auto& r : m_results)
        {
            failures += r.second ? 0 : 1;
            if (f)
                std::fprintf(f, "%s %s\n", r.second ? "PASS" : "FAIL", r.first.c_str());
        }
        if (f)
        {
            std::fprintf(f, "DONE %s (%d checks, %d failed)\n", failures ? "FAIL" : "PASS",
                         static_cast<int>(m_results.size()), failures);
            std::fclose(f);
        }
        std::cerr << "[SystemHierarchyScript] DONE " << (failures ? "FAIL" : "PASS") << " checks=" << m_results.size()
                  << " failed=" << failures << std::endl;
    }
};
} // namespace game::system_map
