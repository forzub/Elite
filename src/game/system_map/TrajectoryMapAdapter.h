#pragma once

#include <string>

#include "src/game/navigation/TrajectoryPredictor.h"
#include "src/game/system_map/MapObjectOverlay.h"

namespace game::system_map
{

/*
    Thin presentation adapter. The predictor stays renderer-independent while
    System/Hub map producers can turn the shared prediction product into the
    existing trajectory seam without discarding server-useful velocity/load
    data from the original result.
*/
inline MapObjectTrajectory makeMapObjectTrajectory(
    const std::string& objectId,
    MapTrajectoryKind kind,
    const game::navigation::TrajectoryPredictionResult& prediction
)
{
    MapObjectTrajectory trajectory;
    trajectory.objectId = objectId;
    trajectory.kind = kind;

    if (!prediction.ok())
        return trajectory;

    trajectory.points.reserve(prediction.samples.size());
    for (const auto& sample : prediction.samples)
    {
        trajectory.points.push_back({
            sample.universeTimeSeconds,
            sample.state.positionMeters
        });
    }

    return trajectory;
}

} // namespace game::system_map
