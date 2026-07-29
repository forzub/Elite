#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

namespace game::system_map
{
    struct SystemMapBodyScreenPoint
    {
        std::string bodyId;
        std::string name;
        glm::vec2 screen {0.0f};
        float depth = 0.0f;
        bool visible = false;
        float screenRadiusPx = 0.0f;
    };

    struct SystemMapHubScreenPoint
    {
        std::string hubId;
        std::string parentBodyId;
        std::string name;
        glm::vec2 screen {0.0f};
        float depth = 0.0f;
        bool visible = false;
        float screenRadiusPx = 12.0f;
    };

    /*
        Presentation-frame data shared by System rendering and interaction.

        It is rebuilt by SystemMapSceneRenderer and consumed by the facade's
        SystemMapInteractionContext implementation. No OpenGL resource or
        persistent world state is stored here.
    */
    struct SystemMapFrameData
    {
        std::vector<SystemMapBodyScreenPoint> bodyScreenPoints;
        std::vector<SystemMapHubScreenPoint> hubScreenPoints;

        std::unordered_map<std::string, glm::dvec3>
            bodyAbsolutePositionById;

        std::unordered_map<std::string, glm::dvec3>
            objectAbsolutePositionById;

        void clearPresentation()
        {
            bodyScreenPoints.clear();
            hubScreenPoints.clear();
            bodyAbsolutePositionById.clear();
            objectAbsolutePositionById.clear();
        }
    };
}
