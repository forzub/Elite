#pragma once

#include <string>
#include <vector>
#include <optional>

#include "game/damage/HitVolume.h"
#include "src/game/equipment/EquipmentDamage.h"
#include "src/game/equipment/engine/EngineUnit.h"
#include "src/game/equipment/reactor/EngineReactorUnit.h"
#include "src/game/equipment/rcs/RcsGasTankUnit.h"

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

enum class ModuleStructuralKind
{
    Generic,
    OuterPanel,
    Equipment,
    StructuralFrame,
    ContainerLock,
    InternalBlock
};

struct ModuleSupportLinkDesc
{
    std::string linkId;
    std::string supportModuleId;

    float maxHealth = 100.0f;
    float impulseTolerance = 250.0f;

    bool loadBearing = true;
};

struct ModuleDescriptor
{
    // Уникальный ID конструктивного узла
    std::string moduleId;

    // Кто родитель в конструктивной иерархии
    std::string parentModuleId;

    // К какой подсистеме относится
    std::string subsystemId;

    // Replacement / salvage compatibility.
    // moduleClass: что это за тип детали.
    // providedReplacementTags: какие теги дает оторванная деталь.
    // acceptedReplacementTags: какие теги принимает это посадочное место.
    std::string moduleClass;
    std::vector<std::string> providedReplacementTags;
    std::vector<std::string> acceptedReplacementTags;

    // Какие mesh-part принадлежат этому узлу
    std::vector<std::string> meshPartIds;

    game::damage::HitZoneType zone = game::damage::HitZoneType::Generic;

    // Упрощённые логические опоры (legacy / UI summary)
    std::vector<std::string> supportModuleIds;

    // Сколько живых опор нужно, чтобы узел считался нормально закреплённым.
    int minSupportsForAttached = 1;

    // Можно ли перейти в состояние "болтается"
    bool hangable = false;

    // Если опор осталось меньше этого числа, но больше 0 -> Hanging
    int minSupportsForStable = 1;

    // Новый конструктивный тип
    ModuleStructuralKind structuralKind = ModuleStructuralKind::Generic;

    // Явные support-links (если пусто — будут автосгенерированы из supportModuleIds)
    std::vector<ModuleSupportLinkDesc> supportLinks;

    // Дефолтные параметры автогенерации support-links
    float autoSupportLinkHealth = 100.0f;
    float autoSupportImpulseTolerance = 250.0f;

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

    // Common equipment damage response. Runtime stores the five accumulated
    // causes; this immutable descriptor stores only their influence weights.
    game::equipment::EquipmentDamageSensitivity equipmentDamageSensitivity;
    game::equipment::EquipmentFailureProfile equipmentFailure;
    game::equipment::EquipmentMaintenanceProfile maintenance;

    // Present only when this equipment module is a propulsion engine.
    std::optional<game::equipment::engine::EngineUnitDescriptor> engine;

    // Dedicated propulsion generator; normal equipment damage is retained
    // by this installed module, not by a shared global reactor instance.
    std::optional<game::equipment::reactor::EngineReactorDescriptor> engineReactor;

    // RCS working-gas tank (its fill is runtime motion state; damage to this
    // module reduces the usable capacity).
    std::optional<game::equipment::rcs::RcsGasTankDescriptor> gasTank;

    bool destroysParentIfCatastrophic = false;
    bool destroysWholeObjectIfCatastrophic = false;

    // Поведение дочерних узлов при потере родителя
    bool detachChildrenOnLoss = true;
    bool destroyChildrenOnLoss = false;
    bool disableChildrenOnLoss = false;
};