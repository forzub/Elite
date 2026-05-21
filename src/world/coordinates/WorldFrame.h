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

inline glm::mat4 makeRenderView(
    const glm::mat4& cameraView
)
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