#include "GameSimulation.h"
#include <iostream>

#include "game/ship/ShipInitData.h"
#include "game/ship/ShipRoleType.h"
#include "game/ship/ShipRegistry.h"
#include "game/ship/ShipVisualIdentity.h"
#include "game/ship/descriptors/EliteCobraMk1.h"

#include "game/items/cryptocard/CryptoCard.h"
#include "game/player/ActorCodeGenerator.h"
#include "src/game/player/ActorIdProvider.h"

#include "src/world/descriptors/ObjectDescriptorRegistry.h"
#include "src/game/geometry/ObjectAssemblyRegistry.h"
#include "src/world/types/ObjectType.h"


#include "game/equipment/radar/RadarModule.h"
// #include "src/world/modules/ObjectHitBuilder.h"
#include "src/game/geometry/AssemblyMeshLibrary.h"

#include "src/world/modules/ObjectRuntimeHitBuilder.h"
#include "src/world/modules/HitVolumeSnapshotBuilder.h"

#include <limits>
#include <glm/gtx/norm.hpp>
#include "src/world/modules/ObjectMissingPartRequest.h"



GameSimulation::GameSimulation()
{
    // ===================== ObjectDescriptor =========================

    ObjectDescriptorRegistry::init();
    game::ship::geometry::ObjectAssemblyRegistry::init();



    // ========================= PLAYER =========================

    ShipVisualIdentity visualIdentity = {
        .shipType = "Cobra MK1",
        .shipName = "Jeraya"
    };

    ShipRegistry registry = {
        .instanceId      = 1,
        .ownerName       = "Jeraya",
        .ownerActor      = ActorIds::Player(),
        .registrationId  = "PL-0001",
        .homePort        = "Lave",
        .shipRole        = ShipRoleType::Civilian
    };

    auto* playerCard = new CryptoCard(
        generateActorCode(),
        "Player Access Card"
    );
    playerCard->actor = ActorIds::Player();

    ShipInitData initData;
    initData.visual = visualIdentity;
    initData.registry = registry;
    initData.initialInventory = {playerCard};

    m_playerId = spawnShip(
        ShipRole::Player,
        EliteCobraMk1::EliteCobraMk1Descriptor(),
        {0.0f, 50.0f, 10.0f},
        initData
    );

    // ========================= NPC #1 =========================

    visualIdentity = {
        .shipType = "Cobra MK3",
        .shipName = "Scarlet Hawk Moth"
    };

    registry.instanceId = 2;

    auto* npc1Card = new CryptoCard(
        generateActorCode(),
        "Player Access Card"
    );
    npc1Card->actor = ActorIds::Player();

    initData.visual = visualIdentity;
    initData.registry = registry;
    initData.initialInventory = {npc1Card};

    EntityId npcId1 = spawnShip(
        ShipRole::NPC,
        EliteCobraMk1::EliteCobraMk1Descriptor(),
        {20.0f, 0.0f, -50.0f},
        initData
    );

    // ========================= NPC #2 =========================

    visualIdentity.shipName = "Hooded snake";
    registry.instanceId = 3;

    auto* npc2Card = new CryptoCard(
        generateActorCode(),
        "Player Access Card"
    );
    npc2Card->actor = ActorIds::Player();

    initData.visual = visualIdentity;
    initData.registry = registry;
    initData.initialInventory = {npc2Card};

    EntityId npcId2 =spawnShip(
        ShipRole::NPC,
        EliteCobraMk1::EliteCobraMk1Descriptor(),
        {-20.0f, 50.0f, -50.0f},
        initData
    );


    // ================= Stantion Lexie Liu =========================
    // spawnStation(ObjectType::Station, {0, 0, -1000});

}




//                       ###              ##
//                        ##              ##
//  ##  ##   ######       ##    ####     #####    ####
//  ##  ##    ##  ##   #####       ##     ##     ##  ##
//  ##  ##    ##  ##  ##  ##    #####     ##     ######
//  ##  ##    #####   ##  ##   ##  ##     ## ##  ##
//   ######   ##       ######   #####      ###    #####
//           ####

void GameSimulation::update(double dt)
{
    static float npcRepairThinkTimer = 0.0f;
    npcRepairThinkTimer += static_cast<float>(dt);

    const bool npcRepairThinkTick = npcRepairThinkTimer >= 15.0f;

    if (npcRepairThinkTick)
        npcRepairThinkTimer = 0.0f;

    
    static bool initialized = false;

    if (!initialized)
    {
        initialized = true;
    }

    // Режим 1: нормальная мощность
      
    float fdt = static_cast<float>(dt);
    m_serverTime += dt;


        for (auto& [id, ship] : m_ships)
    {
        if (!ship)
            continue;

        ship->core().updateAssemblyRuntime(dt);
        ship->core().updateDetachedFragments(fdt);
        ship->core().updateRepairJobs(fdt);


        if (npcRepairThinkTick &&
            ship->core().role() != ShipRole::Player &&
            ship->core().activeRepairJobCount() == 0)
        {
            const auto missing =
                ship->core().buildMissingPartRequests();

            if (!missing.empty())
            {
                // Пока тупо первая недостающая деталь.
                // Позже добавим приоритет: cockpit > engine > weapon > panel.
                startBestRepairJobForMissingSlot(
                    id,
                    missing.front().targetModuleId
                );
            }
        }

        auto& core = ship->core();
        if (core.hitVolumesDirty())
        {
            const auto& desc = core.descriptor();

            world::modules::ObjectRuntimeHitBuilder::rebuild(
                core.hitComponent(),
                desc.typeId,
                desc,
                core.moduleRuntime(),
                core.structuralLinkRuntime(),
                core.assemblyRuntime()
            );

            core.clearHitVolumesDirty();
        }
    }

    for (auto& [id, obj] : m_staticObjects)
    {
        // Вращение/анимация модулей НЕ требует полного rebuild hit-volumes.
        // Геометрия hit-volume уже посчитана в локальных координатах.
        // При движении/вращении должен меняться только transform, а не структура volume.
        obj.assemblyRuntime.update(dt);
        obj.detachedFragmentRuntime.update(fdt);

        if (obj.hitVolumesDirty)
        {
            const auto& desc = ObjectDescriptorRegistry::get(obj.type);

            world::modules::ObjectRuntimeHitBuilder::rebuild(
                obj.hitComponent,
                obj.type,
                desc,
                obj.moduleRuntime,
                obj.structuralLinkRuntime,
                obj.assemblyRuntime
            );

            obj.hitVolumesDirty = false;
        }
    }



    // === 1. AI phase ===
    for (auto& [id, shipPtr] : m_ships)
    {
        Ship& ship = *shipPtr;        
        if (id == m_playerId)
            continue;

        ShipControlState aiControl = m_npcAiSystem.computeControl(ship, fdt);
        ship.setControlState(aiControl);
    }

    for (auto& [id, shipPtr] : m_ships)
    {
        Ship& ship = *shipPtr;
        ship.applyControl();
    }

    for (auto& [id, shipPtr] : m_ships)
    {
        Ship& ship = *shipPtr;
        ship.updatePhysics(fdt, m_world);
    }

    std::vector<ITransmitterSource*> signalSources;
    for (auto& [id, shipPtr] : m_ships)
    signalSources.push_back(shipPtr.get());

    WorldSignalTxSystem::collectFromSources(signalSources, m_worldSignals);

    for (auto& [id, shipPtr] : m_ships)
    {
        Ship& ship = *shipPtr;
        ship.updateSignals(fdt, m_worldSignals, m_planets, m_interferenceSources);
    }

    for (auto& [id, shipPtr] : m_ships)
    {
        Ship& ship = *shipPtr;

        std::vector<world::RadarContactInput> inputs;

        for (auto& [otherId, otherPtr] : m_ships)
        {
            if (id == otherId)
                continue;

            Ship& other = *otherPtr;
            const auto& otherTransform = other.core().transform();
            glm::vec3 otherPos = otherTransform.position;

            inputs.push_back({
                otherId,
                otherPos,
                other.core().desc().radarCrossSection
            });
        }



        ship.updatePerception(fdt, inputs);
    }


    m_snapshot.serverTime = m_serverTime;
    m_snapshot.ships.clear();
    m_snapshot.objects.clear();
    m_snapshot.signals = m_worldSignals;


    // ----- тут собираем snapshot для кораблей ----------

    for (auto& [id, shipPtr] : m_ships)
    {
        Ship& ship = *shipPtr;
        ShipSnapshot s;

        const auto& tr = ship.core().transform();

        s.id = id;
        s.typeId = ship.core().desc().typeId;
        s.role = ship.core().role();

        s.transform = tr;


 
        const auto& receptions = ship.core().signalResults();
        s.receptions = receptions;
        s.receptions = ship.core().signalResults();
        

        s.radarContacts = ship.core().radar().getContacts();
        s.shipCoreStatus = ship.core().getCoreStatus();

        // s.modules.clear();
        // for (const auto& mod : ship.core().moduleRuntime().modules())
        // {
        //     game::simulation::ObjectModuleSnapshot ms;
        //     ms.moduleId = mod.moduleId;
        //     ms.state = static_cast<uint8_t>(mod.state);
        //     ms.health = mod.health;
        //     s.modules.push_back(std::move(ms));
        // }

                
        s.modules.clear();

        const auto& runtimeModules = ship.core().moduleRuntime().modules();
        const auto& descModules = ship.core().desc().moduleDescriptors();

        std::unordered_map<std::string, const world::modules::ObjectModuleState*> runtimeById;
        runtimeById.reserve(runtimeModules.size());
        for (const auto& mod : runtimeModules)
        {
            runtimeById[mod.moduleId] = &mod;
        }

        for (const auto& descMod : descModules)
        {
            game::simulation::ObjectModuleSnapshot ms;

            ms.moduleId = descMod.moduleId;
            ms.parentModuleId = descMod.parentModuleId;
            ms.subsystemId = descMod.subsystemId;

            ms.maxHealth = descMod.maxHealth;
            ms.destructible = descMod.destructible;
            ms.detachable = descMod.detachable;
            ms.hangable = descMod.hangable;

            ms.destroyPolicy = static_cast<int>(descMod.destroyPolicy);
            ms.detachPolicy = static_cast<int>(descMod.detachPolicy);
            ms.attachmentType = static_cast<int>(descMod.attachmentType);

            ms.meshPartIds = descMod.meshPartIds;
            ms.supportModuleIds = descMod.supportModuleIds;
            ms.minSupportsForAttached = descMod.minSupportsForAttached;
            ms.minSupportsForStable = descMod.minSupportsForStable;

            auto itRt = runtimeById.find(descMod.moduleId);
            if (itRt != runtimeById.end() && itRt->second)
            {
                const auto* rt = itRt->second;
                ms.state = static_cast<uint8_t>(rt->state);
                ms.health = rt->health;
                ms.aliveSupportCount = rt->aliveSupportCount;
            }
            else
            {
                ms.state = 0;
                ms.health = descMod.maxHealth;
                ms.aliveSupportCount = 0;
            }

            s.modules.push_back(std::move(ms));
        }




        s.structuralLinks.clear();

        for (const auto& link : ship.core().structuralLinkRuntime().links())
        {
            game::simulation::StructuralLinkSnapshot ls;

            ls.id = link.id;
            ls.ownerModuleId = link.ownerModuleId;
            ls.moduleAId = link.moduleAId;
            ls.moduleBId = link.moduleBId;
            ls.kind = static_cast<int>(link.kind);

            ls.health = link.health;
            ls.maxHealth = link.maxHealth;
            ls.impulseTolerance = link.impulseTolerance;

            ls.loadBearing = link.loadBearing;
            ls.destroyed = link.destroyed;
            ls.autoGenerated = link.autoGenerated;

            ls.center = link.center;
            ls.halfSize = link.halfSize;
            ls.orientation = link.orientation;

            s.structuralLinks.push_back(std::move(ls));
        }





        s.assemblyModules = ship.core().assemblyRuntime().buildSnapshots();
        s.detachedFragments = ship.core().detachedFragmentRuntime().buildSnapshots();
        s.repairJobs = ship.core().buildRepairJobSnapshots();
        s.debugHitVolumes = world::modules::HitVolumeSnapshotBuilder::build(
            ship.core().hitComponent()
        );

        m_snapshot.ships.push_back(s);
    }


    // ----- тут собираем snapshot для объектов ----------

    for (auto& [id, obj] : m_staticObjects)
    {
        ObjectSnapshot o;

        o.id = id;
        o.type = obj.type;
        o.position = obj.position;
        // o.rotation = obj.rotation;
        o.orientation = obj.orientation;

        o.modules.clear();

        const auto& objDesc = ObjectDescriptorRegistry::get(obj.type);
        const auto& runtimeModules = obj.moduleRuntime.modules();
        const auto& descModules = objDesc.moduleDescriptors();

        std::unordered_map<std::string, const world::modules::ObjectModuleState*> runtimeById;
        runtimeById.reserve(runtimeModules.size());
        for (const auto& mod : runtimeModules)
        {
            runtimeById[mod.moduleId] = &mod;
        }

        for (const auto& descMod : descModules)
        {
            game::simulation::ObjectModuleSnapshot ms;

            ms.moduleId = descMod.moduleId;
            ms.parentModuleId = descMod.parentModuleId;
            ms.subsystemId = descMod.subsystemId;

            ms.maxHealth = descMod.maxHealth;
            ms.destructible = descMod.destructible;
            ms.detachable = descMod.detachable;
            ms.hangable = descMod.hangable;

            ms.destroyPolicy = static_cast<int>(descMod.destroyPolicy);
            ms.detachPolicy = static_cast<int>(descMod.detachPolicy);
            ms.attachmentType = static_cast<int>(descMod.attachmentType);

            ms.meshPartIds = descMod.meshPartIds;
            ms.supportModuleIds = descMod.supportModuleIds;
            ms.minSupportsForAttached = descMod.minSupportsForAttached;
            ms.minSupportsForStable = descMod.minSupportsForStable;

            auto itRt = runtimeById.find(descMod.moduleId);
            if (itRt != runtimeById.end() && itRt->second)
            {
                const auto* rt = itRt->second;
                ms.state = static_cast<uint8_t>(rt->state);
                ms.health = rt->health;
                ms.aliveSupportCount = rt->aliveSupportCount;
            }
            else
            {
                ms.state = 0;
                ms.health = descMod.maxHealth;
                ms.aliveSupportCount = 0;
            }

            o.modules.push_back(std::move(ms));
        }




        o.structuralLinks.clear();

        for (const auto& link : obj.structuralLinkRuntime.links())
        {
            game::simulation::StructuralLinkSnapshot ls;

            ls.id = link.id;
            ls.ownerModuleId = link.ownerModuleId;
            ls.moduleAId = link.moduleAId;
            ls.moduleBId = link.moduleBId;
            ls.kind = static_cast<int>(link.kind);

            ls.health = link.health;
            ls.maxHealth = link.maxHealth;
            ls.impulseTolerance = link.impulseTolerance;

            ls.loadBearing = link.loadBearing;
            ls.destroyed = link.destroyed;
            ls.autoGenerated = link.autoGenerated;

            ls.center = link.center;
            ls.halfSize = link.halfSize;
            ls.orientation = link.orientation;

            o.structuralLinks.push_back(std::move(ls));
        }



        o.assemblyModules = obj.assemblyRuntime.buildSnapshots();
        o.detachedFragments = obj.detachedFragmentRuntime.buildSnapshots();
        o.debugHitVolumes = world::modules::HitVolumeSnapshotBuilder::build(
            obj.hitComponent
        );

        m_snapshot.objects.push_back(o);
    }

}











void GameSimulation::setTick(uint32_t tick)
{
    m_snapshot.snapshotTick = tick;
}



EntityId GameSimulation::generateEntityId()
{
    EntityId id;
    id.value = m_nextEntityId++;
    return id;
}



EntityId GameSimulation::spawnShip(
    ShipRole role,
    const ShipDescriptor& descriptor,
    glm::vec3 position,
    const ShipInitData& initData,
    const glm::mat4& orientation
)
{
    // Ship ship;
    auto ship = std::make_unique<Ship>();

    EntityId id = generateEntityId();
    ship->setId(id);

    // ship.init(role, descriptor, position, initData);
    ship->init(role, descriptor, position, initData, orientation);

    // m_ships[id] = std::move(ship);
    m_ships[id] = std::move(ship);

    return id;
}



EntityId GameSimulation::spawnStation(
    ObjectType type,
    const glm::vec3& position,
    const glm::mat4& orientation
)
{
    EntityId id = generateEntityId();

    auto& obj = m_staticObjects[id];

    obj.id = id;
    obj.type = type; // ← ключевое изменение
    obj.position = position;
    obj.orientation = orientation;

    const auto& baseDesc = ObjectDescriptorRegistry::get(type);

    obj.moduleRuntime.init(baseDesc.moduleDescriptors());
    obj.structuralLinkRuntime.init(baseDesc.moduleDescriptors());
    obj.moduleRuntime.reevaluateStructuralStates(&obj.structuralLinkRuntime);   

    if (game::ship::geometry::AssemblyMeshLibrary::has(type))
    {
        const auto& assembly = game::ship::geometry::AssemblyMeshLibrary::get(type);
        obj.assemblyRuntime.init(assembly);
        obj.detachedFragmentRuntime.clear();
    }

    world::modules::ObjectRuntimeHitBuilder::rebuild(
        obj.hitComponent,
        type,
        baseDesc,
        obj.moduleRuntime,
        obj.structuralLinkRuntime,
        obj.assemblyRuntime
    );
    obj.hitVolumesDirty = false;

    return id;
}













void GameSimulation::applyControl(EntityId id, const ShipControlState& control)
{
    Ship* ship = getShip(id);
    if (!ship)
        return;

    ship->setControlState(control);
}




bool GameSimulation::debugSetShipStructuralLinkHealth(
    EntityId id,
    const std::string& linkId,
    float health,
    bool destroyed
)
{
    Ship* ship = getShip(id);
    if (!ship)
        return false;

    return ship->core().debugSetStructuralLinkHealth(
        linkId,
        health,
        destroyed
    );
}











WorldParams& GameSimulation::world()
{
    return m_world;
}

const std::vector<WorldSignal>& GameSimulation::worldSignals() const
{
    return m_worldSignals;
}

const std::vector<Planet>& GameSimulation::planets() const
{
    return m_planets;
}

const std::vector<InterferenceSource>& GameSimulation::interferenceSources() const
{
    return m_interferenceSources;
}

Ship* GameSimulation::getShip(EntityId id)
{
    auto it = m_ships.find(id);
    if (it == m_ships.end())
        return nullptr;
    return it->second.get();
}

const Ship* GameSimulation::getShip(EntityId id) const
{
    auto it = m_ships.find(id);
    if (it == m_ships.end())
        return nullptr;
    return it->second.get();
}

Ship* GameSimulation::playerShip()
{
    return getShip(m_playerId);
}

const Ship* GameSimulation::playerShip() const
{
    return getShip(m_playerId);
}


bool GameSimulation::debugDestroyShipModule(EntityId id, const std::string& moduleId)
{
    Ship* ship = getShip(id);
    if (!ship)
    {
        std::cout
            << "[GameSimulation] debugDestroyShipModule: ship not found, entityId="
            << id.value << "\n";
        return false;
    }

    return ship->core().debugDestroyModuleById(moduleId);
}


bool GameSimulation::debugDetachShipModule(EntityId id, const std::string& moduleId)
{
    Ship* ship = getShip(id);
    if (!ship)
        return false;

    return ship->core().debugDetachModuleById(moduleId);
}



bool GameSimulation::debugReattachShipModule(
    EntityId id,
    const std::string& moduleId
)
{
    Ship* ship = getShip(id);
    if (!ship)
        return false;

    return ship->core().debugReattachModuleById(moduleId);
}




bool GameSimulation::startShipRepairJob(
    EntityId id,
    const std::string& moduleId
)
{
    Ship* ship = getShip(id);
    if (!ship)
        return false;

    return ship->core().startRepairJobForModule(moduleId);
}




bool GameSimulation::ejectShipCockpitCapsule(EntityId id)
{
    Ship* ship = getShip(id);
    if (!ship)
    {
        std::cout
            << "[GameSimulation] ejectShipCockpitCapsule: ship not found, entityId="
            << id.value << "\n";
        return false;
    }

    return ship->core().ejectCockpitCapsule();
}






bool GameSimulation::debugHangShipModule(EntityId id, const std::string& moduleId)
{
    Ship* ship = getShip(id);
    if (!ship)
        return false;

    return ship->core().debugHangModuleById(moduleId);
}

bool GameSimulation::debugReevaluateShipStructure(EntityId id)
{
    Ship* ship = getShip(id);
    if (!ship)
        return false;

    return ship->core().debugReevaluateStructure();
}


bool GameSimulation::debugRestoreShipModule(EntityId id, const std::string& moduleId)
{
    Ship* ship = getShip(id);
    if (!ship)
    {
        std::cout
            << "[GameSimulation] debugRestoreShipModule: ship not found, entityId="
            << id.value << "\n";
        return false;
    }

    return ship->core().debugRestoreModuleById(moduleId);
}

bool GameSimulation::debugResetShipStructure(EntityId id)
{
    Ship* ship = getShip(id);
    if (!ship)
    {
        std::cout
            << "[GameSimulation] debugResetShipStructure: ship not found, entityId="
            << id.value << "\n";
        return false;
    }

    return ship->core().debugResetStructure();
}

void GameSimulation::debugResetAllShipStructures()
{
    for (auto& [id, ship] : m_ships)
    {
        if (ship)
            ship->core().debugResetStructure();
    }

    std::cout << "[GameSimulation] debugResetAllShipStructures\n";
}


void GameSimulation::setPlayerControl(const ShipControlState& control)
{
    m_playerControlState = control;
}

const SimulationSnapshot& GameSimulation::snapshot() const
{
    return m_snapshot;
}

std::unordered_map<EntityId, std::unique_ptr<Ship>>& GameSimulation::ships()
{
    return m_ships;
}


bool GameSimulation::startBestRepairJobForMissingSlot(
    EntityId targetShipId,
    const std::string& targetModuleId
)
{
    Ship* targetShip = getShip(targetShipId);
    if (!targetShip)
        return false;

    const auto requests =
        targetShip->core().buildMissingPartRequests();

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
            << "[GameSimulation] no missing slot targetModuleId="
            << targetModuleId << "\n";
        return false;
    }

    Ship* bestSourceShip = nullptr;
    std::string bestSourceModuleId;
    float bestDistance2 = std::numeric_limits<float>::max();

    const glm::vec3 targetShipPos =
        targetShip->core().transform().position;

    for (auto& [sourceId, sourceShipPtr] : m_ships)
    {
        if (!sourceShipPtr)
            continue;

        Ship* sourceShip = sourceShipPtr.get();

        for (const auto& fragment :
             sourceShip->core().detachedFragmentRuntime().fragments())
        {
            if (!fragment.salvageable && !fragment.repairable)
                continue;

            if (fragment.moduleClass != targetRequest->moduleClass)
                continue;

            if (!world::modules::replacementTagsCompatible(
                    targetRequest->acceptedReplacementTags,
                    fragment.providedReplacementTags))
            {
                continue;
            }

            const float dist2 =
                glm::length2(fragment.position - targetShipPos);

            if (dist2 < bestDistance2)
            {
                bestDistance2 = dist2;
                bestSourceShip = sourceShip;
                bestSourceModuleId = fragment.moduleId;
            }
        }
    }

    if (!bestSourceShip)
    {
        std::cout
            << "[GameSimulation] no compatible detached part for targetModuleId="
            << targetModuleId
            << " moduleClass="
            << targetRequest->moduleClass
            << "\n";
        return false;
    }

    std::cout
        << "[GameSimulation] repair missing slot:"
        << " targetShipId=" << targetShipId.value
        << " targetModuleId=" << targetModuleId
        << " sourceModuleId=" << bestSourceModuleId
        << "\n";

    return targetShip->core().startRepairJobForClaimedReplacement(
        targetModuleId,
        bestSourceShip->core().detachedFragmentRuntime(),
        bestSourceModuleId
    );
}




bool GameSimulation::startBestRepairJobForFirstMissingSlot(EntityId targetShipId)
{
    Ship* targetShip = getShip(targetShipId);
    if (!targetShip)
        return false;

    const auto missing =
        targetShip->core().buildMissingPartRequests();

    if (missing.empty())
    {
        std::cout
            << "[GameSimulation] no missing slots for shipId="
            << targetShipId.value << "\n";
        return false;
    }

    for (const auto& req : missing)
    {
        if (startBestRepairJobForMissingSlot(
                targetShipId,
                req.targetModuleId))
        {
            std::cout
                << "[GameSimulation] started repair for first available missing slot moduleId="
                << req.targetModuleId << "\n";
            return true;
        }
    }

    std::cout
        << "[GameSimulation] no compatible parts for any missing slot shipId="
        << targetShipId.value << "\n";

    return false;
}