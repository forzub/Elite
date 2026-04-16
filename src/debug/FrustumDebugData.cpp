#include "FrustumDebugData.h"

json DebugFrameData::toJson() const
{
    json j;

    j["frame"] = frameNumber;
    j["timestamp"] = timestamp;

    j["camera"]["position"] = {camera.position.x, camera.position.y, camera.position.z};
    j["camera"]["direction"] = {camera.direction.x, camera.direction.y, camera.direction.z};
    j["camera"]["up"] = {camera.up.x, camera.up.y, camera.up.z};
    j["camera"]["fov"] = camera.fov;
    j["camera"]["aspect"] = camera.aspect;
    j["camera"]["near"] = camera.nearPlane;
    j["camera"]["far"] = camera.farPlane;
    j["camera"]["cameraId"] = camera.cameraId;
    j["camera"]["cameraName"] = camera.cameraName;

    for (int i = 0; i < 8; ++i)
    {
        j["camera"]["frustumCorners"][i] = {
            camera.frustumCorners[i].x,
            camera.frustumCorners[i].y,
            camera.frustumCorners[i].z
        };
    }

    for (const auto& ship : ships)
    {
        json s;
        s["id"] = ship.id;
        s["position"] = {ship.position.x, ship.position.y, ship.position.z};
        s["forward"] = {ship.forward.x, ship.forward.y, ship.forward.z};
        s["up"] = {ship.up.x, ship.up.y, ship.up.z};
        s["right"] = {ship.right.x, ship.right.y, ship.right.z};
        s["radius"] = ship.radius;
        s["visible"] = ship.visible;
        s["distance"] = ship.distance;
        s["type"] = ship.type;
        j["ships"].push_back(s);
    }

    for (const auto& obj : objects)
    {
        json o;
        o["id"] = obj.id;
        o["position"] = {obj.position.x, obj.position.y, obj.position.z};
        o["forward"] = {obj.forward.x, obj.forward.y, obj.forward.z};
        o["up"] = {obj.up.x, obj.up.y, obj.up.z};
        o["right"] = {obj.right.x, obj.right.y, obj.right.z};
        o["type"] = obj.type;
        o["distance"] = obj.distance;
        o["visible"] = obj.visible;
        j["objects"].push_back(o);
    }

    return j;
}