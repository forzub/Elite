#pragma once

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <memory>
#include <string>

#include <glm/glm.hpp>

#include "src/game/system_map/GalaxyMapInteraction.h"
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

    // A UI card / panel at a map point (SystemMapRenderer overlay state).
    std::function<bool(glm::vec2)> panelAt;

    // Replaces the GLFW pointer of this input frame (window coordinates).
    void drive(
        GalaxyMapView& view,
        const GalaxyMapInteraction& interaction,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const Viewport& vp,
        double now,
        double& mx,
        double& my,
        bool& leftDown,
        bool& rightDown)
    {
        m_view = &view;
        m_interaction = &interaction;
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
    const GalaxyMapInteraction* m_interaction = nullptr;
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

    // A screen point where the ONE resolver gives an empty-space child
    // cube of the current parent (hierarchy layer), on a ring around the
    // viewport centre, away from stars and glyphs.
    bool layerPoint(glm::vec2& out, int salt) const
    {
        std::optional<navigation::GalaxyNavigationCell> cell;
        return layerPoint(out, salt, cell);
    }
    bool layerPoint(glm::vec2& out, int salt, std::optional<navigation::GalaxyNavigationCell>& cell) const
    {
        if (!m_interaction)
            return false;
        m_probeFrame = GalaxyMapInputFrame {};
        m_probeFrame.viewport = m_vp;
        const auto scene = m_interaction->pointerScene(*m_view, *m_galaxy, m_probeFrame);
        for (int k = 0; k < 120; ++k)
        {
            const float angle = 0.61f * static_cast<float>(k + salt * 7);
            const float radius = (0.06f + 0.0035f * static_cast<float>(k)) * static_cast<float>(m_vp.height);
            const glm::vec2 p(m_vp.width * 0.5f + radius * std::cos(angle),
                              m_vp.height * 0.5f + radius * std::sin(angle));
            bool nearStar = false;
            for (const auto& s : m_view->state().presentedStars)
                nearStar = nearStar || (s.visible && glm::length(s.screen - p) < 40.0f);
            // A UI card there takes the press (overlay contract).
            if (nearStar || (panelAt && panelAt(p)))
                continue;
            const auto hit = resolveGalaxyPointer(scene, p).winner;
            if (hit.kind == GalaxyPointerTargetKind::NavigationCube &&
                hit.reason == GalaxyPointerHitReason::HierarchyLayer)
            {
                out = p;
                cell = hit.cell;
                return true;
            }
        }
        return false;
    }
    mutable GalaxyMapInputFrame m_probeFrame;

    // DEV setup: the entry star's G1 cube as the current parent (the old
    // entry view; in the galaxy overview / a root Sol, Alpha Centauri and
    // Proxima share one glyph - the "What is here?" list resolves that).
    void enterEntryRoot()
    {
        auto& grid = m_view->state().navigationGrid;
        m_view->resetNavigationToEntry();
        for (int level = 0; level <= 1; ++level)
        {
            m_view->enterHierarchyCell(
                grid.cell(grid.nearestIndexForPositionLy(m_view->state().entry.positionLy, level), level), m_vp,
                m_now);
            m_view->cancelCameraFlight(true);
        }
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
                enterEntryRoot();
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
                // Cube single clicks (empty space inside the current parent:
                // the resolver's hierarchy-layer cube), star after cube,
                // cube after star.
                for (int k = 0; k < 10; ++k)
                {
                    calm();
                    auto at = std::make_shared<glm::vec2>(-50.0f, -50.0f);
                    auto ok = std::make_shared<bool>(false);
                    call([this, at, ok, k] { *ok = layerPoint(*at, k); });
                    clickIf(ok, [at] { return *at; }, glm::vec2(2.0f, -1.0f));
                    call([this, ok] {
                        const auto& grid = m_view->state().navigationGrid;
                        const auto& press = m_view->state().pressTarget;
                        record("5 cube single click",
                               *ok && press.cell && grid.hasSelectedCell() &&
                                   grid.selectedCell().index == press.cell->index && m_cameraHeldStill,
                               *ok ? "" : "(no empty-space point)");
                    });
                    wait(0.5);
                    starClicks("6 star after cube", -1, 1);
                    calm();
                    auto keepsStar = std::make_shared<bool>(false);
                    call([this, at, ok, k, keepsStar] {
                        // Navigation semantics: a click on the cube that
                        // contains the current focus refines it (star kept).
                        std::optional<navigation::GalaxyNavigationCell> cell;
                        *ok = layerPoint(*at, k + 20, cell);
                        const auto& st = m_view->state();
                        *keepsStar = cell && st.navigationFocusValid &&
                                     st.navigationGrid.nearestIndexForPositionLy(st.navigationFocusLy, cell->level) ==
                                         cell->index;
                    });
                    clickIf(ok, [at] { return *at; });
                    call([this, ok, keepsStar] {
                        const auto& st = m_view->state();
                        const auto& press = st.pressTarget;
                        const auto& grid = st.navigationGrid;
                        record("7 cube after star",
                               *ok && press.cell && (st.selectedSystemId >= 0) == *keepsStar &&
                                   grid.hasSelectedCell() && grid.selectedCell().index == press.cell->index,
                               std::string(*keepsStar ? "(cube holds the star: kept)" : "(other cube)"));
                    });
                    wait(0.5);
                }
                return;
            }
            case 3:
            {
                // Cube double click on an empty-space cube: the second click
                // on the same point (+1 px) or on the waypoint glyph of the
                // selected cube (its marker + (20, -2), SystemMapRenderer);
                // the cube becomes the parent.
                for (int k = 0; k < 10; ++k)
                {
                    calm();
                    call([this] { m_view->resetNavigationToEntry(); });
                    wait(0.5);
                    auto at = std::make_shared<glm::vec2>(-50.0f, -50.0f);
                    auto ok = std::make_shared<bool>(false);
                    auto cell = std::make_shared<std::optional<navigation::GalaxyNavigationCell>>();
                    call([this, at, ok, k] {
                        std::optional<navigation::GalaxyNavigationCell> probe;
                        *ok = layerPoint(*at, k * 3, probe);
                        // Glyph variant: the first click near the cube centre,
                        // where its waypoint glyph (centre + (20, -2)) appears
                        // under the cursor - the second click hits the glyph.
                        // Cubes where that point is another cube / a card are
                        // skipped (a 12+ px second click is no double click).
                        for (int attempt = 0; k % 2 == 1 && attempt < 16; ++attempt)
                        {
                            if (attempt > 0)
                                *ok = layerPoint(*at, k * 3 + 31 * attempt, probe);
                            if (!*ok || !probe)
                                continue;
                            const glm::mat4 mvp = m_view->projectionMatrix(m_vp) * m_view->viewMatrix();
                            const glm::vec4 clip = mvp * glm::vec4(m_view->positionLyToRender(probe->center), 1.0f);
                            const glm::vec2 centre((clip.x / clip.w * 0.5f + 0.5f) * m_vp.width,
                                                   (1.0f - (clip.y / clip.w * 0.5f + 0.5f)) * m_vp.height);
                            m_probeFrame = GalaxyMapInputFrame {};
                            m_probeFrame.viewport = m_vp;
                            const glm::vec2 nearGlyph = centre + glm::vec2(14.0f, -1.0f);
                            const auto there = resolveGalaxyPointer(
                                m_interaction->pointerScene(*m_view, *m_galaxy, m_probeFrame), nearGlyph).winner;
                            if (there.cell && there.cell->index == probe->index && !(panelAt && panelAt(nearGlyph)))
                            {
                                *at = nearGlyph;
                                break;
                            }
                            *ok = false;
                        }
                    });
                    clickIf(ok, [at] { return *at; });
                    call([this, cell] { *cell = m_view->state().pressTarget.cell; });
                    clickIf(ok, [this, at, k] {
                        if (k % 2 == 0)
                            return *at + glm::vec2(1.0f, 0.0f);
                        for (const auto& m : m_view->state().presentedCubeMarkers)
                            return m.screen + glm::vec2(21.0f, -2.0f);
                        return *at;
                    });
                    calm();
                    call([this, ok, cell, k] {
                        const auto& parent = m_view->state().hierarchyParent;
                        record(k % 2 == 0 ? "8 cube double click (same point)" : "8 cube double click (glyph area)",
                               *ok && cell->has_value() && parent.level == (*cell)->level &&
                                   parent.index == (*cell)->index,
                               "parent level " + std::to_string(parent.level));
                    });
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
                    call([this] { enterEntryRoot(); });
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
        // The 40 largest on-screen structures (each probe is a full resolve).
        std::vector<std::pair<float, const world::celestial::GalaxyMapMediumAtlas::Proxy*>> onScreen;
        for (const auto& proxy : atlas.proxies)
        {
            const glm::vec4 clip = mvp * glm::vec4(glm::vec3(proxy.centreLy) * scene.renderUnitsPerLy, 1.0f);
            if (clip.w <= 0.0f)
                continue;
            const glm::vec3 ndc = glm::vec3(clip) / clip.w;
            if (std::abs(ndc.x) > 0.8f || std::abs(ndc.y) > 0.8f)
                continue;
            onScreen.push_back({proxy.radiusLy / std::max(clip.w, 1.0e-3f), &proxy});
        }
        std::sort(onScreen.begin(), onScreen.end(),
                  [](const auto& a, const auto& b) { return a.first > b.first; });
        if (onScreen.size() > 40)
            onScreen.resize(40);
        for (const auto& [apparent, proxyPtr] : onScreen)
        {
            const auto& proxy = *proxyPtr;
            const glm::vec4 clip = mvp * glm::vec4(glm::vec3(proxy.centreLy) * scene.renderUnitsPerLy, 1.0f);
            const glm::vec3 ndc = glm::vec3(clip) / clip.w;
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
