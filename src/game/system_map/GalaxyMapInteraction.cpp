#include "src/game/system_map/GalaxyMapInteraction.h"

#include "src/game/system_map/GalaxyMapView.h"
#include "src/game/navigation/CubicNavigationInteraction.h"
#include "src/world/celestial/SystemMapTypes.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

namespace
{

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
    bool GalaxyMapInteraction::pickNavigationCell(
        const GalaxyMapView& view,
        const Viewport& viewport,
        double localMouseX,
        double localMouseY,
        navigation::GalaxyNavigationCell& outCell
    ) const
    {
        const auto& state = view.state();

        if (!state.navigationGrid.enabled() ||
            !view.navigationCellsInteractive(viewport))
        {
            return false;
        }

        const glm::mat4 mvp =
            view.projectionMatrix(viewport) *
            view.viewMatrix();

        bool found = false;
        float bestDistancePx =
            view.controls().navigationCellPickRadiusPx;
        float bestDepth = 1.0f;

        std::vector<navigation::GalaxyNavigationCell> cells;
        cells.reserve(2);

        const auto anchorCell =
            state.navigationGrid.anchorCell();

        cells.push_back(anchorCell);

        if (state.navigationGrid.hasHoveredCell())
        {
            const auto& hoveredCell =
                state.navigationGrid.hoveredCell();

            if (hoveredCell.index != anchorCell.index)
                cells.push_back(hoveredCell);
        }

        for (const auto& cell : cells)
        {
            bool visible = false;
            float depth = 1.0f;

            const glm::vec2 screen =
                projectToScreen(
                    view.positionLyToRender(cell.centerLy),
                    mvp,
                    viewport,
                    visible,
                    depth
                );

            if (!visible)
                continue;

            const float dx =
                screen.x - static_cast<float>(localMouseX);

            const float dy =
                screen.y - static_cast<float>(localMouseY);

            const float distancePx =
                std::sqrt(dx * dx + dy * dy);

            const bool betterScreenMatch =
                distancePx < bestDistancePx - 0.25f;

            const bool equalScreenMatchButNearer =
                std::abs(distancePx - bestDistancePx) <= 0.25f &&
                depth < bestDepth;

            if (!betterScreenMatch &&
                !equalScreenMatchButNearer)
            {
                continue;
            }

            found = true;
            bestDistancePx = distancePx;
            bestDepth = depth;
            outCell = cell;
        }

        return found;
    }

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

    int GalaxyMapInteraction::pickSystem(
        const GalaxyMapView& view,
        const Viewport& viewport,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        double localMouseX,
        double localMouseY
    ) const
    {
        const glm::mat4 mvp =
            view.projectionMatrix(viewport) *
            view.viewMatrix();

        int bestId = -1;
        float bestDistance = 999999.0f;

        for (const auto& system : galaxy.systems)
        {
            bool visible = false;
            float depth = 1.0f;

            const glm::vec2 screen =
                projectToScreen(
                    view.positionLyToRender(system.positionLy),
                    mvp,
                    viewport,
                    visible,
                    depth
                );

            if (!visible)
                continue;

            const float dx =
                screen.x - static_cast<float>(localMouseX);

            const float dy =
                screen.y - static_cast<float>(localMouseY);

            const float distance =
                std::sqrt(dx * dx + dy * dy);

            if (distance < view.controls().systemPickRadiusPx &&
                distance < bestDistance)
            {
                bestDistance = distance;
                bestId = system.id;
            }
        }

        return bestId;
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

        const auto beginCameraFlight =
            [&](const glm::vec3& target, float distance)
            {
                view.beginCameraFlight(
                    target,
                    distance,
                    frame.nowSeconds
                );
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

                    Обработчик F10 в HTML при этом остаётся нужен:
                    он работает, пока курсор находится в правой панели.
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







        bool leftStartedThisFrame = false;

        if (inside &&
            leftDown &&
            !view.state().camera.leftWasDown)
        {
            leftStartedThisFrame = true;

            view.state().mouseDownX = mx;
            view.state().mouseDownY = my;

            /*
                Сначала ищем ближайшую к курсору видимую звезду.
                Если она найдена, вращаем карту вокруг неё.
            */
            bool pivotFound = false;

            const glm::vec3 pivot =
                nearestVisibleStarToScreenPoint(view,
                    vp,
                    galaxy,
                    localMx,
                    localMy,
                    controls.pivotPickRadiusPx,
                    pivotFound
                );

            if (pivotFound)
            {
                view.state().orbitPivotWorld =
                    pivot;

                view.state().orbitPivotActive =
                    true;
            }
            else if (
                view.state().navigationGrid.enabled() &&
                view.state().navigationGrid.hasHoveredCell())
            {
                /*
                    No object under the mouse: use the cube which is
                    currently highlighted by the navigation layer.
                */
                view.state().orbitPivotWorld =
                    view.positionLyToRender(
                        view.state().navigationGrid
                            .hoveredCell()
                            .centerLy
                    );

                view.state().orbitPivotActive =
                    true;
            }
            else
            {
                /*
                    If neither an object nor a cube is available,
                    rotate around the current centre of the view.
                */
                view.state().orbitPivotWorld =
                    view.state().camera.target;

                view.state().orbitPivotActive =
                    false;
            }

            /*
                camera.target здесь не заменяем на pivot.

                Отдельный код компенсации ниже удерживает
                выбранную звезду под курсором во время вращения.
            */
            view.state().camera.rotating =
                true;
        }

        if (!leftDown &&
            view.state().camera.leftWasDown)


        {
            if (inside)
            {
                const double move =
                    std::abs(mx - view.state().mouseDownX) +
                    std::abs(my - view.state().mouseDownY);

                if (move < controls.clickMoveThresholdPx)
                {
                                        /*
                        Маркер центра куба является элементом
                        интерфейса навигации и располагается
                        поверх объектов Galaxy.

                        Поэтому сначала проверяем маркер куба,
                        и только затем — звезду.
                    */
                    game::navigation::GalaxyNavigationCell
                        cubeCenterCell;

                    const bool cubeCenterPicked =
                        view.state().navigationGrid.enabled() &&
                        pickNavigationCell(view,
                            vp,
                            localMx,
                            localMy,
                            cubeCenterCell
                        );

                    /*
                        Если курсор попал в маркер куба,
                        звезда под этим маркером не должна
                        перехватывать клик.
                    */
                    const int picked =
                        cubeCenterPicked
                            ? -1
                            : pickSystem(view,
                                vp,
                                galaxy,
                                localMx,
                                localMy
                            );


                    if (picked >= 0)
                    {
                        /*
                            A system click is not part of a cube double-click
                            sequence.
                        */
                        view.state().cubeClickTracker.reset();

                        /*
                            Одна функция отвечает и за выбор системы,
                            и за куб, и за плавное движение камеры.
                        */
                        view.focusSystem(
                            picked,
                            galaxy,
                            true,
                            frame.nowSeconds
                        );
                    }
                    else if (view.state().navigationGrid.enabled())
                    {
                        /*
                            Повторно picking не выполняем.

                            Используем результат, полученный до
                            проверки звезды. Иначе за один клик
                            hover может измениться и две проверки
                            вернут разные кубы.
                        */
                        const auto pickedCell =
                            cubeCenterCell;

                        if (cubeCenterPicked)
                        {







                            const bool isCubeDoubleClick =
                                view.state().cubeClickTracker
                                    .registerClick(
                                        frame.nowSeconds,
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
                                    pickedCell.centerLy;

                                view.state().navigationFocusValid =
                                    true;
                            }

                            /*
                                Сам куб выбирается в обоих случаях:
                                и при продолжении детализации звезды,
                                и при переходе к новому пустому кубу.
                            */
                            view.state().navigationGrid.selectCell(pickedCell);




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
                                                pickedCell.centerLy,
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
                                                zoomReferenceCell.sizeLy
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
                                                zoomReferenceCell.centerLy
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
                                            pickedCell.centerLy
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
                                            pickedCell.centerLy;
                                        view.state().navigationFocusValid = true;
                                    }

                                    if (entryIntent.valid())
                                    {
                                        if (entryIntent.valid())
                        {
                            result.mapIntent = entryIntent;
                        }
                                    }
                                }
                            }




                        }
                        else
                        {
                            view.state().cubeClickTracker.reset();
                        }
                    }
                }
            }

            view.state().camera.rotating = false;
            view.state().orbitPivotActive = false;
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
            !leftStartedThisFrame)
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

                const int pivotSystemId =
                    pickSystem(view,
                        vp,
                        galaxy,
                        localMx,
                        localMy
                    );

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
                            .centerLy;

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
                            .centerLy;

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
}
