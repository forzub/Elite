#pragma once

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "src/game/system_map/MapCameraState.h"
#include "src/render/types/Viewport.h"

namespace game::system_map
{

struct OrbitCameraBasis
{
    glm::dvec3 direction {0.0, 0.0, 1.0};
    glm::dvec3 up {0.0, 1.0, 0.0};
    glm::dvec3 right {1.0, 0.0, 0.0};
};

inline OrbitCameraBasis orbitCameraBasis(
    float yaw,
    float pitch
)
{
    const float cp = std::cos(pitch);
    const float sp = std::sin(pitch);
    const float cy = std::cos(yaw);
    const float sy = std::sin(yaw);

    const glm::vec3 direction(
        cp * sy,
        sp,
        cp * cy
    );

    glm::vec3 up(
        -sp * sy,
        cp,
        -sp * cy
    );

    if (glm::length(up) > 0.000001f)
        up = glm::normalize(up);
    else
        up = glm::vec3(0.0f, 1.0f, 0.0f);

    OrbitCameraBasis basis;
    basis.direction = glm::dvec3(direction);
    basis.up = glm::dvec3(up);

    const glm::dvec3 right = glm::cross(
        basis.up,
        basis.direction
    );

    const double rightLength = glm::length(right);
    if (rightLength > 0.000000001)
        basis.right = right / rightLength;
    else
        basis.right = glm::dvec3(1.0, 0.0, 0.0);

    return basis;
}

struct SystemMapCameraSnapshot
{
    Viewport viewport;
    glm::dvec3 targetAbsolute {0.0};
    OrbitCameraBasis basis;
    glm::dvec3 eyeAbsolute {0.0};
    double eyeDistance = 1.0;
    double halfHeight = 1.0;
    double worldUnitsPerPixel = 1.0;
    glm::mat4 view {1.0f};
    glm::mat4 projection {1.0f};
    glm::mat4 mvp {1.0f};

    glm::vec3 relativePosition(
        const glm::dvec3& absolutePosition
    ) const
    {
        return glm::vec3(
            absolutePosition - targetAbsolute
        );
    }
};

struct GalaxyMapCameraSnapshot
{
    Viewport viewport;
    glm::dvec3 target {0.0};
    OrbitCameraBasis basis;
    glm::dvec3 eye {0.0};
    double distance = 1.0;
    float fieldOfViewDeg = 48.0f;
    glm::mat4 view {1.0f};
    glm::mat4 projection {1.0f};
    glm::mat4 mvp {1.0f};
};

struct LocalMapCameraSnapshot
{
    DetailCameraState state;
    double scale = 1.0;
    glm::dvec2 centerPx {0.0};
    glm::dvec3 originMeters {0.0};
    bool perspectiveEnabled = false;
    double perspectiveCameraDistanceMeters = 1.0;

    glm::dvec3 vectorToCamera(
        const glm::dvec3& worldVector
    ) const
    {
        const double cy = std::cos(state.yaw);
        const double sy = std::sin(state.yaw);
        const double cp = std::cos(state.pitch);
        const double sp = std::sin(state.pitch);

        glm::dvec3 yawed;
        yawed.x = worldVector.x * cy - worldVector.z * sy;
        yawed.y = worldVector.y;
        yawed.z = worldVector.x * sy + worldVector.z * cy;

        glm::dvec3 camera;
        camera.x = yawed.x;
        camera.y = yawed.y * cp - yawed.z * sp;
        camera.z = yawed.y * sp + yawed.z * cp;
        return camera;
    }

    glm::dvec3 vectorFromCamera(
        const glm::dvec3& cameraVector
    ) const
    {
        const double cy = std::cos(state.yaw);
        const double sy = std::sin(state.yaw);
        const double cp = std::cos(state.pitch);
        const double sp = std::sin(state.pitch);

        glm::dvec3 yawed;
        yawed.x = cameraVector.x;
        yawed.y = cameraVector.y * cp + cameraVector.z * sp;
        yawed.z = -cameraVector.y * sp + cameraVector.z * cp;

        return glm::dvec3(
            yawed.x * cy + yawed.z * sy,
            yawed.y,
            -yawed.x * sy + yawed.z * cy
        );
    }

    glm::dvec3 pointToCamera(
        const glm::dvec3& worldPoint
    ) const
    {
        return vectorToCamera(
            worldPoint - originMeters
        );
    }

    double perspectiveFactor(double cameraSpaceZ) const
    {
        if (!perspectiveEnabled)
            return 1.0;

        const double safeDistance =
            std::max(
                perspectiveCameraDistanceMeters,
                1.0
            );

        const double denominator =
            std::max(
                safeDistance - cameraSpaceZ,
                safeDistance * 0.20
            );

        return safeDistance / denominator;
    }

    glm::dvec2 project(
        const glm::dvec3& worldPoint
    ) const
    {
        const glm::dvec3 camera =
            pointToCamera(worldPoint);

        const double finalScale =
            scale * state.zoom;

        const double perspective =
            perspectiveFactor(camera.z);

        return glm::dvec2(
            centerPx.x + state.pan.x +
                camera.x * finalScale * perspective,
            centerPx.y + state.pan.y -
                camera.y * finalScale * perspective
        );
    }

    glm::dvec3 unprojectPlane(
        const glm::dvec2& screenPoint
    ) const
    {
        const double finalScale =
            scale * state.zoom;

        if (std::abs(finalScale) < 0.000001)
            return originMeters;

        const glm::dvec3 camera(
            (screenPoint.x - centerPx.x - state.pan.x) /
                finalScale,
            -(screenPoint.y - centerPx.y - state.pan.y) /
                finalScale,
            0.0
        );

        return originMeters + vectorFromCamera(camera);
    }

    glm::mat4 starfieldViewMatrix() const
    {
        glm::mat4 result(1.0f);

        result = glm::rotate(
            result,
            static_cast<float>(-state.pitch),
            glm::vec3(1.0f, 0.0f, 0.0f)
        );

        result = glm::rotate(
            result,
            static_cast<float>(-state.yaw),
            glm::vec3(0.0f, 1.0f, 0.0f)
        );

        return result;
    }
};

using DetailMapCameraSnapshot = LocalMapCameraSnapshot;
using HubMapCameraSnapshot = LocalMapCameraSnapshot;

} // namespace game::system_map
