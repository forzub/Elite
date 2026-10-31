/*
    System map interaction implementation.

    This is a standalone translation unit. It deliberately depends only on
    SystemMapView and the semantic SystemMapInteractionContext contract.

    The pointer / cube navigation is the SAME as the Galaxy Map's
    (GalaxyMapInteraction): one resolver per press (hub > body > the
    selected cube's marker > the child cube on the hover layer), a press
    never moves the camera (PendingClick until the drag threshold), hover is
    the resolver's winner, the current parent cube changes only by double
    click / up / breadcrumbs / LEVEL 0 and by the wheel through screen size.

    The camera keeps what the user looks at: the wheel zooms about the
    object near the cursor (else the point under it), a rotation turns about
    the object near the press (else the pressed point), a double click on
    an object approaches it, and a body the camera turned / zoomed about is
    followed while it moves (the parent moves sideways with it).
*/

#include <algorithm>
#include <cmath>
#include <vector>

#include "src/game/navigation/CubicNavigationInteraction.h"
#include "src/game/system_map/SystemMapInteraction.h"
#include "src/game/system_map/SystemMapView.h"

namespace game::system_map
{
namespace
{
    double scaleOf(const SystemMapView& view)
    {
        return std::max(static_cast<double>(view.state().lastScale), 1.0e-12);
    }

    bool sameCamera(const SystemCameraState& a, const SystemCameraState& b)
    {
        return a.yaw == b.yaw && a.pitch == b.pitch && a.distance == b.distance && a.target == b.target;
    }

    // The cursor ray in the system grid (AU): from the camera eye.
    void cursorRayAu(
        const SystemMapView& view,
        const Viewport& viewport,
        double localX,
        double localY,
        glm::dvec3& originAu,
        glm::dvec3& directionAu
    )
    {
        const double ndcX = 2.0 * localX / static_cast<double>(std::max(viewport.width, 1)) - 1.0;
        const double ndcY = 1.0 - 2.0 * localY / static_cast<double>(std::max(viewport.height, 1));
        const double tanHalf = std::tan(glm::radians(static_cast<double>(view.visuals().projectionFieldOfViewDeg)) * 0.5);
        const double aspect =
            viewport.height > 0 ? static_cast<double>(viewport.width) / static_cast<double>(viewport.height) : 1.0;
        originAu = view.cameraEyeAbsolute() / scaleOf(view);
        directionAu = glm::normalize(
            -view.cameraDirectionWorld() +
            view.cameraRightWorld() * (ndcX * tanHalf * aspect) +
            view.cameraUpWorld() * (ndcY * tanHalf));
    }

    GalaxyHierarchyLayer hoverLayer(const SystemMapView& view)
    {
        const auto& state = view.state();
        const GalaxyHierarchyBox box = view.hierarchyBox();
        glm::dvec3 through = box.centerLy;
        if (state.navigationGrid.hasSelectedCell() &&
            galaxyHierarchyIsChild(box, state.navigationGrid.selectedCell()))
        {
            through = state.navigationGrid.selectedCell().center;
        }
        return galaxyHierarchyLayer(box, -view.cameraDirectionWorld(), through, state.hoverLayerAxis);
    }

    // ONE resolver over what the last frame presented.
    SystemPointerTarget resolveSystemPointer(
        const SystemMapView& view,
        const SystemMapInteractionContext& context,
        const Viewport& viewport,
        double localX,
        double localY
    )
    {
        SystemPointerTarget target;
        const auto& state = view.state();
        const double scale = scaleOf(view);

        if (const auto hub = context.pickSystemHubSelection(localX, localY))
        {
            const auto position = context.systemObjectAbsolutePosition(hub->hubId);
            if (position && view.hierarchyContains(*position / scale))
            {
                target.kind = SystemPointerTarget::Kind::Hub;
                target.hubId = hub->hubId;
                target.hubParentBodyId = hub->parentBodyId;
                target.pointAu = *position / scale;
                return target;
            }
        }

        if (const auto body = context.pickSystemBodyId(localX, localY))
        {
            const auto position = context.systemBodyAbsolutePosition(*body);
            if (position && view.hierarchyContains(*position / scale))
            {
                target.kind = SystemPointerTarget::Kind::Body;
                target.bodyId = *body;
                target.pointAu = *position / scale;
                return target;
            }
        }

        // The selected cube's marker (drawn control).
        const float margin = static_cast<float>(game::presentation::pointerInteractionSettings().glyphHitMarginPx);
        for (const auto& marker : state.presentedCubeMarkers)
        {
            if (!marker.visible)
                continue;
            const float d = glm::length(glm::vec2(static_cast<float>(localX), static_cast<float>(localY)) - marker.screen);
            if (d <= marker.radiusPx + margin)
            {
                target.kind = SystemPointerTarget::Kind::Cube;
                target.cell = marker.cell;
                target.pointAu = marker.cell.center;
                return target;
            }
        }

        // Empty space inside the parent: the child on the hover layer.
        const GalaxyHierarchyLayer layer = hoverLayer(view);
        glm::dvec3 origin(0.0);
        glm::dvec3 direction(0.0, 0.0, -1.0);
        cursorRayAu(view, viewport, localX, localY, origin, direction);
        const double along = direction[layer.axis];
        if (std::abs(along) > 1.0e-12)
        {
            const double t = (layer.coordinateLy - origin[layer.axis]) / along;
            if (t > 0.0)
            {
                const glm::dvec3 point = origin + direction * t;
                if (const auto child = view.hierarchyChildAt(point))
                {
                    target.kind = SystemPointerTarget::Kind::Cube;
                    target.cell = *child;
                    target.pointAu = point;
                    target.onLayer = true;
                }
            }
        }
        return target;
    }

    std::optional<navigation::CubicNavigationCell> targetChild(
        const SystemMapView& view,
        const SystemPointerTarget& target
    )
    {
        switch (target.kind)
        {
            case SystemPointerTarget::Kind::Cube:
                if (target.cell && galaxyHierarchyIsChild(view.hierarchyBox(), *target.cell))
                    return target.cell;
                return std::nullopt;
            case SystemPointerTarget::Kind::Body:
            case SystemPointerTarget::Kind::Hub:
                return view.hierarchyChildAt(target.pointAu);
            default:
                return std::nullopt;
        }
    }
}

void SystemMapInteraction::focusBody(
    SystemMapView& view,
    const SystemMapInteractionContext& context,
    const std::string& bodyId,
    double nowSeconds
) const
{
    auto& state = view.state();
    if (bodyId.empty() || !state.navigationGrid.enabled() || state.lastScale <= 0.0f)
        return;
    const auto absolutePosition = context.systemBodyAbsolutePosition(bodyId);
    if (!absolutePosition)
        return;

    state.selectedBodyId = bodyId;
    state.selectedHubId.clear();
    state.selectedHubParentBodyId.clear();
    state.navigationCellExplicitlySelected = false;

    // The body's cube inside the current parent is its context selection;
    // it follows the body (handleInput) as it moves.
    if (const auto child = view.hierarchyChildAt(*absolutePosition / scaleOf(view)))
        state.navigationGrid.selectCell(*child);
    else
        state.navigationGrid.clearSelectedCell();
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
    auto& state = view.state();
    if (hub.hubId.empty() || !state.navigationGrid.enabled() || state.lastScale <= 0.0f)
        return;
    const auto absolutePosition = context.systemObjectAbsolutePosition(hub.hubId);
    if (!absolutePosition)
        return;

    state.selectedBodyId.clear();
    state.selectedHubId = hub.hubId;
    state.selectedHubParentBodyId = hub.parentBodyId;
    state.navigationCellExplicitlySelected = false;
    if (const auto child = view.hierarchyChildAt(*absolutePosition / scaleOf(view)))
        state.navigationGrid.selectCell(*child);
    else
        state.navigationGrid.clearSelectedCell();
    state.cubeClickTracker.reset();
    state.bodyClickTracker.reset();
    (void)nowSeconds;
}

void SystemMapInteraction::focusTacticalObjectSelection(
    SystemMapView& view,
    const std::string& navigationHubId,
    const std::string& navigationHubParentBodyId
) const
{
    auto& state = view.state();

    state.selectedBodyId.clear();
    state.selectedHubId = navigationHubId;
    state.selectedHubParentBodyId = navigationHubId.empty()
        ? std::string{}
        : navigationHubParentBodyId;
    state.navigationGrid.clearSelectedCell();
    state.navigationCellExplicitlySelected = false;

    state.selectedBodyTrackingEnabled = false;
    state.trackedBodyId.clear();
    state.trackedBodyPositionValid = false;
    state.orbitPivotActive = false;
}

SystemPointerTarget SystemMapInteraction::resolvePointer(
    const SystemMapView& view,
    const SystemMapInteractionContext& context,
    const Viewport& viewport,
    double localX,
    double localY
) const
{
    return resolveSystemPointer(view, context, viewport, localX, localY);
}

void SystemMapInteraction::selectTarget(
    SystemMapView& view,
    const SystemMapInteractionContext& context,
    const SystemPointerTarget& target,
    SystemMapInputResult& result
) const
{
    auto& state = view.state();
    switch (target.kind)
    {
        case SystemPointerTarget::Kind::Body:
            focusBody(view, context, target.bodyId, 0.0);
            result.selectedBodyId = target.bodyId;
            return;
        case SystemPointerTarget::Kind::Hub:
        {
            SystemMapHubSelection hub;
            hub.hubId = target.hubId;
            hub.parentBodyId = target.hubParentBodyId;
            focusHub(view, context, hub, 0.0);
            return;
        }
        case SystemPointerTarget::Kind::Cube:
            if (!target.cell || !state.navigationGrid.enabled())
                return;
            state.selectedBodyId.clear();
            state.selectedHubId.clear();
            state.selectedHubParentBodyId.clear();
            state.navigationGrid.selectCell(*target.cell);
            state.navigationCellExplicitlySelected = true;
            result.clickedNavigationCell = *target.cell;
            return;
        case SystemPointerTarget::Kind::Empty:
            return;
    }
}

bool SystemMapInteraction::descendTarget(
    SystemMapView& view,
    const SystemMapInteractionContext& context,
    const Viewport& viewport,
    const SystemPointerTarget& target,
    double nowSeconds,
    SystemMapInputResult& result
) const
{
    auto& state = view.state();
    if (!state.navigationGrid.enabled())
        return false;

    if (target.kind == SystemPointerTarget::Kind::Cube)
    {
        if (!target.cell || !view.enterHierarchyCell(*target.cell, viewport, nowSeconds))
            return false;
        state.navigationCellExplicitlySelected = true;
        result.systemLevelChanged = view.hierarchyBox().childLevel;
        return true;
    }

    if (target.kind != SystemPointerTarget::Kind::Body && target.kind != SystemPointerTarget::Kind::Hub)
        return false;

    // Approach: the object's cube becomes the parent (the camera is not
    // framed on that cube), then the camera flies to the object itself.
    const bool body = target.kind == SystemPointerTarget::Kind::Body;
    const auto absolute = body ? context.systemBodyAbsolutePosition(target.bodyId)
                               : context.systemObjectAbsolutePosition(target.hubId);
    const glm::dvec3 pointAu = absolute ? *absolute / scaleOf(view) : target.pointAu;
    bool entered = false;
    if (const auto child = view.hierarchyChildAt(pointAu))
        entered = view.enterHierarchyCell(*child, viewport, nowSeconds, false);
    if (entered)
        result.systemLevelChanged = view.hierarchyBox().childLevel;

    SystemMapInputResult ignored;
    selectTarget(view, context, target, ignored);
    if (body)
        result.selectedBodyId = target.bodyId;

    if (entered)
    {
        view.approachHierarchyPoint(pointAu, viewport, nowSeconds);
    }
    else
    {
        // A terminal cube: no deeper parent, just closer.
        float minimumHalfHeight = SystemMapView::minimumCameraHalfHeight;
        float maximumHalfHeight = SystemMapView::maximumCameraHalfHeight;
        view.hierarchyHalfHeightLimits(viewport, minimumHalfHeight, maximumHalfHeight);
        view.approachHierarchyPoint(pointAu, viewport, nowSeconds,
                                    std::max(minimumHalfHeight, state.camera.distance * 0.35f));
    }
    if (body)
        view.followBody(target.bodyId);
    else
        view.clearFollow();
    return true;
}

/*
    Single click: a body / hub is selected with the cube it is in (no card);
    a cube is selected. Double click: a body / hub is approached, a cube
    becomes the current parent (camera fits it). The info card belongs to
    the context menu (right click / Ctrl + click).
*/
void SystemMapInteraction::applyClick(
    SystemMapView& view,
    const SystemMapInteractionContext& context,
    const Viewport& viewport,
    const SystemPointerTarget& target,
    double x,
    double y,
    double nowSeconds,
    SystemMapInputResult& result
) const
{
    auto& state = view.state();
    const auto& controls = view.controls();

    switch (target.kind)
    {
        case SystemPointerTarget::Kind::Body:
        case SystemPointerTarget::Kind::Hub:
        {
            state.cubeClickTracker.reset();
            const std::string id = target.kind == SystemPointerTarget::Kind::Body ? target.bodyId : target.hubId;
            const bool doubleClick = state.bodyClickTracker.registerClick(nowSeconds, x, y, id);
            selectTarget(view, context, target, result);
            if (doubleClick)
            {
                state.bodyClickTracker.reset();
                descendTarget(view, context, viewport, target, nowSeconds, result);
            }
            return;
        }
        case SystemPointerTarget::Kind::Cube:
        {
            if (!target.cell || !state.navigationGrid.enabled())
                return;
            state.bodyClickTracker.reset();
            const auto& cell = *target.cell;
            const bool doubleClick =
                state.cubeClickTracker.registerClick(nowSeconds, x, y, cell.level, cell.index,
                                                     controls.cubeDoubleClickMaxIntervalSeconds,
                                                     controls.cubeDoubleClickMaxDistancePx);
            selectTarget(view, context, target, result);
            if (doubleClick)
                descendTarget(view, context, viewport, target, nowSeconds, result);
            return;
        }
        case SystemPointerTarget::Kind::Empty:
            state.cubeClickTracker.reset();
            state.bodyClickTracker.reset();
            return;
    }
}

void SystemMapInteraction::updatePointerHover(
    SystemMapView& view,
    const SystemMapInteractionContext& context,
    const SystemMapInputFrame& frame
) const
{
    auto& state = view.state();
    const bool changed =
        !state.hoverTargetValid ||
        std::abs(frame.localMouseX - state.hoverMouseX) + std::abs(frame.localMouseY - state.hoverMouseY) > 0.5 ||
        !sameCamera(state.hoverCamera, state.camera) ||
        state.hoverHierarchyRevision != state.hierarchyRevision;
    if (!changed)
        return;

    state.hoverLayerAxis = hoverLayer(view).axis;
    state.hoverTarget = resolveSystemPointer(view, context, frame.viewport, frame.localMouseX, frame.localMouseY);
    state.hoverTargetValid = true;
    state.hoverMouseX = frame.localMouseX;
    state.hoverMouseY = frame.localMouseY;
    state.hoverCamera = state.camera;
    state.hoverHierarchyRevision = state.hierarchyRevision;

    if (const auto child = targetChild(view, state.hoverTarget))
        state.navigationGrid.setHoveredCell(*child);
    else
        state.navigationGrid.clearHoveredCell();
}

/*
    The wheel walks the hierarchy by screen size (as on the Galaxy Map): the
    child under the cursor (else at the view centre) filling the screen
    becomes the parent; a parent smaller than the screen hands over to its
    own parent; the S0 root never gets smaller than the screen.
*/
void SystemMapInteraction::applyWheelLevels(
    SystemMapView& view,
    const SystemMapInputFrame& frame,
    bool zoomIn,
    SystemMapInputResult& result
) const
{
    auto& state = view.state();
    auto& grid = state.navigationGrid;
    const Viewport& vp = frame.viewport;
    for (int guard = 0; guard < 3; ++guard)
    {
        const GalaxyHierarchyBox box = view.hierarchyBox();
        if (zoomIn)
        {
            std::optional<navigation::CubicNavigationCell> candidate;
            if (grid.hasHoveredCell() && galaxyHierarchyIsChild(box, grid.hoveredCell()))
                candidate = grid.hoveredCell();
            else
                candidate = view.hierarchyChildAt(state.camera.target / scaleOf(view));
            if (!candidate ||
                view.cubeSizeRatio(candidate->size, vp) < SystemMapView::ParentFillRatio ||
                !view.enterHierarchyCell(*candidate, vp, frame.nowSeconds, false))
            {
                return;
            }
            result.systemLevelChanged = view.hierarchyBox().childLevel;
        }
        else
        {
            if (box.parent.level <= 0 ||
                view.cubeSizeRatio(box.edgeLy, vp) >= SystemMapView::ParentHandOverUpRatio ||
                !view.jumpToHierarchyLevel(box.parent.level - 1, vp, frame.nowSeconds, false))
            {
                return;
            }
            result.systemLevelChanged = view.hierarchyBox().childLevel;
        }
    }
}

SystemMapInputResult SystemMapInteraction::handleInput(
    SystemMapView& view,
    const SystemMapInteractionContext& context,
    const SystemMapInputFrame& frame,
    double& pendingScrollY
) const
{
    SystemMapInputResult result;
    auto& state = view.state();
    const auto& controls = view.controls();
    const Viewport& viewport = frame.viewport;

    const double dx = frame.mouseX - state.camera.lastMouseX;
    const double dy = frame.mouseY - state.camera.lastMouseY;

    const auto finishFrame = [&]() {
        state.camera.leftWasDown = frame.leftDown;
        state.camera.rightWasDown = frame.rightDown;
        state.camera.lastMouseX = frame.mouseX;
        state.camera.lastMouseY = frame.mouseY;
    };

    // A press / wheel during a frame flight finishes it (the new parent is
    // already current).
    const bool manualFlightCancel =
        (frame.inside && frame.leftDown && !state.camera.leftWasDown) ||
        (frame.inside && frame.rightDown && !state.camera.rightWasDown) ||
        (frame.inside && pendingScrollY != 0.0);
    if (state.cameraFlight.active && manualFlightCancel)
        view.cancelCameraFlight(true);

    if (state.cameraFlight.active)
    {
        pendingScrollY = 0.0;
        state.navigationGrid.clearHoveredCell();
        state.hoverTargetValid = false;
        state.camera.rotating = false;
        state.camera.panning = false;
        state.pointerGesture.suppress(frame.leftDown);
        finishFrame();
        return result;
    }

    // A followed body leaving the parent takes the parent sideways with it
    // (the camera already moves with the body).
    if (!state.cameraFollowBodyId.empty())
    {
        const auto position = context.systemBodyAbsolutePosition(state.cameraFollowBodyId);
        if (!position)
            view.clearFollow();
        else if (!view.hierarchyContains(*position / scaleOf(view)))
            view.shiftHierarchyParentTo(*position / scaleOf(view));
    }

    // The selected body's cube follows the body as it moves between cubes.
    if (!state.selectedBodyId.empty())
    {
        if (const auto position = context.systemBodyAbsolutePosition(state.selectedBodyId))
        {
            const auto child = view.hierarchyChildAt(*position / scaleOf(view));
            if (child)
            {
                if (!state.navigationGrid.hasSelectedCell() ||
                    state.navigationGrid.selectedCell().index != child->index ||
                    state.navigationGrid.selectedCell().level != child->level)
                {
                    state.navigationGrid.selectCell(*child);
                }
            }
            else if (state.navigationGrid.hasSelectedCell() &&
                     galaxyHierarchyIsChild(view.hierarchyBox(), state.navigationGrid.selectedCell()))
            {
                state.navigationGrid.clearSelectedCell();
            }
        }
    }

    if (frame.inside && !state.camera.rotating && !state.camera.panning)
        updatePointerHover(view, context, frame);
    else if (!frame.inside)
    {
        state.navigationGrid.clearHoveredCell();
        state.hoverTargetValid = false;
    }

    // ---- left button: one gesture (press never moves the camera) ----------
    auto& gesture = state.pointerGesture;
    const auto event =
        gesture.update(frame.leftDown, frame.inside, frame.localMouseX, frame.localMouseY, frame.nowSeconds,
                       controls.clickMoveThresholdPx);

    if (event == GalaxyPointerGesture::Event::Pressed)
        state.pressTarget = resolveSystemPointer(view, context, viewport, frame.localMouseX, frame.localMouseY);

    if (gesture.phase() == GalaxyPointerGesture::Phase::PendingClick &&
        ((frame.inside && frame.rightDown && !state.camera.rightWasDown) ||
         (frame.inside && pendingScrollY != 0.0) || frame.zoomInKeyDown || frame.zoomOutKeyDown))
    {
        gesture.cancelPendingClick();
    }

    if (event == GalaxyPointerGesture::Event::DragStarted)
    {
        // Rotation pivot: the object near the PRESS point (a body is then
        // followed), else the pressed point itself (on the hover layer,
        // else on the target plane) - never a cube centre.
        const auto& press = state.pressTarget;
        if (const auto object =
                context.nearestSystemObject(gesture.pressX(), gesture.pressY(), controls.pivotPickRadiusPx))
        {
            state.orbitPivotAbsolute = object->absolutePosition;
            if (!object->bodyId.empty())
                view.followBody(object->bodyId);
        }
        else if (press.kind == SystemPointerTarget::Kind::Body || press.kind == SystemPointerTarget::Kind::Hub ||
                 (press.kind == SystemPointerTarget::Kind::Cube && press.onLayer))
        {
            state.orbitPivotAbsolute = press.pointAu * scaleOf(view);
        }
        else
        {
            state.orbitPivotAbsolute =
                view.targetPlanePointFromScreen(viewport, gesture.pressX(), gesture.pressY());
        }
        state.orbitPivotActive = true;
        state.camera.rotating = true;
    }

    if (event == GalaxyPointerGesture::Event::Clicked)
    {
        applyClick(view, context, viewport, state.pressTarget, gesture.pressX(), gesture.pressY(),
                   gesture.pressTimeSeconds(), result);
    }

    if (!frame.leftDown)
    {
        state.camera.rotating = false;
        state.orbitPivotActive = false;
    }

    // ---- rotate / pan -------------------------------------------------------
    if (state.camera.rotating && frame.leftDown && event != GalaxyPointerGesture::Event::DragStarted &&
        state.orbitPivotActive)
    {
        const float yawStep = std::clamp(-static_cast<float>(dx) * controls.rotateSensitivity,
                                         -controls.rotationMaxStepRad, controls.rotationMaxStepRad);
        const float pitchStep = std::clamp(static_cast<float>(dy) * controls.rotateSensitivity,
                                           -controls.rotationMaxStepRad, controls.rotationMaxStepRad);
        view.orbitCameraAroundPivot(state.orbitPivotAbsolute, yawStep, pitchStep, controls.pitchLimitRad);
    }

    const bool rightStarted = frame.inside && frame.rightDown && !state.camera.rightWasDown;
    if (rightStarted)
        state.camera.panning = true;
    if (!frame.rightDown)
        state.camera.panning = false;
    if (state.camera.panning && frame.rightDown && !rightStarted && (dx != 0.0 || dy != 0.0))
    {
        // Panning leaves the followed object.
        view.panCameraByScreenDelta(viewport, dx, dy);
        view.clearFollow();
    }

    // ---- wheel: zoom about the point under the cursor, levels by size -------
    if (frame.inside)
    {
        float zoom = 0.0f;
        if (pendingScrollY != 0.0)
        {
            zoom += pendingScrollY > 0.0 ? 1.0f : -1.0f;
            pendingScrollY = 0.0;
        }
        if (frame.zoomInKeyDown)
            zoom += 1.0f;
        if (frame.zoomOutKeyDown)
            zoom -= 1.0f;

        if (zoom != 0.0f)
        {
            // Pivot: the body / hub under or near the cursor (surface
            // clearance; a body is then followed), else the point under the
            // cursor (hover layer, else the target plane). Zoom in and out
            // keep that point where it is on the screen.
            glm::dvec3 pivot = view.targetPlanePointFromScreen(viewport, frame.localMouseX, frame.localMouseY);
            double clearance = 0.0;
            const auto& hover = state.hoverTarget;
            const auto cameraBody =
                context.pickSystemCameraBodyTarget(frame.localMouseX, frame.localMouseY, viewport);
            if (cameraBody && state.hoverTargetValid && hover.kind == SystemPointerTarget::Kind::Body)
            {
                pivot = cameraBody->absolutePosition;
                clearance = cameraBody->physicalRadiusWorld * static_cast<double>(controls.bodyZoomClearanceScale);
                view.followBody(hover.bodyId);
            }
            else if (const auto object = context.nearestSystemObject(frame.localMouseX, frame.localMouseY,
                                                                     controls.pivotPickRadiusPx))
            {
                pivot = object->absolutePosition;
                if (!object->bodyId.empty())
                    view.followBody(object->bodyId);
            }
            else if (state.hoverTargetValid && hover.kind == SystemPointerTarget::Kind::Cube && hover.onLayer)
            {
                pivot = hover.pointAu * scaleOf(view);
            }

            float minimumHalfHeight = SystemMapView::minimumCameraHalfHeight;
            float maximumHalfHeight = SystemMapView::maximumCameraHalfHeight;
            view.hierarchyHalfHeightLimits(viewport, minimumHalfHeight, maximumHalfHeight);

            const float factor =
                zoom > 0.0f ? std::pow(controls.zoomInFactor, zoom) : std::pow(controls.zoomOutFactor, -zoom);
            view.zoomCameraAroundPivot(pivot, factor, minimumHalfHeight, maximumHalfHeight,
                                       zoom > 0.0f ? clearance : 0.0);
            // Only the S0 root, zoomed out to its limit, centres itself so
            // that it fills the whole screen.
            if (zoom < 0.0f && view.hierarchyBox().parent.level <= 0 &&
                state.camera.distance >= maximumHalfHeight * 0.7f)
            {
                view.clearFollow();
                state.camera.target =
                    glm::mix(state.camera.target, view.hierarchyBox().centerLy * scaleOf(view), 0.35);
            }

            applyWheelLevels(view, frame, zoom > 0.0f, result);
        }
    }

    // The S0 root never gets smaller than the screen.
    if (view.hierarchyBox().parent.level <= 0)
    {
        float minimumHalfHeight = 0.0f;
        float maximumHalfHeight = 0.0f;
        view.hierarchyHalfHeightLimits(viewport, minimumHalfHeight, maximumHalfHeight);
        state.camera.distance = std::min(state.camera.distance, maximumHalfHeight);
    }

    finishFrame();
    return result;
}

} // namespace game::system_map
