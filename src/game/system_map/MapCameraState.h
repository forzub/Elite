#pragma once

#include <glm/glm.hpp>

namespace game::system_map
{
    struct GalaxyCameraState
    {
        float yaw = 0.65f;
        float pitch = 0.72f;
        float distance = 82.0f;
        glm::vec3 target {0.0f, 0.0f, 0.0f};

        bool rotating = false;
        bool panning = false;
        bool leftWasDown = false;
        bool rightWasDown = false;

        double lastMouseX = 0.0;
        double lastMouseY = 0.0;

        float lastWheelY = 0.0f;
    };

    struct SystemCameraFlightState
    {
        bool active = false;

        glm::dvec3 startTarget {0.0};
        glm::dvec3 destinationTarget {0.0};

        float startDistance = 95.0f;
        float destinationDistance = 95.0f;

        double startTimeSeconds = 0.0;
        double durationSeconds = 0.58;
    };

    struct SystemCameraState
    {
        float yaw = 0.45f;
        float pitch = 0.85f;
        float distance = 95.0f;
        glm::dvec3 target {0.0, 0.0, 0.0};

        bool rotating = false;
        bool panning = false;

        bool leftWasDown = false;
        bool rightWasDown = false;

        double lastMouseX = 0.0;
        double lastMouseY = 0.0;

        double mouseDownX = 0.0;
        double mouseDownY = 0.0;
    };

    struct DetailCameraState
    {
        double yaw = 0.6;
        double pitch = 0.35;
        double zoom = 1.0;
        glm::dvec2 pan {0.0};

        bool rotating = false;
        bool panning = false;

        double lastMouseX = 0.0;
        double lastMouseY = 0.0;
        double mouseDownX = 0.0;
        double mouseDownY = 0.0;
    };
}
