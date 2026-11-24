#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

#include "src/game/system_map/LocalMapControlSettings.h"
#include "src/game/system_map/LocalMapFrameData.h"
#include "src/game/system_map/MapCameraSnapshot.h"
#include "src/game/system_map/MapCameraState.h"
#include "src/game/system_map/ScreenHitTest.h"
#include "src/game/navigation/HubPlanCatalog.h"
#include <string>

namespace game::system_map
{


// The hub's 2D plan shows the city only (author 2026-11-21: «на 2Д схеме хаба не нужна отдельно висящая станция и
// отдельные модули хаба, которые не относятся к самому городу»): the planar city frame (a HubPlan) and the modules
// generated with it ("<hub>.cty..."). A hub without a city keeps every module on its plan.
inline bool isHubCityModule(const std::string& stableId)
{
    return !stableId.empty() &&
        (game::navigation::HubPlanCatalog::forFrameModule(stableId) != nullptr || stableId.find(".cty") != std::string::npos);
}
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
    void applyPlanCamera() noexcept
    {
        m_state.camera.yaw = 0.0;
        m_state.camera.pitch = PlanPitchRad;
    }
    const HubMapViewState& state() const noexcept { return m_state; }

    DetailCameraState& camera() noexcept { return m_state.camera; }
    const DetailCameraState& camera() const noexcept { return m_state.camera; }
    // History restore: the view owns its camera state; a restored camera never
    // resumes an interrupted rotate / pan gesture.
    void restoreCamera(const DetailCameraState& camera) noexcept
    {
        m_state.camera = camera;
        m_state.camera.rotating = false;
        m_state.camera.panning = false;
    }

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
        if (m_planView)
            applyPlanCamera();
    }

    // The 2D plan of the hub (author 2026-11-18: «2Д как сейчас в мешэдиторе ... для указания дока»; V in the Hub
    // map): the camera looks straight down the hub's +Y (yaw 0, pitch 90 deg: +X right, +Z down the screen, as in
    // the mesh editor's hub mode); no orbiting - a left drag pans. Off: the 3D view comes back as it was.
    bool planView() const noexcept { return m_planView; }
    void setPlanCentre(const glm::dvec3& centre) const noexcept { m_planCentre = centre; }
    void setPlanWarp(const LocalMapPlanWarp& warp) const noexcept { m_planWarp = warp; }
    const LocalMapPlanWarp& planWarp() const noexcept { return m_planWarp; }
    void setPlanView(bool on) noexcept
    {
        if (on == m_planView)
            return;
        m_planView = on;
        if (on)
        {
            m_savedYaw = m_state.camera.yaw;
            m_savedPitch = m_state.camera.pitch;
            m_savedZoom = m_state.camera.zoom;
            m_savedPan = m_state.camera.pan;
            applyPlanCamera();
            // the plan opens on the city, readable (the 3D view frames the hub with its surroundings)
            m_state.camera.zoom = 1.0;   // the plan fits the city (LocalMapPresentationBuilder::buildHub)
            m_state.camera.pan = glm::dvec2(0.0);
        }
        else
        {
            m_state.camera.yaw = m_savedYaw;
            m_state.camera.pitch = m_savedPitch;
            m_state.camera.zoom = m_savedZoom;
            m_state.camera.pan = m_savedPan;
        }
        m_state.orbitPivotActive = false;
    }
    // the camera pitch the plan view uses (the Hub map backend draws the plan instead of the 3D boxes at it)
    static constexpr double PlanPitchRad = 1.5707963267948966;

    HubMapCameraSnapshot cameraSnapshot(
        double scale,
        const glm::dvec2& centerPx
    ) const
    {
        HubMapCameraSnapshot snapshot;
        snapshot.state = m_state.camera;
        snapshot.scale = scale;
        snapshot.centerPx = centerPx;
        snapshot.originMeters = m_planView ? m_planCentre : glm::dvec3(0.0);   // the plan centres on the city
        if (m_planView)
            snapshot.planWarp = m_planWarp;
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
    bool m_planView = false;
    mutable glm::dvec3 m_planCentre {0.0};
    mutable LocalMapPlanWarp m_planWarp;   // per frame from the presentation builder (a bent city's plan)   // per frame from the presentation builder (the city centre)
    double m_savedYaw = 0.60;
    double m_savedPitch = 0.18;
    double m_savedZoom = 1.0;
    glm::dvec2 m_savedPan {0.0};
};

} // namespace game::system_map
