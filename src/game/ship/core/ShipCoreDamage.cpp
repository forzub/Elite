// ShipCore: damage intake, module runtime / hit-volume sync, structure, repairs, capsule ejection and DEV module operations.

#include "src/game/ship/core/ShipCoreInternal.h"

using namespace game::ship::geometry;

namespace game::ship::core
{




// ------------ DAMAGE -----------
void ShipCore::applyDamage(const game::damage::DamageEvent& event)
{
    using namespace game::damage;

    DamageEvent localEvent = event;

    localEvent.setLocalPositionForResolve(
        m_transform.worldPositionToLocalCell(event.worldPosition)
    );

    auto result = m_hitComponent.resolve(localEvent);

    const float healthBefore =
        result.volume ? result.volume->health : 0.0f;
    const float maxHealth =
        result.volume ? result.volume->maxHealth : 0.0f;
    const std::string damagedModuleId =
        result.volume ? result.volume->moduleId : std::string {};

    m_damageHandler.handleDamage(result);

    // Functional equipment damage records the CAUSE of actual health loss.
    // The amount is normalized from the real hit-volume loss; sensitivity
    // weights are not damage values and never enter this accumulation step.
    if (result.volume &&
        !result.volume->supportLinkVolume &&
        !damagedModuleId.empty() &&
        maxHealth > 0.0f)
    {
        const double lost01 = std::clamp(
            static_cast<double>(
                healthBefore - result.volume->health
            ) / static_cast<double>(maxHealth),
            0.0,
            1.0
        );

        using Kind = game::equipment::EquipmentDamageKind;
        bool hasEquipmentCause = true;
        Kind cause = Kind::Impact;

        switch (event.type)
        {
            case DamageType::Laser:
                cause = Kind::Laser;
                break;

            case DamageType::Explosion:
            case DamageType::Collision:
                cause = Kind::Impact;
                break;

            case DamageType::Radiation:
                cause = Kind::Radiation;
                break;

            case DamageType::EMP:
                // EMP is not one of the five physical equipment-damage
                // accumulators. Electrical effects need their own system.
                hasEquipmentCause = false;
                break;
        }

        if (hasEquipmentCause && lost01 > 0.0)
        {
            // The reactor pad is part of the installed ER-5 module. Until
            // separate pad hit volumes exist, actual damage to this reactor
            // also removes the same fraction of its surviving emitting area.
            if(m_desc)
                for(const auto& def:m_desc->modules)
                    if(def.moduleId==damagedModuleId && def.engineReactor)
                    {
                        auto* er=m_moduleRuntime.findModule(damagedModuleId);
                        if(er)
                            er->engineReactorOperating.padIntegrity01 =
                                std::clamp(
                                    er->engineReactorOperating.padIntegrity01-
                                    lost01,0.0,1.0);
                        break;
                    }

            bool isInstalledEngine=false;
            if (m_desc)
                for (const auto& module : m_desc->modules)
                    if (module.moduleId==damagedModuleId && module.engine)
                    {
                        isInstalledEngine=true;
                        break;
                    }
            if (cause==Kind::Impact && isInstalledEngine)
            {
                // Incident-dependent seed. The same engine ID occurs on many
                // ships, so module ID alone must not yield identical outcomes
                // across the entire universe.
                std::uint64_t incidentKey=0xcbf29ce484222325ULL;
                const auto mix=[&](std::uint64_t value) {
                    incidentKey^=value;
                    incidentKey*=0x100000001b3ULL;
                };
                mix(static_cast<std::uint64_t>(
                    event.worldPosition.cell.x
                ));
                mix(static_cast<std::uint64_t>(
                    event.worldPosition.cell.y
                ));
                mix(static_cast<std::uint64_t>(
                    event.worldPosition.cell.z
                ));
                mix(static_cast<std::uint64_t>(std::llround(
                    event.worldPosition.localMeters.x*1000.0
                )));
                mix(static_cast<std::uint64_t>(std::llround(
                    event.worldPosition.localMeters.y*1000.0
                )));
                mix(static_cast<std::uint64_t>(std::llround(
                    event.worldPosition.localMeters.z*1000.0
                )));
                m_moduleRuntime.addEngineImpactShock(
                    damagedModuleId,lost01,incidentKey
                );
            }
            else
                m_moduleRuntime.addEquipmentDamage(
                    damagedModuleId,cause,lost01
                );
        }
    }

    syncDestroyedModulesFromHitVolumes();

    if (m_desc)
    {
        world::modules::ObjectRuntimeHitBuilder::rebuild(
            m_hitComponent,
            m_desc->typeId,
            *m_desc,
            m_moduleRuntime,
            m_structuralLinkRuntime,
            m_assemblyRuntime
        );

        m_hitVolumesDirty = false;
    }
    // An engine shock is observable immediately to the pilot/autopilot; no
    // stale pre-impact capability snapshot may survive until a later system
    // update. This rebuild is incident-driven, never per physics tick.
    refreshRuntimeState();
}



bool ShipCore::debugDestroyModuleById(const std::string& moduleId)
{
    if (!m_desc)
        return false;

    auto* mod = m_moduleRuntime.findModule(moduleId);
    if (!mod)
    {
        std::cout
            << "[ShipCore] debugDestroyModuleById: module not found: "
            << moduleId << "\n";
        return false;
    }

    m_moduleRuntime.markModuleDestroyed(moduleId, 0.0f);

    world::modules::ObjectRuntimeHitBuilder::rebuild(
        m_hitComponent,
        m_desc->typeId,
        *m_desc,
        m_moduleRuntime,
        m_structuralLinkRuntime,
        m_assemblyRuntime
    );

    m_hitVolumesDirty = false;

    std::cout
        << "[ShipCore] debugDestroyModuleById: destroyed moduleId="
        << moduleId << "\n";

    return true;
}


bool ShipCore::debugRestoreModuleById(const std::string& moduleId)
{
    if (!m_desc)
        return false;

    auto* mod = m_moduleRuntime.findModule(moduleId);
    if (!mod)
    {
        std::cout
            << "[ShipCore] debugRestoreModuleById: module not found: "
            << moduleId << "\n";
        return false;
    }

    m_moduleRuntime.restoreModuleBranch(moduleId);

    world::modules::ObjectRuntimeHitBuilder::rebuild(
        m_hitComponent,
        m_desc->typeId,
        *m_desc,
        m_moduleRuntime,
        m_structuralLinkRuntime,
        m_assemblyRuntime
    );

    m_hitVolumesDirty = false;

    std::cout
        << "[ShipCore] debugRestoreModuleById: restored moduleId="
        << moduleId << "\n";

    return true;
}

bool ShipCore::debugResetStructure()
{
    if (!m_desc)
        return false;

    m_moduleRuntime.init(m_desc->moduleDescriptors());
    m_structuralLinkRuntime.init(m_desc->moduleDescriptors());
    m_detachedFragmentRuntime.clear();
    m_repairJobRuntime.clear();

    if (game::ship::geometry::AssemblyMeshLibrary::has(m_desc->typeId))
    {
        const auto& assembly =
            game::ship::geometry::AssemblyMeshLibrary::get(m_desc->typeId);
        m_assemblyRuntime.init(assembly);
    }

    world::modules::ObjectRuntimeHitBuilder::rebuild(
        m_hitComponent,
        m_desc->typeId,
        *m_desc,
        m_moduleRuntime,
        m_structuralLinkRuntime,
        m_assemblyRuntime
    );

    m_hitVolumesDirty = false;

    std::cout << "[ShipCore] debugResetStructure\n";
    return true;
}



void ShipCore::exportHitVolumes() const
{
    for (const auto& v : m_hitComponent.volumes)
    {
        std::cout
            << "HIT_VOLUME "
            << v.m_label << " "
            << v.center.x << " "
            << v.center.y << " "
            << v.center.z << " "
            << v.halfSize.x << " "
            << v.halfSize.y << " "
            << v.halfSize.z
            << std::endl;
    }
}



void ShipCore::restoreVolume(int index)
{
    if (index < 0 || index >= m_hitComponent.volumes.size())
        return;

    auto& v = m_hitComponent.volumes[index];

    v.health = 1.0f;
    v.destroyed = false;
}









void ShipCore::initModuleRuntime()
{
    if (!m_desc)
        return;

    m_moduleRuntime.init(m_desc->moduleDescriptors());
    m_structuralLinkRuntime.init(m_desc->moduleDescriptors());

    m_moduleRuntime.reevaluateStructuralStates(&m_structuralLinkRuntime);
}






void ShipCore::syncDestroyedModulesFromHitVolumes()
{
    bool supportChanged = false;

    for (const auto& volume : m_hitComponent.volumes)
    {
        if (volume.supportLinkVolume)
        {
            if (m_structuralLinkRuntime.setLinkHealth(
                    volume.supportLinkId,
                    volume.health,
                    volume.destroyed))
            {
                supportChanged = true;
            }

            // legacy mirror: пока оставляем для совместимости
            if (m_moduleRuntime.setSupportLinkHealth(
                    volume.moduleId,
                    volume.supportLinkId,
                    volume.health,
                    volume.destroyed))
            {
                supportChanged = true;
            }

            continue;
        }

        m_moduleRuntime.setModuleHealth(volume.moduleId, volume.health);

        if (volume.destroyed)
        {
            m_moduleRuntime.markModuleDestroyed(volume.moduleId, volume.health);
        }
    }

    if (supportChanged)
{
    m_moduleRuntime.reevaluateStructuralStates(&m_structuralLinkRuntime);
}
    syncDetachedFragmentsFromModuleRuntime();
}




void ShipCore::updateAssemblyRuntime(double dt)
{
    // Обычная анимация/вращение модулей не должна пересобирать hit-volumes.
    // Rebuild нужен только при структурных изменениях:
    // damage, detach, restore, descriptor/assembly edit.
    m_assemblyRuntime.update(dt);
}



bool ShipCore::debugReattachModuleById(const std::string& moduleId)
{
    if (!m_desc)
        return false;

    if (!m_detachedFragmentRuntime.hasFragment(moduleId))
    {
        std::cout
            << "[ShipCore] debugReattachModuleById failed: no detached fragment for moduleId="
            << moduleId << "\n";
        return false;
    }

    if (!m_moduleRuntime.reattachModule(moduleId))
    {
        std::cout
            << "[ShipCore] debugReattachModuleById failed: module runtime rejected moduleId="
            << moduleId << "\n";
        return false;
    }

    const int restoredLinks =
        m_structuralLinkRuntime.restoreLinksTouchingModule(moduleId);

    m_detachedFragmentRuntime.removeFragment(moduleId);

    m_moduleRuntime.reevaluateStructuralStates(&m_structuralLinkRuntime);

    world::modules::ObjectRuntimeHitBuilder::rebuild(
        m_hitComponent,
        m_desc->typeId,
        *m_desc,
        m_moduleRuntime,
        m_structuralLinkRuntime,
        m_assemblyRuntime
    );

    m_hitVolumesDirty = false;

    std::cout
        << "[ShipCore] debugReattachModuleById moduleId="
        << moduleId
        << " restoredLinks=" << restoredLinks
        << "\n";

    return true;
}






bool ShipCore::startRepairJobForModule(const std::string& moduleId)
{
    if (!m_desc)
        return false;

    // Repair runtime работает в локальной системе корабля.
    // Никакой глобальной трансляции сюда передавать нельзя.
    const glm::mat4 ownerModel =
        m_transform.orientation;

        
    glm::vec3 ownerLinearVelocity =
        m_transform.forward() * m_transform.forwardVelocity +
        m_transform.right()   * m_transform.localVelocity.x +
        m_transform.up()      * m_transform.localVelocity.y;

    glm::vec3 dockWorldPosition =
        glm::vec3(ownerModel * glm::vec4(0, 0, 0, 1));

    if (const auto dock =
            resolveShipAttachment(
                m_desc,
                m_transform,
                "repair_drone_dock_main",
                nullptr,
                nullptr))
    {
        dockWorldPosition =
            glm::vec3(ownerModel * glm::vec4(dock->localPosition, 1.0f));
    }


    glm::vec3 launchWorldPosition = dockWorldPosition;
    glm::vec3 recoveryWorldPosition = dockWorldPosition;

if (const auto launch =
        resolveShipAttachment(
            m_desc,
            m_transform,
            "repair_drone_launch_main",
            nullptr,
            nullptr))
{
    launchWorldPosition =
        glm::vec3(ownerModel * glm::vec4(launch->localPosition, 1.0f));
}

if (const auto recovery =
        resolveShipAttachment(
            m_desc,
            m_transform,
            "repair_drone_recovery_main",
            nullptr,
            nullptr))
{
    recoveryWorldPosition =
        glm::vec3(ownerModel * glm::vec4(recovery->localPosition, 1.0f));
}



std::vector<glm::vec3> repairWorkWorldPoints;

for (const auto* point :
        findShipAttachmentPointsByKind(
            m_desc,
            ShipAttachmentKind::RepairWorkPoint
        ))
{
    if (!point)
        continue;

    if (const auto resolved =
            resolveShipAttachment(
                m_desc,
                m_transform,
                point->id,
                nullptr,
                nullptr))
    {
        repairWorkWorldPoints.push_back(
            glm::vec3(ownerModel * glm::vec4(resolved->localPosition, 1.0f))
        );
    }
}

    const bool ok = m_repairJobRuntime.startJob(
    moduleId,
    ownerModel,
    ownerLinearVelocity,
    dockWorldPosition,
    launchWorldPosition,
    recoveryWorldPosition,
    repairWorkWorldPoints,
    m_detachedFragmentRuntime
);

    std::cout
        << "[ShipCore] startRepairJobForModule moduleId="
        << moduleId
        << " ok=" << ok
        << "\n";

    return ok;
}










std::vector<world::modules::ObjectMissingPartRequest>
ShipCore::buildMissingPartRequests() const
{
    std::vector<world::modules::ObjectMissingPartRequest> out;

    if (!m_desc)
        return out;

    if (!game::ship::geometry::AssemblyMeshLibrary::has(m_desc->typeId))
        return out;

    const auto& assembly =
        game::ship::geometry::AssemblyMeshLibrary::get(m_desc->typeId);

    for (const auto& module : m_moduleRuntime.modules())
    {
        const bool missing =
            module.state == world::modules::ObjectModuleRuntimeState::Detached ||
            module.state == world::modules::ObjectModuleRuntimeState::Destroyed;

        if (!missing)
            continue;

        if (!module.repairable && !module.salvageable)
            continue;

        const auto* assemblyModule =
            world::modules::findAssemblyModuleById(
                assembly,
                module.moduleId
            );

        if (!assemblyModule)
            continue;

        world::modules::ObjectMissingPartRequest req;
        req.targetModuleId = module.moduleId;

        req.moduleClass = module.moduleClass;
        if (req.moduleClass.empty())
            req.moduleClass = module.moduleId;

        req.acceptedReplacementTags = module.acceptedReplacementTags;
        if (req.acceptedReplacementTags.empty())
            req.acceptedReplacementTags.push_back(module.moduleId);

        req.homeLocalModel =
            world::modules::buildAssemblyModuleHierarchicalLocalModel(
                assembly,
                m_assemblyRuntime,
                module.moduleId
            );



            // ВАЖНО:
// Если модуль уже Detached, его active hit-volume мог быть удалён из m_hitComponent
// после rebuild. Поэтому сначала берём homeCenterLocal из уже созданного detached fragment.
if (const auto* existingFragment =
        m_detachedFragmentRuntime.findFragment(module.moduleId))
{
    req.homeCenterLocal = existingFragment->homeCenterLocal;

    std::cout
        << "[ShipCore] missing slot center from detached fragment moduleId="
        << module.moduleId
        << " center=("
        << req.homeCenterLocal.x << ", "
        << req.homeCenterLocal.y << ", "
        << req.homeCenterLocal.z << ")"
        << "\n";
}
else
{
    glm::vec3 accumulatedCenter(0.0f);
    int accumulatedCount = 0;

    for (const auto& v : m_hitComponent.volumes)
    {
        if (v.moduleId != module.moduleId)
            continue;

        if (v.supportLinkVolume)
            continue;

        accumulatedCenter += v.center;
        ++accumulatedCount;
    }

    if (accumulatedCount > 0)
    {
        req.homeCenterLocal =
            accumulatedCenter / static_cast<float>(accumulatedCount);
    }
    else
    {
        req.homeCenterLocal =
            glm::vec3(req.homeLocalModel * glm::vec4(0, 0, 0, 1));

        std::cout
            << "[ShipCore] WARNING missing slot fallback center used moduleId="
            << module.moduleId
            << " center=("
            << req.homeCenterLocal.x << ", "
            << req.homeCenterLocal.y << ", "
            << req.homeCenterLocal.z << ")"
            << "\n";
    }
}








        out.push_back(std::move(req));
    }

    return out;
}

bool ShipCore::startRepairJobForClaimedReplacement(
    const std::string& targetModuleId,
    world::modules::ObjectDetachedFragmentRuntime& sourceRuntime,
    const std::string& sourceModuleId
)
{
    if (!m_desc)
        return false;

    const auto requests = buildMissingPartRequests();

    const world::modules::ObjectMissingPartRequest* targetRequest = nullptr;

    for (const auto& req : requests)
    {
        if (req.targetModuleId == targetModuleId)
        {
            targetRequest = &req;
            break;
        }
    }

    if (!targetRequest)
    {
        std::cout
            << "[ShipCore] repair failed: no missing slot targetModuleId="
            << targetModuleId << "\n";
        return false;
    }

    if (!m_detachedFragmentRuntime.claimFragmentAsReplacement(
        sourceRuntime,
        sourceModuleId,
        targetModuleId,
        targetRequest->homeLocalModel,
        targetRequest->homeCenterLocal))
    {
        return false;
    }

    // Важно: repair job теперь всегда работает с targetModuleId
    // внутри runtime целевого корабля.
    return startRepairJobForModule(targetModuleId);
}







bool ShipCore::debugDetachModuleById(const std::string& moduleId)
{
    if (!m_desc)
        return false;

    if (!m_moduleRuntime.debugForceDetach(moduleId))
        return false;

    syncDetachedFragmentsFromModuleRuntime();

    world::modules::ObjectRuntimeHitBuilder::rebuild(
        m_hitComponent,
        m_desc->typeId,
        *m_desc,
        m_moduleRuntime,
        m_structuralLinkRuntime,
        m_assemblyRuntime
    );

    m_hitVolumesDirty = false;

    std::cout << "[ShipCore] debugDetachModuleById: moduleId="
              << moduleId << "\n";
    return true;
}




bool ShipCore::ejectCockpitCapsule()
{
    if (!m_desc)
        return false;

    constexpr const char* COCKPIT_MODULE_ID = "ship_cockpit";

    auto* cockpit = m_moduleRuntime.findModule(COCKPIT_MODULE_ID);
    if (!cockpit)
    {
        std::cout
            << "[ShipCore] ejectCockpitCapsule failed: cockpit module not found\n";
        return false;
    }

    if (!cockpit->detachable)
    {
        std::cout
            << "[ShipCore] ejectCockpitCapsule failed: cockpit is not detachable\n";
        return false;
    }

    if (cockpit->state == world::modules::ObjectModuleRuntimeState::Detached)
    {
        std::cout
            << "[ShipCore] ejectCockpitCapsule ignored: cockpit already detached\n";
        return false;
    }

    if (!m_moduleRuntime.debugForceDetach(COCKPIT_MODULE_ID))
    {
        std::cout
            << "[ShipCore] ejectCockpitCapsule failed: detach rejected\n";
        return false;
    }

    syncDetachedFragmentsFromModuleRuntime();

    const glm::vec3 forward = m_transform.forward();

    // Скорость подбери потом по вкусу.
    // Сейчас это именно "катапульта", не медленный отпад.
    const glm::vec3 ejectVelocity = forward * 35.0f;

    const glm::vec3 ejectAngularVelocity =
        glm::vec3(0.0f, 0.15f, 0.0f);

    if (!m_detachedFragmentRuntime.setFragmentMotion(
            COCKPIT_MODULE_ID,
            ejectVelocity,
            ejectAngularVelocity))
    {
        std::cout
            << "[ShipCore] ejectCockpitCapsule failed: detached fragment was not spawned\n";
        return false;
    }

    world::modules::ObjectRuntimeHitBuilder::rebuild(
        m_hitComponent,
        m_desc->typeId,
        *m_desc,
        m_moduleRuntime,
        m_structuralLinkRuntime,
        m_assemblyRuntime
    );

    m_hitVolumesDirty = false;

    std::cout
        << "[ShipCore] COCKPIT CAPSULE EJECTED forward=("
        << forward.x << ", "
        << forward.y << ", "
        << forward.z << ")"
        << "\n";

    return true;
}





bool ShipCore::debugHangModuleById(const std::string& moduleId)
{
    if (!m_desc)
        return false;

    if (!m_moduleRuntime.debugForceHang(moduleId))
        return false;

    world::modules::ObjectRuntimeHitBuilder::rebuild(
        m_hitComponent,
        m_desc->typeId,
        *m_desc,
        m_moduleRuntime,
        m_structuralLinkRuntime,
        m_assemblyRuntime
    );

    m_hitVolumesDirty = false;

    std::cout << "[ShipCore] debugHangModuleById: moduleId="
              << moduleId << "\n";
    return true;
}

bool ShipCore::debugReevaluateStructure()
{
    if (!m_desc)
        return false;

    m_moduleRuntime.reevaluateStructuralStates(&m_structuralLinkRuntime);
syncDetachedFragmentsFromModuleRuntime();

    world::modules::ObjectRuntimeHitBuilder::rebuild(
        m_hitComponent,
        m_desc->typeId,
        *m_desc,
        m_moduleRuntime,
        m_structuralLinkRuntime,
        m_assemblyRuntime
    );

    m_hitVolumesDirty = false;

    std::cout << "[ShipCore] debugReevaluateStructure\n";
    return true;
}




bool ShipCore::debugSetStructuralLinkHealth(
    const std::string& linkId,
    float health,
    bool destroyed
)
{
    if (!m_desc)
        return false;

    // if (!m_structuralLinkRuntime.setLinkHealth(linkId, health, destroyed))
    //     return false;

    if (!m_structuralLinkRuntime.setLinkHealth(linkId, health, destroyed))
{
    std::cout
        << "[ShipCore] debugSetStructuralLinkHealth FAILED linkId="
        << linkId << "\n";
    return false;
}

if (const auto* link = m_structuralLinkRuntime.findById(linkId))
{
    std::cout
        << "[ShipCore] set structural link health:"
        << " id=" << link->id
        << " A=" << link->moduleAId
        << " B=" << link->moduleBId
        << " kind=" << static_cast<int>(link->kind)
        << " health=" << link->health
        << " destroyed=" << link->destroyed
        << " loadBearing=" << link->loadBearing
        << "\n";
}


    for (auto& v : m_hitComponent.volumes)
    {
        if (v.supportLinkVolume && v.supportLinkId == linkId)
        {
            v.health = std::max(0.0f, health);
            v.destroyed = destroyed || v.health <= 0.0f;
        }
    }

    m_moduleRuntime.reevaluateStructuralStates(&m_structuralLinkRuntime);
    syncDetachedFragmentsFromModuleRuntime();


    if (const auto* link = m_structuralLinkRuntime.findById(linkId))
        {
            const auto* a = m_moduleRuntime.findModule(link->moduleAId);
            const auto* b = m_moduleRuntime.findModule(link->moduleBId);

            if (a)
            {
                std::cout
                    << "[ShipCore] module after link damage:"
                    << " module=" << a->moduleId
                    << " state=" << static_cast<int>(a->state)
                    << " aliveSupports=" << a->aliveSupportCount
                    << " detachable=" << a->detachable
                    << "\n";
            }

            if (b)
            {
                std::cout
                    << "[ShipCore] module after link damage:"
                    << " module=" << b->moduleId
                    << " state=" << static_cast<int>(b->state)
                    << " aliveSupports=" << b->aliveSupportCount
                    << " detachable=" << b->detachable
                    << "\n";
            }
        }

    world::modules::ObjectRuntimeHitBuilder::rebuild(
        m_hitComponent,
        m_desc->typeId,
        *m_desc,
        m_moduleRuntime,
        m_structuralLinkRuntime,
        m_assemblyRuntime
    );

    m_hitVolumesDirty = false;

    return true;
}







void ShipCore::syncDetachedFragmentsFromModuleRuntime()
{
    if (!m_desc)
        return;

    if (!game::ship::geometry::AssemblyMeshLibrary::has(m_desc->typeId))
        return;

    const auto& assembly =
        game::ship::geometry::AssemblyMeshLibrary::get(m_desc->typeId);

    const glm::mat4 ownerModel =    m_transform.orientation;

    m_detachedFragmentRuntime.syncFromDetachedModules(
        ownerModel,
        assembly,
        m_assemblyRuntime,
        m_moduleRuntime,
        m_hitComponent
    );
}

void ShipCore::updateDetachedFragments(float dt)
{
    m_detachedFragmentRuntime.update(dt);
}


void ShipCore::updateRepairJobs(float dt)
{
    if (!m_desc)
        return;

    if (m_repairJobRuntime.activeJobCount() <= 0)
        return;

    const glm::mat4 ownerModel =   m_transform.orientation;

    std::vector<world::navigation::NavigationObstacle> obstacles =
        buildNavigationObstaclesForRepair("");

    const auto completed =
        m_repairJobRuntime.update(
            dt,
            ownerModel,
            m_detachedFragmentRuntime,
            obstacles
        );

    for (const auto& moduleId : completed)
    {
        debugReattachModuleById(moduleId);
    }
}





std::vector<game::simulation::ObjectRepairJobSnapshot>
ShipCore::buildRepairJobSnapshots() const
{
    const glm::mat4 ownerModel =
    m_transform.orientation;

    return m_repairJobRuntime.buildSnapshots(
        ownerModel,
        m_transform.worldPosition,
        m_detachedFragmentRuntime
    );
}

int ShipCore::activeRepairJobCount() const
{
    return m_repairJobRuntime.activeJobCount();
}






std::vector<world::navigation::NavigationObstacle>
ShipCore::buildNavigationObstaclesForRepair(
    const std::string& ignoredModuleId
) const
{
    (void)ignoredModuleId;

    std::vector<world::navigation::NavigationObstacle> out;

    const auto bodies =
        world::navigation::buildNavigationBodiesForShip(*this);

    const glm::mat4 ownerModel =
    m_transform.orientation;

    const glm::mat3 ownerRotation =
        glm::mat3(ownerModel);

    for (const auto& body : bodies)
    {
        world::navigation::NavigationObstacle o;

        o.shape =
            world::navigation::NavigationObstacleShape::Box;

        o.centerMeters =
            glm::dvec3(ownerModel * glm::vec4(body.center, 1.0f));

        o.halfExtentsMeters =
            glm::dvec3(body.halfExtents);

        o.localToWorldBasis =
            glm::dmat3(ownerRotation);

        // Conservative radius retained for contact/debug consumers.
        o.radiusMeters =
            glm::length(glm::dvec3(body.halfExtents));

        o.entityId = body.entityId;

        out.push_back(o);
    }

    if (out.empty())
    {
        world::navigation::NavigationObstacle o;
        o.shape = world::navigation::NavigationObstacleShape::Sphere;
        o.centerMeters = glm::dvec3(ownerModel * glm::vec4(0, 0, 0, 1));
        o.radiusMeters = 8.0;
        out.push_back(o);
    }

    return out;
}


}
