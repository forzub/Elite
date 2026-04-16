#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "src/world/descriptors/ModuleDescriptor.h"

namespace world::modules
{

enum class ObjectModuleRuntimeState : uint8_t
{
    Attached = 0,
    Disabled = 1,
    Destroyed = 2,
    Detached = 3
};

struct ObjectModuleState
{
    std::string moduleId;
    std::string parentModuleId;
    std::string subsystemId;

    std::vector<std::string> meshPartIds;
    std::vector<int> children;

    ModuleDestroyPolicy destroyPolicy = ModuleDestroyPolicy::DisableOnly;
    ModuleDetachPolicy detachPolicy = ModuleDetachPolicy::Never;
    ModuleAttachmentType attachmentType = ModuleAttachmentType::Fixed;

    bool detachChildrenOnLoss = false;
    bool destroyChildrenOnLoss = false;
    bool disableChildrenOnLoss = false;

    bool destructible = false;
    bool detachable = false;
    bool repairable = false;
    bool salvageable = false;

    bool isCritical = false;
    bool destroysParentIfCatastrophic = false;
    bool destroysWholeObjectIfCatastrophic = false;

    float maxHealth = 100.0f;
    float health = 100.0f;

    ObjectModuleRuntimeState state = ObjectModuleRuntimeState::Attached;

    bool isVisibleInHull() const
    {
        return state == ObjectModuleRuntimeState::Attached;
    }
};

} // namespace world::modules