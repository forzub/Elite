#pragma once

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>

#include <glm/glm.hpp>

#include "src/game/system_map/GalaxyMapView.h"
#include "src/render/types/Viewport.h"
#include "src/world/celestial/SystemMapTypes.h"

namespace game::system_map
{
/*
    DEV only (ELITE_DEV_GALAXY_POINTER_SCRIPT=1): repeated pointer sequences
    against the REAL Galaxy Map. The script only substitutes the cursor
    position and button state that SystemMapRenderer::handleInput reads from
    GLFW; everything after that (overlay, the one pointer resolver, the
    gesture, click dispatch, rendering) is the production path. Targets are
    taken from what the last frame presented. Results: stderr
    [GalaxyPointerScript] lines and D:/tmp/elite_galaxy_pointer_script.txt.
    Without the env var: enabled() is false and nothing else runs.
*/
class GalaxyPointerDevScript
{
public:
    static bool enabled()
    {
        static const bool on = [] {
            const char* v = std::getenv("ELITE_DEV_GALAXY_POINTER_SCRIPT");
            return v && *v && *v != '0';
        }();
        return on;
    }

    // Replaces the GLFW pointer of this input frame (window coordinates).
    void drive(
        GalaxyMapView& view,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const Viewport& vp,
        double now,
        double& mx,
        double& my,
        bool& leftDown,
        bool& rightDown)
    {
        m_view = &view;
        m_galaxy = &galaxy;
        m_vp = vp;
        m_now = now;
        if (m_startSeconds < 0.0)
            m_startSeconds = now;
        if (!m_finished && m_ops.empty() && now - m_startSeconds > 4.0 && !view.state().presentedStars.empty())
            planNextPhase();
        while (!m_ops.empty() && m_ops.front().kind != Op::Frame)
        {
            auto op = std::move(m_ops.front());
            m_ops.pop_front();
            if (op.kind == Op::Wait)
            {
                if (now < op.untilSeconds)
                {
                    m_ops.push_front(std::move(op));
                    break;
                }
            }
            else if (op.kind == Op::StartWait)
            {
                Op wait;
                wait.kind = Op::Wait;
                wait.untilSeconds = now + op.seconds;
                m_ops.push_front(std::move(wait));
            }
            else if (op.kind == Op::WaitCalm)
            {
                if (view.state().cameraFlight.active())
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
        if (!m_ops.empty() && m_ops.front().kind == Op::Frame)
        {
            auto op = std::move(m_ops.front());
            m_ops.pop_front();
            if (op.point)
                m_pointer = op.point();
            m_pointer += op.offset;
            m_left = op.left;
        }
        mx = vp.x + m_pointer.x;
        my = vp.y + m_pointer.y;
        leftDown = m_left;
        rightDown = false;
    }

private:
    struct Op
    {
        enum Kind { Frame, Wait, StartWait, WaitCalm, Call } kind = Frame;
        std::function<glm::vec2()> point; // Frame: absolute target (else keep)
        glm::vec2 offset {0.0f};          // Frame: added to the pointer
        bool left = false;
        double seconds = 0.0;
        double untilSeconds = 0.0;
        std::function<void()> call;
    };

    GalaxyMapView* m_view = nullptr;
    const world::celestial::GalaxyMapSnapshot* m_galaxy = nullptr;
    Viewport m_vp;
    double m_now = 0.0;
    double m_startSeconds = -1.0;
    int m_phase = 0;
    bool m_finished = false;
    std::deque<Op> m_ops;
    glm::vec2 m_pointer {20.0f, 20.0f};
    bool m_left = false;
    std::map<std::string, std::pair<int, int>> m_results; // name -> (pass, total)
    std::vector<std::string> m_order;
    GalaxyCameraState m_pressCamera;
    bool m_cameraHeldStill = true;

    void frame(std::function<glm::vec2()> point, bool left, glm::vec2 offset = glm::vec2(0.0f))
    {
        Op op;
        op.kind = Op::Frame;
        op.point = std::move(point);
        op.left = left;
        op.offset = offset;
        m_ops.push_back(std::move(op));
    }
    void wait(double seconds)
    {
        Op op;
        op.kind = Op::StartWait;
        op.seconds = seconds;
        m_ops.push_back(std::move(op));
    }
    void calm()
    {
        Op op;
        op.kind = Op::WaitCalm;
        m_ops.push_back(std::move(op));
        wait(0.25);
    }
    void call(std::function<void()> fn)
    {
        Op op;
        op.kind = Op::Call;
        op.call = std::move(fn);
        m_ops.push_back(std::move(op));
    }
    void record(const std::string& name, bool pass, const std::string& detail = std::string())
    {
        if (!m_results.count(name))
            m_order.push_back(name);
        auto& r = m_results[name];
        r.second += 1;
        r.first += pass ? 1 : 0;
        std::cerr << "[GalaxyPointerScript] " << (pass ? "ok   " : "MISS ") << name << " " << detail << std::endl;
    }

    bool sameCamera(const GalaxyCameraState& a) const
    {
        const auto& b = m_view->state().camera;
        return a.yaw == b.yaw && a.pitch == b.pitch && a.distance == b.distance && a.target == b.target;
    }

    // One click: press at the target, optional jitter frames, release.
    void click(std::function<glm::vec2()> target, glm::vec2 jitter = glm::vec2(0.0f))
    {
        frame(target, false);
        frame(nullptr, false);
        call([this] {
            m_pressCamera = m_view->state().camera;
            m_cameraHeldStill = true;
        });
        frame(nullptr, true);
        if (jitter != glm::vec2(0.0f))
        {
            frame(nullptr, true, jitter * 0.5f);
            frame(nullptr, true, jitter * 0.5f);
        }
        frame(nullptr, true);
        call([this] { m_cameraHeldStill = sameCamera(m_pressCamera); });
        frame(nullptr, false);
        frame(nullptr, false);
    }

    // A click only when the target exists (decided when the op runs).
    void clickIf(std::shared_ptr<bool> ok, std::function<glm::vec2()> target, glm::vec2 jitter = glm::vec2(0.0f))
    {
        auto skip = std::make_shared<bool>(false);
        call([this, ok, skip] {
            *skip = !*ok;
            if (*skip)
                std::cerr << "[GalaxyPointerScript] no target - click skipped" << std::endl;
        });
        // Without a target the frames move off the map viewport's content.
        click([ok, target] { return *ok ? target() : glm::vec2(-50.0f, -50.0f); }, jitter);
    }

    const GalaxyPointerStar* presentedStar(int id) const
    {
        for (const auto& s : m_view->state().presentedStars)
            if (s.systemId == id && s.visible)
                return &s;
        return nullptr;
    }

    // Catalog names may be localised: by name first, else the entry system
    // ("Sol") and its nearest neighbour ("Alpha Centauri").
    int systemNamed(const char* needle) const
    {
        for (const auto& s : m_galaxy->systems)
            if (s.name.find(needle) != std::string::npos)
                return s.id;
        const int entry = m_view->state().entry.systemId;
        if (std::string(needle) == "Sol")
            return entry;
        const world::celestial::GalaxyMapSystem* home = nullptr;
        for (const auto& s : m_galaxy->systems)
            if (s.id == entry)
                home = &s;
        if (!home)
            return -1;
        int best = -1;
        double bestLy = 1.0e30;
        for (const auto& s : m_galaxy->systems)
        {
            const double d = glm::length(s.positionLy - home->positionLy);
            if (s.id != entry && d < bestLy)
            {
                bestLy = d;
                best = s.id;
            }
        }
        return best;
    }

    std::string systemName(int id) const
    {
        for (const auto& s : m_galaxy->systems)
            if (s.id == id)
                return s.name;
        return "?";
    }

    // id < 0: the visible star nearest the viewport centre (chosen when the
    // click runs, from the presented frame).
    void starClicks(const std::string& name, int requestedId, int reps)
    {
        for (int k = 0; k < reps; ++k)
        {
            calm();
            auto chosenId = std::make_shared<int>(requestedId);
            call([this, chosenId, requestedId] {
                if (requestedId >= 0)
                    return;
                float best = 1.0e9f;
                const glm::vec2 centre(m_vp.width * 0.5f, m_vp.height * 0.5f);
                for (const auto& s : m_view->state().presentedStars)
                    if (s.visible && glm::length(s.screen - centre) < best)
                    {
                        best = glm::length(s.screen - centre);
                        *chosenId = s.systemId;
                    }
            });
            click(
                [this, chosenId] {
                    const auto* s = presentedStar(*chosenId);
                    return s ? s->screen : glm::vec2(5.0f, 5.0f);
                },
                glm::vec2(static_cast<float>(1 + k % 4), static_cast<float>(k % 3) - 1.0f));
            call([this, name, chosenId] {
                const int id = *chosenId;
                const auto& press = m_view->state().pressTarget;
                record(name, m_view->state().selectedSystemId == id && m_cameraHeldStill,
                       "target=" + std::to_string(id) + " press=" + galaxyPointerTargetKindName(press.kind) + "/" +
                           galaxyPointerHitReasonName(press.reason) + ":" + press.overlayObjectId +
                           " selected=" + std::to_string(m_view->state().selectedSystemId) +
                           " camera_still=" + (m_cameraHeldStill ? "yes" : "no"));
            });
            wait(0.5);
        }
    }

    // A presented cube marker no star glyph covers (or the first one).
    bool freeMarker(GalaxyNavigationMarker& out) const
    {
        const auto& markers = m_view->state().presentedCubeMarkers;
        for (const auto& m : markers)
        {
            // Room for the glyph-area second click inside the map viewport.
            if (!m.visible || m.screen.x < 40.0f || m.screen.y < 40.0f ||
                m.screen.x > m_vp.width - 40.0f || m.screen.y > m_vp.height - 40.0f)
                continue;
            bool covered = false;
            for (const auto& s : m_view->state().presentedStars)
                covered = covered || (s.visible && glm::length(s.screen - m.screen) < s.glyphRadiusPx + 8.0f);
            if (!covered)
            {
                out = m;
                return true;
            }
        }
        std::cerr << "[GalaxyPointerScript] no free marker among " << markers.size() << ":";
        for (const auto& m : markers)
            std::cerr << " (" << m.screen.x << "," << m.screen.y << " vis=" << m.visible << ")";
        std::cerr << " viewport=" << m_vp.width << "x" << m_vp.height
                  << " hovered=" << m_view->state().navigationGrid.hasHoveredCell() << std::endl;
        return false;
    }

    // ELITE_DEV_GALAXY_POINTER_PHASES="0235": run only these phases.
    static bool phaseSelected(int phase)
    {
        static const std::string phases = [] {
            const char* v = std::getenv("ELITE_DEV_GALAXY_POINTER_PHASES");
            return std::string(v ? v : "");
        }();
        return phases.empty() || phase > 9 || phases.find(static_cast<char>('0' + phase)) != std::string::npos;
    }

    void planNextPhase()
    {
        auto& state = m_view->state();
        while (m_phase <= 5 && !phaseSelected(m_phase))
            ++m_phase;
        switch (m_phase++)
        {
            case 0:
            {
                const int sol = systemNamed("Sol");
                const int alphaId = systemNamed("Alpha Centauri");
                std::cerr << "[GalaxyPointerScript] Sol=" << sol << " (" << systemName(sol) << ") AlphaCen="
                          << alphaId << " (" << systemName(alphaId) << ")" << std::endl;
                if (sol >= 0)
                    starClicks("1 Sol", sol, 10);
                const int alpha = alphaId;
                if (alpha >= 0)
                    starClicks("2 Alpha Centauri", alpha, 10);
                return;
            }
            case 1:
            {
                // Closest visible pair of stars on screen (>= 5 px apart).
                int a = -1, b = -1;
                float best = 1.0e9f;
                const auto& stars = state.presentedStars;
                for (std::size_t i = 0; i < stars.size(); ++i)
                    for (std::size_t j = i + 1; j < stars.size(); ++j)
                    {
                        if (!stars[i].visible || !stars[j].visible)
                            continue;
                        const float d = glm::length(stars[i].screen - stars[j].screen);
                        if (d >= 5.0f && d < best)
                        {
                            best = d;
                            a = stars[i].systemId;
                            b = stars[j].systemId;
                        }
                    }
                if (a >= 0)
                {
                    std::cerr << "[GalaxyPointerScript] close pair " << a << "/" << b << " " << best << " px"
                              << std::endl;
                    for (int k = 0; k < 5; ++k)
                    {
                        starClicks("3 close stars", a, 1);
                        starClicks("3 close stars", b, 1);
                    }
                }
                return;
            }
            case 2:
            {
                // Cube single clicks, star after cube, cube after star.
                for (int k = 0; k < 10; ++k)
                {
                    calm();
                    auto chosen = std::make_shared<GalaxyNavigationMarker>();
                    auto ok = std::make_shared<bool>(false);
                    call([this, chosen, ok] { *ok = freeMarker(*chosen); });
                    clickIf(ok, [chosen] { return chosen->screen; }, glm::vec2(2.0f, -1.0f));
                    call([this, chosen, ok] {
                        const auto& grid = m_view->state().navigationGrid;
                        record("5 cube single click",
                               *ok && grid.hasSelectedCell() && grid.selectedCell().index == chosen->cell.index &&
                                   m_cameraHeldStill,
                               *ok ? "" : "(no free marker)");
                    });
                    wait(0.5);
                    starClicks("6 star after cube", -1, 1);
                    calm();
                    auto keepsStar = std::make_shared<bool>(false);
                    call([this, chosen, ok, keepsStar] {
                        *ok = freeMarker(*chosen);
                        // Navigation semantics: a click on the cube that
                        // contains the current focus refines it (star kept).
                        const auto& st = m_view->state();
                        *keepsStar = st.navigationFocusValid &&
                                     st.navigationGrid.nearestIndexForPositionLy(st.navigationFocusLy,
                                                                                 chosen->cell.level) ==
                                         chosen->cell.index;
                    });
                    clickIf(ok, [chosen] { return chosen->screen; });
                    call([this, chosen, ok, keepsStar] {
                        const auto& grid = m_view->state().navigationGrid;
                        record("7 cube after star",
                               *ok && (m_view->state().selectedSystemId >= 0) == *keepsStar &&
                                   grid.hasSelectedCell() && grid.selectedCell().index == chosen->cell.index,
                               std::string(*keepsStar ? "(cube holds the star: kept)" : "(other cube)"));
                    });
                    wait(0.5);
                }
                return;
            }
            case 3:
            {
                // Cube double click: marker, then the glyph area (+10 px) of
                // the same cube; refine must follow every time.
                for (int k = 0; k < 10; ++k)
                {
                    calm();
                    // DEV setup (not interaction): entry view, camera moved
                    // back so the neighbour cube's marker is inside the map.
                    call([this] {
                        m_view->resetNavigationToEntry();
                        m_view->state().camera.distance *= 1.6f;
                    });
                    wait(0.4);
                    auto chosen = std::make_shared<GalaxyNavigationMarker>();
                    auto level = std::make_shared<int>(0);
                    auto ok = std::make_shared<bool>(false);
                    // The entry cube is centred on Sol (its marker is under
                    // the star): hover the neighbour cube so its marker shows.
                    frame([this] {
                        const float d = m_view->navigationAnchorDiameterPx(m_vp);
                        return glm::vec2(m_vp.width * 0.5f + 0.9f * d, m_vp.height * 0.5f);
                    }, false);
                    wait(0.5);
                    call([this, chosen, level, ok] {
                        *ok = freeMarker(*chosen);
                        *level = m_view->state().navigationGrid.level();
                    });
                    clickIf(ok, [chosen] { return chosen->screen; });
                    const glm::vec2 second = k % 2 == 0 ? glm::vec2(1.0f, 0.0f) : glm::vec2(10.0f, 0.0f);
                    clickIf(ok, [chosen, second] { return chosen->screen + second; });
                    call([this, level, ok, k] {
                        record(k % 2 == 0 ? "8 cube double click (marker)" : "8 cube double click (glyph area)",
                               *ok && m_view->state().navigationGrid.level() > *level,
                               "level " + std::to_string(*level) + "->" +
                                   std::to_string(m_view->state().navigationGrid.level()));
                    });
                    calm();
                    call([this] { m_view->resetNavigationToEntry(); });
                    wait(0.6);
                }
                return;
            }
            case 4:
            {
                // Cloud: the largest proxy whose projected centre resolves
                // to it (re-resolved per click from the presented frame).
                if (!m_galaxy->mediumAtlas)
                    return;
                for (int k = 0; k < 10; ++k)
                {
                    calm();
                    call([this] { m_view->resetNavigationToEntry(); });
                    wait(0.4);
                    auto object = std::make_shared<int>(-1);
                    auto point = std::make_shared<glm::vec2>(5.0f, 5.0f);
                    auto found = std::make_shared<bool>(false);
                    call([this, object, point, found] {
                        findCloudPoint(*object, *point);
                        *found = *object >= 0;
                    });
                    clickIf(found, [point] { return *point; }, glm::vec2(1.0f, 1.0f));
                    call([this, object] {
                        record("9 cloud single click",
                               *object >= 0 && m_view->state().selectedMediumObject == *object,
                               "object=" + std::to_string(*object) + " selected=" +
                                   std::to_string(m_view->state().selectedMediumObject));
                    });
                    wait(0.6);
                    const int sol = systemNamed("Sol");
                    if (sol >= 0)
                        starClicks("10 star after cloud", sol, 1);
                    call([this] {
                        record("10 star click drops cloud", m_view->state().selectedMediumObject < 0);
                    });
                }
                return;
            }
            case 5:
            {
                // Drag: rotation, never a selection.
                for (int k = 0; k < 5; ++k)
                {
                    calm();
                    auto before = std::make_shared<GalaxyCameraState>();
                    auto selected = std::make_shared<int>(0);
                    call([this, before, selected] {
                        *before = m_view->state().camera;
                        *selected = m_view->state().selectedSystemId;
                    });
                    frame([this] { return glm::vec2(m_vp.width * 0.5f, m_vp.height * 0.3f); }, false);
                    frame(nullptr, true);
                    for (int s = 0; s < 8; ++s)
                        frame(nullptr, true, glm::vec2(6.0f, 0.0f));
                    frame(nullptr, false);
                    call([this, before, selected] {
                        record("11 drag rotates, never selects",
                               !sameCamera(*before) && m_view->state().selectedSystemId == *selected);
                    });
                    wait(0.4);
                }
                return;
            }
            default:
                finish();
        }
    }

    void findCloudPoint(int& object, glm::vec2& point) const
    {
        object = -1;
        float bestRadius = 0.0f;
        const auto& atlas = *m_galaxy->mediumAtlas;
        const glm::mat4 mvp = m_view->projectionMatrix(m_vp) * m_view->viewMatrix();
        GalaxyPointerScene scene;
        scene.stars = &m_view->state().presentedStars;
        scene.cubeMarkers = &m_view->state().presentedCubeMarkers;
        scene.galaxy = m_galaxy;
        scene.mvp = mvp;
        scene.viewport = m_vp;
        scene.renderUnitsPerLy = GalaxyMapView::RenderUnitsPerLightYear;
        for (const auto& proxy : atlas.proxies)
        {
            const glm::vec4 clip = mvp * glm::vec4(glm::vec3(proxy.centreLy) * scene.renderUnitsPerLy, 1.0f);
            if (clip.w <= 0.0f)
                continue;
            const glm::vec3 ndc = glm::vec3(clip) / clip.w;
            if (std::abs(ndc.x) > 0.8f || std::abs(ndc.y) > 0.8f)
                continue;
            const glm::vec2 screen((ndc.x * 0.5f + 0.5f) * m_vp.width, (1.0f - (ndc.y * 0.5f + 0.5f)) * m_vp.height);
            const auto hit = resolveGalaxyPointer(scene, screen).winner;
            if (hit.kind == GalaxyPointerTargetKind::Medium && hit.mediumObject == proxy.objectIndex &&
                proxy.radiusLy / std::max(clip.w, 1.0e-3f) > bestRadius)
            {
                bestRadius = proxy.radiusLy / std::max(clip.w, 1.0e-3f);
                object = proxy.objectIndex;
                point = screen;
            }
        }
    }

    void finish()
    {
        m_finished = true;
        std::FILE* f = std::fopen("D:/tmp/elite_galaxy_pointer_script.txt", "w");
        int failures = 0;
        for (const auto& name : m_order)
        {
            const auto& r = m_results[name];
            failures += r.second - r.first;
            char line[256];
            std::snprintf(line, sizeof line, "%s %s %d/%d", r.first == r.second ? "PASS" : "FAIL", name.c_str(),
                          r.first, r.second);
            std::cerr << "[GalaxyPointerScript] " << line << std::endl;
            if (f)
                std::fprintf(f, "%s\n", line);
        }
        std::cerr << "[GalaxyPointerScript] DONE " << (failures ? "FAIL" : "PASS") << std::endl;
        if (f)
        {
            std::fprintf(f, "DONE %s\n", failures ? "FAIL" : "PASS");
            std::fclose(f);
        }
    }
};
} // namespace game::system_map
