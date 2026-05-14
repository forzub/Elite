#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace game::shared
{

struct ModuleViewData
{
    std::string moduleId;
    std::string parentModuleId;
    std::string subsystemId;

    uint8_t state = 0;
    float health = 0.0f;
    float maxHealth = 0.0f;

    bool destructible = false;
    bool detachable = false;
    bool hangable = false;

    int destroyPolicy = 0;
    int detachPolicy = 0;
    int attachmentType = 0;

    std::vector<std::string> meshPartIds;
    std::vector<std::string> supportModuleIds;

    int minSupportsForAttached = 1;
    int minSupportsForStable = 1;
    int aliveSupportCount = 0;
};

} // namespace game::shared