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
inline glm::vec3 toLocal(const glm::vec3& worldPosition, const WorldFrame& frame)
{
    // Считаем полную позицию Origin в метрах как double
    glm::dvec3 originFullMeters(
        static_cast<double>(frame.origin.cell.x) * GalacticCellSizeM,
        static_cast<double>(frame.origin.cell.y) * GalacticCellSizeM,
        static_cast<double>(frame.origin.cell.z) * GalacticCellSizeM
    );
    originFullMeters += frame.origin.localMeters;
    
    // Вычитаем из worldPosition (который в float) полные метры Origin
    return glm::vec3(glm::dvec3(worldPosition) - originFullMeters);
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