#include "GameSimulation.h"
#include <iostream>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <algorithm>

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

#include "game/promo/PromoFlybyScenario.h"
#include "game/scene/GameSceneSetup.h"

#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "src/world/coordinates/WorldPosition.h"
#include "src/world/celestial/CelestialTypes.h"
#include "src/world/orbits/OrbitalMotion.h"
#include "src/game/navigation/DynamicMotionSystem.h"

namespace
{
    glm::dvec3 transformPointNoTranslation(
        const glm::mat4& m,
        const glm::dvec3& v
    )
    {
        const glm::dvec4 r =
            glm::dmat4(m) * glm::dvec4(v, 0.0);

        return glm::dvec3(r);
    }




    glm::mat4 localEulerDegToMatrix(
        const glm::dvec3& deg
    )
    {
        glm::mat4 r(1.0f);

        if (std::abs(deg.x) > 0.000001)
        {
            r =
                glm::rotate(
                    r,
                    glm::radians(static_cast<float>(deg.x)),
                    glm::vec3(1.0f, 0.0f, 0.0f)
                );
        }

        if (std::abs(deg.y) > 0.000001)
        {
            r =
                glm::rotate(
                    r,
                    glm::radians(static_cast<float>(deg.y)),
                    glm::vec3(0.0f, 1.0f, 0.0f)
                );
        }

        if (std::abs(deg.z) > 0.000001)
        {
            r =
                glm::rotate(
                    r,
                    glm::radians(static_cast<float>(deg.z)),
                    glm::vec3(0.0f, 0.0f, 1.0f)
                );
        }

        return r;
    }



    glm::mat4 makeHubAttachedObjectOrientation(
        const glm::mat4& hubOrientation,
        const glm::dvec3& localRotationDeg
    )
    {
        return
            hubOrientation *
            localEulerDegToMatrix(
                localRotationDeg
            );
    }









    glm::dvec3 safeNormalizeD(
        const glm::dvec3& v,
        const glm::dvec3& fallback
    )
    {
        const double len2 =
            glm::dot(v, v);

        if (len2 < 1e-12)
            return fallback;

        return v / std::sqrt(len2);
    }





    glm::dvec3 matAxisX(const glm::mat4& m)
    {
        return glm::dvec3(m[0]);
    }

    glm::dvec3 matAxisY(const glm::mat4& m)
    {
        return glm::dvec3(m[1]);
    }

    glm::dvec3 matAxisZ(const glm::mat4& m)
    {
        return glm::dvec3(m[2]);
    }

    double dotD(const glm::dvec3& a, const glm::dvec3& b)
    {
        return glm::dot(a, b);
    }






    game::simulation::ObjectModuleSnapshot buildModuleSnapshot(
        const ModuleDescriptor& descMod,
        const world::modules::ObjectModuleState* rt
    )
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

        if (rt)
        {
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

        return ms;
    }

    game::simulation::StructuralLinkSnapshot buildStructuralLinkSnapshot(
        const world::modules::StructuralLinkState& link
    )
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

        return ls;
    }
}

GameSimulation::GameSimulation()
{
    // ===================== ObjectDescriptor =========================

    ObjectDescriptorRegistry::init();
    game::ship::geometry::ObjectAssemblyRegistry::init();



    // ========================= PLAYER =========================

    // ShipVisualIdentity visualIdentity = {
    //     .shipType = "Cobra MK1",
    //     .shipName = "Jeraya"
    // };

    // ShipRegistry registry = {
    //     .instanceId      = 1,
    //     .ownerName       = "Jeraya",
    //     .ownerActor      = ActorIds::Player(),
    //     .registrationId  = "PL-0001",
    //     .homePort        = "Lave",
    //     .shipRole        = ShipRoleType::Civilian
    // };

    // auto* playerCard = new CryptoCard(
    //     generateActorCode(),
    //     "Player Access Card"
    // );
    // playerCard->actor = ActorIds::Player();

    // ShipInitData initData;
    // initData.visual = visualIdentity;
    // initData.registry = registry;
    // initData.initialInventory = {playerCard};

    // m_playerId = spawnShip(
    //     ShipRole::Player,
    //     EliteCobraMk1::EliteCobraMk1Descriptor(),
    //     {0.0f, 50.0f, 10.0f},
    //     initData
    // );

    // ========================= NPC #1 =========================

    // visualIdentity = {
    //     .shipType = "Cobra MK3",
    //     .shipName = "Scarlet Hawk Moth"
    // };

    // registry.instanceId = 2;

    // auto* npc1Card = new CryptoCard(
    //     generateActorCode(),
    //     "Player Access Card"
    // );
    // npc1Card->actor = ActorIds::Player();

    // initData.visual = visualIdentity;
    // initData.registry = registry;
    // initData.initialInventory = {npc1Card};

    // EntityId npcId1 = spawnShip(
    //     ShipRole::NPC,
    //     EliteCobraMk1::EliteCobraMk1Descriptor(),
    //     {20.0f, 0.0f, -50.0f},
    //     initData
    // );

    // // ========================= NPC #2 =========================

    // visualIdentity.shipName = "Hooded snake";
    // registry.instanceId = 3;

    // auto* npc2Card = new CryptoCard(
    //     generateActorCode(),
    //     "Player Access Card"
    // );
    // npc2Card->actor = ActorIds::Player();

    // initData.visual = visualIdentity;
    // initData.registry = registry;
    // initData.initialInventory = {npc2Card};

    // EntityId npcId2 =spawnShip(
    //     ShipRole::NPC,
    //     EliteCobraMk1::EliteCobraMk1Descriptor(),
    //     {-20.0f, 50.0f, -50.0f},
    //     initData
    // );


    // ================= Stantion Lexie Liu =========================
    //  spawnStation(ObjectType::Station, {0, 0, -1000});



    // ========================= INITIAL SCENE =========================



}




void GameSimulation::buildInitialScene()
{
    m_playerId =
        game::scene::buildInitialScene(*this);

    if constexpr (game::promo::PromoFlybyScenario::Enabled)
    {
        m_promoFlybyScenario.setup(*this);
    }
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







// ------------------------------------------------------------
// Celestial body velocity cache.
//
// Позиции тел приходят из CelestialRuntime в AU.
// Для хабов нам нужна не только локальная скорость вокруг планеты,
// но и скорость самой планеты вокруг звезды.
// Поэтому считаем производную позиции каждого небесного тела.
// ------------------------------------------------------------
// ------------------------------------------------------------
// Celestial body velocity cache.
//
// ВАЖНО:
// при dt == 0 не пересчитываем и не очищаем скорости.
// На старте они уже посеяны через setCelestialBodyKinematicStateAu().
// Иначе стартовый update(0.0) убивает скорость Земли,
// и игрок спаунится с неполным орбитальным вектором.
// ------------------------------------------------------------
if (dt > 0.000001)
{
    m_celestialBodyVelocitiesMetersPerSecond.clear();

    for (const auto& [bodyId, positionAu] : m_celestialBodyPositionsAu)
    {
        const glm::dvec3 positionMeters =
            positionAu * world::celestial::MetersPerAu;

        glm::dvec3 velocityMetersPerSecond {0.0};

        auto prevIt =
            m_previousCelestialBodyPositionsMeters.find(bodyId);

        if (prevIt != m_previousCelestialBodyPositionsMeters.end())
        {
            velocityMetersPerSecond =
                (positionMeters - prevIt->second) / dt;
        }

        m_celestialBodyVelocitiesMetersPerSecond[bodyId] =
            velocityMetersPerSecond;

        m_previousCelestialBodyPositionsMeters[bodyId] =
            positionMeters;
    }
}














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































    for (auto& [hubId, hub] : m_orbitalHubs)
    {
        if (!hub.motion.enabled)
            continue;

        if (!hub.parentBodyId.empty())
        {
            auto parentIt =
                m_celestialBodyPositionsAu.find(hub.parentBodyId);

            if (parentIt != m_celestialBodyPositionsAu.end())
            {
                hub.motion.centerMeters =
                    parentIt->second *
                    world::celestial::MetersPerAu;
            }
        }

        const glm::dvec3 hubPosMeters =
            world::orbits::computeOrbitPositionMeters(
                hub.motion,
                m_orbitalUniverseTimeSeconds
            );






























// Скорость хаба должна соответствовать фактическому смещению
// позиции хаба в текущей серверной симуляции.
//
// ВАЖНО:
// hub.worldPosition считается через m_orbitalUniverseTimeSeconds.
// Если universe time ускорен или дискретен, аналитическая орбитальная
// скорость может не совпасть с реальным смещением хаба между кадрами.
// Тогда корабль получает один вектор, а станция уезжает по другому.
//
// Поэтому для NavigationFrame используем производную фактической позиции.
const glm::dvec3 localOrbitVelocityMetersPerSecond =
    world::orbits::computeOrbitVelocityMetersPerSecond(
        hub.motion,
        m_orbitalUniverseTimeSeconds
    );

glm::dvec3 parentVelocityMetersPerSecond {0.0};

auto parentVelocityIt =
    m_celestialBodyVelocitiesMetersPerSecond.find(
        hub.parentBodyId
    );

if (parentVelocityIt !=
    m_celestialBodyVelocitiesMetersPerSecond.end())
{
    parentVelocityMetersPerSecond =
        parentVelocityIt->second;
}

// Полная мировая скорость хаба:
// скорость родительского тела + локальная орбитальная скорость хаба.
const glm::dvec3 hubVelocityMetersPerSecond =
    parentVelocityMetersPerSecond +
    localOrbitVelocityMetersPerSecond;

m_hubVelocityMetersPerSecond[hubId] =
    hubVelocityMetersPerSecond;

m_previousHubPositionMeters[hubId] =
    hubPosMeters;









        hub.worldPosition =
            world::coordinates::makeWorldPositionFromMeters(
                hubPosMeters
            );

        // Хаб НЕ вращается вокруг собственной оси.
        // Его orientation задаётся позже из HubNavigationFrame:
        // X = normal, Y = radial, Z = -prograde.
        hub.orientation = glm::mat4(1.0f);

    }




    rebuildHubNavigationFrames(dt);



    for (auto& [id, obj] : m_staticObjects)
    {
        if (obj.attachedToHub)
        {
            auto hubIt =
                m_orbitalHubs.find(obj.hubId);

            if (hubIt != m_orbitalHubs.end())
            {
                const auto& hub =
                    hubIt->second;

                const glm::dvec3 hubMeters =
                    world::coordinates::fullMeters(
                        hub.worldPosition
                    );

                const glm::dvec3 rotatedOffset =
                    obj.inheritHubOrientation
                        ? transformPointNoTranslation(
                            hub.orientation,
                            obj.hubLocalOffsetMeters
                        )
                        : obj.hubLocalOffsetMeters;

                obj.setWorldPositionMeters(
                    hubMeters + rotatedOffset
                );

                
                

                if (obj.inheritHubOrientation)
                {
                    obj.orientation =
                        makeHubAttachedObjectOrientation(
                            hub.orientation,
                            obj.hubLocalRotationDeg
                        );
                }







            }
        }
        else if (obj.orbitalMotion.enabled)
        {
                if (!obj.mapParentBodyId.empty())
                {
                    auto parentIt =
                        m_celestialBodyPositionsAu.find(obj.mapParentBodyId);

                    if (parentIt != m_celestialBodyPositionsAu.end())
                    {
                        obj.orbitalMotion.centerMeters =
                            parentIt->second * world::celestial::MetersPerAu;
                    }
                }

                const glm::dvec3 pos =
                    world::orbits::computeOrbitPositionMeters(
                        obj.orbitalMotion,
                        m_orbitalUniverseTimeSeconds
                    );

                obj.setWorldPositionMeters(pos);

                obj.orientation =
                    world::orbits::computeSelfRotation(
                        obj.orbitalMotion,
                        m_orbitalUniverseTimeSeconds
                    );
            }
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
                obj.staticSnapshotPayloadDirty = true;
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
        if (!shipPtr)
            continue;

        Ship& ship =
            *shipPtr;

        // Теперь это только attitude/orientation.
        // Линейное движение здесь не считается.
        ship.updatePhysics(fdt, m_world);
    }





    for (auto& [id, shipPtr] : m_ships)
    {
        if (!shipPtr)
            continue;

        auto& tr =
            shipPtr->core().transform();

        if (tr.motion.mode != game::navigation::MotionMode::HubTactical)
            continue;

        const auto* frame =
            hubNavigationFrame(tr.motion.hubId);

        if (!frame || !frame->valid)
            continue;


        const auto& control =
            shipPtr->core().control();

        game::navigation::DynamicMotionSystem::applyHubTacticalInput(
            tr.motion,
            *frame,
            fdt,
            control.targetSpeedRate,
            control.cruiseActive,
            control.forwardInput,
            control.liftInput,
            control.strafeInput,
            tr.forward(),
            tr.right(),
            tr.up()
        );


    }











    updateShipReferenceFrames(dt);

    rebuildNavigationGravityContext();

    // Сначала считаем гравитацию и орбитальный контекст
    // по текущей позиции.
    updateDynamicNavigationContext(dt);





    // Потом применяем гравитацию и двигаем корабль.
    for (auto& [id, shipPtr] : m_ships)
    {
        if (!shipPtr)
            continue;

        auto& tr =
            shipPtr->core().transform();

        if (tr.motion.mode != game::navigation::MotionMode::HubTactical)
            continue;

        const auto* frame =
            hubNavigationFrame(tr.motion.hubId);

        if (!frame || !frame->valid)
            continue;

        game::navigation::DynamicMotionSystem::updateHubTactical(
            tr.motion,
            tr.worldPosition,
            *frame,
            dt
        );





























        tr.referenceVelocityMetersPerSecond =
            tr.motion.referenceVelocityMps;
    }


    updateDynamicNavigationContext(dt);









    // debugLogHubStationPlayer();
    // debugLogHubFrameAxes();
    // debugLogStationOrientation();








    


            









    // for (auto& [id, shipPtr] : m_ships)
    // {
    //     Ship& ship = *shipPtr;

    //     const auto& tr =
    //         ship.core().transform();

    //     if (tr.motion.mode == game::navigation::MotionMode::HubTactical)
    //         continue;

    //     ship.updatePhysics(fdt, m_world);
    // }







    
    // debugLogHubPlayerChain(dt);



    // Старый огромный лог временно отключаем.
    // debugLogServerNavState(dt);

    // Главный диагностический лог движения игрока.
    debugLogPlayerMotion(dt);





    if constexpr (game::promo::PromoFlybyScenario::Enabled)
    {
        m_promoFlybyScenario.update(*this, fdt);
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

            inputs.push_back({
                otherId,
                otherTransform.worldPosition,
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

                
        auto& graph = s.graph;

        auto resendIt = m_shipGraphPayloadFramesRemaining.find(id);
        const bool sendStructuralGraph =
            m_initializedShipGraphIds.find(id) == m_initializedShipGraphIds.end() ||
            resendIt != m_shipGraphPayloadFramesRemaining.end();

        if (sendStructuralGraph)
        {
            graph.hasModules = true;
            graph.hasStructuralLinks = true;
            graph.hasAssemblyModules = true;
            graph.hasDebugHitVolumes = true;

            const auto& runtimeModules = ship.core().moduleRuntime().modules();
            const auto& descModules = ship.core().desc().moduleDescriptors();

            std::unordered_map<std::string, const world::modules::ObjectModuleState*> runtimeById;
            runtimeById.reserve(runtimeModules.size());
            for (const auto& mod : runtimeModules)
                runtimeById[mod.moduleId] = &mod;

            graph.modules.reserve(descModules.size());
            for (const auto& descMod : descModules)
            {
                const auto itRt = runtimeById.find(descMod.moduleId);
                const auto* rt = (itRt != runtimeById.end()) ? itRt->second : nullptr;
                graph.modules.push_back(buildModuleSnapshot(descMod, rt));
            }

            const auto& links = ship.core().structuralLinkRuntime().links();
            graph.structuralLinks.reserve(links.size());
            for (const auto& link : links)
                graph.structuralLinks.push_back(buildStructuralLinkSnapshot(link));

            graph.assemblyModules = ship.core().assemblyRuntime().buildSnapshots();

            graph.debugHitVolumes = world::modules::HitVolumeSnapshotBuilder::build(
                ship.core().hitComponent()
            );

            m_initializedShipGraphIds.insert(id);

            if (resendIt != m_shipGraphPayloadFramesRemaining.end())
            {
                --resendIt->second;
                if (resendIt->second <= 0)
                    m_shipGraphPayloadFramesRemaining.erase(resendIt);
            }

            ship.core().clearHitVolumesDirty();
        }
        else if (ship.core().hitVolumesDirty())
        {
            // Debug-only payload: hit volumes can change after rebuilds.
            // Do not resend modules/links just because debug geometry changed.
            graph.hasDebugHitVolumes = true;
            graph.debugHitVolumes = world::modules::HitVolumeSnapshotBuilder::build(
                ship.core().hitComponent()
            );
            ship.core().clearHitVolumesDirty();
        }

        auto detachedFragments =
            ship.core().detachedFragmentRuntime().buildSnapshots(
                ship.core().transform().worldPosition
            );


        const bool hadDetachedFragments =
            m_shipsWithDetachedFragmentPayload.find(id) != m_shipsWithDetachedFragmentPayload.end();

        if (!detachedFragments.empty() || hadDetachedFragments)
        {
            graph.hasDetachedFragments = true;
            graph.detachedFragments = std::move(detachedFragments);

            if (graph.detachedFragments.empty())
                m_shipsWithDetachedFragmentPayload.erase(id);
            else
                m_shipsWithDetachedFragmentPayload.insert(id);
        }

        auto repairJobs = ship.core().buildRepairJobSnapshots();
        const bool hadRepairJobs =
            m_shipsWithRepairJobPayload.find(id) != m_shipsWithRepairJobPayload.end();

        if (!repairJobs.empty() || hadRepairJobs)
        {
            graph.hasRepairJobs = true;
            graph.repairJobs = std::move(repairJobs);

            if (graph.repairJobs.empty())
                m_shipsWithRepairJobPayload.erase(id);
            else
                m_shipsWithRepairJobPayload.insert(id);
        }

        m_snapshot.ships.push_back(s);
    }


    // ----- тут собираем snapshot для объектов ----------

    for (auto& [id, obj] : m_staticObjects)
    {
        ObjectSnapshot o;

        o.id = id;
        o.type = obj.type;

        // Теперь obj.worldPosition — это настоящая double-позиция
        // (её нужно будет добавить в структуру StaticObjectData)
        // Истина — obj.worldPosition.
        // position оставляем только как legacy mirror.
        o.worldPosition = obj.worldPosition;
        o.setWorldPosition(obj.worldPosition);
        o.orientation = obj.orientation;

        // Вращение станции меняет только assemblyModules.
        // Тяжелые статические payload'ы не пересобираем и не шлем каждый fixed tick.
        bool sendStaticPayload = obj.staticSnapshotPayloadDirty;

        if (obj.staticSnapshotPayloadDirty)
        {
            obj.cachedModuleSnapshots.clear();

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

                obj.cachedModuleSnapshots.push_back(std::move(ms));
            }

            obj.cachedStructuralLinkSnapshots.clear();

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

                obj.cachedStructuralLinkSnapshots.push_back(std::move(ls));
            }

            obj.cachedDebugHitVolumeSnapshots =
                world::modules::HitVolumeSnapshotBuilder::build(obj.hitComponent);

            obj.staticSnapshotPayloadDirty = false;
        }

        auto& graph = o.graph;

        if (sendStaticPayload)
        {
            graph.hasModules = true;
            graph.hasStructuralLinks = true;
            graph.hasDebugHitVolumes = true;

            graph.modules = obj.cachedModuleSnapshots;
            graph.structuralLinks = obj.cachedStructuralLinkSnapshots;
            graph.debugHitVolumes = obj.cachedDebugHitVolumeSnapshots;
        }

        // Это единственная часть станции, которая реально меняется при вращении.
        graph.hasAssemblyModules = true;
        graph.hasDetachedFragments = true;

        graph.assemblyModules = obj.assemblyRuntime.buildSnapshots();
        graph.detachedFragments =
            obj.detachedFragmentRuntime.buildSnapshots(
                obj.worldPosition
            );

        m_snapshot.objects.push_back(o);
    }

}











void GameSimulation::markShipGraphDirty(EntityId id)
{
    // GameSimulation builds an internal snapshot every simulation tick,
    // while GameServer publishes only every m_snapshotInterval ticks.
    // Keep heavy graph payload alive for several frames so the published
    // snapshot cannot miss the dirty structural update.
    m_shipGraphPayloadFramesRemaining[id] = 6;
}


void GameSimulation::debugForceFullShipGraphPayload()
{
    // structure_debug.html needs a complete graph on demand,
    // but normal gameplay snapshots must stay sparse/lightweight.
    for (const auto& [id, ship] : m_ships)
    {
        if (!ship)
            continue;

        markShipGraphDirty(id);
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
    const glm::dvec3& positionMeters,
    const ShipInitData& initData,
    const glm::mat4& orientation
)
{
    auto ship = std::make_unique<Ship>();

    EntityId id = generateEntityId();
    ship->setId(id);

    ship->setTypeId(
        descriptor.typeId
    );

    // Пока Ship::init принимает vec3, но сразу внутри переводит в WorldPosition.
    // Поэтому здесь НЕ кастуем AU в float до последнего момента.
    ship->init(
        role,
        descriptor,
        glm::vec3(positionMeters - positionMeters), // временно ноль
        initData,
        orientation
    );

    ship->core().transform().setWorldPositionMeters(positionMeters);

    m_ships[id] = std::move(ship);
    markShipGraphDirty(id);

    return id;
}



EntityId GameSimulation::spawnStation(
    ObjectType type,
    const glm::dvec3& positionMeters,
    const glm::mat4& orientation
)
{
    EntityId id = generateEntityId();

    auto& obj = m_staticObjects[id];

    obj.id = id;
    obj.type = type;
    obj.setWorldPositionMeters(positionMeters);
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
    obj.staticSnapshotPayloadDirty = true;

    return id;
}




const std::unordered_map<EntityId, StaticObject>&
GameSimulation::staticObjects() const
{
    return m_staticObjects;
}


void GameSimulation::setCelestialBodyWorldPositionsAu(
    const std::unordered_map<std::string, glm::dvec3>& positionsAu
)
{
    m_celestialBodyPositionsAu = positionsAu;
}



void GameSimulation::setCelestialBodyKinematicStateAu(
    const std::unordered_map<std::string, glm::dvec3>& currentPositionsAu,
    const std::unordered_map<std::string, glm::dvec3>& previousPositionsAu,
    double sampleDtSeconds
)
{
    m_celestialBodyPositionsAu =
        currentPositionsAu;

    m_previousCelestialBodyPositionsMeters.clear();
    m_celestialBodyVelocitiesMetersPerSecond.clear();

    if (sampleDtSeconds <= 0.000001)
        return;

    for (const auto& [bodyId, currentAu] : currentPositionsAu)
    {
        const glm::dvec3 currentMeters =
            currentAu * world::celestial::MetersPerAu;

        m_previousCelestialBodyPositionsMeters[bodyId] =
            currentMeters;

        auto prevIt =
            previousPositionsAu.find(bodyId);

        if (prevIt == previousPositionsAu.end())
        {
            m_celestialBodyVelocitiesMetersPerSecond[bodyId] =
                glm::dvec3(0.0);

            continue;
        }

        const glm::dvec3 previousMeters =
            prevIt->second * world::celestial::MetersPerAu;

        m_celestialBodyVelocitiesMetersPerSecond[bodyId] =
            (currentMeters - previousMeters) / sampleDtSeconds;
    }
}







void GameSimulation::setOrbitalUniverseTimeSeconds(double t)
{
    m_orbitalUniverseTimeSeconds = t;
}


const game::navigation::HubNavigationFrame*
GameSimulation::hubNavigationFrame(
    const std::string& hubId
) const
{
    auto it =
        m_hubNavigationFrames.find(hubId);

    if (it == m_hubNavigationFrames.end())
        return nullptr;

    return &it->second;
}


void GameSimulation::rebuildHubNavigationFrames(double dt)
{
    for (auto& [hubId, hub] : m_orbitalHubs)
    {
        game::navigation::HubNavigationFrame frame;

        frame.hubId = hub.id;
        frame.parentBodyId = hub.parentBodyId;

        frame.originMeters =
            world::coordinates::fullMeters(
                hub.worldPosition
            );

        glm::dvec3 parentMeters {0.0};

        auto parentIt =
            m_celestialBodyPositionsAu.find(hub.parentBodyId);

        if (parentIt != m_celestialBodyPositionsAu.end())
        {
            parentMeters =
                parentIt->second *
                world::celestial::MetersPerAu;
        }

        const glm::dvec3 radial =
            safeNormalizeD(
                frame.originMeters - parentMeters,
                glm::dvec3(0.0, 1.0, 0.0)
            );

        // Скорость хаба.
        auto velIt =
            m_hubVelocityMetersPerSecond.find(hubId);

        if (velIt != m_hubVelocityMetersPerSecond.end())
        {
            frame.velocityMetersPerSecond =
                velIt->second;
        }
        else
        {
            frame.velocityMetersPerSecond =
                world::orbits::computeOrbitVelocityMetersPerSecond(
                    hub.motion,
                    m_orbitalUniverseTimeSeconds
                );
        }

        glm::dvec3 prograde =
            safeNormalizeD(
                frame.velocityMetersPerSecond,
                glm::dvec3(1.0, 0.0, 0.0)
            );

        // Убираем радиальную примесь, чтобы prograde был касательным.
        prograde =
            safeNormalizeD(
                prograde - radial * glm::dot(prograde, radial),
                glm::dvec3(1.0, 0.0, 0.0)
            );

        const glm::dvec3 normal =
            safeNormalizeD(
                glm::cross(prograde, radial),
                glm::dvec3(0.0, 0.0, 1.0)
            );

        // Пересобираем prograde через normal/radial,
        // чтобы оси были ортогональными.
        prograde =
            safeNormalizeD(
                glm::cross(radial, normal),
                prograde
            );

        frame.radialAxis = radial;
        frame.progradeAxis = prograde;
        frame.normalAxis = normal;

        // Пока prime ищем как первый модуль.
        // Позже лучше сохранить явно из initial_world_state.json.
        if (!hub.modules.empty())
        {
            const EntityId primeObjectId =
                hub.modules.front();

            auto objIt =
                m_staticObjects.find(primeObjectId);

            if (objIt != m_staticObjects.end())
                frame.primeModuleId = objIt->second.hubModuleId;
        }

        frame.valid = true;





        glm::mat4 hubOrientation(1.0f);

        hubOrientation[0] =
            glm::vec4(glm::vec3(frame.normalAxis), 0.0f);

        hubOrientation[1] =
            glm::vec4(glm::vec3(frame.radialAxis), 0.0f);

        hubOrientation[2] =
            glm::vec4(glm::vec3(-frame.progradeAxis), 0.0f);

        hubOrientation[3] =
            glm::vec4(0, 0, 0, 1);

        hub.orientation = hubOrientation;









        m_hubNavigationFrames[hubId] = frame;
    }
}








void GameSimulation::prepareReferenceFramesForSpawn()
{
    // ------------------------------------------------------------
    // Подготовка орбитальных хабов и reference frames ДО спауна
    // игрока в frame.
    //
    // Важно:
    // это НЕ полный simulation update.
    // Тут нет AI, physics, snapshot, debug logs.
    // Мы только приводим хабы/станции/reference frame
    // в корректное стартовое состояние.
    // ------------------------------------------------------------

    for (auto& [hubId, hub] : m_orbitalHubs)
    {
        if (!hub.motion.enabled)
            continue;

        if (!hub.parentBodyId.empty())
        {
            auto parentIt =
                m_celestialBodyPositionsAu.find(
                    hub.parentBodyId
                );

            if (parentIt != m_celestialBodyPositionsAu.end())
            {
                hub.motion.centerMeters =
                    parentIt->second *
                    world::celestial::MetersPerAu;
            }
        }

        const glm::dvec3 hubPosMeters =
            world::orbits::computeOrbitPositionMeters(
                hub.motion,
                m_orbitalUniverseTimeSeconds
            );

        const glm::dvec3 localOrbitVelocityMetersPerSecond =
            world::orbits::computeOrbitVelocityMetersPerSecond(
                hub.motion,
                m_orbitalUniverseTimeSeconds
            );

        glm::dvec3 parentVelocityMetersPerSecond {0.0};

        auto parentVelocityIt =
            m_celestialBodyVelocitiesMetersPerSecond.find(
                hub.parentBodyId
            );

        if (parentVelocityIt !=
            m_celestialBodyVelocitiesMetersPerSecond.end())
        {
            parentVelocityMetersPerSecond =
                parentVelocityIt->second;
        }

        const glm::dvec3 hubVelocityMetersPerSecond =
            parentVelocityMetersPerSecond +
            localOrbitVelocityMetersPerSecond;

        m_hubVelocityMetersPerSecond[hubId] =
            hubVelocityMetersPerSecond;

        m_previousHubPositionMeters[hubId] =
            hubPosMeters;

        hub.worldPosition =
            world::coordinates::makeWorldPositionFromMeters(
                hubPosMeters
            );

        hub.orientation =
            glm::mat4(1.0f);
    }

    rebuildHubNavigationFrames(0.0);

    // Обновляем объекты, прикреплённые к хабам,
    // чтобы станция уже была в правильной мировой позиции
    // до первого snapshot.
    for (auto& [id, obj] : m_staticObjects)
    {
        if (!obj.attachedToHub)
            continue;

        auto hubIt =
            m_orbitalHubs.find(obj.hubId);

        if (hubIt == m_orbitalHubs.end())
            continue;

        const auto& hub =
            hubIt->second;

        const glm::dvec3 hubMeters =
            world::coordinates::fullMeters(
                hub.worldPosition
            );

        const glm::dvec3 rotatedOffset =
            obj.inheritHubOrientation
                ? transformPointNoTranslation(
                    hub.orientation,
                    obj.hubLocalOffsetMeters
                )
                : obj.hubLocalOffsetMeters;

        obj.setWorldPositionMeters(
            hubMeters + rotatedOffset
        );

        if (obj.inheritHubOrientation)
        {
            obj.orientation =
                makeHubAttachedObjectOrientation(
                    hub.orientation,
                    obj.hubLocalRotationDeg
                );
        }
    }

    rebuildNavigationGravityContext();
}



















bool GameSimulation::setStaticObjectMapInfo(
    EntityId id,
    const std::string& name,
    const std::string& owner,
    int systemId,
    const std::string& parentBodyId,
    const std::string& hubId,
    const std::string& hubModuleId
)
{
    auto it = m_staticObjects.find(id);

    if (it == m_staticObjects.end())
        return false;

    it->second.displayName = name;
    it->second.ownerName = owner;
    it->second.mapSystemId = systemId;
    it->second.mapParentBodyId = parentBodyId;
    it->second.hubId = hubId;
    it->second.hubModuleId = hubModuleId;

    return true;
}


































bool GameSimulation::setStaticObjectOrbitalMotion(
    EntityId id,
    const world::orbits::OrbitalMotion& motion
)
{
    auto it = m_staticObjects.find(id);

    if (it == m_staticObjects.end())
        return false;

    it->second.orbitalMotion = motion;
    it->second.orbitalMotion.enabled = true;

    return true;
}







bool GameSimulation::registerOrbitalHub(
    const world::hubs::OrbitalHubRuntime& hub
)
{
    if (hub.id.empty())
        return false;

    m_orbitalHubs[hub.id] = hub;
    return true;
}

bool GameSimulation::attachStaticObjectToHub(
    EntityId objectId,
    const std::string& hubId,
    const std::string& hubModuleId,
    const glm::dvec3& localOffsetMeters,
    const glm::dvec3& localRotationDeg,
    bool inheritHubOrientation
)
{
    auto objIt =
        m_staticObjects.find(objectId);

    if (objIt == m_staticObjects.end())
        return false;

    auto hubIt =
        m_orbitalHubs.find(hubId);

    if (hubIt == m_orbitalHubs.end())
        return false;

    StaticObject& obj =
        objIt->second;

    obj.attachedToHub = true;
    obj.hubId = hubId;
    obj.hubModuleId = hubModuleId;
    obj.hubLocalOffsetMeters = localOffsetMeters;
    obj.hubLocalRotationDeg = localRotationDeg;
    obj.inheritHubOrientation = inheritHubOrientation;

    obj.orbitalMotion.enabled = false;

    hubIt->second.modules.push_back(objectId);

    return true;
}













void GameSimulation::updateStaticObjectOrbitParentParameters(
    const std::string& parentBodyId,
    double parentRadiusMeters,
    double parentGravitationalParameterM3s2,
    bool forceKeplerPeriod
)
{
    for (auto& [id, obj] : m_staticObjects)
    {
        if (!obj.orbitalMotion.enabled)
            continue;

        if (obj.mapParentBodyId != parentBodyId)
            continue;

        obj.orbitalMotion.parentRadiusMeters =
            parentRadiusMeters;

        if (forceKeplerPeriod)
        {
            obj.orbitalMotion.orbitalPeriodSeconds =
                world::orbits::computeCircularOrbitPeriodSeconds(
                    obj.orbitalMotion.parentRadiusMeters,
                    obj.orbitalMotion.altitudeMeters,
                    parentGravitationalParameterM3s2
                );
        }
    }

    // Важно:
    // orbital hubs НЕ являются StaticObject.
    // Они живут отдельно в m_orbitalHubs, поэтому им тоже надо пересчитать
    // parentRadiusMeters и Kepler-период.
    for (auto& [hubId, hub] : m_orbitalHubs)
    {
        if (!hub.motion.enabled)
            continue;

        if (hub.parentBodyId != parentBodyId)
            continue;

        hub.motion.parentRadiusMeters =
            parentRadiusMeters;

        if (forceKeplerPeriod)
        {
            hub.motion.orbitalPeriodSeconds =
                world::orbits::computeCircularOrbitPeriodSeconds(
                    hub.motion.parentRadiusMeters,
                    hub.motion.altitudeMeters,
                    parentGravitationalParameterM3s2
                );
        }
    }
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

    const bool changed = ship->core().debugSetStructuralLinkHealth(
        linkId,
        health,
        destroyed
    );
    if (changed)
        markShipGraphDirty(id);
    return changed;
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

    const bool changed = ship->core().debugDestroyModuleById(moduleId);
    if (changed)
        markShipGraphDirty(id);
    return changed;
}


bool GameSimulation::debugDetachShipModule(EntityId id, const std::string& moduleId)
{
    Ship* ship = getShip(id);
    if (!ship)
        return false;

    const bool changed = ship->core().debugDetachModuleById(moduleId);
    if (changed)
        markShipGraphDirty(id);
    return changed;
}



bool GameSimulation::debugReattachShipModule(
    EntityId id,
    const std::string& moduleId
)
{
    Ship* ship = getShip(id);
    if (!ship)
        return false;

    const bool changed = ship->core().debugReattachModuleById(moduleId);
    if (changed)
        markShipGraphDirty(id);
    return changed;
}




bool GameSimulation::startShipRepairJob(
    EntityId id,
    const std::string& moduleId
)
{
    Ship* ship = getShip(id);
    if (!ship)
        return false;

    const bool changed = ship->core().startRepairJobForModule(moduleId);
    if (changed)
        markShipGraphDirty(id);
    return changed;
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

    const bool changed = ship->core().ejectCockpitCapsule();
    if (changed)
        markShipGraphDirty(id);
    return changed;
}






bool GameSimulation::debugHangShipModule(EntityId id, const std::string& moduleId)
{
    Ship* ship = getShip(id);
    if (!ship)
        return false;

    const bool changed = ship->core().debugHangModuleById(moduleId);
    if (changed)
        markShipGraphDirty(id);
    return changed;
}

bool GameSimulation::debugReevaluateShipStructure(EntityId id)
{
    Ship* ship = getShip(id);
    if (!ship)
        return false;

    const bool changed = ship->core().debugReevaluateStructure();
    if (changed)
        markShipGraphDirty(id);
    return changed;
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

    const bool changed = ship->core().debugRestoreModuleById(moduleId);
    if (changed)
        markShipGraphDirty(id);
    return changed;
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

    const bool changed = ship->core().debugResetStructure();
    if (changed)
        markShipGraphDirty(id);
    return changed;
}

void GameSimulation::debugResetAllShipStructures()
{
    for (auto& [id, ship] : m_ships)
    {
        if (ship)
        {
            const bool changed = ship->core().debugResetStructure();
            if (changed)
                markShipGraphDirty(id);
        }
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

            const world::coordinates::WorldPosition fragmentWorldPosition =
                world::coordinates::translated(
                    sourceShip->core().transform().worldPosition,
                    glm::dvec3(fragment.position)
                );

            const float dist2 =
                targetShip->core().transform().distanceSquaredToWorldPosition(
                    fragmentWorldPosition
                );

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







glm::vec3 GameSimulation::promoWingCenterAtTime(float time) const
{
    const glm::vec3 playerPos {0.0f, 50.0f, 0.0f};
    const glm::vec3 flightDir {0.0f, 0.0f, -1.0f};

    const float wingSpeed = 120.0f;
    const float passHeight = 260.0f;
    const float spawnDistance = 1800.0f;

    const float forwardDist =
        -spawnDistance + wingSpeed * time;

    glm::vec3 center =
        playerPos + flightDir * forwardDist;

    center.y =
        playerPos.y + passHeight;

    // ВАЖНО:
    // Пока звено не пролетело игрока, НЕ учитываем разделение.
    // Иначе нос игрока начинает заранее уходить за будущей траекторией.
    const float splitStart =
        260.0f;

    float splitT =
        (forwardDist - splitStart) / 900.0f;

    splitT =
        std::clamp(splitT, 0.0f, 1.0f);

    splitT =
        splitT * splitT * splitT *
        (splitT * (splitT * 6.0f - 15.0f) + 10.0f);

    center += glm::vec3(
        0.0f,
        360.0f,
        -360.0f
    ) * splitT;

    return center;
}









glm::mat4 GameSimulation::makePromoLookOrientation(
    const glm::vec3& forward,
    const glm::vec3& upHint
) const
{
    glm::vec3 f = forward;

    if (glm::length2(f) < 0.000001f)
        f = glm::vec3(0.0f, 0.0f, -1.0f);

    f = glm::normalize(f);

    glm::vec3 r =
        glm::cross(f, upHint);

    if (glm::length2(r) < 0.000001f)
        r = glm::cross(f, glm::vec3(0.0f, 0.0f, 1.0f));

    r = glm::normalize(r);

    glm::vec3 u =
        glm::normalize(glm::cross(r, f));

    glm::mat4 m(1.0f);

    // Engine convention:
    // +X right, +Y up, -Z forward
    m[0] = glm::vec4(r, 0.0f);
    m[1] = glm::vec4(u, 0.0f);
    m[2] = glm::vec4(-f, 0.0f);
    m[3] = glm::vec4(0, 0, 0, 1);

    return m;
}

void GameSimulation::updatePromoPlayerTracking(float dt)
{
   

    auto it =
        m_ships.find(m_playerId);

    if (it == m_ships.end() || !it->second)
        return;

    Ship& playerShip =
        *it->second;

    auto& tr =
        playerShip.core().transform();

    auto& control =
        playerShip.core().control();

    // Фиксируем игрока как операторскую платформу.
    const glm::vec3 playerPos {0.0f, 50.0f, 0.0f};

    tr.setWorldPositionMeters(glm::dvec3(playerPos));
    tr.forwardVelocity = 0.0f;
    tr.targetSpeed = 0.0f;
    tr.localVelocity = glm::vec3(0.0f);

    tr.pitchRate = 0.0f;
    tr.yawRate = 0.0f;
    tr.rollRate = 0.0f;

    control.forwardInput = 0.0f;
    control.strafeInput = 0.0f;
    control.liftInput = 0.0f;

    control.pitchInput = 0.0f;
    control.yawInput = 0.0f;
    control.rollInput = 0.0f;

    control.cruiseActive = false;
    control.jumpActive = false;
    control.targetSpeedRate = 0.0f;

    const float localTime =
        std::fmod(
            static_cast<float>(m_serverTime),
            22.0f
        );

    const glm::vec3 target =
        promoWingCenterAtTime(localTime);

    const world::coordinates::WorldPosition targetWorldPosition =
        world::coordinates::makeWorldPositionFromMeters(
            glm::dvec3(target)
        );

    glm::vec3 forward =
        tr.worldPositionToLocalCell(targetWorldPosition);

    if (glm::length2(forward) < 0.000001f)
        forward = glm::vec3(0.0f, 0.0f, -1.0f);

    forward =
        glm::normalize(forward);

    // Когда звено проходит над кораблём, делаем roll 180°,
    // чтобы звено оставалось в верхней части кадра.
    //
    // forwardDist совпадает с promoWingCenterAtTime().
    const float wingSpeed = 120.0f;
    const float spawnDistance = 1800.0f;

    const float forwardDist =
        -spawnDistance + wingSpeed * localTime;

    float rollT =
    (forwardDist - 40.0f) / 520.0f;

    rollT =
        std::clamp(rollT, 0.0f, 1.0f);

    rollT =
        rollT * rollT * rollT *
        (rollT * (rollT * 6.0f - 15.0f) + 10.0f);

    glm::vec3 upHint =
        glm::mix(
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, -1.0f, 0.0f),
            rollT
        );

    glm::mat4 targetOrientation =
        makePromoLookOrientation(
            forward,
            upHint
        );

    glm::quat currentQ =
        glm::quat_cast(tr.orientation);

    glm::quat targetQ =
        glm::quat_cast(targetOrientation);

    // Никаких резких поворотов.
    const float turnResponse = 1.15f;

    const float blend =
        1.0f - std::exp(-turnResponse * dt);

    glm::quat q =
        glm::slerp(
            currentQ,
            targetQ,
            blend
        );

    tr.orientation =
        glm::toMat4(glm::normalize(q));
}





bool GameSimulation::resolveCelestialBodyMeters(
    const std::string& bodyId,
    glm::dvec3& outCenterMeters,
    double& outRadiusMeters
) const
{
    auto it = m_celestialBodyPositionsAu.find(bodyId);

    if (it == m_celestialBodyPositionsAu.end())
        return false;

    outCenterMeters =
        it->second * world::celestial::MetersPerAu;

    // Пока радиусы ещё не храним в GameSimulation.
    // Для Земли ставим правильный радиус, позже перенесём радиусы из CelestialRuntime.
    if (bodyId == "system_0.Sol.Земля")
    {
        outRadiusMeters = 6371000.0;
        return true;
    }

    outRadiusMeters = 0.0;
    return true;
}









game::navigation::ResolvedFrameState GameSimulation::resolveReferenceFrame(
    const game::navigation::ReferenceFrame& frame
) const
{
    using namespace game::navigation;

    ResolvedFrameState result;

    if (frame.type == ReferenceFrameType::OrbitalHub ||
        frame.type == ReferenceFrameType::HubModule)
    {
        const auto* hubFrame =
            hubNavigationFrame(frame.hubId);

        if (!hubFrame || !hubFrame->valid)
            return result;

        result.positionMeters =
            hubFrame->originMeters;

        result.velocityMetersPerSecond =
            hubFrame->velocityMetersPerSecond;

        result.orientation = glm::mat4(1.0f);

        result.orientation[0] =
            glm::vec4(glm::vec3(hubFrame->normalAxis), 0.0f);

        result.orientation[1] =
            glm::vec4(glm::vec3(hubFrame->radialAxis), 0.0f);

        result.orientation[2] =
            glm::vec4(glm::vec3(-hubFrame->progradeAxis), 0.0f);

        result.orientation[3] =
            glm::vec4(0, 0, 0, 1);

        result.positionMeters =
            hubFrame->localToWorldPosition(
                frame.localOffsetMeters
            );

        result.valid = true;
        return result;
    }

    return result;
}



bool GameSimulation::placeShipInReferenceFrame(
    EntityId shipId,
    const game::navigation::ReferenceFrame& frame
)
{
    auto it =
        m_ships.find(shipId);

    if (it == m_ships.end())
        return false;

    const auto resolved =
        resolveReferenceFrame(frame);

    if (!resolved.valid)
        return false;

    auto& tr =
        it->second->core().transform();

    tr.setWorldPositionMeters(
        resolved.positionMeters
    );



    const auto* hubFrame =
        hubNavigationFrame(frame.hubId);

    if (hubFrame && hubFrame->valid)
    {
        const glm::vec3 forward =
            glm::normalize(
                glm::vec3(
                    hubFrame->originMeters -
                    resolved.positionMeters
                )
            );

        tr.orientation =
            makePromoLookOrientation(
                forward,
                glm::vec3(hubFrame->radialAxis)
            );
    }





    const glm::vec3 worldVelocity =
        glm::vec3(resolved.velocityMetersPerSecond);




    tr.referenceVelocityMetersPerSecond =
        resolved.velocityMetersPerSecond;

    tr.motion.mode =
        game::navigation::MotionMode::HubTactical;

    tr.motion.hubId =
        frame.hubId;





    tr.motion.referenceVelocityMps =
        resolved.velocityMetersPerSecond;
    
    // TEST SCENARIO:
    // игрок появляется уже с орбитальным вектором хаба.
    // В будущем сюда будет приходить результат навигации/прыжка:
    // speed + direction + error.
    tr.motion.worldVelocityMps =
        resolved.velocityMetersPerSecond;

    // tr.motion.pendingReferenceVelocityMatch = true;

    tr.motion.desiredRelativeVelocityMps =
        glm::dvec3(0.0);

    
    tr.motion.localPositionMeters =
        frame.localOffsetMeters;

    // ВАЖНО:
    // Игрок не наследует скорость хаба автоматически.
    // Хаб — ориентир, не родительская тележка.
    // TEST SPAWN:
    // игрок появляется уже на той же орбите, что и хаб.
    // Это НЕ автоматическое наследование в механике,
    // а только стартовое условие сценария.
    tr.motion.worldVelocityMps =
        resolved.velocityMetersPerSecond;

    tr.motion.gravityAccelerationMps2 =
        glm::dvec3(0.0);

    tr.motion.primaryGravityBodyId.clear();
    tr.motion.orbitalCorridorId.clear();
    tr.motion.orbitalCorridorState = 0;

    tr.motion.targetForwardSpeedMps = 0.0;
    tr.motion.forwardSpeedMps = 0.0;
    tr.motion.strafeSpeedMps = 0.0;
    tr.motion.liftSpeedMps = 0.0;







    tr.motion.lockedToFramePosition = false;

    tr.forwardVelocity = 0.0f;
    tr.targetSpeed = 0.0f;
    tr.localVelocity = glm::vec3(0.0f);




    m_shipReferenceBindings[shipId] = {
        frame,
        false
    };

    return true;
}


void GameSimulation::updateShipReferenceFrames(double dt)
{
    for (auto& [shipId, binding] : m_shipReferenceBindings)
    {
        Ship* ship = getShip(shipId);
        if (!ship)
            continue;

        const auto resolved =
            resolveReferenceFrame(binding.frame);

        if (!resolved.valid)
            continue;

        auto& tr =
            ship->core().transform();

        // Обновляем только скорость системы отсчёта.
        // Позицию свободного корабля не телепортируем.
        tr.referenceVelocityMetersPerSecond =
            resolved.velocityMetersPerSecond;


        tr.motion.referenceVelocityMps =
    resolved.velocityMetersPerSecond;

if (tr.motion.pendingReferenceVelocityMatch)
{
    const double referenceSpeed =
        glm::length(
            resolved.velocityMetersPerSecond
        );

    // Ждём, пока reference frame получит полноценную мировую скорость.
    // Для хаба Земли это примерно 30 км/с.
    // На первом кадре она может быть только локальной скоростью орбиты.
    if (referenceSpeed > 10000.0)
    {
        tr.motion.worldVelocityMps =
            resolved.velocityMetersPerSecond;

        tr.motion.desiredRelativeVelocityMps =
            glm::dvec3(0.0);

        tr.motion.localVelocityMps =
            glm::dvec3(0.0);

        tr.motion.pendingReferenceVelocityMatch = false;
    }
}








        // Жёсткая позиционная привязка нужна только потом:
        // docked / landed / attached.
        if (binding.lockPositionToFrame)
        {
            tr.setWorldPositionMeters(
                resolved.positionMeters
            );
        }
    }
}








void GameSimulation::rebuildNavigationGravityContext()
{
    m_gravityBodies.clear();
    m_orbitalCorridors.clear();

    glm::dvec3 earthCenterMeters {0.0};
    double earthRadiusMeters = 0.0;

    const std::string earthId =
        "system_0.Sol.Земля";

    if (!resolveCelestialBodyMeters(
            earthId,
            earthCenterMeters,
            earthRadiusMeters))
    {
        return;
    }

    game::navigation::GravityBody earth;

    earth.id =
        earthId;

    earth.centerMeters =
        earthCenterMeters;

    earth.radiusMeters =
        earthRadiusMeters;

    earth.gravitationalParameterM3s2 =
        3.986004418e14;

    earth.atmosphereRadiusMeters =
        earth.radiusMeters + 120000.0;

    earth.influenceRadiusMeters =
        50000000.0;

    m_gravityBodies.push_back(
        earth
    );

    auto hubIt =
        m_orbitalHubs.find("earth_orbital_hub");

    if (hubIt != m_orbitalHubs.end())
    {
        const auto& hub =
            hubIt->second;

        game::navigation::OrbitalCorridor corridor;

        corridor.id =
            "earth_orbital_hub_corridor";

        corridor.parentBodyId =
            earthId;

        corridor.hubId =
            hub.id;

        corridor.targetAltitudeMeters =
            hub.motion.altitudeMeters;

        corridor.halfWidthMeters =
            50000.0;

        corridor.dangerBelowMeters =
            50000.0;

        corridor.escapeAboveMeters =
            50000.0;

        m_orbitalCorridors.push_back(
            corridor
        );
    }
}


void GameSimulation::updateDynamicNavigationContext(double dt)
{
    (void)dt;

    for (auto& [id, shipPtr] : m_ships)
    {
        if (!shipPtr)
            continue;

        auto& tr =
            shipPtr->core().transform();

        const glm::dvec3 positionMeters =
            world::coordinates::fullMeters(
                tr.worldPosition
            );

        const auto gravity =
            game::navigation::GravityFieldSystem::sample(
                positionMeters,
                m_gravityBodies
            );

        tr.motion.gravityAccelerationMps2 =
            gravity.accelerationMps2;






            static int row = 0;

if (row < 1200)
{
    std::ofstream out(
        "gravity_debug.csv",
        row == 0
            ? std::ios::out
            : std::ios::app
    );

    if (row == 0)
    {
        out
            << "row,"
            << "worldX,worldY,worldZ,"
            << "localX,localY,localZ,"
            << "worldVx,worldVy,worldVz,"
            << "gravityX,gravityY,gravityZ,"
            << "gravityMag,"
            << "referenceVx,referenceVy,referenceVz\n";
    }

    const auto& tr =
        shipPtr->core().transform();

    const double gravityMag =
        glm::length(
            tr.motion.gravityAccelerationMps2
        );

    const glm::dvec3 p =
        world::coordinates::fullMeters(
            tr.worldPosition
        );

    out
        << row << ","
        << p.x << ","
        << p.y << ","
        << p.z << ","

        << tr.motion.localPositionMeters.x << ","
        << tr.motion.localPositionMeters.y << ","
        << tr.motion.localPositionMeters.z << ","

        << tr.motion.worldVelocityMps.x << ","
        << tr.motion.worldVelocityMps.y << ","
        << tr.motion.worldVelocityMps.z << ","

        << tr.motion.gravityAccelerationMps2.x << ","
        << tr.motion.gravityAccelerationMps2.y << ","
        << tr.motion.gravityAccelerationMps2.z << ","

        << gravityMag << ","

        << tr.motion.referenceVelocityMps.x << ","
        << tr.motion.referenceVelocityMps.y << ","
        << tr.motion.referenceVelocityMps.z
        << "\n";

    ++row;
}












        tr.motion.primaryGravityBodyId =
            gravity.primaryBodyId;

        tr.motion.primaryGravityDistanceMeters =
            gravity.primaryDistanceMeters;

        tr.motion.primaryGravityAltitudeMeters =
            gravity.primaryAltitudeMeters;

        tr.motion.primaryGravityAccelerationMps2 =
            gravity.primaryAccelerationMps2;

        tr.motion.orbitalCorridorId.clear();
        tr.motion.orbitalCorridorState = 0;

        tr.motion.orbitalAltitudeMeters = 0.0;
        tr.motion.orbitalAltitudeErrorMeters = 0.0;
        tr.motion.orbitalTargetSpeedMps = 0.0;
        tr.motion.orbitalTangentialSpeedMps = 0.0;
        tr.motion.orbitalRadialSpeedMps = 0.0;
        tr.motion.orbitalSpeedErrorMps = 0.0;

        for (const auto& body : m_gravityBodies)
        {
            if (body.id != gravity.primaryBodyId)
                continue;

            for (const auto& corridor : m_orbitalCorridors)
            {
                if (corridor.parentBodyId != body.id)
                    continue;

                const auto sample =
                    game::navigation::OrbitalCorridorSystem::classify(
                        positionMeters,
                        tr.motion.worldVelocityMps,
                        body,
                        corridor
                    );

                if (!sample.valid)
                    continue;

                tr.motion.orbitalCorridorId =
                    sample.corridorId;

                tr.motion.orbitalCorridorState =
                    static_cast<int>(sample.state);

                tr.motion.orbitalAltitudeMeters =
                    sample.altitudeMeters;

                tr.motion.orbitalAltitudeErrorMeters =
                    sample.altitudeErrorMeters;

                tr.motion.orbitalTargetSpeedMps =
                    sample.targetCircularSpeedMps;

                tr.motion.orbitalTangentialSpeedMps =
                    sample.tangentialSpeedMps;

                tr.motion.orbitalRadialSpeedMps =
                    sample.radialSpeedMps;

                tr.motion.orbitalSpeedErrorMps =
                    sample.speedErrorMps;

                break;
            }

            break;
        }
    }
}































void GameSimulation::debugLogServerNavState(double dt)
{
    static int row = 0;

    if (row >= 3600)
        return;

    const auto* frame =
        hubNavigationFrame("earth_orbital_hub");

    auto hubIt =
        m_orbitalHubs.find("earth_orbital_hub");

    const Ship* player =
        playerShip();

    const StaticObject* station = nullptr;

    for (const auto& [id, obj] : m_staticObjects)
    {
        if (obj.hubId == "earth_orbital_hub" &&
            obj.hubModuleId == "earth_high_orbital")
        {
            station = &obj;
            break;
        }
    }

    std::ofstream out(
        "server_nav_state_debug.csv",
        row == 0 ? std::ios::out : std::ios::app
    );

    if (row == 0)
    {
        out
            << "row,time,dt,"
            << "playerX,playerY,playerZ,"
            << "hubX,hubY,hubZ,"
            << "parentX,parentY,parentZ,"
            << "hubParentDist,"
            << "hubDeltaFromStart,"
            << "hubAngleDeg,"
            << "hubAngularDeltaDeg,"
            << "hubSpeedMps,"
            << "hubRadialSpeedMps,"
            << "hubTangentialSpeedMps,"
            << "stationX,stationY,stationZ,"
            << "playerHubDist,playerStationDist,hubStationDist,"
            << "playerLocalX,playerLocalY,playerLocalZ,"
            << "motionLocalX,motionLocalY,motionLocalZ,"
            << "motionLocalVx,motionLocalVy,motionLocalVz,"
            << "refVx,refVy,refVz,"
            << "forwardInput,liftInput,strafeInput,targetSpeedRate,cruiseActive,"
            << "frameRadialX,frameRadialY,frameRadialZ,"
            << "frameProgradeX,frameProgradeY,frameProgradeZ,"
            << "frameNormalX,frameNormalY,frameNormalZ,"
            << "stationXx,stationXy,stationXz,"
            << "stationYx,stationYy,stationYz,"
            << "stationZx,stationZy,stationZz,"
            << "dotStationYRadial,"
            << "dotStationZNormal,"
            << "dotStationXPrograde\n";
    }

    glm::dvec3 playerM {0.0};
    glm::dvec3 hubM {0.0};
    glm::dvec3 stationM {0.0};

    glm::dvec3 parentM {0.0};

    static bool hubOrbitDebugInitialized = false;
    static glm::dvec3 hubStartM {0.0};
    static glm::dvec3 hubStartRadial {1.0, 0.0, 0.0};
    static double hubStartAngleDeg = 0.0;

    glm::dvec3 playerLocal {0.0};
    glm::dvec3 motionLocal {0.0};
    glm::dvec3 motionLocalV {0.0};
    glm::dvec3 refV {0.0};

    float forwardInput = 0.0f;
    float liftInput = 0.0f;
    float strafeInput = 0.0f;
    float targetSpeedRate = 0.0f;
    int cruiseActive = 0;

    glm::dvec3 radial {0.0};
    glm::dvec3 prograde {0.0};
    glm::dvec3 normal {0.0};

    glm::dvec3 sx {0.0};
    glm::dvec3 sy {0.0};
    glm::dvec3 sz {0.0};

    bool havePlayer = false;
    bool haveHub = false;
    bool haveStation = false;

    if (player)
    {
        const auto& tr =
            player->core().transform();
        
        const auto& c =
            player->core().control();

        playerM =
            world::coordinates::fullMeters(
                tr.worldPosition
            );

        motionLocal =
            tr.motion.localPositionMeters;

        motionLocalV =
            tr.motion.localVelocityMps;

        refV =
            tr.referenceVelocityMetersPerSecond;

        forwardInput = c.forwardInput;
        liftInput = c.liftInput;
        strafeInput = c.strafeInput;
        targetSpeedRate = c.targetSpeedRate;
        cruiseActive = c.cruiseActive ? 1 : 0;

        havePlayer = true;
    }



    if (hubIt != m_orbitalHubs.end())
    {
        hubM =
            world::coordinates::fullMeters(
                hubIt->second.worldPosition
            );

        auto parentIt =
            m_celestialBodyPositionsAu.find(
                hubIt->second.parentBodyId
            );

        if (parentIt != m_celestialBodyPositionsAu.end())
        {
            parentM =
                parentIt->second *
                world::celestial::MetersPerAu;
        }

        haveHub = true;
    }



    if (station)
    {
        stationM =
            world::coordinates::fullMeters(
                station->worldPosition
            );

        sx = glm::dvec3(station->orientation[0]);
        sy = glm::dvec3(station->orientation[1]);
        sz = glm::dvec3(station->orientation[2]);

        haveStation = true;
    }

    if (frame && frame->valid)
    {
        radial = frame->radialAxis;
        prograde = frame->progradeAxis;
        normal = frame->normalAxis;

        if (havePlayer)
        {
            playerLocal =
                frame->worldToLocalPosition(playerM);
        }
    }

    const double playerHubDist =
        havePlayer && haveHub
            ? glm::length(playerM - hubM)
            : -1.0;

    const double playerStationDist =
        havePlayer && haveStation
            ? glm::length(playerM - stationM)
            : -1.0;

    const double hubStationDist =
        haveHub && haveStation
            ? glm::length(hubM - stationM)
            : -1.0;

    const double dotStationYRadial =
        haveStation && frame && frame->valid
            ? glm::dot(sy, radial)
            : 0.0;

    const double dotStationZNormal =
        haveStation && frame && frame->valid
            ? glm::dot(sz, normal)
            : 0.0;

    const double dotStationXPrograde =
        haveStation && frame && frame->valid
            ? glm::dot(sx, prograde)
            : 0.0;








double hubParentDist = -1.0;
double hubDeltaFromStart = -1.0;
double hubAngleDeg = 0.0;
double hubAngularDeltaDeg = 0.0;
double hubSpeedMps = 0.0;
double hubRadialSpeedMps = 0.0;
double hubTangentialSpeedMps = 0.0;

if (haveHub)
{
    const glm::dvec3 radialVec =
        hubM - parentM;

    hubParentDist =
        glm::length(radialVec);

    const glm::dvec3 radialDir =
        hubParentDist > 1e-6
            ? radialVec / hubParentDist
            : glm::dvec3(1.0, 0.0, 0.0);

    if (!hubOrbitDebugInitialized)
    {
        hubOrbitDebugInitialized = true;
        hubStartM = hubM;
        hubStartRadial = radialDir;
        hubStartAngleDeg = 0.0;
    }

    hubDeltaFromStart =
        glm::length(hubM - hubStartM);

    const double dotStart =
        glm::clamp(
            glm::dot(hubStartRadial, radialDir),
            -1.0,
            1.0
        );

    hubAngleDeg =
        glm::degrees(std::acos(dotStart));

    hubAngularDeltaDeg =
        hubAngleDeg - hubStartAngleDeg;

    auto hubVelIt =
        m_hubVelocityMetersPerSecond.find("earth_orbital_hub");

    if (hubVelIt != m_hubVelocityMetersPerSecond.end())
    {
        const glm::dvec3 hubV =
            hubVelIt->second;

        hubSpeedMps =
            glm::length(hubV);

        hubRadialSpeedMps =
            glm::dot(hubV, radialDir);

        const glm::dvec3 radialV =
            radialDir * hubRadialSpeedMps;

        hubTangentialSpeedMps =
            glm::length(hubV - radialV);
    }
}













    out
        << row << ","
        << std::fixed << std::setprecision(6)
        << m_orbitalUniverseTimeSeconds << ","
        << dt << ","

        << playerM.x << "," << playerM.y << "," << playerM.z << ","
        << hubM.x << "," << hubM.y << "," << hubM.z << ","

        << parentM.x << "," << parentM.y << "," << parentM.z << ","
        << hubParentDist << ","
        << hubDeltaFromStart << ","
        << hubAngleDeg << ","
        << hubAngularDeltaDeg << ","
        << hubSpeedMps << ","
        << hubRadialSpeedMps << ","
        << hubTangentialSpeedMps << ","

        << stationM.x << "," << stationM.y << "," << stationM.z << ","

        << playerHubDist << ","
        << playerStationDist << ","
        << hubStationDist << ","

        << playerLocal.x << "," << playerLocal.y << "," << playerLocal.z << ","

        << motionLocal.x << "," << motionLocal.y << "," << motionLocal.z << ","
        << motionLocalV.x << "," << motionLocalV.y << "," << motionLocalV.z << ","

        << refV.x << "," << refV.y << "," << refV.z << ","

        << forwardInput << ","
        << liftInput << ","
        << strafeInput << ","
        << targetSpeedRate << ","
        << cruiseActive << ","

        << radial.x << "," << radial.y << "," << radial.z << ","
        << prograde.x << "," << prograde.y << "," << prograde.z << ","
        << normal.x << "," << normal.y << "," << normal.z << ","

        << sx.x << "," << sx.y << "," << sx.z << ","
        << sy.x << "," << sy.y << "," << sy.z << ","
        << sz.x << "," << sz.y << "," << sz.z << ","

        << dotStationYRadial << ","
        << dotStationZNormal << ","
        << dotStationXPrograde
        << "\n";

    ++row;
}








void GameSimulation::debugLogPlayerMotion(double dt)
{
    static int row = 0;

    if (row >= 2400)
        return;

    const Ship* player =
        playerShip();

    if (!player)
        return;

    const auto& core =
        player->core();

    const auto& tr =
        core.transform();

    const auto& control =
        core.control();

    const auto* frame =
        hubNavigationFrame(tr.motion.hubId);

    const glm::dvec3 playerWorld =
        world::coordinates::fullMeters(
            tr.worldPosition
        );

    const glm::dvec3 shipForward =
        safeNormalizeD(
            glm::dvec3(tr.forward()),
            glm::dvec3(0.0, 0.0, -1.0)
        );

    const glm::dvec3 shipRight =
        safeNormalizeD(
            glm::dvec3(tr.right()),
            glm::dvec3(1.0, 0.0, 0.0)
        );

    const glm::dvec3 shipUp =
        safeNormalizeD(
            glm::dvec3(tr.up()),
            glm::dvec3(0.0, 1.0, 0.0)
        );

    glm::dvec3 worldVelocity {0.0};
    glm::dvec3 relativeWorldVelocity {0.0};
    glm::dvec3 velocityDir {0.0};

    double relativeSpeed = 0.0;
    double forwardVelocityDot = 0.0;
    double forwardVelocityAngleDeg = 0.0;

    glm::dvec3 frameVelocity {0.0};
    glm::dvec3 frameRadial {0.0};
    glm::dvec3 framePrograde {0.0};
    glm::dvec3 frameNormal {0.0};

    int frameValid = 0;

    if (frame && frame->valid)
    {
        frameValid = 1;

        frameVelocity =
            frame->velocityMetersPerSecond;

        frameRadial =
            frame->radialAxis;

        framePrograde =
            frame->progradeAxis;

        frameNormal =
            frame->normalAxis;

        worldVelocity =
            frame->localToWorldVelocity(
                tr.motion.localVelocityMps
            );

        relativeWorldVelocity =
            worldVelocity - frameVelocity;
    }
    else
    {
        relativeWorldVelocity =
            tr.motion.localVelocityMps;
    }

    relativeSpeed =
        glm::length(relativeWorldVelocity);

    if (relativeSpeed > 0.001)
    {
        velocityDir =
            relativeWorldVelocity / relativeSpeed;

        forwardVelocityDot =
            glm::clamp(
                glm::dot(shipForward, velocityDir),
                -1.0,
                1.0
            );

        forwardVelocityAngleDeg =
            glm::degrees(
                std::acos(forwardVelocityDot)
            );
    }

    std::ofstream out(
        "server_motion_debug.csv",
        row == 0 ? std::ios::out : std::ios::app
    );

    if (row == 0)
    {
        out
            << "row,time,dt,"
            << "mode,hubId,frameValid,"
            << "controlTick,"
            << "pitchInput,yawInput,rollInput,"
            << "forwardInput,liftInput,strafeInput,"
            << "targetSpeedRate,cruiseActive,jumpActive,"
            << "playerWorldX,playerWorldY,playerWorldZ,"
            << "motionLocalX,motionLocalY,motionLocalZ,"
            << "motionLocalVx,motionLocalVy,motionLocalVz,"
            << "frameVx,frameVy,frameVz,"
            << "worldVx,worldVy,worldVz,"
            << "relativeWorldVx,relativeWorldVy,relativeWorldVz,"
            << "relativeSpeed,"
            << "shipForwardX,shipForwardY,shipForwardZ,"
            << "shipRightX,shipRightY,shipRightZ,"
            << "shipUpX,shipUpY,shipUpZ,"
            << "velocityDirX,velocityDirY,velocityDirZ,"
            << "forwardVelocityDot,"
            << "forwardVelocityAngleDeg,"
            << "frameRadialX,frameRadialY,frameRadialZ,"
            << "frameProgradeX,frameProgradeY,frameProgradeZ,"
            << "frameNormalX,frameNormalY,frameNormalZ\n";
    }

    out
        << row << ","
        << std::fixed << std::setprecision(6)
        << m_orbitalUniverseTimeSeconds << ","
        << dt << ","

        << static_cast<int>(tr.motion.mode) << ","
        << tr.motion.hubId << ","
        << frameValid << ","

        << control.controlTick << ","

        << control.pitchInput << ","
        << control.yawInput << ","
        << control.rollInput << ","

        << control.forwardInput << ","
        << control.liftInput << ","
        << control.strafeInput << ","

        << control.targetSpeedRate << ","
        << (control.cruiseActive ? 1 : 0) << ","
        << (control.jumpActive ? 1 : 0) << ","

        << playerWorld.x << "," << playerWorld.y << "," << playerWorld.z << ","

        << tr.motion.localPositionMeters.x << ","
        << tr.motion.localPositionMeters.y << ","
        << tr.motion.localPositionMeters.z << ","

        << tr.motion.localVelocityMps.x << ","
        << tr.motion.localVelocityMps.y << ","
        << tr.motion.localVelocityMps.z << ","

        << frameVelocity.x << "," << frameVelocity.y << "," << frameVelocity.z << ","

        << worldVelocity.x << "," << worldVelocity.y << "," << worldVelocity.z << ","

        << relativeWorldVelocity.x << ","
        << relativeWorldVelocity.y << ","
        << relativeWorldVelocity.z << ","

        << relativeSpeed << ","

        << shipForward.x << "," << shipForward.y << "," << shipForward.z << ","
        << shipRight.x << "," << shipRight.y << "," << shipRight.z << ","
        << shipUp.x << "," << shipUp.y << "," << shipUp.z << ","

        << velocityDir.x << "," << velocityDir.y << "," << velocityDir.z << ","

        << forwardVelocityDot << ","
        << forwardVelocityAngleDeg << ","

        << frameRadial.x << "," << frameRadial.y << "," << frameRadial.z << ","
        << framePrograde.x << "," << framePrograde.y << "," << framePrograde.z << ","
        << frameNormal.x << "," << frameNormal.y << "," << frameNormal.z
        << "\n";

    ++row;
}


void GameSimulation::debugLogHubPlayerChain(double dt)
{
    static int row = 0;

    if (row >= 1200)
        return;

    const auto* player =
        playerShip();

    if (!player)
        return;

    const auto& tr =
        player->core().transform();

    auto hubIt =
        m_orbitalHubs.find("earth_orbital_hub");

    if (hubIt == m_orbitalHubs.end())
        return;

    const auto& hub =
        hubIt->second;

    const auto* frame =
        hubNavigationFrame("earth_orbital_hub");

    if (!frame || !frame->valid)
        return;

    glm::dvec3 stationM {0.0};
    glm::dvec3 stationX {0.0};
    glm::dvec3 stationY {0.0};
    glm::dvec3 stationZ {0.0};

    bool haveStation = false;

    for (const auto& [id, obj] : m_staticObjects)
    {
        if (obj.hubId == "earth_orbital_hub")
        {
            stationM =
                world::coordinates::fullMeters(
                    obj.worldPosition
                );

            stationX = glm::dvec3(obj.orientation[0]);
            stationY = glm::dvec3(obj.orientation[1]);
            stationZ = glm::dvec3(obj.orientation[2]);

            haveStation = true;
            break;
        }
    }

    const glm::dvec3 playerM =
        world::coordinates::fullMeters(
            tr.worldPosition
        );

    const glm::dvec3 hubM =
        world::coordinates::fullMeters(
            hub.worldPosition
        );

    const glm::dvec3 playerLocal =
        frame->worldToLocalPosition(
            playerM
        );

    const glm::dvec3 stationLocal =
        haveStation
            ? frame->worldToLocalPosition(stationM)
            : glm::dvec3(0.0);

    const glm::dvec3 analyticHubV =
        world::orbits::computeOrbitVelocityMetersPerSecond(
            hub.motion,
            m_orbitalUniverseTimeSeconds
        );

    glm::dvec3 finiteHubV {0.0};
    glm::dvec3 observedPlayerLocalV {0.0};
    glm::dvec3 observedStationLocalV {0.0};

    static bool havePrev = false;
    static glm::dvec3 prevHubM {0.0};
    static glm::dvec3 prevPlayerLocal {0.0};
    static glm::dvec3 prevStationLocal {0.0};

    if (havePrev && dt > 0.000001)
    {
        finiteHubV =
            (hubM - prevHubM) / dt;

        observedPlayerLocalV =
            (playerLocal - prevPlayerLocal) / dt;

        observedStationLocalV =
            (stationLocal - prevStationLocal) / dt;
    }

    const glm::dvec3 relativeWorldV =
        tr.motion.worldVelocityMps -
        frame->velocityMetersPerSecond;

    const glm::dvec3 predictedLocalV =
        frame->worldToLocalVelocity(
            tr.motion.worldVelocityMps
        );

    const glm::dvec3 predictedRelativeLocalV =
        frame->worldToLocalVelocity(
            relativeWorldV
        );

    std::ofstream out(
        "hub_player_chain_debug.csv",
        row == 0 ? std::ios::out : std::ios::app
    );

    if (row == 0)
    {
        out
            << "row,time,dt,"
            << "playerWorldX,playerWorldY,playerWorldZ,"
            << "hubWorldX,hubWorldY,hubWorldZ,"
            << "stationWorldX,stationWorldY,stationWorldZ,"
            << "playerLocalX,playerLocalY,playerLocalZ,"
            << "stationLocalX,stationLocalY,stationLocalZ,"
            << "frameVx,frameVy,frameVz,"
            << "analyticHubVx,analyticHubVy,analyticHubVz,"
            << "finiteHubVx,finiteHubVy,finiteHubVz,"
            << "worldVx,worldVy,worldVz,"
            << "relativeWorldVx,relativeWorldVy,relativeWorldVz,"
            << "observedPlayerLocalVx,observedPlayerLocalVy,observedPlayerLocalVz,"
            << "predictedLocalVx,predictedLocalVy,predictedLocalVz,"
            << "predictedRelativeLocalVx,predictedRelativeLocalVy,predictedRelativeLocalVz,"
            << "observedStationLocalVx,observedStationLocalVy,observedStationLocalVz,"
            << "radialX,radialY,radialZ,"
            << "progradeX,progradeY,progradeZ,"
            << "normalX,normalY,normalZ,"
            << "stationXx,stationXy,stationXz,"
            << "stationYx,stationYy,stationYz,"
            << "stationZx,stationZy,stationZz,"
            << "dotStationYRadial,"
            << "dotStationZNegPrograde,"
            << "dotStationXNormal\n";
    }

    out
        << row << ","
        << std::fixed << std::setprecision(6)
        << m_orbitalUniverseTimeSeconds << ","
        << dt << ","

        << playerM.x << "," << playerM.y << "," << playerM.z << ","
        << hubM.x << "," << hubM.y << "," << hubM.z << ","
        << stationM.x << "," << stationM.y << "," << stationM.z << ","

        << playerLocal.x << "," << playerLocal.y << "," << playerLocal.z << ","
        << stationLocal.x << "," << stationLocal.y << "," << stationLocal.z << ","

        << frame->velocityMetersPerSecond.x << ","
        << frame->velocityMetersPerSecond.y << ","
        << frame->velocityMetersPerSecond.z << ","

        << analyticHubV.x << "," << analyticHubV.y << "," << analyticHubV.z << ","
        << finiteHubV.x << "," << finiteHubV.y << "," << finiteHubV.z << ","

        << tr.motion.worldVelocityMps.x << ","
        << tr.motion.worldVelocityMps.y << ","
        << tr.motion.worldVelocityMps.z << ","

        << relativeWorldV.x << "," << relativeWorldV.y << "," << relativeWorldV.z << ","

        << observedPlayerLocalV.x << ","
        << observedPlayerLocalV.y << ","
        << observedPlayerLocalV.z << ","

        << predictedLocalV.x << ","
        << predictedLocalV.y << ","
        << predictedLocalV.z << ","

        << predictedRelativeLocalV.x << ","
        << predictedRelativeLocalV.y << ","
        << predictedRelativeLocalV.z << ","

        << observedStationLocalV.x << ","
        << observedStationLocalV.y << ","
        << observedStationLocalV.z << ","

        << frame->radialAxis.x << "," << frame->radialAxis.y << "," << frame->radialAxis.z << ","
        << frame->progradeAxis.x << "," << frame->progradeAxis.y << "," << frame->progradeAxis.z << ","
        << frame->normalAxis.x << "," << frame->normalAxis.y << "," << frame->normalAxis.z << ","

        << stationX.x << "," << stationX.y << "," << stationX.z << ","
        << stationY.x << "," << stationY.y << "," << stationY.z << ","
        << stationZ.x << "," << stationZ.y << "," << stationZ.z << ","

        << glm::dot(stationY, frame->radialAxis) << ","
        << glm::dot(stationZ, -frame->progradeAxis) << ","
        << glm::dot(stationX, frame->normalAxis)
        << "\n";

    prevHubM =
        hubM;

    prevPlayerLocal =
        playerLocal;

    prevStationLocal =
        stationLocal;

    havePrev = true;
    ++row;
}