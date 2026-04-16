#pragma once

#include <string>
#include <cstdint>

namespace game::simulation
{

struct ObjectModuleSnapshot
{
    std::string moduleId;
    uint8_t state = 0;
    float health = 0.0f;
};

} // namespace game::simulation