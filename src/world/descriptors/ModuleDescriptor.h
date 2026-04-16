#pragma once

#include <string>
#include <vector>

#include "game/damage/HitVolume.h"

enum class ModuleDetachPolicy
{
    Never,
    OnDestroyed,
    OnCriticalHit,
    ScriptedOnly
};

enum class ModuleDestroyPolicy
{
    DisableOnly,
    Detach,
    Vaporize,
    Catastrophic
};

enum class ModuleAttachmentType
{
    Fixed,
    Structural,
    Hardpoint,
    SurfaceMounted,
    Internal
};

struct ModuleDescriptor
{
    // Уникальный ID конструктивного узла
    std::string moduleId;

    // Кто родитель в конструктивной иерархии
    std::string parentModuleId;

    // К какой подсистеме относится
    std::string subsystemId;

    // Какие mesh-part принадлежат этому узлу
    std::vector<std::string> meshPartIds;

    game::damage::HitZoneType zone = game::damage::HitZoneType::Generic;

    bool enabled = true;
    bool destructible = false;
    bool detachable = false;
    bool salvageable = false;
    bool repairable = false;

    ModuleDetachPolicy detachPolicy = ModuleDetachPolicy::Never;
    ModuleDestroyPolicy destroyPolicy = ModuleDestroyPolicy::DisableOnly;
    ModuleAttachmentType attachmentType = ModuleAttachmentType::Fixed;

    // 0 = внешний слой, 1 = под ним и т.д.
    int layerIndex = 0;
    int hitPriority = 0;

    float maxHealth = 100.0f;
    float armor = 0.0f;
    float penetrationResistance = 0.0f;

    // Если разрушение этого узла критично
    bool isCritical = false;
    bool destroysParentIfCatastrophic = false;
    bool destroysWholeObjectIfCatastrophic = false;

    // Поведение дочерних узлов при потере родителя
    bool detachChildrenOnLoss = true;
    bool destroyChildrenOnLoss = false;
    bool disableChildrenOnLoss = false;
};