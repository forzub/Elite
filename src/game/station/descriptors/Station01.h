#pragma once

#include <string>
#include "src/world/descriptors/StationDescriptor.h"

namespace Station1
{
    inline void apply(StationDescriptor& d)
    {
        d.m_meshId = "stantion-01";

        d.m_rotationSpeed = 0.05f;
        d.m_boundingRadius = 500.0f;

        d.meshSizeMeters = glm::vec3(
            500.f,
            500.0f,
            4000.0f
        );

        // Смысловые размеры станции в логическом игровом базисе.
        // Подставь реальные значения станции.
        d.logicalDimensionsValue.length = 5021.38f;
        d.logicalDimensionsValue.width  = 4000.00f;
        d.logicalDimensionsValue.height = 4089.56f;
        d.logicalDimensionsValue.scaleReference = ScaleReference::Length;
        d.logicalDimensionsValue.enabled = true;


        // Если станция авторилась в том же базисе Blender:
        // -X = forward, +Z = up
        d.meshForwardAxisValue = glm::vec3(-1.0f, 0.0f, 0.0f);
        d.meshUpAxisValue      = glm::vec3( 0.0f, 0.0f, 1.0f);

        // LEGACY fallback для старого single-mesh path.
        // Пока не трогаем, потому что assembly путь скоро будет сам
        // приводить меш в логический базис.
        d.visualBasisRotationDegValue = glm::vec3(0.0f, 0.0f, 0.0f);


        d.modules =
        {
            ModuleDescriptor{
                .moduleId = "station_core_rear",
                .parentModuleId = "",
                .subsystemId = "station_core",
                .meshPartIds = { "station_core_rear" },
                .zone = game::damage::HitZoneType::Hull,
                .enabled = true,
                .destructible = false,
                .detachable = false,
                .salvageable = false,
                .repairable = false,
                .detachPolicy = ModuleDetachPolicy::Never,
                .destroyPolicy = ModuleDestroyPolicy::DisableOnly,
                .attachmentType = ModuleAttachmentType::Structural,
                .layerIndex = 2,
                .hitPriority = 100,
                .maxHealth = 1000.0f,
                .armor = 150.0f,
                .penetrationResistance = 250.0f,
                .isCritical = true,
                .destroysParentIfCatastrophic = false,
                .destroysWholeObjectIfCatastrophic = false,
                .detachChildrenOnLoss = true,
                .destroyChildrenOnLoss = false,
                .disableChildrenOnLoss = true
            },

            ModuleDescriptor{
                .moduleId = "station_core_front",
                .parentModuleId = "station_core_rear",
                .subsystemId = "station_front",
                .meshPartIds = { "station_core_front" },
                .zone = game::damage::HitZoneType::Hull,
                .enabled = true,
                .destructible = true,
                .detachable = false,
                .salvageable = false,
                .repairable = true,
                .detachPolicy = ModuleDetachPolicy::Never,
                .destroyPolicy = ModuleDestroyPolicy::DisableOnly,
                .attachmentType = ModuleAttachmentType::Structural,
                .layerIndex = 1,
                .hitPriority = 60,
                .maxHealth = 450.0f,
                .armor = 80.0f,
                .penetrationResistance = 120.0f,
                .isCritical = false,
                .destroysParentIfCatastrophic = false,
                .destroysWholeObjectIfCatastrophic = false,
                .detachChildrenOnLoss = true,
                .destroyChildrenOnLoss = false,
                .disableChildrenOnLoss = true
            },

            ModuleDescriptor{
                .moduleId = "station_habitat_s1",
                .parentModuleId = "station_core_front",
                .subsystemId = "habitat_ring",
                .meshPartIds = { "station_habitat_s1" },
                .zone = game::damage::HitZoneType::Cargo,
                .enabled = true,
                .destructible = true,
                .detachable = true,
                .salvageable = true,
                .repairable = true,
                .detachPolicy = ModuleDetachPolicy::OnDestroyed,
                .destroyPolicy = ModuleDestroyPolicy::Detach,
                .attachmentType = ModuleAttachmentType::Structural,
                .layerIndex = 0,
                .hitPriority = 70,
                .maxHealth = 260.0f,
                .armor = 35.0f,
                .penetrationResistance = 50.0f,
                .isCritical = false,
                .destroysParentIfCatastrophic = false,
                .destroysWholeObjectIfCatastrophic = false,
                .detachChildrenOnLoss = false,
                .destroyChildrenOnLoss = false,
                .disableChildrenOnLoss = false
            },

            ModuleDescriptor{
                .moduleId = "station_habitat_s2",
                .parentModuleId = "station_core_front",
                .subsystemId = "habitat_ring",
                .meshPartIds = { "station_habitat_s2" },
                .zone = game::damage::HitZoneType::Cargo,
                .enabled = true,
                .destructible = true,
                .detachable = true,
                .salvageable = true,
                .repairable = true,
                .detachPolicy = ModuleDetachPolicy::OnDestroyed,
                .destroyPolicy = ModuleDestroyPolicy::Detach,
                .attachmentType = ModuleAttachmentType::Structural,
                .layerIndex = 0,
                .hitPriority = 70,
                .maxHealth = 260.0f,
                .armor = 35.0f,
                .penetrationResistance = 50.0f,
                .isCritical = false,
                .destroysParentIfCatastrophic = false,
                .destroysWholeObjectIfCatastrophic = false,
                .detachChildrenOnLoss = false,
                .destroyChildrenOnLoss = false,
                .disableChildrenOnLoss = false
            },

            ModuleDescriptor{
                .moduleId = "station_habitat_s3",
                .parentModuleId = "station_core_front",
                .subsystemId = "habitat_ring",
                .meshPartIds = { "station_habitat_s3" },
                .zone = game::damage::HitZoneType::Cargo,
                .enabled = true,
                .destructible = true,
                .detachable = true,
                .salvageable = true,
                .repairable = true,
                .detachPolicy = ModuleDetachPolicy::OnDestroyed,
                .destroyPolicy = ModuleDestroyPolicy::Detach,
                .attachmentType = ModuleAttachmentType::Structural,
                .layerIndex = 0,
                .hitPriority = 70,
                .maxHealth = 260.0f,
                .armor = 35.0f,
                .penetrationResistance = 50.0f,
                .isCritical = false,
                .destroysParentIfCatastrophic = false,
                .destroysWholeObjectIfCatastrophic = false,
                .detachChildrenOnLoss = false,
                .destroyChildrenOnLoss = false,
                .disableChildrenOnLoss = false
            },

            ModuleDescriptor{
                .moduleId = "station_solar_panels",
                .parentModuleId = "station_core_front",
                .subsystemId = "power_generation",
                .meshPartIds = { "station_solar_panels" },
                .zone = game::damage::HitZoneType::Radiator,
                .enabled = true,
                .destructible = true,
                .detachable = true,
                .salvageable = false,
                .repairable = true,
                .detachPolicy = ModuleDetachPolicy::OnDestroyed,
                .destroyPolicy = ModuleDestroyPolicy::Detach,
                .attachmentType = ModuleAttachmentType::SurfaceMounted,
                .layerIndex = 0,
                .hitPriority = 90,
                .maxHealth = 120.0f,
                .armor = 5.0f,
                .penetrationResistance = 8.0f,
                .isCritical = false,
                .destroysParentIfCatastrophic = false,
                .destroysWholeObjectIfCatastrophic = false,
                .detachChildrenOnLoss = false,
                .destroyChildrenOnLoss = false,
                .disableChildrenOnLoss = false
            }
        };

    }
}