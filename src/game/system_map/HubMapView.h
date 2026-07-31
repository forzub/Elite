#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

#include "src/game/system_map/LocalMapControlSettings.h"
#include "src/game/system_map/LocalMapFrameData.h"
#include "src/game/system_map/MapCameraState.h"

namespace game::system_map
{

struct HubMapViewState
{
    DetailCameraState camera;
    glm::dvec3 orbitPivotLocalMeters {0.0};
    glm::dvec2 orbitPivotScreenPx {0.0};
};

class HubMapView
{
public:
    HubMapView()
    {
        m_controls.rotateSensitivityScale = 0.65;
        m_controls.zoomStep = 1.06;
        m_controls.minZoom = 0.15;
        m_controls.maxZoom = 8.0;
        m_controls.constrainPitch = true;
    }

    HubMapViewState& state() noexcept { return m_state; }
    const HubMapViewState& state() const noexcept { return m_state; }

    DetailCameraState& camera() noexcept { return m_state.camera; }
    const DetailCameraState& camera() const noexcept { return m_state.camera; }

    LocalMapControlSettings& controls() noexcept { return m_controls; }
    const LocalMapControlSettings& controls() const noexcept { return m_controls; }

    HubMapFrameData& frame() noexcept { return m_frame; }
    const HubMapFrameData& frame() const noexcept { return m_frame; }

    void reset()
    {
        m_state = HubMapViewState{};
        m_frame = HubMapFrameData{};
    }

    void beginScene()
    {
        reset();
        m_state.camera.yaw = 0.60;
        m_state.camera.pitch = 0.18;
        m_state.camera.zoom = 1.0;
    }

    glm::dvec2 project(
        const glm::dvec3& localMeters,
        double scale,
        const glm::dvec2& centerPx
    ) const
    {
        const auto& camera = m_state.camera;
        const double cy = std::cos(camera.yaw);
        const double sy = std::sin(camera.yaw);
        const double cp = std::cos(camera.pitch);
        const double sp = std::sin(camera.pitch);

        glm::dvec3 a;
        a.x = localMeters.x * cy - localMeters.z * sy;
        a.y = localMeters.y;
        a.z = localMeters.x * sy + localMeters.z * cy;

        glm::dvec3 b;
        b.x = a.x;
        b.y = a.y * cp - a.z * sp;
        b.z = a.y * sp + a.z * cp;

        const double finalScale = scale * camera.zoom;

        return {
            centerPx.x + camera.pan.x + b.x * finalScale,
            centerPx.y + camera.pan.y - b.y * finalScale
        };
    }

    glm::dvec3 unprojectCursorToLocal(
        const glm::dvec2& mousePx,
        double scale,
        const glm::dvec2& centerPx
    ) const
    {
        const auto& camera = m_state.camera;
        const double finalScale = scale * camera.zoom;
        if (std::abs(finalScale) < 0.000001)
            return glm::dvec3(0.0);

        const glm::dvec3 b(
            (mousePx.x - centerPx.x - camera.pan.x) / finalScale,
            -(mousePx.y - centerPx.y - camera.pan.y) / finalScale,
            0.0
        );

        const double cy = std::cos(camera.yaw);
        const double sy = std::sin(camera.yaw);
        const double cp = std::cos(camera.pitch);
        const double sp = std::sin(camera.pitch);

        glm::dvec3 a;
        a.x = b.x;
        a.y = b.y * cp + b.z * sp;
        a.z = -b.y * sp + b.z * cp;

        return {
            a.x * cy + a.z * sy,
            a.y,
            -a.x * sy + a.z * cy
        };
    }

    bool pickOrbitPivot(
        const glm::dvec2& mousePx,
        glm::dvec3& outPivotLocalMeters
    ) const
    {
        const HubMapPickable* best = nullptr;
        double bestScore = std::numeric_limits<double>::max();

        for (const auto& pickable : m_frame.pickables)
        {
            const double distance =
                glm::length(mousePx - pickable.screenCenterPx);
            const double pickRadius =
                std::clamp(
                    pickable.screenRadiusPx,
                    m_controls.pivotPickMinimumRadiusPx,
                    m_controls.pivotPickMaximumRadiusPx
                );

            if (distance >
                pickRadius + m_controls.pivotPickMarginPx)
                continue;

            const double score =
                distance -
                static_cast<double>(pickable.priority) *
                    m_controls.pivotPriorityBiasPx;

            if (score < bestScore)
            {
                bestScore = score;
                best = &pickable;
            }
        }

        if (!best)
            return false;

        outPivotLocalMeters = best->localCenterMeters;
        return true;
    }

private:
    HubMapViewState m_state;
    LocalMapControlSettings m_controls;
    HubMapFrameData m_frame;
};

} // namespace game::system_map
