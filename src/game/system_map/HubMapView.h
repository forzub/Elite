#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

#include "src/game/system_map/LocalMapControlSettings.h"
#include "src/game/system_map/LocalMapFrameData.h"
#include "src/game/system_map/MapCameraSnapshot.h"
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
        m_controls.maxZoom = 64.0;
        m_controls.constrainPitch = true;
    }

    HubMapViewState& state() noexcept { return m_state; }
    const HubMapViewState& state() const noexcept { return m_state; }

    DetailCameraState& camera() noexcept { return m_state.camera; }
    const DetailCameraState& camera() const noexcept { return m_state.camera; }

    LocalMapControlSettings& controls() noexcept { return m_controls; }
    const LocalMapControlSettings& controls() const noexcept { return m_controls; }

    void reset()
    {
        m_state = HubMapViewState{};
    }

    void beginScene()
    {
        reset();
        m_state.camera.yaw = 0.60;
        m_state.camera.pitch = 0.18;
        m_state.camera.zoom = 1.0;
    }

    HubMapCameraSnapshot cameraSnapshot(
        double scale,
        const glm::dvec2& centerPx
    ) const
    {
        HubMapCameraSnapshot snapshot;
        snapshot.state = m_state.camera;
        snapshot.scale = scale;
        snapshot.centerPx = centerPx;
        snapshot.originMeters = glm::dvec3(0.0);
        snapshot.perspectiveEnabled = false;
        snapshot.perspectiveCameraDistanceMeters = 1.0;
        return snapshot;
    }

    glm::dvec2 project(
        const glm::dvec3& localMeters,
        double scale,
        const glm::dvec2& centerPx
    ) const
    {
        return cameraSnapshot(
            scale,
            centerPx
        ).project(localMeters);
    }

    glm::dvec3 unprojectCursorToLocal(
        const glm::dvec2& mousePx,
        double scale,
        const glm::dvec2& centerPx
    ) const
    {
        return cameraSnapshot(
            scale,
            centerPx
        ).unprojectPlane(mousePx);
    }

    bool pickOrbitPivot(
        const HubMapFrameData& frame,
        const glm::dvec2& mousePx,
        glm::dvec3& outPivotLocalMeters
    ) const
    {
        const HubMapPickable* best = nullptr;
        double bestScore = std::numeric_limits<double>::max();

        for (const auto& pickable : frame.pickables)
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
};

} // namespace game::system_map
