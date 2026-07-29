/*
    System map interaction implementation.

    Included by SystemMapRenderer.cpp during the incremental phase-3 split.
    The class boundary is complete; moving this file to its own translation
    unit later only requires adding SystemMapInteraction.cpp to CMake.
*/

#include <algorithm>
#include <cmath>
#include <vector>

#include <glm/gtc/constants.hpp>

#include "src/game/navigation/CubicNavigationHierarchy.h"
#include "src/game/navigation/CubicNavigationInteraction.h"
#include "src/game/system_map/SystemMapInteraction.h"
#include "src/game/system_map/SystemMapView.h"

namespace
{
    constexpr double SystemInteractionKilometersPerAu =
        149597870.7;

    double systemInteractionWorldUnitsPerPixel(
        double cameraHalfHeight,
        int viewportHeight
    )
    {
        const double safeHeight =
            static_cast<double>(
                std::max(
                    viewportHeight,
                    1
                )
            );

        const double halfHeight =
            std::clamp(
                cameraHalfHeight,
                static_cast<double>(
                    game::system_map::SystemMapView::
                        minimumCameraHalfHeight
                ),
                static_cast<double>(
                    game::system_map::SystemMapView::
                        maximumCameraHalfHeight
                )
            );

        return
            (halfHeight * 2.0) /
            safeHeight;
    }

    glm::vec3 systemInteractionViewRight(
        const glm::mat4& view
    )
    {
        return glm::normalize(
            glm::vec3(
                view[0][0],
                view[1][0],
                view[2][0]
            )
        );
    }

    glm::vec3 systemInteractionViewUp(
        const glm::mat4& view
    )
    {
        return glm::normalize(
            glm::vec3(
                view[0][1],
                view[1][1],
                view[2][1]
            )
        );
    }

    glm::dvec3 systemInteractionTargetPlanePointFromScreen(
        const Viewport& viewport,
        const glm::mat4& view,
        const glm::dvec3& target,
        double cameraHalfHeight,
        double localMouseX,
        double localMouseY
    )
    {
        const double safeWidth =
            static_cast<double>(
                std::max(
                    viewport.width,
                    1
                )
            );

        const double safeHeight =
            static_cast<double>(
                std::max(
                    viewport.height,
                    1
                )
            );

        const double aspect =
            safeWidth /
            safeHeight;

        const double halfHeightWorld =
            std::clamp(
                cameraHalfHeight,
                static_cast<double>(
                    game::system_map::SystemMapView::
                        minimumCameraHalfHeight
                ),
                static_cast<double>(
                    game::system_map::SystemMapView::
                        maximumCameraHalfHeight
                )
            );

        const double halfWidthWorld =
            halfHeightWorld *
            aspect;

        const double ndcX =
            localMouseX /
            safeWidth *
            2.0 -
            1.0;

        const double ndcY =
            1.0 -
            localMouseY /
            safeHeight *
            2.0;

        const glm::vec3 rightF =
            systemInteractionViewRight(
                view
            );

        const glm::vec3 upF =
            systemInteractionViewUp(
                view
            );

        const glm::dvec3 right(
            rightF.x,
            rightF.y,
            rightF.z
        );

        const glm::dvec3 up(
            upF.x,
            upF.y,
            upF.z
        );

        return
            target +
            right * ndcX * halfWidthWorld +
            up * ndcY * halfHeightWorld;
    }

    glm::vec2 systemInteractionProjectToScreen(
        const glm::vec3& world,
        const glm::mat4& mvp,
        const Viewport& viewport,
        bool& visible,
        float& depth
    )
    {
        const glm::vec4 clip =
            mvp *
            glm::vec4(
                world,
                1.0f
            );

        visible = false;
        depth = 1.0f;

        if (std::abs(clip.w) < 0.00001f)
            return glm::vec2(0.0f);

        const glm::vec3 ndc =
            glm::vec3(clip) /
            clip.w;

        visible =
            ndc.x >= -1.0f && ndc.x <= 1.0f &&
            ndc.y >= -1.0f && ndc.y <= 1.0f &&
            ndc.z >= -1.0f && ndc.z <= 1.0f;

        depth = ndc.z;

        return {
            (ndc.x * 0.5f + 0.5f) *
                static_cast<float>(viewport.width),
            (1.0f - (ndc.y * 0.5f + 0.5f)) *
                static_cast<float>(viewport.height)
        };
    }

    glm::vec2 systemInteractionProjectAbsoluteToScreen(
        const game::system_map::SystemMapView& view,
        const Viewport& viewport,
        const glm::dvec3& absolutePosition,
        bool& visible,
        float& depth
    )
    {
        const glm::dvec3 relative =
            absolutePosition -
            view.state().camera.target;

        const glm::vec3 renderPosition(
            static_cast<float>(relative.x),
            static_cast<float>(relative.y),
            static_cast<float>(relative.z)
        );

        const glm::mat4 mvp =
            view.projectionMatrix(viewport) *
            view.viewMatrix();

        return systemInteractionProjectToScreen(
            renderPosition,
            mvp,
            viewport,
            visible,
            depth
        );
    }

    float systemInteractionWrapAngleRad(
        float angle
    )
    {
        const float twoPi =
            glm::two_pi<float>();

        while (angle > glm::pi<float>())
            angle -= twoPi;

        while (angle < -glm::pi<float>())
            angle += twoPi;

        return angle;
    }
}

namespace game::system_map
{

void SystemMapInteraction::updateNavigationHoverFromCursor(
    SystemMapView& view,
    const Viewport& viewport,
    double localMouseX,
    double localMouseY
) const
{
    auto& state =
        view.state();

    if (!state.navigationGrid.enabled() ||
        state.lastScale <= 0.0f ||
        !view.navigationCellsInteractive(viewport))
    {
        state.navigationGrid.clearHoveredCell();
        return;
    }

    const glm::mat4 cameraView =
        view.viewMatrix();

    const glm::dvec3 cursorMapPosition =
        systemInteractionTargetPlanePointFromScreen(
            viewport,
            cameraView,
            state.camera.target,
            static_cast<double>(
                state.camera.distance
            ),
            localMouseX,
            localMouseY
        );

    const glm::dvec3 cursorAu =
        cursorMapPosition /
        static_cast<double>(
            state.lastScale
        );

    const auto index =
        state.navigationGrid.nearestIndexForPosition(
            cursorAu,
            state.navigationGrid.level()
        );

    const int minimumLevel =
        state.navigationGrid
            .definition()
            .minimumLevel;

    const navigation::CubicGridIndex
        systemRootIndex {};

    const auto rootIndexForCursor =
        state.navigationGrid.nearestIndexForPosition(
            cursorAu,
            minimumLevel
        );

    if (rootIndexForCursor != systemRootIndex)
    {
        state.navigationGrid.clearHoveredCell();
        return;
    }

    state.navigationGrid.setHoveredCell(
        state.navigationGrid.cell(
            index,
            state.navigationGrid.level()
        )
    );
}

bool SystemMapInteraction::pickNavigationCell(
    const SystemMapView& view,
    const Viewport& viewport,
    double localMouseX,
    double localMouseY,
    navigation::CubicNavigationCell& outCell
) const
{
    const auto& state =
        view.state();

    if (!state.navigationGrid.enabled() ||
        state.lastScale <= 0.0f ||
        !view.navigationCellsInteractive(viewport))
    {
        return false;
    }

    std::vector<navigation::CubicNavigationCell>
        cells;

    cells.reserve(3);

    const auto anchor =
        state.navigationGrid.anchorCell();

    cells.push_back(anchor);

    const auto addUniqueCell =
        [&](const navigation::CubicNavigationCell& cell)
        {
            const bool alreadyPresent =
                std::any_of(
                    cells.begin(),
                    cells.end(),
                    [&](const auto& existing)
                    {
                        return
                            existing.level == cell.level &&
                            existing.index == cell.index;
                    }
                );

            if (!alreadyPresent)
                cells.push_back(cell);
        };

    if (state.navigationGrid.hasSelectedCell())
    {
        const auto selected =
            state.navigationGrid.selectedCell();

        if (selected.level ==
            state.navigationGrid.level())
        {
            addUniqueCell(selected);
        }
    }

    if (state.navigationGrid.hasHoveredCell())
    {
        addUniqueCell(
            state.navigationGrid.hoveredCell()
        );
    }

    const glm::mat4 mvp =
        view.projectionMatrix(viewport) *
        view.viewMatrix();

    bool found = false;
    float bestDistance = 18.0f;
    float bestDepth = 1.0f;

    for (const auto& cell : cells)
    {
        const glm::dvec3 absoluteMap =
            cell.center *
            static_cast<double>(
                state.lastScale
            );

        const glm::vec3 relative =
            glm::vec3(
                absoluteMap -
                state.camera.target
            );

        bool visible = false;
        float depth = 1.0f;

        const glm::vec2 screen =
            systemInteractionProjectToScreen(
                relative,
                mvp,
                viewport,
                visible,
                depth
            );

        if (!visible)
            continue;

        const glm::vec2 delta =
            screen -
            glm::vec2(
                static_cast<float>(localMouseX),
                static_cast<float>(localMouseY)
            );

        const float distance =
            glm::length(delta);

        if (distance < bestDistance ||
            (
                std::abs(distance - bestDistance) < 0.01f &&
                depth < bestDepth
            ))
        {
            bestDistance = distance;
            bestDepth = depth;
            outCell = cell;
            found = true;
        }
    }

    return found;
}

void SystemMapInteraction::focusBody(
    SystemMapView& view,
    const SystemMapInteractionContext& context,
    const std::string& bodyId,
    double nowSeconds
) const
{
    auto& state =
        view.state();

    if (bodyId.empty() ||
        !state.navigationGrid.enabled() ||
        state.lastScale <= 0.0f)
    {
        return;
    }

    const auto absolutePosition =
        context.systemBodyAbsolutePosition(
            bodyId
        );

    if (!absolutePosition)
        return;

    state.selectedBodyId = bodyId;
    state.selectedHubId.clear();
    state.selectedHubParentBodyId.clear();
    state.navigationCellExplicitlySelected = false;

    const glm::dvec3 bodyPositionAu =
        *absolutePosition /
        static_cast<double>(
            state.lastScale
        );

    state.navigationGrid.setAnchorFromPosition(
        bodyPositionAu
    );

    state.navigationGrid.selectCell(
        state.navigationGrid.anchorCell()
    );

    state.navigationGrid.clearHoveredCell();

    state.hoverVisualCell.reset();
    state.hoverVisualAlpha = 0.0f;
    state.hoverOutgoingCell.reset();
    state.hoverOutgoingAlpha = 0.0f;
    state.cubeClickTracker.reset();

    view.beginCameraFlight(
        *absolutePosition,
        state.camera.distance,
        nowSeconds
    );
}

void SystemMapInteraction::focusHub(
    SystemMapView& view,
    const SystemMapInteractionContext& context,
    const SystemMapHubSelection& hub,
    double nowSeconds
) const
{
    auto& state =
        view.state();

    if (hub.hubId.empty() ||
        !state.navigationGrid.enabled() ||
        state.lastScale <= 0.0f)
    {
        return;
    }

    const auto absolutePosition =
        context.systemObjectAbsolutePosition(
            hub.hubId
        );

    if (!absolutePosition)
        return;

    state.selectedBodyId.clear();
    state.selectedHubId = hub.hubId;
    state.selectedHubParentBodyId =
        hub.parentBodyId;
    state.navigationCellExplicitlySelected = false;

    const glm::dvec3 hubPositionAu =
        *absolutePosition /
        static_cast<double>(
            state.lastScale
        );

    state.navigationGrid.setAnchorFromPosition(
        hubPositionAu
    );

    state.navigationGrid.selectCell(
        state.navigationGrid.anchorCell()
    );

    state.navigationGrid.clearHoveredCell();

    view.beginCameraFlight(
        *absolutePosition,
        state.camera.distance,
        nowSeconds
    );
}

SystemMapInputResult SystemMapInteraction::handleInput(
    SystemMapView& view,
    const SystemMapInteractionContext& context,
    const SystemMapInputFrame& frame,
    double& pendingScrollY
) const
{
    SystemMapInputResult result;

    auto& state =
        view.state();

    const auto& controls =
        view.controls();

    const Viewport& viewport =
        frame.viewport;

    const double dx =
        frame.mouseX -
        state.camera.lastMouseX;

    const double dy =
        frame.mouseY -
        state.camera.lastMouseY;

    view.constrainCameraToNavigationBoundary(
        viewport
    );

    bool leftStartedThisFrame = false;
    bool rightStartedThisFrame = false;

    const bool wheelInputPending =
        frame.inside &&
        pendingScrollY != 0.0;

    const bool manualFlightCancel =
        (
            frame.inside &&
            frame.leftDown &&
            !state.camera.leftWasDown
        ) ||
        (
            frame.inside &&
            frame.rightDown &&
            !state.camera.rightWasDown
        ) ||
        wheelInputPending;

    if (state.cameraFlight.active &&
        manualFlightCancel)
    {
        view.cancelCameraFlight(false);
    }

    if (state.cameraFlight.active)
    {
        pendingScrollY = 0.0;
        state.navigationGrid.clearHoveredCell();
        state.camera.rotating = false;
        state.camera.panning = false;

        view.constrainCameraToNavigationBoundary(
            viewport
        );

        state.camera.leftWasDown =
            frame.leftDown;

        state.camera.rightWasDown =
            frame.rightDown;

        state.camera.lastMouseX =
            frame.mouseX;

        state.camera.lastMouseY =
            frame.mouseY;

        return result;
    }

    if (frame.inside)
    {
        updateNavigationHoverFromCursor(
            view,
            viewport,
            frame.localMouseX,
            frame.localMouseY
        );
    }
    else
    {
        state.navigationGrid.clearHoveredCell();
    }

    const auto captureSystemOrbitPivot =
        [&]()
        {
            const float maximumDistance =
                view.navigationMaximumCameraDistance(
                    viewport
                );

            const float maximumDistanceEpsilon =
                std::max(
                    0.000001f,
                    maximumDistance * 0.0005f
                );

            if (state.camera.distance >=
                maximumDistance -
                    maximumDistanceEpsilon)
            {
                state.orbitPivotAbsolute =
                    view.navigationBoundaryCenterWorld();

                state.orbitPivotActive = false;
                return;
            }

            navigation::CubicNavigationCell
                pivotCell;

            const bool cubePivotFound =
                state.navigationGrid.enabled() &&
                pickNavigationCell(
                    view,
                    viewport,
                    frame.localMouseX,
                    frame.localMouseY,
                    pivotCell
                );

            if (cubePivotFound)
            {
                state.orbitPivotAbsolute =
                    pivotCell.center *
                    static_cast<double>(
                        state.lastScale
                    );

                state.orbitPivotActive = true;
                return;
            }

            const auto pivotBodyId =
                context.pickSystemOrbitPivotBodyId(
                    frame.localMouseX,
                    frame.localMouseY,
                    viewport
                );

            if (pivotBodyId)
            {
                const auto absolutePosition =
                    context.systemBodyAbsolutePosition(
                        *pivotBodyId
                    );

                if (absolutePosition)
                {
                    state.orbitPivotAbsolute =
                        *absolutePosition;

                    state.orbitPivotActive = true;
                    return;
                }
            }

            state.orbitPivotAbsolute =
                state.camera.target;

            state.orbitPivotActive = false;
        };

    if (frame.inside &&
        frame.leftDown &&
        !state.camera.leftWasDown)
    {
        leftStartedThisFrame = true;

        state.camera.mouseDownX =
            frame.mouseX;

        state.camera.mouseDownY =
            frame.mouseY;

        state.camera.rotating = true;
        state.camera.lastMouseX = frame.mouseX;
        state.camera.lastMouseY = frame.mouseY;

        captureSystemOrbitPivot();
    }

    if (!frame.leftDown &&
        state.camera.leftWasDown)
    {
        if (frame.inside)
        {
            const double move =
                std::abs(
                    frame.mouseX -
                    state.camera.mouseDownX
                ) +
                std::abs(
                    frame.mouseY -
                    state.camera.mouseDownY
                );

            if (move < controls.clickMoveThresholdPx)
            {
                navigation::CubicNavigationCell
                    cubeCenterCell;

                const auto pickedHub =
                    context.pickSystemHubSelection(
                        frame.localMouseX,
                        frame.localMouseY
                    );

                const bool cubeCenterPicked =
                    !pickedHub &&
                    state.navigationGrid.enabled() &&
                    pickNavigationCell(
                        view,
                        viewport,
                        frame.localMouseX,
                        frame.localMouseY,
                        cubeCenterCell
                    );

                const auto pickedBodyId =
                    cubeCenterPicked || pickedHub
                        ? std::optional<std::string>{}
                        : context.pickSystemBodyId(
                            frame.localMouseX,
                            frame.localMouseY
                        );

                if (pickedHub)
                {
                    focusHub(
                        view,
                        context,
                        *pickedHub,
                        frame.nowSeconds
                    );
                }
                else if (pickedBodyId)
                {
                    focusBody(
                        view,
                        context,
                        *pickedBodyId,
                        frame.nowSeconds
                    );
                }
                else if (state.navigationGrid.enabled())
                {
                    if (cubeCenterPicked)
                    {
                        const bool isCubeDoubleClick =
                            state.cubeClickTracker
                                .registerClick(
                                    frame.nowSeconds,
                                    frame.localMouseX,
                                    frame.localMouseY,
                                    cubeCenterCell.level,
                                    cubeCenterCell.index,
                                    controls
                                        .cubeDoubleClickMaxIntervalSeconds,
                                    controls
                                        .cubeDoubleClickMaxDistancePx
                                );

                        state.selectedBodyId.clear();
                        state.selectedHubId.clear();
                        state.selectedHubParentBodyId.clear();

                        state.navigationGrid.selectCell(
                            cubeCenterCell
                        );
                        state.navigationCellExplicitlySelected = true;

                        if (isCubeDoubleClick)
                        {
                            const auto levelAction =
                                navigation::
                                    cubicNavigationDoubleClickAction(
                                        state.navigationGrid.canRefine(),
                                        false
                                    );

                            const auto newParentCell =
                                cubeCenterCell;

                            const bool levelChanged =
                                navigation::
                                    applyCubicNavigationLevelActionAtPosition(
                                        levelAction,
                                        state.navigationGrid,
                                        cubeCenterCell.center,
                                        [](
                                            auto& grid,
                                            const glm::dvec3& position
                                        )
                                        {
                                            grid.setAnchorFromPosition(
                                                position
                                            );
                                        },
                                        true
                                    );

                            if (levelChanged)
                            {
                                result.systemLevelChanged =
                                    state.navigationGrid.level();

                                state.hoverVisualCell.reset();
                                state.hoverVisualAlpha = 0.0f;
                                state.hoverOutgoingCell.reset();
                                state.hoverOutgoingAlpha = 0.0f;
                                state.cubeClickTracker.reset();

                                const float parentEdgeRender =
                                    static_cast<float>(
                                        newParentCell.size *
                                        static_cast<double>(
                                            state.lastScale
                                        )
                                    );

                                const float fittedHalfHeight =
                                    navigation::
                                        cubicNavigationOrthographicFitHalfHeight(
                                            parentEdgeRender,
                                            viewport.width,
                                            viewport.height
                                        );

                                view.beginCameraFlight(
                                    newParentCell.center *
                                        static_cast<double>(
                                            state.lastScale
                                        ),
                                    std::clamp(
                                        fittedHalfHeight,
                                        SystemMapView::
                                            minimumCameraHalfHeight,
                                        view.navigationMaximumCameraDistance(
                                            viewport
                                        )
                                    ),
                                    frame.nowSeconds
                                );
                            }
                        }
                    }
                    else
                    {
                        state.cubeClickTracker.reset();
                    }
                }
            }
        }

        state.camera.rotating = false;
        state.orbitPivotActive = false;
    }

    if (!frame.leftDown)
    {
        state.camera.rotating = false;
        state.orbitPivotActive = false;
    }

    if (frame.inside &&
        frame.rightDown &&
        !state.camera.rightWasDown)
    {
        rightStartedThisFrame = true;
        state.camera.panning = true;
        state.camera.lastMouseX = frame.mouseX;
        state.camera.lastMouseY = frame.mouseY;
    }

    if (!frame.rightDown)
        state.camera.panning = false;

    if (state.camera.rotating &&
        frame.leftDown &&
        !leftStartedThisFrame)
    {
        bool beforeVisible = false;
        float beforeDepth = 1.0f;

        const glm::vec2 pivotBefore =
            systemInteractionProjectAbsoluteToScreen(
                view,
                viewport,
                state.orbitPivotAbsolute,
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

        state.camera.yaw += yawStep;
        state.camera.pitch += pitchStep;

        state.camera.yaw =
            systemInteractionWrapAngleRad(
                state.camera.yaw
            );

        state.camera.pitch =
            std::clamp(
                state.camera.pitch,
                -controls.pitchLimitRad,
                controls.pitchLimitRad
            );

        if (state.orbitPivotActive)
        {
            bool afterVisible = false;
            float afterDepth = 1.0f;

            const glm::vec2 pivotAfter =
                systemInteractionProjectAbsoluteToScreen(
                    view,
                    viewport,
                    state.orbitPivotAbsolute,
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
                const glm::mat4 viewAfter =
                    view.viewMatrix();

                const glm::vec3 rightF =
                    systemInteractionViewRight(
                        viewAfter
                    );

                const glm::vec3 upF =
                    systemInteractionViewUp(
                        viewAfter
                    );

                const glm::dvec3 right(
                    rightF.x,
                    rightF.y,
                    rightF.z
                );

                const glm::dvec3 up(
                    upF.x,
                    upF.y,
                    upF.z
                );

                const double worldUnitsPerPixel =
                    systemInteractionWorldUnitsPerPixel(
                        static_cast<double>(
                            state.camera.distance
                        ),
                        viewport.height
                    );

                state.camera.target -=
                    right *
                    static_cast<double>(screenDelta.x) *
                    worldUnitsPerPixel;

                state.camera.target +=
                    up *
                    static_cast<double>(screenDelta.y) *
                    worldUnitsPerPixel;
            }
        }
    }

    if (state.camera.panning &&
        frame.rightDown &&
        !rightStartedThisFrame)
    {
        const glm::mat4 cameraView =
            view.viewMatrix();

        const glm::vec3 rightF =
            systemInteractionViewRight(
                cameraView
            );

        const glm::vec3 upF =
            systemInteractionViewUp(
                cameraView
            );

        const glm::dvec3 right(
            rightF.x,
            rightF.y,
            rightF.z
        );

        const glm::dvec3 up(
            upF.x,
            upF.y,
            upF.z
        );

        const double worldUnitsPerPixel =
            systemInteractionWorldUnitsPerPixel(
                static_cast<double>(
                    state.camera.distance
                ),
                viewport.height
            );

        state.camera.target -=
            right *
            dx *
            worldUnitsPerPixel;

        state.camera.target +=
            up *
            dy *
            worldUnitsPerPixel;

        view.syncNavigationAnchorToCursor();
    }

    if (frame.inside)
    {
        float zoom = 0.0f;

        if (pendingScrollY != 0.0)
        {
            zoom +=
                pendingScrollY > 0.0
                    ? 1.0f
                    : -1.0f;

            pendingScrollY = 0.0;
        }

        if (frame.zoomInKeyDown)
            zoom += 1.0f;

        if (frame.zoomOutKeyDown)
            zoom -= 1.0f;

        if (zoom != 0.0f)
        {
            glm::dvec3 navigationPointAu =
                view.navigationCursorAu();

            const auto pivotBodyId =
                context.pickSystemOrbitPivotBodyId(
                    frame.localMouseX,
                    frame.localMouseY,
                    viewport
                );

            if (pivotBodyId)
            {
                const auto absolutePosition =
                    context.systemBodyAbsolutePosition(
                        *pivotBodyId
                    );

                if (absolutePosition)
                {
                    navigationPointAu =
                        *absolutePosition /
                        static_cast<double>(
                            state.lastScale
                        );
                }
            }
            else if (state.navigationGrid.enabled() &&
                state.navigationGrid.hasHoveredCell())
            {
                navigationPointAu =
                    state.navigationGrid
                        .hoveredCell()
                        .center;
            }

            const auto rootIndexForNavigationPoint =
                state.navigationGrid.nearestIndexForPosition(
                    navigationPointAu,
                    state.navigationGrid
                        .definition()
                        .minimumLevel
                );

            if (rootIndexForNavigationPoint !=
                navigation::CubicGridIndex {})
            {
                navigationPointAu =
                    view.navigationCursorAu();
            }

            const glm::dvec3 navigationPointWorld =
                navigationPointAu *
                static_cast<double>(
                    state.lastScale
                );

            bool pivotBeforeVisible = false;
            float pivotBeforeDepth = 1.0f;

            const glm::vec2 pivotBeforeScreen =
                systemInteractionProjectAbsoluteToScreen(
                    view,
                    viewport,
                    navigationPointWorld,
                    pivotBeforeVisible,
                    pivotBeforeDepth
                );

            const float factor =
                std::pow(
                    controls.zoomStep,
                    -zoom
                );

            state.camera.distance *= factor;

            const float minHalfHeightForConfiguredKmPerPixel =
                static_cast<float>(
                    (
                        controls.minKmPerPixel *
                        static_cast<double>(
                            state.lastScale
                        ) /
                        SystemInteractionKilometersPerAu
                    ) *
                    static_cast<double>(
                        viewport.height
                    ) *
                    0.5
                );

            const float dynamicMinHalfHeight =
                std::max(
                    SystemMapView::minimumCameraHalfHeight,
                    minHalfHeightForConfiguredKmPerPixel
                );

            state.camera.distance =
                std::clamp(
                    state.camera.distance,
                    dynamicMinHalfHeight,
                    SystemMapView::maximumCameraHalfHeight
                );

            bool pivotAfterVisible = false;
            float pivotAfterDepth = 1.0f;

            const glm::vec2 pivotAfterScreen =
                systemInteractionProjectAbsoluteToScreen(
                    view,
                    viewport,
                    navigationPointWorld,
                    pivotAfterVisible,
                    pivotAfterDepth
                );

            const glm::vec2 pivotScreenDelta =
                pivotBeforeScreen -
                pivotAfterScreen;

            if (pivotBeforeVisible &&
                pivotAfterVisible &&
                std::isfinite(pivotScreenDelta.x) &&
                std::isfinite(pivotScreenDelta.y))
            {
                const glm::mat4 viewAfter =
                    view.viewMatrix();

                const glm::vec3 rightF =
                    systemInteractionViewRight(
                        viewAfter
                    );

                const glm::vec3 upF =
                    systemInteractionViewUp(
                        viewAfter
                    );

                const glm::dvec3 right(
                    rightF.x,
                    rightF.y,
                    rightF.z
                );

                const glm::dvec3 up(
                    upF.x,
                    upF.y,
                    upF.z
                );

                const double worldUnitsPerPixel =
                    systemInteractionWorldUnitsPerPixel(
                        static_cast<double>(
                            state.camera.distance
                        ),
                        viewport.height
                    );

                state.camera.target -=
                    right *
                    static_cast<double>(
                        pivotScreenDelta.x
                    ) *
                    worldUnitsPerPixel;

                state.camera.target +=
                    up *
                    static_cast<double>(
                        pivotScreenDelta.y
                    ) *
                    worldUnitsPerPixel;
            }

            view.syncNavigationAnchorToCursor();

            if (state.navigationGrid.enabled())
            {
                const float cubeDiameterPx =
                    view.navigationAnchorDiameterPx(
                        viewport
                    );

                const auto levelAction =
                    navigation::
                        cubicNavigationWheelAction(
                            zoom,
                            cubeDiameterPx,
                            viewport.width,
                            viewport.height,
                            state.navigationGrid.canRefine(),
                            state.navigationGrid.canCoarsen(),
                            false
                        );

                const bool levelChanged =
                    navigation::
                        applyCubicNavigationLevelActionAtPosition(
                            levelAction,
                            state.navigationGrid,
                            navigationPointAu,
                            [](
                                auto& grid,
                                const glm::dvec3& position
                            )
                            {
                                grid.setAnchorFromPosition(
                                    position
                                );
                            },
                            false
                        );

                if (levelChanged)
                {
                    result.systemLevelChanged =
                        state.navigationGrid.level();

                    state.hoverVisualCell.reset();
                    state.hoverVisualAlpha = 0.0f;
                    state.hoverOutgoingCell.reset();
                    state.hoverOutgoingAlpha = 0.0f;
                    state.cubeClickTracker.reset();
                }

                const float terminalMinHalfHeight =
                    static_cast<float>(
                        (
                            controls.minKmPerPixel *
                            static_cast<double>(
                                state.lastScale
                            ) /
                            SystemInteractionKilometersPerAu
                        ) *
                        static_cast<double>(
                            viewport.height
                        ) *
                        0.5
                    );

                const float navigationMaximumHalfHeight =
                    view.navigationMaximumCameraDistance(
                        viewport
                    );

                const float navigationMinimumHalfHeight =
                    std::min(
                        navigationMaximumHalfHeight,
                        std::max(
                            SystemMapView::minimumCameraHalfHeight,
                            terminalMinHalfHeight
                        )
                    );

                state.camera.distance =
                    std::clamp(
                        state.camera.distance,
                        navigationMinimumHalfHeight,
                        navigationMaximumHalfHeight
                    );
            }
        }
    }

    view.constrainCameraToNavigationBoundary(
        viewport
    );

    state.camera.leftWasDown =
        frame.leftDown;

    state.camera.rightWasDown =
        frame.rightDown;

    state.camera.lastMouseX =
        frame.mouseX;

    state.camera.lastMouseY =
        frame.mouseY;

    return result;
}

} // namespace game::system_map
