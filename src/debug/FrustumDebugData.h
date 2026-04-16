#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

struct DebugShipInfo
{
    uint64_t id = 0;
    glm::vec3 position {0.0f, 0.0f, 0.0f};
    glm::vec3 forward {0.0f, 0.0f, -1.0f};
    glm::vec3 up      {0.0f, 1.0f, 0.0f};
    glm::vec3 right   {1.0f, 0.0f, 0.0f};
    float radius = 0.0f;
    bool visible = false;
    float distance = 0.0f;
    std::string type;
};

struct DebugCameraInfo
{
    glm::vec3 position {0.0f, 0.0f, 0.0f};
    glm::vec3 direction {0.0f, 0.0f, -1.0f};
    glm::vec3 up {0.0f, 1.0f, 0.0f};
    float fov = 45.0f;
    float aspect = 1.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
    int cameraId = 0;
    std::string cameraName;
    glm::vec3 frustumCorners[8];
};

struct DebugObjectInfo
{
    uint32_t id = 0;
    glm::vec3 position {0.0f, 0.0f, 0.0f};
    glm::vec3 forward {0.0f, 0.0f, -1.0f};
    glm::vec3 up      {0.0f, 1.0f, 0.0f};
    glm::vec3 right   {1.0f, 0.0f, 0.0f};
    std::string type;
    float distance = 0.0f;
    bool visible = false;
};

struct DebugFrameData
{
    uint64_t frameNumber = 0;
    float timestamp = 0.0f;
    DebugCameraInfo camera;
    std::vector<DebugShipInfo> ships;
    std::vector<DebugObjectInfo> objects;

    json toJson() const;
};  