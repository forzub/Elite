#include "src/game/system_map/GalaxyMapInteraction.h"
#include <chrono>
#include <cstdio>
#include "src/core/RuntimeTrace.h"
#include <cstdlib>
#include <string>

#include "src/game/system_map/GalaxyMapView.h"
#include "src/game/navigation/CubicNavigationInteraction.h"
#include "src/world/celestial/SystemMapTypes.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

namespace
{
    // DEV pointer trace (ELITE_GALAXY_PICK_TRACE=1): read once.
    bool galaxyPointerTraceEnabled()
    {
        static const bool enabled = [] {
            const char* v = std::getenv("ELITE_GALAXY_PICK_TRACE");
            return v && *v && *v != '0';
        }();
        return enabled;
    }

    std::string cellText(const std::optional<game::navigation::GalaxyNavigationCell>& cell)
    {
        if (!cell)
            return "-";
        char buf[96];
        std::snprintf(buf, sizeof buf, "L%d/%lld,%lld,%lld", cell->level,
                      static_cast<long long>(cell->index.x), static_cast<long long>(cell->index.y),
                      static_cast<long long>(cell->index.z));
        return buf;
    }

    // [GalaxyMapPerf] hover resolve cost (ELITE_TRACE_RUNTIME=1): the ONE
    // resolver runs when the cursor / camera / frame changed.
    void traceHoverResolveCost(double ms)
    {
        if (!core::runtimeTraceEnabled())
            return;
        static int calls = 0;
        static double sum = 0.0, peak = 0.0;
        static auto begin = std::chrono::steady_clock::now();
        ++calls;
        sum += ms;
        peak = std::max(peak, ms);
        const auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration<double>(now - begin).count() >= 2.0)
        {
            std::fprintf(stderr, "[GalaxyMapPerf] hover_resolve_avg_ms=%.3f hover_resolve_max_ms=%.3f calls=%d\n",
                         sum / calls, peak, calls);
            calls = 0;
            sum = peak = 0.0;
            begin = now;
        }
    }

    float wrapAngleRadF(float angle)
    {
        constexpr float pi = 3.14159265358979323846f;
        constexpr float twoPi = pi * 2.0f;

        angle = std::fmod(angle + pi, twoPi);
        if (angle < 0.0f)
            angle += twoPi;

        return angle - pi;
    }

    glm::vec2 projectToScreen(
        const glm::vec3& world,
        const glm::mat4& mvp,
        const Viewport& viewport,
        bool& visible,
        float& depth
    )
    {
        const glm::vec4 clip =
            mvp * glm::vec4(world, 1.0f);

        visible = false;
        depth = 1.0f;

        if (std::abs(clip.w) < 0.00001f)
            return glm::vec2(0.0f);

        const glm::vec3 ndc =
            glm::vec3(clip) / clip.w;

        visible =
            ndc.x >= -1.0f && ndc.x <= 1.0f &&
            ndc.y >= -1.0f && ndc.y <= 1.0f &&
            ndc.z >= -1.0f && ndc.z <= 1.0f;

        depth = ndc.z;

        return glm::vec2(
            (ndc.x * 0.5f + 0.5f) *
                static_cast<float>(viewport.width),
            (1.0f - (ndc.y * 0.5f + 0.5f)) *
                static_cast<float>(viewport.height)
        );
    }
}

namespace game::system_map
{
    /*
        The point under the cursor in light-years: the target's own point
        (star, cloud hit, hover-layer hit, marker cube), else the cursor ray
        at the depth of the camera target.
    */
    glm::dvec3 GalaxyMapInteraction::pointUnderCursorLy(
        const GalaxyMapView& view,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const GalaxyMapInputFrame& frame,
        const GalaxyPointerTarget& target,
        double localX,
        double localY
    ) const
    {
        if (const auto point = semanticPointLy(galaxy, target))
            return *point;
        const glm::dvec3 targetLy = view.renderToPositionLy(view.state().camera.target);
        glm::dvec3 origin(0.0);
        glm::dvec3 direction(0.0);
        galaxyPointerRayLy(pointerScene(view, galaxy, frame),
                           glm::vec2(static_cast<float>(localX), static_cast<float>(localY)), origin, direction);
        if (glm::length(direction) < 1.0e-9)
            return targetLy;
        const glm::dvec3 forward = -glm::dvec3(view.cameraDirectionWorld());
        const double along = glm::dot(direction, forward);
        if (std::abs(along) < 1.0e-9)
            return targetLy;
        return origin + direction * (glm::dot(targetLy - origin, forward) / along);
    }

    std::optional<glm::dvec3> GalaxyMapInteraction::semanticPointLy(
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const GalaxyPointerTarget& target
    )
    {
        switch (target.kind)
        {
            case GalaxyPointerTargetKind::Star:
                for (const auto& system : galaxy.systems)
                    if (system.id == target.systemId)
                        return system.positionLy;
                return std::nullopt;
            case GalaxyPointerTargetKind::Medium:
                return target.hitLy;
            case GalaxyPointerTargetKind::NavigationCube:
                if (target.reason == GalaxyPointerHitReason::HierarchyLayer)
                    return target.hitLy;
                if (target.cell)
                    return target.cell->center;
                return std::nullopt;
            case GalaxyPointerTargetKind::OverlayGlyph:
                if (target.cell)
                    return target.cell->center;
                return std::nullopt;
            default:
                return std::nullopt;
        }
    }

    std::optional<navigation::GalaxyNavigationCell>
    GalaxyMapInteraction::semanticChild(
        const GalaxyMapView& view,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const GalaxyPointerTarget& target
    )
    {
        if ((target.kind == GalaxyPointerTargetKind::NavigationCube ||
             target.kind == GalaxyPointerTargetKind::OverlayGlyph) &&
            target.cell)
        {
            if (galaxyHierarchyIsChild(view.hierarchyBox(), *target.cell))
                return target.cell;
            return std::nullopt;
        }
        if (const auto point = semanticPointLy(galaxy, target))
            return view.hierarchyChildAt(*point);
        return std::nullopt;
    }

    GalaxyHierarchyLayer GalaxyMapInteraction::hoverLayer(
        const GalaxyMapView& view
    )
    {
        const auto& state = view.state();
        const GalaxyHierarchyBox box = view.hierarchyBox();

        // The layer passes through the selected child (else the selected
        // object, else the parent centre): what the user works around.
        glm::dvec3 through = box.centerLy;
        if (state.navigationGrid.hasSelectedCell() &&
            galaxyHierarchyIsChild(box, state.navigationGrid.selectedCell()))
        {
            through = state.navigationGrid.selectedCell().center;
        }
        else if (state.navigationFocusValid &&
                 galaxyHierarchyContains(box, state.navigationFocusLy))
        {
            through = state.navigationFocusLy;
        }

        return galaxyHierarchyLayer(
            box,
            -view.cameraDirectionWorld(),
            through,
            state.hoverLayerAxis
        );
    }

    /*
        Hover = the ONE resolver's winner under the cursor (what a click
        would hit) and the child cube that holds its semantic point. Only
        re-resolved when the cursor, the camera or the frame changed.
    */
    void GalaxyMapInteraction::updatePointerHover(
        GalaxyMapView& view,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const GalaxyMapInputFrame& frame
    ) const
    {
        auto& state = view.state();
        const bool camerasEqual =
            state.hoverCamera.yaw == state.camera.yaw &&
            state.hoverCamera.pitch == state.camera.pitch &&
            state.hoverCamera.distance == state.camera.distance &&
            state.hoverCamera.target == state.camera.target;
        const bool changed =
            !state.hoverTargetValid ||
            std::abs(frame.localMouseX - state.hoverMouseX) +
                    std::abs(frame.localMouseY - state.hoverMouseY) > 0.5 ||
            !camerasEqual ||
            state.hoverHierarchyRevision != state.hierarchyRevision;
        if (!changed)
            return;

        const GalaxyHierarchyLayer layer = hoverLayer(view);
        state.hoverLayerAxis = layer.axis;

        const auto resolveBegin = std::chrono::steady_clock::now();
        state.hoverTarget =
            resolveGalaxyPointer(
                pointerScene(view, galaxy, frame),
                glm::vec2(
                    static_cast<float>(frame.localMouseX),
                    static_cast<float>(frame.localMouseY)
                )
            ).winner;
        traceHoverResolveCost(
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - resolveBegin).count());
        state.hoverTargetValid = true;
        state.hoverMouseX = frame.localMouseX;
        state.hoverMouseY = frame.localMouseY;
        state.hoverCamera = state.camera;
        state.hoverHierarchyRevision = state.hierarchyRevision;

        // A cloud is highlighted only where a click would select it.
        state.hoveredMediumObject =
            state.hoverTarget.kind == GalaxyPointerTargetKind::Medium
                ? state.hoverTarget.mediumObject
                : -1;

        if (const auto child = semanticChild(view, galaxy, state.hoverTarget))
            state.navigationGrid.setHoveredCell(*child);
        else
            state.navigationGrid.clearHoveredCell();
    }

    void GalaxyMapInteraction::clearPointerHover(GalaxyMapView& view)
    {
        auto& state = view.state();
        state.navigationGrid.clearHoveredCell();
        state.hoveredMediumObject = -1;
        state.hoverTarget = GalaxyPointerTarget {};
        state.hoverTargetValid = false;
        state.hoverMouseX = -1.0e9;
    }

    /*
        Double click on an object or a child cube: one level down into the
        child cube holding `positionLy` (it becomes the current parent). On
        the terminal Galaxy level the System Map is entered instead.
    */
    bool GalaxyMapInteraction::descendAt(
        GalaxyMapView& view,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const Viewport& vp,
        const glm::dvec3& positionLy,
        int systemId,
        double nowSeconds,
        GalaxyMapInputResult& result
    ) const
    {
        auto& grid = view.state().navigationGrid;
        const int childLevel = view.hierarchyBox().childLevel;
        const auto cell =
            grid.cell(grid.nearestIndexForPositionLy(positionLy, childLevel), childLevel);

        // Approach: the cube of the point becomes the parent and the camera
        // flies to the point itself (it stays in the view centre).
        if (view.enterHierarchyCell(cell, vp, nowSeconds, false))
        {
            view.approachHierarchyPoint(positionLy, vp, nowSeconds);
            result.galaxyLevelChanged = view.hierarchyBox().childLevel;
            return true;
        }

        if (cell.level != grid.maximumLevel())
            return false;

        // Terminal cube: the star / empty sector it holds opens the System Map.
        view.state().cubeClickTracker.reset();
        view.state().starClickTracker.reset();
        const MapIntent entryIntent =
            view.entryIntentForPosition(galaxy, positionLy, systemId);
        if (entryIntent.entersKnownSystem())
        {
            view.state().selectedSystemId = entryIntent.systemId;
            view.state().focusedSystemId = entryIntent.systemId;
        }
        else
        {
            view.state().selectedSystemId = -1;
            view.state().focusedSystemId = -1;
            view.state().navigationFocusLy = positionLy;
            view.state().navigationFocusValid = true;
        }
        if (entryIntent.valid())
            result.mapIntent = entryIntent;
        return entryIntent.valid();
    }

    /*
        Pointer contract for a cloud / icy field: the single click selects it
        (star selection dropped, info card follows the selection); the
        second click of a double click keeps it selected and flies the camera
        to the navigation cube containing the clicked point. Never a route.
    */
    bool GalaxyMapInteraction::applyMediumClick(
        GalaxyMapView& view,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const Viewport& vp,
        int object,
        const glm::dvec3& hitLy,
        double localMx,
        double localMy,
        double nowSeconds,
        GalaxyMapInputResult& result
    ) const
    {
        auto& state = view.state();
        state.cubeClickTracker.reset();
        state.starClickTracker.reset();
        state.selectedSystemId = -1;
        state.focusedSystemId = -1;
        state.selectedMediumObject = object;
        state.selectedMediumHitLy = hitLy;
        state.navigationFocusLy = hitLy;
        state.navigationFocusValid = true;

        // The clicked point's child cube is the selected address (context).
        if (const auto child = view.hierarchyChildAt(hitLy))
            state.navigationGrid.selectCell(*child);

        const bool doubleClick =
            state.mediumClickTracker.registerClick(nowSeconds, localMx, localMy, object);
        if (!doubleClick || !state.navigationGrid.enabled())
            return doubleClick;

        // Double click: into the child cube holding the clicked point (the
        // cloud stays selected); never a map change from a cloud.
        const int childLevel = view.hierarchyBox().childLevel;
        if (childLevel < state.navigationGrid.maximumLevel())
            descendAt(view, galaxy, vp, hitLy, -1, nowSeconds, result);
        return true;
    }

    GalaxyPointerScene GalaxyMapInteraction::pointerScene(
        const GalaxyMapView& view,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const GalaxyMapInputFrame& frame
    ) const
    {
        GalaxyPointerScene scene;
        scene.stars = &view.state().presentedStars;
        scene.cubeMarkers =
            view.state().navigationGrid.enabled()
                ? &view.state().presentedCubeMarkers
                : nullptr;
        scene.overlayGlyphs = &frame.overlayGlyphs;
        scene.panelUnderCursor = frame.panelUnderCursor;
        scene.galaxy = &galaxy;
        scene.mvp = view.projectionMatrix(frame.viewport) * view.viewMatrix();
        scene.viewport = frame.viewport;
        scene.renderUnitsPerLy = GalaxyMapView::RenderUnitsPerLightYear;

        GalaxyPointerCamera camera;
        camera.eyeLy = view.renderToPositionLy(glm::vec3(view.cameraEyeWorld()));
        camera.right = view.cameraRightWorld();
        camera.up = view.cameraUpWorld();
        camera.forward = -view.cameraDirectionWorld();
        camera.tanHalfFovY = std::tan(glm::radians(48.0) * 0.5);
        camera.aspect =
            frame.viewport.height > 0
                ? static_cast<double>(frame.viewport.width) / static_cast<double>(frame.viewport.height)
                : 1.0;
        scene.camera = camera;
        scene.glyphHitMarginPx = static_cast<float>(
            game::presentation::pointerInteractionSettings().glyphHitMarginPx);
        /*
            Object LOD for picking: the proximity ring of a star / icy field
            never exceeds ~0.6 of a child cube on screen (in the overview the
            cubes are the targets and a star is hit on its glyph only); a
            medium structure smaller than a few pixels is not a target.
        */
        const GalaxyHierarchyBox box = view.hierarchyBox();
        const float depth =
            std::max(
                0.0001f,
                static_cast<float>(glm::length(
                    view.cameraEyeWorld() -
                    glm::dvec3(view.positionLyToRender(box.centerLy))))
            );
        const float childPx =
            static_cast<float>(box.childEdgeLy) * GalaxyMapView::RenderUnitsPerLightYear *
            static_cast<float>(std::max(frame.viewport.height, 1)) /
            (2.0f * depth * std::tan(glm::radians(48.0f) * 0.5f));
        const float proximity =
            box.parent.overview()
                ? 0.0f
                : std::clamp(childPx * 0.6f, 8.0f, view.controls().systemPickRadiusPx);
        scene.starProximityRadiusPx = proximity;
        scene.icyFieldPickRadiusPx = proximity;
        scene.mediumMinRadiusPx = view.controls().mediumPickMinRadiusPx;

        if (view.state().navigationGrid.enabled())
        {
            GalaxyPointerHierarchy hierarchy;
            hierarchy.grid = &view.state().navigationGrid;
            hierarchy.box = box;
            hierarchy.layer = hoverLayer(view);
            scene.hierarchy = hierarchy;
        }
        return scene;
    }

    GalaxyPointerTarget GalaxyMapInteraction::resolvePressTarget(
        GalaxyMapView& view,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const GalaxyMapInputFrame& frame
    ) const
    {
        auto resolution =
            resolveGalaxyPointer(
                pointerScene(view, galaxy, frame),
                glm::vec2(
                    static_cast<float>(frame.localMouseX),
                    static_cast<float>(frame.localMouseY)
                ),
                galaxyPointerTraceEnabled()
            );

        /*
            Semantic double click: the waypoint glyph of a cube is the same
            navigation target as that cube's marker. When this press would
            complete the cube's double click, it is a cube click (refine),
            not a waypoint-card click.
        */
        auto& winner = resolution.winner;
        const auto& controls = view.controls();
        if (winner.kind == GalaxyPointerTargetKind::OverlayGlyph &&
            winner.cell.has_value() &&
            view.state().cubeClickTracker.wouldComplete(
                frame.nowSeconds,
                frame.localMouseX,
                frame.localMouseY,
                winner.cell->level,
                winner.cell->index,
                controls.cubeDoubleClickMaxIntervalSeconds,
                controls.cubeDoubleClickMaxDistancePx))
        {
            winner.kind = GalaxyPointerTargetKind::NavigationCube;
            winner.reason = GalaxyPointerHitReason::CubeSequenceGlyph;
        }

        view.state().pressTarget = winner;
        view.state().pressResolution = std::move(resolution);
        return winner;
    }

    bool GalaxyMapInteraction::applyClick(
        GalaxyMapView& view,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const Viewport& vp,
        const GalaxyPointerTarget& target,
        double pressX,
        double pressY,
        double pressTimeSeconds,
        GalaxyMapInputResult& result
    ) const
    {
        switch (target.kind)
        {
            case GalaxyPointerTargetKind::Star:
            {
                /*
                    A system click is not part of a cube or cloud double
                    click sequence; it drops the cloud selection. Single:
                    select the star (and its child cube); the current parent
                    stays. Double: into the child cube holding the star
                    (System Map from the terminal level).
                */
                view.state().selectedMediumObject = -1;
                view.state().mediumClickTracker.reset();
                view.state().cubeClickTracker.reset();

                const bool doubleClick =
                    view.state().starClickTracker.registerClick(
                        pressTimeSeconds, pressX, pressY, target.systemId);

                view.focusSystem(
                    target.systemId,
                    galaxy,
                    true,
                    pressTimeSeconds
                );

                if (doubleClick && view.state().navigationFocusValid)
                {
                    descendAt(view, galaxy, vp, view.state().navigationFocusLy,
                              target.systemId, pressTimeSeconds, result);
                }
                return doubleClick;
            }

            case GalaxyPointerTargetKind::NavigationCube:
                if (!target.cell.has_value() ||
                    !view.state().navigationGrid.enabled())
                {
                    return false;
                }
                return applyCubeClick(
                    view, galaxy, vp, *target.cell,
                    pressX, pressY, pressTimeSeconds, result);

            case GalaxyPointerTargetKind::Medium:
                return applyMediumClick(
                    view, galaxy, vp, target.mediumObject, target.hitLy,
                    pressX, pressY, pressTimeSeconds, result);

            case GalaxyPointerTargetKind::Empty:
                // Empty space drops the cloud selection and every sequence.
                view.state().selectedMediumObject = -1;
                view.state().mediumClickTracker.reset();
                view.state().cubeClickTracker.reset();
                view.state().starClickTracker.reset();
                return false;

            case GalaxyPointerTargetKind::Panel:
            case GalaxyPointerTargetKind::OverlayGlyph:
                // Owned by the overlay (SystemMapRenderer); never a map click.
                return false;
        }
        return false;
    }

    glm::vec3 GalaxyMapInteraction::nearestVisibleStarToScreenPoint(
        const GalaxyMapView& view,
        const Viewport& viewport,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        double localMouseX,
        double localMouseY,
        float maxRadiusPx,
        bool& found
    ) const
    {
        found = false;

        const glm::mat4 mvp =
            view.projectionMatrix(viewport) *
            view.viewMatrix();

        const glm::vec2 mouse(
            static_cast<float>(localMouseX),
            static_cast<float>(localMouseY)
        );

        const float maxDistanceSquared =
            maxRadiusPx * maxRadiusPx;

        float bestDistanceSquared =
            maxDistanceSquared;

        glm::vec3 bestWorld(0.0f);

        for (const auto& system : galaxy.systems)
        {
            bool visible = false;
            float depth = 1.0f;

            const glm::vec3 world =
                view.positionLyToRender(system.positionLy);

            const glm::vec2 screen =
                projectToScreen(
                    world,
                    mvp,
                    viewport,
                    visible,
                    depth
                );

            if (!visible)
                continue;

            const glm::vec2 delta = screen - mouse;
            const float distanceSquared =
                glm::dot(delta, delta);

            if (distanceSquared < bestDistanceSquared)
            {
                bestDistanceSquared = distanceSquared;
                bestWorld = world;
                found = true;
            }
        }

        return bestWorld;
    }

    GalaxyMapInputResult GalaxyMapInteraction::handleInput(
        GalaxyMapView& view,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const GalaxyMapInputFrame& frame,
        double& pendingScrollY
    ) const
    {

        const Viewport& vp = frame.viewport;
        const double mx = frame.mouseX;
        const double my = frame.mouseY;
        const double localMx = frame.localMouseX;
        const double localMy = frame.localMouseY;
        const bool inside = frame.inside;
        const bool leftDown = frame.leftDown;
        const bool rightDown = frame.rightDown;

        GalaxyMapInputResult result;
        const auto announceGalaxyLevel =
            [&](int level)
            {
                result.galaxyLevelChanged = level;
            };



    // =========================================================
    // GALAXY MODE INPUT
    // =========================================================

        const game::system_map::GalaxyMapControlSettings& controls = view.controls();


        view.state().lastViewport = vp;

        /*
            The wheel range belongs to the current parent: from a child
            filling the view to slightly beyond the fitted parent. (The old
            clamp to the label distance kept upper cubes from fitting.)
        */
        float galaxyMinimumDistance = controls.minDistance;
        float galaxyMaximumDistance = controls.maxDistance;
        view.cameraDistanceLimits(vp, galaxyMinimumDistance, galaxyMaximumDistance);

        if (!view.state().cameraFlight.active())
        {
            view.state().camera.distance =
                std::clamp(
                    view.state().camera.distance,
                    galaxyMinimumDistance,
                    galaxyMaximumDistance
                );
        }

        const double dx = mx - view.state().camera.lastMouseX;
        const double dy = my - view.state().camera.lastMouseY;





        if (frame.transitionActive)
        {
            view.state().camera.rotating = false;
            view.state().camera.panning = false;
            view.state().pointerGesture.suppress(leftDown);

            view.state().camera.leftWasDown =
                leftDown;

            view.state().camera.rightWasDown =
                rightDown;

            view.state().camera.lastMouseX =
                mx;

            view.state().camera.lastMouseY =
                my;

            return result;
        }



        if (view.state().whatIsHere.open &&
            handleWhatIsHere(view, galaxy, frame, result))
        {
            return result;
        }

        /*
            Любое осознанное ручное управление отменяет
            автоматический перелёт камеры.

            Колесо не должно теряться во время camera flight:
            оно отменяет перелёт, после чего этот же scroll-импульс
            обрабатывается обычной zoom-веткой ниже.
        */
        const bool wheelInputPending =
            inside &&
            pendingScrollY != 0.0;

        const bool manualFlightCancel =
            (
                inside &&
                leftDown &&
                !view.state().camera.leftWasDown
            ) ||
            (
                inside &&
                rightDown &&
                !view.state().camera.rightWasDown
            ) ||
            wheelInputPending;






                if (view.state().cameraFlight.active() &&
                    manualFlightCancel)
                {
                    // A flight into / out of a nested parent is finished at
                    // once (the new frame is already the current parent);
                    // any other flight stops where it is.
                    view.cancelCameraFlight(
                        view.state().nestedFrameFlight
                    );
                }

                /*
                    Пока камера летит автоматически, не пересчитываем hover:
                    иначе неподвижный курсор будет подсвечивать разные кубы,
                    пролетающие под ним.
                */
                if (view.state().cameraFlight.active())
                {
                    /*
                        Не сохраняем wheel-импульсы до окончания перелёта.
                        После завершения пользователь должен сделать новый
                        осознанный шаг прокрутки.
                    */
                    pendingScrollY = 0.0;

                    clearPointerHover(view);

                    view.state().camera.rotating = false;
                    view.state().camera.panning = false;
                    view.state().pointerGesture.suppress(leftDown);

                    view.state().camera.leftWasDown =
                        leftDown;

                    view.state().camera.rightWasDown =
                        rightDown;

                    view.state().camera.lastMouseX =
                        mx;

                    view.state().camera.lastMouseY =
                        my;

                    return result;
                }




                /*
                    WebView является отдельным дочерним окном и удерживает
                    клавиатурный фокус. При клике по 3D-карте возвращаем
                    фокус GLFW-окну.

                    Navigation function keys are handled by Application, so
                    returning focus here only restores normal GLFW map input.
                */
                const bool mapMousePressed =
                    (
                        leftDown &&
                        !view.state().camera.leftWasDown
                    ) ||
                    (
                        rightDown &&
                        !view.state().camera.rightWasDown
                    );

                if (inside && mapMousePressed)
                {
                    result.requestWindowFocus = true;
                }










        if (inside &&
            !view.state().camera.rotating &&
            !view.state().camera.panning)
        {
            updatePointerHover(view, galaxy, frame);
        }
        else if (!inside)
        {
            clearPointerHover(view);
        }







        // =====================================================
        // LEFT BUTTON: ONE gesture. A press never moves the camera; the
        // target is resolved at the press from what the last frame
        // presented. Within the drag threshold the release is a click on
        // that target; beyond it the click is cancelled and the drag
        // rotates the camera.
        // =====================================================
        auto& gesture = view.state().pointerGesture;
        const auto gestureEvent =
            gesture.update(
                leftDown,
                inside,
                localMx,
                localMy,
                frame.nowSeconds,
                controls.clickMoveThresholdPx
            );

        if (gestureEvent == GalaxyPointerGesture::Event::Pressed && frame.altDown)
        {
            // Alt + press: the list of everything here, never a click/drag.
            openWhatIsHere(view, galaxy, frame);
            gesture.suppress(leftDown);
        }
        else if (gestureEvent == GalaxyPointerGesture::Event::Pressed)
        {
            if (!frame.pressTargetResolved)
                resolvePressTarget(view, galaxy, frame);
            view.state().pressCamera = view.state().camera;
        }

        // Another camera gesture during a pending click cancels the click
        // (the projection the target was resolved in is about to change).
        if (gesture.phase() == GalaxyPointerGesture::Phase::PendingClick &&
            ((inside && rightDown && !view.state().camera.rightWasDown) ||
             (inside && pendingScrollY != 0.0) ||
             frame.zoomInKeyDown ||
             frame.zoomOutKeyDown))
        {
            gesture.cancelPendingClick();
            tracePointerSequence(view, galaxy, frame, "cancelled", false);
        }

        if (gestureEvent == GalaxyPointerGesture::Event::DragStarted)
        {
            /*
                Rotation pivot at the PRESS point: the nearest visible star,
                else the pressed point itself (cloud hit / hover layer, else
                the target plane) - never a cube centre.
                camera.target is not replaced by the pivot; the compensation
                below keeps the pivot under the cursor while rotating.
            */
            bool pivotFound = false;

            const glm::vec3 pivot =
                nearestVisibleStarToScreenPoint(view,
                    vp,
                    galaxy,
                    gesture.pressX(),
                    gesture.pressY(),
                    controls.pivotPickRadiusPx,
                    pivotFound
                );

            if (pivotFound)
            {
                view.state().orbitPivotWorld = pivot;
                view.state().orbitPivotActive = true;
            }
            else
            {
                view.state().orbitPivotWorld =
                    view.positionLyToRender(
                        pointUnderCursorLy(view, galaxy, frame, view.state().pressTarget,
                                           gesture.pressX(), gesture.pressY()));
                view.state().orbitPivotActive = true;
            }

            view.state().camera.rotating = true;
            tracePointerSequence(view, galaxy, frame, "drag", false);
        }

        if (gestureEvent == GalaxyPointerGesture::Event::Clicked)
        {
            const bool doubleClick =
                applyClick(
                    view,
                    galaxy,
                    vp,
                    view.state().pressTarget,
                    gesture.pressX(),
                    gesture.pressY(),
                    gesture.pressTimeSeconds(),
                    result
                );
            tracePointerSequence(view, galaxy, frame, "click", doubleClick);
        }

        if (!leftDown)
        {
            view.state().camera.rotating = false;
            view.state().orbitPivotActive = false;
        }


        if (inside &&
            rightDown &&
            !view.state().camera.rightWasDown)
        {
            view.state().camera.panning = true;
            view.state().camera.lastMouseX = mx;
            view.state().camera.lastMouseY = my;
        }

        if (!rightDown)
        {
            view.state().camera.panning = false;
        }

        if (view.state().camera.rotating &&
            leftDown &&
            gestureEvent != GalaxyPointerGesture::Event::DragStarted)
        {
            bool beforeVisible =
                false;

            float beforeDepth =
                1.0f;

            const glm::mat4 mvpBefore =
                view.projectionMatrix(vp) *
                view.viewMatrix();

            const glm::vec2 pivotBefore =
                projectToScreen(
                    view.state().orbitPivotWorld,
                    mvpBefore,
                    vp,
                    beforeVisible,
                    beforeDepth
                );

            const float yawStep =
                std::clamp(
                    -static_cast<float>(dx) *
                        controls.rotateSensitivity,
                    -controls.rotationMaxStepRad,
                    controls.rotationMaxStepRad
                );

            const float pitchStep =
                std::clamp(
                    static_cast<float>(dy) *
                        controls.rotateSensitivity,
                    -controls.rotationMaxStepRad,
                    controls.rotationMaxStepRad
                );

            view.state().camera.yaw +=
                yawStep;

            view.state().camera.pitch +=
                pitchStep;

            view.state().camera.yaw =
                wrapAngleRadF(
                    view.state().camera.yaw
                );

            view.state().camera.pitch =
                std::clamp(
                    view.state().camera.pitch,
                    -controls.pitchLimitRad,
                    controls.pitchLimitRad
                );

            if (view.state().orbitPivotActive)
            {
                bool afterVisible =
                    false;

                float afterDepth =
                    1.0f;

                const glm::mat4 viewAfter =
                    view.viewMatrix();

                const glm::mat4 mvpAfter =
                    view.projectionMatrix(vp) *
                    viewAfter;

                const glm::vec2 pivotAfter =
                    projectToScreen(
                        view.state().orbitPivotWorld,
                        mvpAfter,
                        vp,
                        afterVisible,
                        afterDepth
                    );

                const glm::vec2 screenDelta =
                    pivotBefore -
                    pivotAfter;

                if (beforeVisible &&
                    afterVisible &&
                    std::isfinite(screenDelta.x) &&
                    std::isfinite(screenDelta.y))
                {
                    const glm::vec3 right(
                        viewAfter[0][0],
                        viewAfter[1][0],
                        viewAfter[2][0]
                    );

                    const glm::vec3 up(
                        viewAfter[0][1],
                        viewAfter[1][1],
                        viewAfter[2][1]
                    );

                    const glm::vec3 dir =
                        glm::vec3(view.cameraDirectionWorld());

                    const glm::vec3 eye =
                        view.state().camera.target +
                        dir *
                        view.state().camera.distance;

                    const float pivotDepth =
                        std::max(
                            0.0001f,
                            glm::dot(
                                view.state().orbitPivotWorld - eye,
                                -dir
                            )
                        );

                    const float fovRad =
                        glm::radians(
                            48.0f
                        );

                    const float worldUnitsPerPixel =
                        2.0f *
                        pivotDepth *
                        std::tan(fovRad * 0.5f) /
                        static_cast<float>(
                            std::max(vp.height, 1)
                        );

                    view.state().camera.target -=
                        right *
                        screenDelta.x *
                        worldUnitsPerPixel;

                    view.state().camera.target +=
                        up *
                        screenDelta.y *
                        worldUnitsPerPixel;
                }
            }
        }

        if (view.state().camera.panning && rightDown)
        {
            const glm::mat4 cameraView =
                view.viewMatrix();

            const glm::vec3 right(
                cameraView[0][0],
                cameraView[1][0],
                cameraView[2][0]
            );

            const glm::vec3 up(
                cameraView[0][1],
                cameraView[1][1],
                cameraView[2][1]
            );

            const float panScale =
                view.state().camera.distance *
                controls.panScaleByDistance;

            view.state().camera.target -=
                right *
                static_cast<float>(dx) *
                panScale;

            view.state().camera.target +=
                up *
                static_cast<float>(dy) *
                panScale;
        }














        /*
            Колесо только непрерывно меняет расстояние камеры внутри
            текущего parent. Уровень иерархии колесом не меняется:
            вниз - двойной клик, вверх - кнопка / хлебные крошки.
        */
        if (inside)
        {
            float zoom = 0.0f;

            if (pendingScrollY != 0.0)
            {
                zoom +=
                    static_cast<float>(
                        pendingScrollY
                    );

                pendingScrollY = 0.0;
            }

            if (frame.zoomInKeyDown)
            {
                zoom += 1.0f;
            }

            if (frame.zoomOutKeyDown)
            {
                zoom -= 1.0f;
            }

            if (zoom != 0.0f)
            {
                /*
                    Navigation point priority:

                    1. exact star under the mouse;
                    2. highlighted cube under the mouse;
                    3. current centre of the view.

                    Explicit selection is deliberately not consulted.
                */
                glm::dvec3 navigationPointLy =
                    view.renderToPositionLy(
                        view.state().camera.target
                    );

                glm::vec3 zoomPivotWorld =
                    view.state().camera.target;

                bool zoomPivotActive = false;

                // Wheel pivot: the star the resolver puts under the cursor,
                // else the point under the cursor (both directions).
                const auto wheelTarget =
                    resolveGalaxyPointer(
                        pointerScene(view, galaxy, frame),
                        glm::vec2(
                            static_cast<float>(localMx),
                            static_cast<float>(localMy)
                        )
                    ).winner;
                const int pivotSystemId =
                    wheelTarget.kind == GalaxyPointerTargetKind::Star
                        ? wheelTarget.systemId
                        : -1;

                if (pivotSystemId >= 0)
                {
                    const auto pivotSystem =
                        std::find_if(
                            galaxy.systems.begin(),
                            galaxy.systems.end(),
                            [&](const auto& system)
                            {
                                return
                                    system.id ==
                                    pivotSystemId;
                            }
                        );

                    if (pivotSystem !=
                        galaxy.systems.end())
                    {
                        navigationPointLy =
                            pivotSystem->positionLy;

                        zoomPivotWorld =
                            view.positionLyToRender(
                                navigationPointLy
                            );

                        zoomPivotActive = true;
                    }
                }
                if (!zoomPivotActive)
                {
                    navigationPointLy =
                        pointUnderCursorLy(view, galaxy, frame, wheelTarget, localMx, localMy);
                    zoomPivotWorld = view.positionLyToRender(navigationPointLy);
                    zoomPivotActive = true;
                }

                const float factor =
                    zoom > 0.0f
                        ? std::pow(controls.zoomInFactor, zoom)
                        : std::pow(controls.zoomOutFactor, -zoom);

                const float distanceBefore =
                    view.state().camera.distance;

                view.state().camera.distance =
                    std::clamp(
                        distanceBefore * factor,
                        galaxyMinimumDistance,
                        galaxyMaximumDistance
                    );

                /*
                    Zoom about the point under the cursor: the whole camera
                    rig scales about that point, so the point stays under
                    the cursor and the camera really approaches / leaves it
                    (the cube under the cursor grows until it fills the
                    screen).
                */
                if (zoomPivotActive)
                {
                    const float applied =
                        view.state().camera.distance /
                        std::max(distanceBefore, 1.0e-9f);
                    view.state().camera.target =
                        zoomPivotWorld +
                        (view.state().camera.target - zoomPivotWorld) * applied;
                }

                // Only the overview, zoomed out to its limit, centres itself
                // so that it fills the whole screen.
                if (zoom < 0.0f && view.hierarchyBox().parent.overview() &&
                    view.state().camera.distance >= galaxyMaximumDistance * 0.7f)
                {
                    const glm::vec3 centre =
                        view.positionLyToRender(view.hierarchyBox().centerLy);
                    view.state().camera.target =
                        glm::mix(view.state().camera.target, centre, 0.35f);
                }

                applyWheelLevels(
                    view,
                    galaxy,
                    frame,
                    zoom > 0.0f,
                    pivotSystemId,
                    result
                );

                (void)navigationPointLy;
            }
        }

        view.state().camera.leftWasDown = leftDown;







        view.state().camera.rightWasDown = rightDown;
        view.state().camera.lastMouseX = mx;
        view.state().camera.lastMouseY = my;

        return result;
    }

    /*
        Navigation cube click (pointer contract): a single click selects the
        cube; a double click on the SAME semantic cube (its marker, or the
        waypoint glyph of that cube as the second click) refines / enters
        the child map. Press position and time identify the click.
    */
    bool GalaxyMapInteraction::applyCubeClick(
        GalaxyMapView& view,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const Viewport& vp,
        const navigation::GalaxyNavigationCell& pickedCell,
        double localMx,
        double localMy,
        double nowSeconds,
        GalaxyMapInputResult& result
    ) const
    {
        const auto& controls = view.controls();

        // A cube / empty-space click drops the cloud selection.
        view.state().selectedMediumObject = -1;
        view.state().mediumClickTracker.reset();
        view.state().starClickTracker.reset();








        const bool isCubeDoubleClick =
            view.state().cubeClickTracker
                .registerClick(
                    nowSeconds,
                    localMx,
                    localMy,
                    pickedCell.level,
                    pickedCell.index,
                    controls
                        .cubeDoubleClickMaxIntervalSeconds,
                    controls
                        .cubeDoubleClickMaxDistancePx
                );










        /*
            Если внутри выбранного куба уже находится текущая точка
            навигации — например выбранная звезда Alpha Centauri —
            сохраняем точную координату этой звезды.

            Клик по центру такого куба означает дальнейшее уточнение
            прежней цели, а не переключение на геометрический центр куба.

            Только клик по другому кубу сбрасывает выбранную систему
            и делает центром навигации геометрический центр нового куба.
        */
        bool clickedCellContainsExistingFocus = false;

        if (view.state().navigationFocusValid)
        {
            const auto focusCellIndex =
                view.state().navigationGrid.nearestIndexForPositionLy(
                    view.state().navigationFocusLy,
                    pickedCell.level
                );

            clickedCellContainsExistingFocus =
                focusCellIndex == pickedCell.index;
        }

        if (!clickedCellContainsExistingFocus)
        {
            /*
                Пользователь выбрал другой куб, не содержащий прежнюю
                навигационную цель. Теперь работаем от центра этого куба.
            */
            view.state().selectedSystemId = -1;
            view.state().focusedSystemId = -1;

            view.state().navigationFocusLy =
                pickedCell.center;

            view.state().navigationFocusValid =
                true;
        }

        /*
            Сам куб выбирается в обоих случаях:
            и при продолжении детализации звезды,
            и при переходе к новому пустому кубу.
        */
        view.state().navigationGrid.selectCell(pickedCell);
        result.clickedNavigationCell = pickedCell;




                                    /*
            Одиночный клик только выбирает куб.

            Только двойной клик:
            - использует выбранный куб как точку уточнения;
            - переключает сетку на следующий уровень;
            - плавно вписывает этот куб в экран.
        */

                                    /*
            Само решение о переходе уровня принимает
            общий слой кубической навигации.

            Galaxy здесь только сообщает:
            - можно ли ещё уточнять сетку;
            - можно ли открыть System.
        */
        if (isCubeDoubleClick)
        {
            /*
                Double click: the cube becomes the current parent (camera
                flies into it); on the terminal Galaxy level the star or
                empty sector it holds opens the System Map.
            */
            const bool keepsStar =
                clickedCellContainsExistingFocus &&
                view.state().selectedSystemId >= 0;
            descendAt(
                view,
                galaxy,
                vp,
                keepsStar ? view.state().navigationFocusLy : pickedCell.center,
                keepsStar ? view.state().selectedSystemId : -1,
                nowSeconds,
                result
            );
        }

        return isCubeDoubleClick;
    }

    void GalaxyMapInteraction::openWhatIsHere(
        GalaxyMapView& view,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const GalaxyMapInputFrame& frame
    ) const
    {
        auto& menu = view.state().whatIsHere;
        menu = GalaxyWhatIsHereMenu {};

        const auto resolution =
            resolveGalaxyPointer(
                pointerScene(view, galaxy, frame),
                glm::vec2(
                    static_cast<float>(frame.localMouseX),
                    static_cast<float>(frame.localMouseY)
                ),
                true
            );

        for (const auto& candidate : resolution.candidates)
        {
            // Overlay glyphs are presentations of a cube / waypoint; panels
            // are UI. Every semantic object appears once.
            if (candidate.kind == GalaxyPointerTargetKind::Panel ||
                candidate.kind == GalaxyPointerTargetKind::OverlayGlyph ||
                candidate.kind == GalaxyPointerTargetKind::Empty)
            {
                continue;
            }
            const bool duplicate =
                std::any_of(menu.targets.begin(), menu.targets.end(),
                            [&](const GalaxyPointerTarget& t) { return t.sameSemanticTarget(candidate); });
            if (duplicate)
                continue;

            std::string label;
            switch (candidate.kind)
            {
                case GalaxyPointerTargetKind::Star:
                    label = "Star: ";
                    for (const auto& system : galaxy.systems)
                        if (system.id == candidate.systemId)
                            label += system.name;
                    break;
                case GalaxyPointerTargetKind::Medium:
                {
                    const auto index = static_cast<std::size_t>(candidate.mediumObject);
                    const bool icy = index < galaxy.objects.size() &&
                                     galaxy.objects[index].objectType == "icy_debris_field";
                    label = icy ? "Icy field: " : "Cloud: ";
                    if (index < galaxy.objects.size())
                        label += galaxy.objects[index].name;
                    break;
                }
                case GalaxyPointerTargetKind::NavigationCube:
                    if (candidate.cell)
                    {
                        const auto& c = *candidate.cell;
                        label = "Navigation cube G" + std::to_string(c.level) + " [" +
                                std::to_string(c.index.x) + ", " + std::to_string(c.index.y) + ", " +
                                std::to_string(c.index.z) + "]";
                    }
                    break;
                default:
                    break;
            }
            if (label.empty())
                continue;
            menu.targets.push_back(candidate);
            menu.labels.push_back(label);
        }

        if (menu.targets.empty())
            return;

        menu.open = true;
        menu.hovered = -1;
        const float height =
            GalaxyWhatIsHereMenu::HeaderHeightPx +
            GalaxyWhatIsHereMenu::RowHeightPx * static_cast<float>(menu.labels.size());
        menu.originPx = glm::vec2(
            std::clamp(static_cast<float>(frame.localMouseX) + 14.0f, 4.0f,
                       static_cast<float>(frame.viewport.width) - GalaxyWhatIsHereMenu::WidthPx - 4.0f),
            std::clamp(static_cast<float>(frame.localMouseY) + 10.0f, 4.0f,
                       static_cast<float>(frame.viewport.height) - height - 8.0f));
    }

    bool GalaxyMapInteraction::handleWhatIsHere(
        GalaxyMapView& view,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const GalaxyMapInputFrame& frame,
        GalaxyMapInputResult& result
    ) const
    {
        auto& state = view.state();
        auto& menu = state.whatIsHere;
        menu.hovered = frame.inside ? menu.rowAt(frame.localMouseX, frame.localMouseY) : -1;

        const bool leftPressed = frame.leftDown && !state.camera.leftWasDown;
        const bool rightPressed = frame.rightDown && !state.camera.rightWasDown;

        if (leftPressed || rightPressed)
        {
            const int chosen = leftPressed ? menu.hovered : -1;
            const GalaxyPointerTarget target =
                chosen >= 0 ? menu.targets[static_cast<std::size_t>(chosen)] : GalaxyPointerTarget {};
            menu = GalaxyWhatIsHereMenu {};
            if (chosen >= 0)
            {
                applyClick(view, galaxy, frame.viewport, target, frame.localMouseX, frame.localMouseY,
                           frame.nowSeconds, result);
            }
        }

        // The list owns this frame's pointer: no hover, click or drag below.
        state.pointerGesture.suppress(frame.leftDown);
        state.camera.leftWasDown = frame.leftDown;
        state.camera.rightWasDown = frame.rightDown;
        state.camera.lastMouseX = frame.mouseX;
        state.camera.lastMouseY = frame.mouseY;
        return true;
    }

    /*
        The wheel moves through the hierarchy by screen size alone:
        - in: the child under the cursor (else at the view centre) that fills
          the screen becomes the parent; a terminal Galaxy cube filling the
          screen opens the System Map (its star / empty sector);
        - out: a parent smaller than the screen hands over to its own parent
          (the cube left stays selected); the overview never gets smaller
          than the screen (distance range).
        No flight: the camera already shows the new parent.
    */
    void GalaxyMapInteraction::applyWheelLevels(
        GalaxyMapView& view,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const GalaxyMapInputFrame& frame,
        bool zoomIn,
        int pivotSystemId,
        GalaxyMapInputResult& result
    ) const
    {
        const Viewport& vp = frame.viewport;
        auto& state = view.state();
        auto& grid = state.navigationGrid;

        for (int guard = 0; guard < 3; ++guard)
        {
            const GalaxyHierarchyBox box = view.hierarchyBox();
            if (zoomIn)
            {
                std::optional<navigation::GalaxyNavigationCell> candidate;
                if (grid.hasHoveredCell() && galaxyHierarchyIsChild(box, grid.hoveredCell()))
                    candidate = grid.hoveredCell();
                else
                    candidate = view.hierarchyChildAt(view.renderToPositionLy(state.camera.target));
                if (!candidate ||
                    view.cubeSizeRatio(candidate->size, vp) < GalaxyMapView::ParentFillRatio)
                {
                    return;
                }

                if (candidate->level >= grid.maximumLevel())
                {
                    // Terminal cube on the whole screen: its star / sector.
                    state.cubeClickTracker.reset();
                    const MapIntent entryIntent =
                        view.entryIntentForPosition(galaxy, candidate->center, pivotSystemId);
                    if (entryIntent.entersKnownSystem())
                    {
                        state.selectedSystemId = entryIntent.systemId;
                        state.focusedSystemId = entryIntent.systemId;
                    }
                    if (entryIntent.valid())
                        result.mapIntent = entryIntent;
                    return;
                }

                if (!view.enterHierarchyCell(*candidate, vp, frame.nowSeconds, false))
                    return;
                result.galaxyLevelChanged = view.hierarchyBox().childLevel;
            }
            else
            {
                if (box.parent.overview() ||
                    view.cubeSizeRatio(box.edgeLy, vp) >= view.controls().parentHandOverUpRatio)
                {
                    return;
                }
                if (!view.jumpToHierarchyLevel(box.parent.level - 1, vp, frame.nowSeconds, false))
                    return;
                result.galaxyLevelChanged = view.hierarchyBox().childLevel;
            }
        }
    }

    GalaxyMapInputResult GalaxyMapInteraction::applyChosenTarget(
        GalaxyMapView& view,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const Viewport& viewport,
        const GalaxyPointerTarget& target,
        double x,
        double y,
        double nowSeconds
    ) const
    {
        GalaxyMapInputResult result;
        applyClick(view, galaxy, viewport, target, x, y, nowSeconds, result);
        return result;
    }

    void GalaxyMapInteraction::tracePointerSequence(
        const GalaxyMapView& view,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const GalaxyMapInputFrame& frame,
        const char* gesture,
        bool doubleClick
    ) const
    {
        if (!galaxyPointerTraceEnabled())
            return;
        static std::FILE* file = [] {
            std::FILE* f = std::fopen("D:/tmp/elite_galaxy_pick_trace.csv", "a");
            if (f)
                std::fprintf(f,
                    "time_s,press_x,press_y,release_x,release_y,movement_px,gesture,camera_changed,"
                    "stars,cubes,media,overlay,winner_kind,winner_id,reason,click,"
                    "selected_star,selected_cube,selected_medium\n");
            return f;
        }();
        if (!file)
            return;

        const auto& state = view.state();
        const auto& gestureState = state.pointerGesture;
        const auto& pressCamera = state.pressCamera;
        const bool cameraChanged =
            pressCamera.yaw != state.camera.yaw ||
            pressCamera.pitch != state.camera.pitch ||
            pressCamera.distance != state.camera.distance ||
            pressCamera.target != state.camera.target;

        std::string stars, cubes, media, overlay;
        char buf[160];
        for (const auto& c : state.pressResolution.candidates)
        {
            switch (c.kind)
            {
                case GalaxyPointerTargetKind::Star:
                    std::snprintf(buf, sizeof buf, "%s%d:%s:%.1f:%.5f", stars.empty() ? "" : "|", c.systemId,
                                  galaxyPointerHitReasonName(c.reason), c.screenDistancePx, c.depth);
                    stars += buf;
                    break;
                case GalaxyPointerTargetKind::NavigationCube:
                    std::snprintf(buf, sizeof buf, "%s%s:%.1f:%.5f", cubes.empty() ? "" : "|",
                                  cellText(c.cell).c_str(), c.screenDistancePx, c.depth);
                    cubes += buf;
                    break;
                case GalaxyPointerTargetKind::Medium:
                    std::snprintf(buf, sizeof buf, "%s%d:%.5f", media.empty() ? "" : "|", c.mediumObject, c.depth);
                    media += buf;
                    break;
                case GalaxyPointerTargetKind::OverlayGlyph:
                    std::snprintf(buf, sizeof buf, "%s%s(%s):%.1f", overlay.empty() ? "" : "|",
                                  c.overlayObjectId.c_str(), cellText(c.cell).c_str(), c.screenDistancePx);
                    overlay += buf;
                    break;
                case GalaxyPointerTargetKind::Panel:
                    overlay += overlay.empty() ? "panel" : "|panel";
                    break;
                default:
                    break;
            }
        }

        const auto& w = state.pressTarget;
        std::string winnerId = "-";
        if (w.kind == GalaxyPointerTargetKind::Star)
            winnerId = std::to_string(w.systemId);
        else if (w.kind == GalaxyPointerTargetKind::NavigationCube)
            winnerId = cellText(w.cell);
        else if (w.kind == GalaxyPointerTargetKind::Medium)
            winnerId = std::to_string(w.mediumObject) +
                (w.mediumObject >= 0 && static_cast<std::size_t>(w.mediumObject) < galaxy.objects.size()
                     ? "(" + galaxy.objects[static_cast<std::size_t>(w.mediumObject)].name + ")"
                     : std::string());
        else if (w.kind == GalaxyPointerTargetKind::OverlayGlyph)
            winnerId = w.overlayObjectId;

        const std::string selectedCube =
            state.navigationGrid.hasSelectedCell()
                ? cellText(state.navigationGrid.selectedCell())
                : std::string("-");
        const bool click = std::string(gesture) == "click";

        std::fprintf(file,
            "%.3f,%.1f,%.1f,%.1f,%.1f,%.1f,%s,%s,%s,%s,%s,%s,%s,\"%s\",%s,%s,%d,%s,%d\n",
            frame.nowSeconds, gestureState.pressX(), gestureState.pressY(), frame.localMouseX,
            frame.localMouseY, gestureState.maxMovementPx(), gesture, cameraChanged ? "yes" : "no",
            stars.empty() ? "-" : stars.c_str(), cubes.empty() ? "-" : cubes.c_str(),
            media.empty() ? "-" : media.c_str(), overlay.empty() ? "-" : overlay.c_str(),
            galaxyPointerTargetKindName(w.kind), winnerId.c_str(), galaxyPointerHitReasonName(w.reason),
            click ? (doubleClick ? "double" : "single") : "-", state.selectedSystemId, selectedCube.c_str(),
            state.selectedMediumObject);
        std::fflush(file);
    }
}
