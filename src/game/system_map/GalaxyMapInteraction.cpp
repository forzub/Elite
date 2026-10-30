#include "src/game/system_map/GalaxyMapInteraction.h"
#include <cstdio>
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
    void GalaxyMapInteraction::updateNavigationHoverFromCursor(
        GalaxyMapView& view,
        const Viewport& viewport,
        double localMouseX,
        double localMouseY
    ) const
    {
        auto& state = view.state();

        if (!state.navigationGrid.enabled() ||
            !view.navigationCellsInteractive(viewport) ||
            viewport.width <= 0 ||
            viewport.height <= 0)
        {
            state.navigationGrid.clearHoveredCell();
            return;
        }

        const float mouseWindowX =
            static_cast<float>(localMouseX);

        const float mouseWindowY =
            static_cast<float>(viewport.height) -
            static_cast<float>(localMouseY);

        const glm::vec4 viewportRect(
            0.0f,
            0.0f,
            static_cast<float>(viewport.width),
            static_cast<float>(viewport.height)
        );

        const glm::vec3 rayNear =
            glm::unProject(
                glm::vec3(mouseWindowX, mouseWindowY, 0.0f),
                view.viewMatrix(),
                view.projectionMatrix(viewport),
                viewportRect
            );

        const glm::vec3 rayFar =
            glm::unProject(
                glm::vec3(mouseWindowX, mouseWindowY, 1.0f),
                view.viewMatrix(),
                view.projectionMatrix(viewport),
                viewportRect
            );

        const glm::vec3 rayVector = rayFar - rayNear;
        const float rayLength = glm::length(rayVector);

        if (rayLength < 0.000001f)
        {
            state.navigationGrid.clearHoveredCell();
            return;
        }

        const glm::vec3 rayDirection =
            rayVector / rayLength;

        const glm::vec3 planePoint = state.camera.target;
        const glm::vec3 planeNormal =
            glm::vec3(view.cameraDirectionWorld());

        const float denominator =
            glm::dot(rayDirection, planeNormal);

        if (std::abs(denominator) < 0.000001f)
        {
            state.navigationGrid.clearHoveredCell();
            return;
        }

        const float distanceAlongRay =
            glm::dot(planePoint - rayNear, planeNormal) /
            denominator;

        if (distanceAlongRay <= 0.0f)
        {
            state.navigationGrid.clearHoveredCell();
            return;
        }

        const glm::vec3 cursorRenderPosition =
            rayNear + rayDirection * distanceAlongRay;

        const glm::dvec3 cursorPositionLy =
            view.renderToPositionLy(cursorRenderPosition);

        const int currentLevel =
            state.navigationGrid.level();

        const auto hoveredIndex =
            state.navigationGrid.nearestIndexForPositionLy(
                cursorPositionLy,
                currentLevel
            );

        if (!state.navigationGrid.isCellNavigable(
                hoveredIndex,
                currentLevel
            ))
        {
            state.navigationGrid.clearHoveredCell();
            return;
        }

        state.navigationGrid.setHoveredCell(
            state.navigationGrid.cell(
                hoveredIndex,
                currentLevel
            )
        );
    }

    /*
        Pointer contract for a cloud / icy field: the single click selects it
        (star selection dropped, info card follows the selection); the
        second click of a double click keeps it selected and flies the camera
        to the navigation cube containing the clicked point. Never a route.
    */
    bool GalaxyMapInteraction::applyMediumClick(
        GalaxyMapView& view,
        const Viewport& vp,
        int object,
        const glm::dvec3& hitLy,
        double localMx,
        double localMy,
        double nowSeconds
    ) const
    {
        const auto& controls = view.controls();
        view.state().cubeClickTracker.reset();
        view.state().selectedSystemId = -1;
        view.state().selectedMediumObject = object;
        view.state().selectedMediumHitLy = hitLy;
        const bool doubleClick =
            view.state().mediumClickTracker.registerClick(nowSeconds, localMx, localMy, object);
        if (!doubleClick || !view.state().navigationGrid.enabled())
            return doubleClick;
        auto& grid = view.state().navigationGrid;
        const int level = grid.level();
        navigation::GalaxyNavigationCell cell;
        cell.level = level;
        cell.index = grid.nearestIndexForPositionLy(hitLy, level);
        cell.center = grid.cellCenterLy(cell.index, level);
        cell.size = grid.cellSizeLy(level);
        grid.setAnchorFromPositionLy(hitLy);
        grid.selectCell(cell);
        view.state().navigationFocusLy = hitLy;
        view.state().navigationFocusValid = true;
        const float fitted = game::navigation::cubicNavigationPerspectiveFitDistance(
            static_cast<float>(cell.size) * GalaxyMapView::RenderUnitsPerLightYear,
            glm::radians(48.0f), vp.width, vp.height);
        view.beginCameraFlight(view.positionLyToRender(cell.center),
                               std::clamp(fitted, controls.minDistance, controls.maxDistance), nowSeconds);        return true;
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
            view.state().navigationGrid.enabled() &&
                    view.navigationCellsInteractive(frame.viewport)
                ? &view.state().presentedCubeMarkers
                : nullptr;
        scene.overlayGlyphs = &frame.overlayGlyphs;
        scene.panelUnderCursor = frame.panelUnderCursor;
        scene.galaxy = &galaxy;
        scene.mvp = view.projectionMatrix(frame.viewport) * view.viewMatrix();
        scene.viewport = frame.viewport;
        scene.renderUnitsPerLy = GalaxyMapView::RenderUnitsPerLightYear;
        scene.glyphHitMarginPx = static_cast<float>(
            game::presentation::pointerInteractionSettings().glyphHitMarginPx);
        scene.starProximityRadiusPx = view.controls().systemPickRadiusPx;
        scene.icyFieldPickRadiusPx = view.controls().systemPickRadiusPx;
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
                /*
                    A system click is not part of a cube or cloud double
                    click sequence; it drops the cloud selection.
                */
                view.state().selectedMediumObject = -1;
                view.state().mediumClickTracker.reset();
                view.state().cubeClickTracker.reset();

                // One function selects the system, its cube and flies.
                view.focusSystem(
                    target.systemId,
                    galaxy,
                    true,
                    pressTimeSeconds
                );
                return false;

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
                    view, vp, target.mediumObject, target.hitLy,
                    pressX, pressY, pressTimeSeconds);

            case GalaxyPointerTargetKind::Empty:
                // Empty space drops the cloud selection and every sequence.
                view.state().selectedMediumObject = -1;
                view.state().mediumClickTracker.reset();
                view.state().cubeClickTracker.reset();
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


        /*
            Камера не может отлететь дальше границы,
            после которой скрываются названия систем.
        */
        const float galaxyMaximumDistance =
            std::max(
                controls.minDistance,
                view.visuals().labelMaxCameraDistance
            );

        view.state().camera.distance =
            std::clamp(
                view.state().camera.distance,
                controls.minDistance,
                galaxyMaximumDistance
            );

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
                    view.cancelCameraFlight(
                        false
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

                    view.state().navigationGrid.clearHoveredCell();

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










        if (view.state().navigationGrid.enabled() &&
            inside)
        {
            updateNavigationHoverFromCursor(view,
                vp,
                localMx,
                localMy
            );
        }
        else
        {
            view.state().navigationGrid.clearHoveredCell();
        }
        // Medium hover: re-picked only when the cursor moved (cheap when idle).
        if (inside && !view.state().camera.rotating && !view.state().camera.panning)
        {
            if (std::abs(localMx - view.state().mediumHoverMouseX) +
                    std::abs(localMy - view.state().mediumHoverMouseY) > 0.5)
            {
                // The same resolver as a click: a cloud is highlighted only
                // where a click would select it (never under a star / cube
                // marker / glyph / panel).
                const auto hover =
                    resolveGalaxyPointer(
                        pointerScene(view, galaxy, frame),
                        glm::vec2(
                            static_cast<float>(localMx),
                            static_cast<float>(localMy)
                        )
                    ).winner;
                view.state().hoveredMediumObject =
                    hover.kind == GalaxyPointerTargetKind::Medium
                        ? hover.mediumObject
                        : -1;
                view.state().mediumHoverMouseX = localMx;
                view.state().mediumHoverMouseY = localMy;
            }
        }
        else
        {
            view.state().hoveredMediumObject = -1;
            view.state().mediumHoverMouseX = -1.0e9;
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

        if (gestureEvent == GalaxyPointerGesture::Event::Pressed)
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
                else the highlighted cube, else the view centre.
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
            else if (
                view.state().navigationGrid.enabled() &&
                view.state().navigationGrid.hasHoveredCell())
            {
                view.state().orbitPivotWorld =
                    view.positionLyToRender(
                        view.state().navigationGrid
                            .hoveredCell()
                            .center
                    );
                view.state().orbitPivotActive = true;
            }
            else
            {
                view.state().orbitPivotWorld = view.state().camera.target;
                view.state().orbitPivotActive = false;
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

            view.syncNavigationAnchorToCameraTarget();
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

            view.syncNavigationAnchorToCameraTarget();
        }














        /*
            Колесо выполняет две независимые операции:

            1. непрерывно меняет расстояние камеры;
            2. при пересечении порога переключает уровень сетки.

            Переключение уровня колесом никогда не запускает
            camera flight и не умножает дистанцию на subdivision.
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

                // Wheel pivot: the star the resolver puts under the cursor.
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
                else if (
                    view.state().navigationGrid.enabled() &&
                    view.state().navigationGrid.hasHoveredCell())
                {
                    navigationPointLy =
                        view.state().navigationGrid
                            .hoveredCell()
                            .center;

                    zoomPivotWorld =
                        view.positionLyToRender(
                            navigationPointLy
                        );

                    zoomPivotActive = true;
                }

                /*
                    Never allow a stale screen-space point outside
                    the five Root cubes to become a level anchor.
                */
                const auto navigationPointIndex =
                    view.state().navigationGrid
                        .nearestIndexForPositionLy(
                            navigationPointLy,
                            view.state().navigationGrid.level()
                        );

                if (!view.state().navigationGrid.isCellNavigable(
                        navigationPointIndex,
                        view.state().navigationGrid.level()
                    ))
                {
                    navigationPointLy =
                        view.state().navigationGrid
                            .anchorCell()
                            .center;

                    zoomPivotWorld =
                        view.state().camera.target;

                    zoomPivotActive = false;
                }

                bool pivotBeforeVisible = false;
                float pivotBeforeDepth = 1.0f;

                glm::vec2 pivotBeforeScreen(0.0f);

                if (zoomPivotActive)
                {
                    pivotBeforeScreen =
                        projectToScreen(
                            zoomPivotWorld,
                            view.projectionMatrix(vp) *
                                view.viewMatrix(),
                            vp,
                            pivotBeforeVisible,
                            pivotBeforeDepth
                        );
                }

                const float factor =
                    zoom > 0.0f
                        ? std::pow(
                            controls.zoomInFactor,
                            zoom
                        )
                        : std::pow(
                            controls.zoomOutFactor,
                            -zoom
                        );

                view.state().camera.distance *=
                    factor;

                view.state().camera.distance =
                    std::clamp(
                        view.state().camera.distance,
                        controls.minDistance,
                        galaxyMaximumDistance
                    );

                /*
                    Keep the chosen navigation point under the mouse
                    while the perspective distance changes.
                */
                if (zoomPivotActive)
                {
                    bool pivotAfterVisible = false;
                    float pivotAfterDepth = 1.0f;

                    const glm::mat4 viewAfter =
                        view.viewMatrix();

                    const glm::vec2 pivotAfterScreen =
                        projectToScreen(
                            zoomPivotWorld,
                            view.projectionMatrix(vp) *
                                viewAfter,
                            vp,
                            pivotAfterVisible,
                            pivotAfterDepth
                        );

                    const glm::vec2 screenDelta =
                        pivotBeforeScreen -
                        pivotAfterScreen;

                    if (pivotBeforeVisible &&
                        pivotAfterVisible &&
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

                        const glm::vec3 direction =
                            glm::vec3(view.cameraDirectionWorld());

                        const glm::vec3 eye =
                            view.state().camera.target +
                            direction *
                            view.state().camera.distance;

                        const float pivotDepth =
                            std::max(
                                0.0001f,
                                glm::dot(
                                    zoomPivotWorld - eye,
                                    -direction
                                )
                            );

                        const float worldUnitsPerPixel =
                            2.0f *
                            pivotDepth *
                            std::tan(
                                glm::radians(48.0f) *
                                0.5f
                            ) /
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

                view.syncNavigationAnchorToCameraTarget();

                /*
                    Затем общий слой решает, нужно ли менять
                    уровень иерархии.
                */
                if (view.state().navigationGrid.enabled())
                {
                    const float currentCellDiameterPx =
                        view.navigationAnchorDiameterPx(
                            vp
                        );

                    const auto levelAction =
                        game::navigation::
                            cubicNavigationWheelAction(
                                zoom,
                                currentCellDiameterPx,
                                vp.width,
                                vp.height,
                                view.state().navigationGrid
                                    .canRefine(),
                                view.state().navigationGrid
                                    .canCoarsen(),
                                true
                            );

                    if (levelAction ==
                        game::navigation::
                            CubicNavigationLevelAction::
                                EnterChildMap)
                    {
                        /*
                            At the terminal Galaxy level, enter the
                            star or empty sector under the navigation
                            point. A selected star is accepted only while
                            it belongs to this exact terminal cube.

                            Здесь нет camera flight: смену режима
                            выполнит SpaceState через существующий
                            запрос перехода в System Map.
                        */
                        view.state().cubeClickTracker.reset();

                        const MapIntent entryIntent =
                            view.entryIntentForPosition(
                                galaxy,
                                navigationPointLy,
                                pivotSystemId
                            );

                        if (entryIntent.entersKnownSystem())
                        {
                            view.state().selectedSystemId =
                                entryIntent.systemId;

                            view.state().focusedSystemId =
                                entryIntent.systemId;
                        }
                        else
                        {
                            view.state().selectedSystemId = -1;
                            view.state().focusedSystemId = -1;

                            view.state().navigationFocusLy =
                                navigationPointLy;

                            view.state().navigationFocusValid =
                                true;
                        }

                        if (entryIntent.valid())
                        {
                            result.mapIntent = entryIntent;
                        }
                    }
                    else
                    {
                        const bool levelChanged =
                            game::navigation::
                                applyCubicNavigationLevelActionAtPosition(
                                    levelAction,
                                    view.state().navigationGrid,
                                    navigationPointLy,
                                    [](
                                        auto& grid,
                                        const glm::dvec3& positionLy
                                    )
                                    {
                                        grid.setAnchorFromPositionLy(
                                            positionLy
                                        );
                                    },
                                    false
                                );

                        if (levelChanged)
                        {
                            announceGalaxyLevel(
                                            view.state().navigationGrid.level()
                                        );

                            view.state().hoverVisualCell.reset();
                            view.state().hoverVisualAlpha = 0.0f;

                            view.state().hoverOutgoingCell.reset();
                            view.state().hoverOutgoingAlpha = 0.0f;

                            view.state().cubeClickTracker.reset();
                        }
                    }
                }
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

        const auto announceGalaxyLevel =
            [&](int level)
            {
                result.galaxyLevelChanged = level;
            };

        const auto beginCameraFlight =
            [&](const glm::vec3& target, float distance)
            {
                view.beginCameraFlight(
                    target,
                    distance,
                    nowSeconds
                );
            };

        // A cube / empty-space click drops the cloud selection.
        view.state().selectedMediumObject = -1;
        view.state().mediumClickTracker.reset();








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
            const auto levelAction =
                game::navigation::
                    cubicNavigationDoubleClickAction(
                        view.state().navigationGrid
                            .canRefine(),

                        true
                    );

            if (levelAction ==
                game::navigation::
                    CubicNavigationLevelAction::Refine)
            {
                /*
                    Сохраняем куб двойного клика до
                    изменения уровня: он нужен только
                    как цель плавного camera fit.
                */
                const auto zoomReferenceCell =
                    pickedCell;


                const bool levelChanged =
                    game::navigation::
                        applyCubicNavigationLevelActionAtPosition(
                            levelAction,
                            view.state().navigationGrid,
                            pickedCell.center,
                            [](
                                auto& grid,
                                const glm::dvec3& positionLy
                            )
                            {
                                grid.setAnchorFromPositionLy(
                                    positionLy
                                );
                            },
                            false
                        );

                if (levelChanged)
                {
                    announceGalaxyLevel(
                        view.state().navigationGrid.level()
                    );

                    view.state().hoverVisualCell.reset();
                    view.state().hoverVisualAlpha = 0.0f;

                    view.state().hoverOutgoingCell.reset();
                    view.state().hoverOutgoingAlpha = 0.0f;

                    view.state().cubeClickTracker.reset();

                    /*
                        Вписываем куб двойного клика,
                        а не дочерний куб нового уровня.

                        Поэтому используем размер
                        zoomReferenceCell, сохранённый до
                        refineAroundAnchor().
                    */
                    const float parentEdgeRender =
                        static_cast<float>(
                            zoomReferenceCell.size
                        ) *
                        GalaxyMapView::RenderUnitsPerLightYear;

                    const float fittedDistance =
                        game::navigation::
                            cubicNavigationPerspectiveFitDistance(
                                parentEdgeRender,
                                glm::radians(48.0f),
                                vp.width,
                                vp.height
                            );

                    /*
                        Двойной клик является явной
                        командой: камера плавно смотрит
                        в центр выбранного куба.
                    */
                    beginCameraFlight(
                        view.positionLyToRender(
                            zoomReferenceCell.center
                        ),
                        std::clamp(
                            fittedDistance,
                            controls.minDistance,
                            controls.maxDistance
                        )
                    );
                }
            }
            else if (
                levelAction ==
                game::navigation::
                    CubicNavigationLevelAction::
                        EnterChildMap)
            {
                /*
                    На G3 следующий режим — System.

                    Здесь переход запрошен явным
                    двойным кликом. Колесо использует
                    тот же MapIntent в своей
                    ветке ниже.
                */
                view.state().cubeClickTracker.reset();

                const MapIntent entryIntent =
                    view.entryIntentForPosition(
                        galaxy,
                        pickedCell.center
                    );

                if (entryIntent.entersKnownSystem())
                {
                    view.state().selectedSystemId =
                        entryIntent.systemId;

                    view.state().focusedSystemId =
                        entryIntent.systemId;
                }
                else
                {
                    view.state().selectedSystemId = -1;
                    view.state().focusedSystemId = -1;
                    view.state().navigationFocusLy =
                        pickedCell.center;
                    view.state().navigationFocusValid = true;
                }

                if (entryIntent.valid())
                {
                    result.mapIntent = entryIntent;
                }
            }
        }

        return isCubeDoubleClick;
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
