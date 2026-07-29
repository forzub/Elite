/* Included from SystemMapRenderer.cpp during phase 4 extraction. */

#include <algorithm>
#include <cmath>

#include <glm/gtc/constants.hpp>

#include "src/game/system_map/DetailMapView.h"
#include "src/game/system_map/HubMapView.h"
#include "src/game/system_map/LocalMapInteraction.h"
#include "src/game/system_map/SystemMapView.h"

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
}

void LocalMapInteraction::handle(
    MapMode mode,
    DetailMapView& detailView,
    HubMapView& hubView,
    SystemMapView& systemView,
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
            const glm::dvec2 centerPx(
                static_cast<double>(viewport.width) * 0.5,
                static_cast<double>(viewport.height) * 0.5
            );
            const glm::dvec2 mousePx(localMouseX, localMouseY);
            const double safeScale =
                std::max(0.000001, hubView.frame().scale);

            glm::dvec3 pivot(0.0);
            if (!hubView.pickOrbitPivot(mousePx, pivot))
            {
                pivot = hubView.unprojectCursorToLocal(
                    mousePx,
                    safeScale,
                    centerPx
                );
            }

            hubView.state().orbitPivotLocalMeters = pivot;
            hubView.state().orbitPivotScreenPx =
                hubView.project(pivot, safeScale, centerPx);
        }
    }

    if (!leftDown && camera.rotating)
    {
        const double movement =
            std::abs(mouseX - camera.mouseDownX) +
            std::abs(mouseY - camera.mouseDownY);

        if (inside && mode == MapMode::Detail && movement <= 8.0)
        {
            const int picked =
                detailView.pickHub(localMouseX, localMouseY);

            auto& state = systemView.state();
            if (picked >= 0 &&
                picked < static_cast<int>(
                    detailView.frame().hubScreenPoints.size()
                ))
            {
                const auto& point =
                    detailView.frame().hubScreenPoints[picked];
                state.selectedBodyId.clear();
                state.selectedHubId = point.hubId;
                state.selectedHubParentBodyId = point.parentBodyId;
            }
            else
            {
                state.selectedHubId.clear();
                state.selectedHubParentBodyId.clear();
            }
        }

        camera.rotating = false;
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
            controls.rotateSensitivity * (hubMode ? 0.65 : 1.0);

        camera.yaw = wrapLocalMapAngle(camera.yaw + dx * sensitivity);
        camera.pitch += dy * sensitivity;

        if (hubMode)
            camera.pitch = std::clamp(camera.pitch, 0.12, 1.20);
        else
            camera.pitch = wrapLocalMapAngle(camera.pitch);
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
        camera.pitch = std::clamp(camera.pitch, 0.12, 1.20);
        camera.pan.x = std::clamp(
            camera.pan.x,
            -static_cast<double>(viewport.width) * 0.55,
            static_cast<double>(viewport.width) * 0.55
        );
        camera.pan.y = std::clamp(
            camera.pan.y,
            -static_cast<double>(viewport.height) * 0.45,
            static_cast<double>(viewport.height) * 0.45
        );
    }

    camera.lastMouseX = mouseX;
    camera.lastMouseY = mouseY;
}

} // namespace game::system_map
