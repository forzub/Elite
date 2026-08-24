/* Standalone translation unit for the local-map subsystem. */

#include <algorithm>
#include <cmath>

#include <glm/gtc/constants.hpp>

#include "src/game/system_map/DetailMapView.h"
#include "src/game/system_map/HubMapView.h"
#include "src/game/system_map/LocalMapInteraction.h"

namespace game::system_map
{
namespace
{
double wrapLocalMapAngle(double angle)
{
    const double twoPi = glm::two_pi<double>();
    angle = std::fmod(angle, twoPi);
    if (angle > glm::pi<double>())
        angle -= twoPi;
    else if (angle < -glm::pi<double>())
        angle += twoPi;
    return angle;
}

int pickDetailHub(
    const DetailMapFrameData& frame,
    double mouseX,
    double mouseY
)
{
    int bestIndex = -1;
    float bestDistance = 1.0e30f;
    const glm::vec2 mouse(
        static_cast<float>(mouseX),
        static_cast<float>(mouseY)
    );

    for (int i = 0;
         i < static_cast<int>(frame.hubScreenPoints.size());
         ++i)
    {
        const auto& point = frame.hubScreenPoints[i];
        if (!point.visible)
            continue;

        const float distance =
            glm::length(point.screen - mouse);

        if (distance <= point.screenRadiusPx &&
            distance < bestDistance)
        {
            bestDistance = distance;
            bestIndex = i;
        }
    }

    return bestIndex;
}
}

LocalMapInteractionResult LocalMapInteraction::handle(
    MapMode mode,
    DetailMapView& detailView,
    HubMapView& hubView,
    const DetailMapFrameData& detailFrame,
    const HubMapFrameData& hubFrame,
    const Viewport& viewport,
    GLFWwindow*,
    double mouseX,
    double mouseY,
    double localMouseX,
    double localMouseY,
    bool inside,
    bool leftDown,
    bool rightDown,
    double& pendingScrollY
) const
{
    LocalMapInteractionResult result;

    const bool hubMode = mode == MapMode::Hub;
    DetailCameraState& camera =
        hubMode ? hubView.camera() : detailView.camera();
    const LocalMapControlSettings& controls =
        hubMode ? hubView.controls() : detailView.controls();

    double wheel = 0.0;
    if (inside && pendingScrollY != 0.0)
    {
        wheel = pendingScrollY;
        pendingScrollY = 0.0;
    }

    bool leftStartedThisFrame = false;
    bool rightStartedThisFrame = false;

    if (inside && leftDown && !camera.rotating)
    {
        leftStartedThisFrame = true;
        camera.rotating = true;
        camera.lastMouseX = mouseX;
        camera.lastMouseY = mouseY;
        camera.mouseDownX = mouseX;
        camera.mouseDownY = mouseY;

        if (hubMode)
        {
            const glm::dvec2 centerPx = hubFrame.centerPx;
            const glm::dvec2 mousePx(localMouseX, localMouseY);
            const double safeScale =
                std::max(0.000001, hubFrame.scale);

            glm::dvec3 pivot(0.0);
            if (!hubView.pickOrbitPivot(hubFrame, mousePx, pivot))
            {
                pivot = hubView.unprojectCursorToLocal(
                    mousePx,
                    safeScale,
                    centerPx
                );
            }

            hubView.captureOrbitPivot(
                pivot,
                safeScale,
                centerPx
            );
        }
    }

    if (!leftDown && camera.rotating)
    {
        const double movement =
            std::abs(mouseX - camera.mouseDownX) +
            std::abs(mouseY - camera.mouseDownY);

        if (inside &&
            mode == MapMode::Detail &&
            movement <= controls.clickMoveThresholdPx)
        {
            const int picked =
                pickDetailHub(
                    detailFrame,
                    localMouseX,
                    localMouseY
                );

            if (picked >= 0 &&
                picked < static_cast<int>(
                    detailFrame.hubScreenPoints.size()
                ))
            {
                const auto& point =
                    detailFrame.hubScreenPoints[picked];

                result.selectionAction =
                    LocalMapInteractionResult::SelectionAction::SelectHub;
                result.hubId = point.hubId;
                result.parentBodyId = point.parentBodyId;
            }
            else
            {
                result.selectionAction =
                    LocalMapInteractionResult::SelectionAction::ClearHub;
            }
        }

        camera.rotating = false;
        if (hubMode)
            hubView.clearOrbitPivot();
    }

    if (inside &&
        rightDown &&
        !camera.panning &&
        !(mode == MapMode::Detail &&
          detailView.state().sceneIsSpatialVolume))
    {
        rightStartedThisFrame = true;
        camera.panning = true;
        camera.lastMouseX = mouseX;
        camera.lastMouseY = mouseY;
    }

    if (!rightDown)
        camera.panning = false;

    const double dx = mouseX - camera.lastMouseX;
    const double dy = mouseY - camera.lastMouseY;

    if (camera.rotating && leftDown && !leftStartedThisFrame)
    {
        const double sensitivity =
            controls.rotateSensitivity *
            controls.rotateSensitivityScale;

        camera.yaw = wrapLocalMapAngle(camera.yaw + dx * sensitivity);
        camera.pitch += dy * sensitivity;

        if (controls.constrainPitch)
        {
            camera.pitch =
                std::clamp(
                    camera.pitch,
                    controls.minimumPitchRad,
                    controls.maximumPitchRad
                );
        }
        else
        {
            camera.pitch =
                wrapLocalMapAngle(
                    camera.pitch
                );
        }

        if (hubMode)
        {
            hubView.stabilizeCapturedOrbitPivot(
                std::max(0.000001, hubFrame.scale),
                hubFrame.centerPx
            );
        }
    }

    if (camera.panning && rightDown && !rightStartedThisFrame)
    {
        camera.pan.x += dx;
        camera.pan.y += dy;
    }

    if (std::abs(wheel) > 0.001)
    {
        const double oldZoom = camera.zoom;
        const double minimumZoom =
            mode == MapMode::Detail
                ? std::max(
                    controls.minZoom,
                    detailView.state().minimumZoom
                )
                : controls.minZoom;

        const double newZoom =
            std::clamp(
                oldZoom * std::pow(controls.zoomStep, wheel),
                minimumZoom,
                controls.maxZoom
            );

        if (std::abs(newZoom - oldZoom) > 0.000001)
        {
            const glm::dvec2 centerPx(
                static_cast<double>(viewport.width) * 0.5,
                static_cast<double>(viewport.height) * 0.5
            );
            const glm::dvec2 mousePx(localMouseX, localMouseY);
            const double zoomFactor = newZoom / oldZoom;

            camera.pan =
                mousePx - centerPx -
                (mousePx - centerPx - camera.pan) * zoomFactor;
            camera.zoom = newZoom;
        }
    }

    if (mode == MapMode::Detail &&
        detailView.state().sceneIsSpatialVolume)
    {
        camera.pan = glm::dvec2(0.0);
        camera.panning = false;
        camera.zoom = std::max(
            camera.zoom,
            detailView.state().minimumZoom
        );
    }

    if (hubMode)
    {
        camera.pitch =
            std::clamp(
                camera.pitch,
                controls.minimumPitchRad,
                controls.maximumPitchRad
            );

        // Hub zoom may now approach individual craft. Pan allowance grows
        // with zoom so a ship does not become unreachable merely because the
        // camera is magnifying a small portion of the station neighbourhood.
        const double panZoomAllowance =
            std::max(1.0, camera.zoom * 2.0);

        const double panLimitX =
            static_cast<double>(viewport.width) *
            controls.panLimitViewportFractionX *
            panZoomAllowance;

        const double panLimitY =
            static_cast<double>(viewport.height) *
            controls.panLimitViewportFractionY *
            panZoomAllowance;

        camera.pan.x =
            std::clamp(
                camera.pan.x,
                -panLimitX,
                panLimitX
            );

        camera.pan.y =
            std::clamp(
                camera.pan.y,
                -panLimitY,
                panLimitY
            );
    }

    camera.lastMouseX = mouseX;
    camera.lastMouseY = mouseY;

    return result;
}

} // namespace game::system_map
