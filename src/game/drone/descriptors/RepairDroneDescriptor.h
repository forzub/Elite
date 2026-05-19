#pragma once

#include <string>
#include <glm/glm.hpp>

namespace game::drone
{

struct RepairDroneDescriptor
{
    std::string meshPath;

    // Размер debug-дрона в метрах.
    glm::vec3 visualScale {1.2f, 1.2f, 1.2f};

    // Локальная система дрона:
    // +X right
    // +Y up
    // -Z forward
    glm::vec3 localForward {0.0f, 0.0f, -1.0f};
    glm::vec3 localUp      {0.0f, 1.0f,  0.0f};
};

inline const RepairDroneDescriptor& getDefaultRepairDroneDescriptor()
{
    static RepairDroneDescriptor desc {
        "assets/models/drones/repair_drone_debug.obj",
        glm::vec3(1.2f, 1.2f, 1.2f),
        glm::vec3(0.0f, 0.0f, -1.0f),
        glm::vec3(0.0f, 1.0f,  0.0f)
    };

    return desc;
}

} // namespace game::drone