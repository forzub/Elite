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



inline WorldFrame makeWorldFrameFromOrigin(
    const WorldPosition& origin
)
{
    WorldFrame frame;
    frame.origin = origin;
    return frame;
}

inline WorldFrame makeRenderFrameFromCamera(
    const WorldPosition& cameraWorldPosition
)
{
    return makeWorldFrameFromOrigin(cameraWorldPosition);
}





// Единственный правильный путь:
// WorldPosition -> render-local float.
inline glm::vec3 toRenderLocal(
    const WorldPosition& worldPosition,
    const WorldFrame& frame
)
{
    return relativeMetersFloat(worldPosition, frame.origin);
}

// Совместимость для уже написанного кода.
// Позже заменим вызовы toLocal(...) на toRenderLocal(...).
inline glm::vec3 toLocal(
    const WorldPosition& worldPosition,
    const WorldFrame& frame
)
{
    return toRenderLocal(worldPosition, frame);
}

// ВАЖНО:
// glm::vec3 НЕ является глобальной мировой координатой.
// Поэтому перегрузку toLocal(glm::vec3, WorldFrame) не вводим.
// Иначе снова получим старый яд: owner-local смешается с world-local.

inline glm::mat4 makeRenderView(const glm::mat4& cameraView)
{
    return cameraView;
}





inline glm::mat4 makeRenderModelMatrix(
    const WorldPosition& worldPosition,
    const glm::mat4& orientation,
    const WorldFrame& frame
)
{
    return glm::translate(
        glm::mat4(1.0f),
        toRenderLocal(worldPosition, frame)
    ) * orientation;
}






} // namespace world::coordinates