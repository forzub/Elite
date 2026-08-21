#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/system_map/MapObjectOverlay.h"

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
        double physicalSizeMeters = 0.0;
    };

    struct SystemMapOrbitPivotScreenPoint
    {
        std::string bodyId;
        glm::vec2 screen {0.0f};
        float depth = 0.0f;
        double cameraDepthWorld = 0.0;
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
        double physicalSizeMeters = 0.0;
    };

    /*
        Presentation-frame data shared by System rendering and interaction.

        It is rebuilt by SystemMapSceneFrameBuilder before input and consumed
        by SystemMapFrameInteractionContext. The scene renderer receives the
        same prepared CPU frame. No OpenGL resource or persistent world state
        is stored here.
    */
    struct SystemMapFrameData
    {
        std::vector<SystemMapBodyScreenPoint> bodyScreenPoints;
        std::vector<SystemMapOrbitPivotScreenPoint> orbitPivotScreenPoints;
        std::vector<SystemMapHubScreenPoint> hubScreenPoints;
        MapObjectOverlayFrame objectOverlay;

        std::unordered_map<std::string, glm::dvec3>
            bodyAbsolutePositionById;

        std::unordered_map<std::string, float>
            bodyPhysicalRadiusWorldById;

        std::unordered_map<std::string, glm::dvec3>
            objectAbsolutePositionById;

        void clearPresentation()
        {
            bodyScreenPoints.clear();
            orbitPivotScreenPoints.clear();
            hubScreenPoints.clear();
            objectOverlay.items.clear();
            objectOverlay.trajectories.clear();
            bodyAbsolutePositionById.clear();
            bodyPhysicalRadiusWorldById.clear();
            objectAbsolutePositionById.clear();
        }
    };
}
