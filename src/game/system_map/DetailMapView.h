#pragma once

#include <algorithm>
#include <cmath>

#include "src/game/system_map/LocalMapControlSettings.h"
#include "src/game/system_map/LocalMapFrameData.h"
#include "src/game/system_map/MapCameraState.h"
#include "src/world/celestial/SystemMapTypes.h"

namespace game::system_map
{

struct DetailMapViewState
{
    DetailCameraState camera;
    bool sceneIsSpatialVolume = false;
    double minimumZoom = 0.15;
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

    DetailMapFrameData& frame() noexcept { return m_frame; }
    const DetailMapFrameData& frame() const noexcept { return m_frame; }

    void reset()
    {
        m_state = DetailMapViewState{};
        m_state.minimumZoom = m_controls.minZoom;
        m_frame.hubScreenPoints.clear();
    }

    void beginScene(const world::celestial::DetailMapSnapshot& snapshot)
    {
        m_state.sceneIsSpatialVolume =
            snapshot.valid &&
            snapshot.detailTarget.sceneKind ==
                world::celestial::DetailSceneKind::SpatialVolume;

        m_state.minimumZoom =
            m_state.sceneIsSpatialVolume
                ? m_controls.spatialVolumeMinimumZoom
                : m_controls.minZoom;

        m_state.camera.zoom =
            std::max(
                m_state.camera.zoom,
                m_state.minimumZoom
            );

        if (m_state.sceneIsSpatialVolume)
            m_state.camera.pan = glm::dvec2(0.0);
    }

    int pickHub(double mouseX, double mouseY) const
    {
        int bestIndex = -1;
        float bestDistance = 1.0e30f;
        const glm::vec2 mouse(
            static_cast<float>(mouseX),
            static_cast<float>(mouseY)
        );

        for (int i = 0;
             i < static_cast<int>(m_frame.hubScreenPoints.size());
             ++i)
        {
            const auto& point = m_frame.hubScreenPoints[i];
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

private:
    DetailMapViewState m_state;
    LocalMapControlSettings m_controls;
    DetailMapFrameData m_frame;
};

} // namespace game::system_map
