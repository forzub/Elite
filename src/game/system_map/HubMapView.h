#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

#include "src/game/system_map/LocalMapControlSettings.h"
#include "src/game/system_map/LocalMapFrameData.h"
#include "src/game/system_map/MapCameraSnapshot.h"
#include "src/game/system_map/MapCameraState.h"
#include "src/game/system_map/ScreenHitTest.h"

namespace game::system_map
{

struct HubMapViewState
{
    DetailCameraState camera;
    glm::dvec3 orbitPivotLocalMeters {0.0};
    glm::dvec2 orbitPivotScreenPx {0.0};
    bool orbitPivotActive = false;
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

    void captureOrbitPivot(
        const glm::dvec3& localPivotMeters,
        double scale,
        const glm::dvec2& centerPx
    )
    {
        m_state.orbitPivotLocalMeters = localPivotMeters;
        m_state.orbitPivotScreenPx = project(
            localPivotMeters,
            scale,
            centerPx
        );
        m_state.orbitPivotActive = true;
    }

    void stabilizeCapturedOrbitPivot(
        double scale,
        const glm::dvec2& centerPx
    )
    {
        if (!m_state.orbitPivotActive)
            return;

        const glm::dvec2 projectedAfter = project(
            m_state.orbitPivotLocalMeters,
            scale,
            centerPx
        );
        const glm::dvec2 correction =
            m_state.orbitPivotScreenPx - projectedAfter;
        if (std::isfinite(correction.x) && std::isfinite(correction.y))
            m_state.camera.pan += correction;
    }

    void clearOrbitPivot() noexcept
    {
        m_state.orbitPivotActive = false;
    }

    bool pickOrbitPivot(
        const HubMapFrameData& frame,
        const glm::dvec2& mousePx,
        glm::dvec3& outPivotLocalMeters
    ) const
    {
        const HubMapPickable* best = nullptr;
        bool bestDirectHit = false;
        double bestProximity = std::numeric_limits<double>::max();
        double bestCenterDistance = std::numeric_limits<double>::max();
        int bestPriority = std::numeric_limits<int>::min();

        for (const auto& pickable : frame.pickables)
        {
            const double centerDistance =
                glm::length(mousePx - pickable.screenCenterPx);

            bool directHit = false;
            double proximity = 0.0;
            if (!pickable.hitPolygonPx.empty())
            {
                proximity = screenDistanceToConvexPolygon(
                    mousePx,
                    pickable.hitPolygonPx
                );
                directHit = proximity <= 1.0e-6;
            }
            else
            {
                const double pickRadius = std::clamp(
                    pickable.screenRadiusPx,
                    m_controls.pivotPickMinimumRadiusPx,
                    m_controls.pivotPickMaximumRadiusPx
                );
                directHit = centerDistance <= pickRadius;
                proximity = std::max(0.0, centerDistance - pickRadius);
            }

            if (!directHit && proximity > m_controls.pivotPickMarginPx)
                continue;

            const bool betterDirectness = directHit && !bestDirectHit;
            const bool sameDirectness = directHit == bestDirectHit;
            const bool nearer =
                sameDirectness && proximity < bestProximity - 1.0e-6;
            const bool sameProximity =
                sameDirectness &&
                std::abs(proximity - bestProximity) <= 1.0e-6;
            const bool higherPriority =
                sameProximity && pickable.priority > bestPriority;
            const bool samePriority =
                sameProximity && pickable.priority == bestPriority;
            const bool nearerCenter =
                samePriority &&
                centerDistance < bestCenterDistance - 1.0e-6;

            if (!best || betterDirectness || nearer || higherPriority ||
                nearerCenter)
            {
                best = &pickable;
                bestDirectHit = directHit;
                bestProximity = proximity;
                bestCenterDistance = centerDistance;
                bestPriority = pickable.priority;
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
