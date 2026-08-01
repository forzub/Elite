#pragma once

#include <string>
#include <unordered_map>

#include <glm/glm.hpp>

#include "src/game/system_map/SystemMapFrameData.h"
#include "src/game/system_map/SystemMapRenderContext.h"
#include "src/render/types/Viewport.h"

namespace game::system_map
{
    /*
        CPU presentation shared by System input/picking and rendering.

        It contains no OpenGL resources and is rebuilt whenever the
        authoritative snapshot, viewport or camera projection changes.
    */
    struct SystemMapSceneFrame
    {
        bool valid = false;
        int systemId = -1;
        Viewport viewport;

        float systemScale = 1.0f;
        double worldUnitsPerPixel = 1.0;
        glm::dvec3 cameraOrigin {0.0};

        glm::mat4 projection {1.0f};
        glm::mat4 view {1.0f};
        glm::mat4 mvp {1.0f};

        std::unordered_map<std::string, glm::vec3>
            bodyVisualPositionById;

        std::unordered_map<std::string, float>
            bodyVisualRadiusById;

        std::unordered_map<std::string, SystemBodyVisualMetrics>
            bodyVisualMetricsById;

        std::unordered_map<std::string, float>
            bodySelectionRadiusById;

        std::unordered_map<std::string, glm::vec3>
            objectVisualPositionById;

        SystemMapFrameData interaction;
    };
}
