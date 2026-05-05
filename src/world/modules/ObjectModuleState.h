#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "src/world/descriptors/ModuleDescriptor.h"

namespace world::modules
{

enum class ObjectModuleRuntimeState : uint8_t
{
    Attached  = 0,
    Disabled  = 1,
    Destroyed = 2,
    Detached  = 3,
    Hanging   = 4
};

struct ModuleSupportLinkState
{
    std::string linkId;
    std::string supportModuleId;

    float maxHealth = 100.0f;
    float health = 100.0f;

    float impulseTolerance = 250.0f;

    bool loadBearing = true;
    bool destroyed = false;

    bool isAlive() const
    {
        return !destroyed && health > 0.0f;
    }
};

struct ObjectModuleState
{
    std::string moduleId;
    std::string parentModuleId;
    int parentIndex = -1;

    std::string subsystemId;
    std::string moduleClass;
    std::vector<std::string> providedReplacementTags;
    std::vector<std::string> acceptedReplacementTags;

    std::vector<std::string> meshPartIds;
    std::vector<int> children;

    ModuleDestroyPolicy destroyPolicy = ModuleDestroyPolicy::DisableOnly;
    ModuleDetachPolicy detachPolicy = ModuleDetachPolicy::Never;
    ModuleAttachmentType attachmentType = ModuleAttachmentType::Fixed;
    ModuleStructuralKind structuralKind = ModuleStructuralKind::Generic;

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

    bool hangable = false;

    // legacy summary для UI/совместимости
    std::vector<std::string> supportModuleIds;
    int minSupportsForAttached = 1;
    int minSupportsForStable = 1;
    int aliveSupportCount = 0;

    // новый слой реальных support-links
    std::vector<ModuleSupportLinkState> supportLinks;

    ObjectModuleRuntimeState state = ObjectModuleRuntimeState::Attached;

    bool isAttachedLike() const
    {
        return state == ObjectModuleRuntimeState::Attached ||
               state == ObjectModuleRuntimeState::Disabled ||
               state == ObjectModuleRuntimeState::Hanging;
    }

    bool participatesInHits() const
    {
        return state == ObjectModuleRuntimeState::Attached ||
               state == ObjectModuleRuntimeState::Disabled ||
               state == ObjectModuleRuntimeState::Hanging;
    }

    bool isVisibleInHull() const
    {
        return state == ObjectModuleRuntimeState::Attached ||
               state == ObjectModuleRuntimeState::Hanging;
    }

    bool isDestroyedLike() const
    {
        return state == ObjectModuleRuntimeState::Destroyed ||
               state == ObjectModuleRuntimeState::Detached;
    }
};

} // namespace world::modules