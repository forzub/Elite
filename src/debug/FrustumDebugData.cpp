#include "FrustumDebugData.h"

json DebugFrameData::toJson() const
{
    json j;
    
    j["frame"] = frameNumber;
    j["timestamp"] = timestamp;
    
    // Камера
    j["camera"]["position"] = {camera.position.x, camera.position.y, camera.position.z};
    j["camera"]["direction"] = {camera.direction.x, camera.direction.y, camera.direction.z};
    j["camera"]["up"] = {camera.up.x, camera.up.y, camera.up.z};
    j["camera"]["fov"] = camera.fov;
    j["camera"]["aspect"] = camera.aspect;
    j["camera"]["near"] = camera.nearPlane;
    j["camera"]["far"] = camera.farPlane;
    j["camera"]["cameraId"] = camera.cameraId;           // <-- НОВОЕ
    j["camera"]["cameraName"] = camera.cameraName;       // <-- НОВОЕ
    
    // Corners фрустума
    for(int i = 0; i < 8; i++)
    {
        j["camera"]["frustumCorners"][i] = {
            camera.frustumCorners[i].x,
            camera.frustumCorners[i].y,
            camera.frustumCorners[i].z
        };
    }
    
    // Корабли
    for(const auto& ship : ships)
    {
        json s;
        s["id"] = ship.id;
        s["position"] = {ship.position.x, ship.position.y, ship.position.z};
        s["radius"] = ship.radius;
        s["visible"] = ship.visible;
        s["distance"] = ship.distance;
        s["type"] = ship.type;
        j["ships"].push_back(s);
    }
    
    return j;
}