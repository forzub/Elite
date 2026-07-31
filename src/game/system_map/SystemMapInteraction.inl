/*
    System map interaction implementation.

    Included by SystemMapRenderer.cpp during the incremental phase-3 split.
    The class boundary is complete; moving this file to its own translation
    unit later only requires adding SystemMapInteraction.cpp to CMake.
*/

#include <algorithm>
#include <cmath>
#include <vector>

#include "src/game/navigation/CubicNavigationHierarchy.h"
#include "src/game/navigation/CubicNavigationInteraction.h"
#include "src/game/system_map/SystemMapInteraction.h"
#include "src/game/system_map/SystemMapView.h"

namespace
{
    constexpr double SystemInteractionKilometersPerAu =
        149597870.7;


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

    const glm::dvec3 cursorMapPosition =
        view.targetPlanePointFromScreen(
            viewport,
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

    if (state.navigationCellExplicitlySelected &&
        state.navigationGrid.hasSelectedCell())
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

    bool found = false;
    float bestDistance =
        view.controls().navigationCellPickRadiusPx;
    float bestDepth = 1.0f;

    for (const auto& cell : cells)
    {
        const glm::dvec3 absoluteMap =
            cell.center *
            static_cast<double>(
                state.lastScale
            );

        bool visible = false;
        float depth = 2.0f;

        const glm::vec2 screen =
            view.projectAbsoluteToScreen(
                viewport,
                absoluteMap,
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

    state.navigationGrid.clearSelectedCell();

    state.navigationGrid.clearHoveredCell();

    state.hoverVisualCell.reset();
    state.hoverVisualAlpha = 0.0f;
    state.hoverOutgoingCell.reset();
    state.hoverOutgoingAlpha = 0.0f;
    state.cubeClickTracker.reset();

    (void)nowSeconds;
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

    state.navigationGrid.clearSelectedCell();

    state.navigationGrid.clearHoveredCell();

    (void)nowSeconds;
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

    const auto cursorBodyTarget =
        frame.inside
            ? context.pickSystemCameraBodyTarget(
                frame.localMouseX,
                frame.localMouseY,
                viewport
            )
            : std::nullopt;

    const auto captureSystemOrbitPivot =
        [&]()
        {
            /*
                Rotation priority:

                1. nearest visible star / planet / moon;
                2. explicitly selected cube centre;
                3. cube centre under the cursor;
                4. physical point under the mouse.
            */
            if (cursorBodyTarget)
            {
                state.orbitPivotAbsolute =
                    cursorBodyTarget->absolutePosition;
                state.orbitPivotActive = true;
                return;
            }

            if (state.navigationGrid.enabled() &&
                state.navigationGrid.hasSelectedCell() &&
                state.navigationCellExplicitlySelected)
            {
                state.orbitPivotAbsolute =
                    state.navigationGrid
                        .selectedCell()
                        .center *
                    static_cast<double>(state.lastScale);
                state.orbitPivotActive = true;
                return;
            }

            navigation::CubicNavigationCell pivotCell;

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
                    static_cast<double>(state.lastScale);
                state.orbitPivotActive = true;
                return;
            }

            state.orbitPivotAbsolute =
                view.targetPlanePointFromScreen(
                    viewport,
                    frame.localMouseX,
                    frame.localMouseY
                );
            state.orbitPivotActive = true;
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

                const auto pickedBodyId =
                    pickedHub
                        ? std::optional<std::string>{}
                        : context.pickSystemBodyId(
                            frame.localMouseX,
                            frame.localMouseY
                        );

                const bool cubeCenterPicked =
                    !pickedHub &&
                    !pickedBodyId &&
                    state.navigationGrid.enabled() &&
                    pickNavigationCell(
                        view,
                        viewport,
                        frame.localMouseX,
                        frame.localMouseY,
                        cubeCenterCell
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

    const double leftDragDistance =
        std::abs(
            frame.mouseX -
            state.camera.mouseDownX
        ) +
        std::abs(
            frame.mouseY -
            state.camera.mouseDownY
        );

    if (state.camera.rotating &&
        frame.leftDown &&
        !leftStartedThisFrame &&
        leftDragDistance >=
            controls.clickMoveThresholdPx)
    {
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

        if (state.orbitPivotActive)
        {
            view.orbitCameraAroundPivot(
                state.orbitPivotAbsolute,
                yawStep,
                pitchStep,
                controls.pitchLimitRad
            );

            view.syncNavigationAnchorToCursor();
        }
    }


    if (state.camera.panning &&
        frame.rightDown &&
        !rightStartedThisFrame)
    {
        view.panCameraByScreenDelta(
            viewport,
            dx,
            dy
        );

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
            /*
                Zoom priority:

                1. nearest front-facing body anchor near the cursor;
                2. the current camera target.

                Cube selection changes navigation state, but it is not a
                hidden wheel pivot. A body selection is not sticky either.
            */
            glm::dvec3 navigationPointWorld =
                state.camera.target;

            double minimumEyeDistanceFromPivot = 0.0;

            if (cursorBodyTarget)
            {
                navigationPointWorld =
                    cursorBodyTarget->absolutePosition;

                minimumEyeDistanceFromPivot =
                    cursorBodyTarget->physicalRadiusWorld *
                    static_cast<double>(
                        controls.bodyZoomClearanceScale
                    );
            }

            glm::dvec3 navigationPointAu =
                navigationPointWorld /
                static_cast<double>(state.lastScale);

            if (state.navigationGrid.enabled())
            {
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
                    /*
                        Keep hierarchy refinement inside S0 without changing
                        the camera's cursor-centred zoom pivot.
                    */
                    navigationPointAu =
                        view.navigationCursorAu();
                }
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

            const float minHalfHeightForConfiguredKmPerPixel =
                static_cast<float>(
                    (
                        controls.minKmPerPixel *
                        static_cast<double>(state.lastScale) /
                        SystemInteractionKilometersPerAu
                    ) *
                    static_cast<double>(viewport.height) *
                    0.5
                );

            float minimumHalfHeight =
                std::max(
                    SystemMapView::minimumCameraHalfHeight,
                    minHalfHeightForConfiguredKmPerPixel
                );

            float maximumHalfHeight =
                SystemMapView::maximumCameraHalfHeight;

            if (state.navigationGrid.enabled())
            {
                maximumHalfHeight =
                    view.navigationMaximumCameraDistance(
                        viewport
                    );

                minimumHalfHeight =
                    std::min(
                        maximumHalfHeight,
                        minimumHalfHeight
                    );
            }

            view.zoomCameraAroundPivot(
                navigationPointWorld,
                factor,
                minimumHalfHeight,
                maximumHalfHeight,
                minimumEyeDistanceFromPivot
            );

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
