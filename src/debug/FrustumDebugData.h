#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

struct DebugShipInfo
{
    uint64_t id;
    glm::vec3 position;
    float radius;
    bool visible;
    float distance;      // дистанция от камеры
    std::string type;    // тип корабля (для отладки)
};

struct DebugCameraInfo
{
    glm::vec3 position;
    glm::vec3 direction;
    glm::vec3 up;
    float fov;
    float aspect;
    float nearPlane;
    float farPlane;
    int cameraId;        // 0 = front, 1 = rear, 2 = drone etc
    std::string cameraName; // "front", "rear", "drone"
    
    // Плоскости фрустума в world space для отрисовки
    glm::vec3 frustumCorners[8];
};


struct DebugObjectInfo
{
    uint32_t id;
    glm::vec3 position;
    std::string type;
    float distance;
    bool visible;
};



struct DebugFrameData
{
    uint64_t frameNumber;
    float timestamp;
    DebugCameraInfo camera;
    std::vector<DebugShipInfo> ships;
    std::vector<DebugObjectInfo> objects;
    
    json toJson() const;
};