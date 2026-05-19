#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "src/world/coordinates/WorldPosition.h"

namespace world::coordinates
{

struct WorldFrame
{
    WorldPosition origin;
};

inline glm::vec3 toLocal(
    const WorldPosition& worldPosition,
    const WorldFrame& frame
)
{
    return relativeMetersFloat(worldPosition, frame.origin);
}

// Временно для старых glm::vec3, которые ещё не переведены.
inline glm::vec3 toLocal(
    const glm::vec3& worldPosition,
    const WorldFrame& frame
)
{
    return worldPosition - glm::vec3(frame.origin.localMeters);
}


inline WorldFrame makeLegacyFrame(const glm::vec3& cameraPos)
{
    WorldFrame frame;
    frame.origin = makeWorldPositionFromMeters(glm::dvec3(cameraPos));
    return frame;
}





inline glm::mat4 makeRenderView(
    const glm::mat4& cameraView
)
{
    // ВАЖНО:
    // view пока оставляем с локальной трансляцией камеры.
    // Камера теперь хранит позицию относительно игрока.
    return cameraView;
}

} // namespace world::coordinates