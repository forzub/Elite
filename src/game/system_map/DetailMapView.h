#pragma once

#include <string>
#include <utility>

#include "src/game/system_map/LocalMapControlSettings.h"
#include "src/game/system_map/MapCameraSnapshot.h"
#include "src/game/system_map/MapCameraState.h"

namespace game::system_map
{

struct DetailMapViewState
{
    DetailCameraState camera;
    bool sceneIsSpatialVolume = false;
    double minimumZoom = 0.15;
    std::string selectedHubId;
    std::string selectedHubParentBodyId;
};

class DetailMapView
{
public:
    DetailMapViewState& state() noexcept { return m_state; }
    const DetailMapViewState& state() const noexcept { return m_state; }

    DetailCameraState& camera() noexcept { return m_state.camera; }
    const DetailCameraState& camera() const noexcept { return m_state.camera; }

    LocalMapControlSettings& controls() noexcept { return m_controls; }
    const LocalMapControlSettings& controls() const noexcept { return m_controls; }

    void reset()
    {
        m_state = DetailMapViewState{};
        m_state.minimumZoom = m_controls.minZoom;
    }


    DetailMapCameraSnapshot cameraSnapshot(
        double scale,
        const glm::dvec2& centerPx,
        const glm::dvec3& originMeters,
        bool perspectiveEnabled,
        double perspectiveCameraDistanceMeters
    ) const
    {
        DetailMapCameraSnapshot snapshot;
        snapshot.state = m_state.camera;
        snapshot.scale = scale;
        snapshot.centerPx = centerPx;
        snapshot.originMeters = originMeters;
        snapshot.perspectiveEnabled = perspectiveEnabled;
        snapshot.perspectiveCameraDistanceMeters =
            perspectiveCameraDistanceMeters;
        return snapshot;
    }

    void selectHub(
        std::string hubId,
        std::string parentBodyId
    )
    {
        m_state.selectedHubId = std::move(hubId);
        m_state.selectedHubParentBodyId =
            std::move(parentBodyId);
    }

    void clearHubSelection()
    {
        m_state.selectedHubId.clear();
        m_state.selectedHubParentBodyId.clear();
    }

private:
    DetailMapViewState m_state;
    LocalMapControlSettings m_controls;
};

} // namespace game::system_map
