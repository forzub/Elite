#pragma once

#include <string>

namespace game::simulation
{

struct ObjectAssemblyModuleSnapshot
{
    std::string moduleId;
    float rotationAngleRad = 0.0f;
};

} // namespace game::simulation