#include "ClientWorldState.h"
#include "src/game/shared/SharedShipPhysics.h"
#include <iostream>
#include "src/game/ship/ShipDescriptorRegistry.h"
#include "src/world/descriptors/ObjectDescriptorRegistry.h"
#include "src/game/geometry/AssemblyMeshLibrary.h"
#include "src/game/geometry/ObjectAssemblyRegistry.h"
#include <algorithm>

#include <cmath>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/common.hpp>

#include "src/world/coordinates/WorldPosition.h"

namespace
{
    bool isModuleLocallyVisible(uint8_t state)
    {
        return state == 0; // Attached
    }

    bool isModuleDetachedVisible(uint8_t state)
    {
        // 3 = Detached, 4 = Hanging.
        // Destroyed/Disabled пока не рисуем как отдельные фрагменты.
        return state == 3 || state == 4;
    }

float renderSmoothingAlpha(float dt, float responsiveness)
{
    dt = glm::clamp(dt, 0.0f, 0.05f);
    return 1.0f - std::exp(-responsiveness * dt);
}

glm::mat4 smoothOrientationMatrix(
    const glm::mat4& current,
    const glm::mat4& target,
    float alpha
)
{
    glm::quat a = glm::normalize(glm::quat_cast(current));
    glm::quat b = glm::normalize(glm::quat_cast(target));

    glm::quat q =
        glm::slerp(
            a,
            b,
            glm::clamp(alpha, 0.0f, 1.0f)
        );

    return glm::mat4_cast(glm::normalize(q));
}

ShipTransform smoothShipRenderTransform(
    const ShipTransform& current,
    const ShipTransform& target,
    float dt
)
{
    ShipTransform out = target;

    const glm::dvec3 worldDelta =
        world::coordinates::relativeMeters(
            target.worldPosition,
            current.worldPosition
        );

    const double error = glm::length(worldDelta);

    if (error > 500.0)
    {
        out.setWorldPosition(target.worldPosition);
        out.orientation = target.orientation;
        return out;
    }

    const float posAlpha =
        renderSmoothingAlpha(dt, 18.0f);

    const float rotAlpha =
        renderSmoothingAlpha(dt, 22.0f);

    const world::coordinates::WorldPosition smoothedWorld =
        world::coordinates::translated(
            current.worldPosition,
            worldDelta * static_cast<double>(posAlpha)
        );

    out.setWorldPosition(smoothedWorld);

    out.orientation =
        smoothOrientationMatrix(
            current.orientation,
            target.orientation,
            rotAlpha
        );

    return out;
}

    

    


    static void applyGraphSnapshot(
        const game::simulation::ObjectGraphSnapshot& graph,
        std::vector<game::simulation::ObjectModuleSnapshot>& modules,
        std::vector<game::simulation::StructuralLinkSnapshot>& structuralLinks,
        std::vector<game::simulation::ObjectAssemblyModuleSnapshot>& assemblyModules,
        std::vector<game::simulation::ObjectDetachedFragmentSnapshot>& detachedFragments,
        std::vector<game::simulation::ObjectRepairJobSnapshot>& repairJobs,
        std::vector<game::simulation::DebugHitVolumeSnapshot>& debugHitVolumes
    )
    {
        if (graph.hasModules)
            modules = graph.modules;

        if (graph.hasStructuralLinks)
            structuralLinks = graph.structuralLinks;

        if (graph.hasAssemblyModules)
            assemblyModules = graph.assemblyModules;

        if (graph.hasDetachedFragments)
            detachedFragments = graph.detachedFragments;

        if (graph.hasRepairJobs)
            repairJobs = graph.repairJobs;

        if (graph.hasDebugHitVolumes)
            debugHitVolumes = graph.debugHitVolumes;
    }

    static void applyGraphSnapshot(
        const game::simulation::ObjectGraphSnapshot& graph,
        std::vector<game::simulation::ObjectModuleSnapshot>& modules,
        std::vector<game::simulation::StructuralLinkSnapshot>& structuralLinks,
        std::vector<game::simulation::ObjectAssemblyModuleSnapshot>& assemblyModules,
        std::vector<game::simulation::ObjectDetachedFragmentSnapshot>& detachedFragments,
        std::vector<game::simulation::DebugHitVolumeSnapshot>& debugHitVolumes
    )
    {
        static std::vector<game::simulation::ObjectRepairJobSnapshot> ignoredRepairJobs;

        applyGraphSnapshot(
            graph,
            modules,
            structuralLinks,
            assemblyModules,
            detachedFragments,
            ignoredRepairJobs,
            debugHitVolumes
        );
    }

    template<typename TState>
    void rebuildHiddenPartIds(TState& state)
    {
        state.hiddenPartIds.clear();

        if (!state.descriptor)
            return;

        std::unordered_map<std::string, uint8_t> stateById;
        stateById.reserve(state.modules.size());

        for (const auto& modSnap : state.modules)
            stateById[modSnap.moduleId] = modSnap.state;

        std::unordered_map<std::string, std::string> parentById;
        parentById.reserve(state.descriptor->moduleDescriptors().size());

        for (const auto& modDesc : state.descriptor->moduleDescriptors())
            parentById[modDesc.moduleId] = modDesc.parentModuleId;

        auto isEffectivelyVisible = [&](const std::string& moduleId) -> bool
        {
            std::string current = moduleId;
            int guard = 0;

            while (!current.empty())
            {
                auto itState = stateById.find(current);
                if (itState != stateById.end())
                {
                    if (!isModuleLocallyVisible(itState->second))
                        return false;
                }

                auto itParent = parentById.find(current);
                if (itParent == parentById.end())
                    break;

                current = itParent->second;
                ++guard;

                if (guard > static_cast<int>(parentById.size()) + 1)
                {
                    // fail-safe: если цикл, модуль считаем скрытым
                    return false;
                }
            }

            return true;
        };

        for (const auto& modDesc : state.descriptor->moduleDescriptors())
        {
            if (isEffectivelyVisible(modDesc.moduleId))
                continue;

            for (const auto& partId : modDesc.meshPartIds)
                state.hiddenPartIds.insert(partId);
        }
    }
}


//                              ###                                                                    ###                 ##
//                               ##                                                                     ##                 ##
//   ####    ######   ######     ##     ##  ##             #####   #####     ####    ######    #####    ##       ####     #####
//      ##    ##  ##   ##  ##    ##     ##  ##            ##       ##  ##       ##    ##  ##  ##        #####   ##  ##     ##
//   #####    ##  ##   ##  ##    ##     ##  ##             #####   ##  ##    #####    ##  ##   #####    ##  ##  ##  ##     ##
//  ##  ##    #####    #####     ##      #####                 ##  ##  ##   ##  ##    #####        ##   ##  ##  ##  ##     ## ##
//   #####    ##       ##       ####        ##            ######   ##  ##    #####    ##      ######   ###  ##   ####       ###
//           ####     ####              #####                                        ####

void ClientWorldState::applySnapshot(const SimulationSnapshot& snapshot)
{

    // ------- передача ShipSnapshot ------
    for (const auto& s : snapshot.ships)
    {
        auto it = m_ships.find(s.id.value);

        if (it == m_ships.end())
        {
            auto& state = m_ships[s.id.value];
            state.id   = s.id;
            state.role = s.role;
            state.descriptor =
                &ShipDescriptorRegistry::get(s.typeId);
                // &ObjectDescriptorRegistry::get(s.typeId);

            state.transform       = s.transform;
            state.renderTransform = s.transform;
            applyGraphSnapshot(
                s.graph,
                state.modules,
                state.structuralLinks,
                state.assemblyModules,
                state.detachedFragments,
                state.repairJobs,
                state.debugHitVolumes
            );

            // state.gpuMesh = &render::MeshLibrary::get(s.typeId);

            if (!game::ship::geometry::AssemblyMeshLibrary::has(s.typeId))
            {
                throw std::runtime_error(
                    "[ClientWorldState] missing assembly for ship typeId=" +
                    std::to_string(static_cast<int>(s.typeId))
                );
            }

            state.assembly =
                &game::ship::geometry::AssemblyMeshLibrary::get(s.typeId);

            rebuildHiddenPartIds(state);

            
        }
        else
        {
            auto& state = it->second;

            state.transform = s.transform;
            state.receptions = s.receptions;
            state.radarContacts = s.radarContacts;
            state.shipCoreStatus = s.shipCoreStatus;
            applyGraphSnapshot(
                s.graph,
                state.modules,
                state.structuralLinks,
                state.assemblyModules,
                state.detachedFragments,
                state.repairJobs,
                state.debugHitVolumes
            );

            rebuildHiddenPartIds(state);
        }

    }


    // ------- передача ObjectSnapshot ------
    for (const auto& o : snapshot.objects)
    {
        auto it = m_objects.find(o.id.value);

        if (it == m_objects.end())
        {
            auto& state = m_objects[o.id.value];
            state.id   = o.id;
            state.type = o.type;
            state.descriptor = &ObjectDescriptorRegistry::get(o.type);

            // Новая истинная позиция
            state.worldPosition = o.worldPosition;

            // Legacy mirror — пересчитываем через relativeMetersFloat
            state.setWorldPosition(o.worldPosition);

            state.orientation = o.orientation;
            state.renderOrientation = o.orientation;

            applyGraphSnapshot(
                o.graph,
                state.modules,
                state.structuralLinks,
                state.assemblyModules,
                state.detachedFragments,
                state.debugHitVolumes
            );

            state.renderWorldPosition = o.worldPosition;
            state.renderPosition = world::coordinates::legacyFloatMeters(o.worldPosition);  
            
            const auto& desc = ObjectDescriptorRegistry::get(o.type);
            
            if (!game::ship::geometry::AssemblyMeshLibrary::has(o.type))
            {
                throw std::runtime_error(
                    "[ClientWorldState] missing assembly for object type=" +
                    std::to_string(static_cast<int>(o.type))
                );
            }

            state.assembly =
                &game::ship::geometry::AssemblyMeshLibrary::get(o.type);

            rebuildHiddenPartIds(state);
        }
        else
        {
            auto& state = it->second;

            state.worldPosition = o.worldPosition;
            state.setWorldPosition(o.worldPosition);
            state.orientation = o.orientation;

            // Static object payload is sparse: for a rotating station the server sends
            // only graph.assemblyModules every tick. Heavy module/debug payload arrives
            // only on first snapshot or after structural changes.
            const bool modulesChanged = o.graph.hasModules;

            applyGraphSnapshot(
                o.graph,
                state.modules,
                state.structuralLinks,
                state.assemblyModules,
                state.detachedFragments,
                state.debugHitVolumes
            );

            if (modulesChanged)
                rebuildHiddenPartIds(state);
        }
    }


    m_visualDrones.clear();

    for (const auto& shipPair : m_ships)
    {
        const ClientShipState& ship = shipPair.second;

        uint32_t droneIndex = 0;

        for (const auto& job : ship.repairJobs)
        {
            game::visual::VisualDrone drone;

            drone.id =
                700000u +
                ship.id.value * 100u +
                droneIndex++;

            drone.kind = game::visual::VisualDroneKind::Repair;
            drone.type = ObjectType::RepairDroneDebug;

            if (!game::ship::geometry::AssemblyMeshLibrary::has(drone.type))
            {
                continue;
            }

            drone.assembly =
                &game::ship::geometry::AssemblyMeshLibrary::get(drone.type);

            drone.transform.setWorldPosition(job.droneWorldPosition);
            drone.renderTransform = drone.transform;

            drone.visible = true;
            drone.visualScale = 1.0f;

            m_visualDrones.push_back(drone);
        }
    }









    // ------- передача в буфер Snapshot ------
    m_snapshotBuffer.push_back(snapshot);

    while (m_snapshotBuffer.size() > 20)
        m_snapshotBuffer.pop_front();

    
}







//                       ###              ##
//                        ##              ##
//  ##  ##   ######       ##    ####     #####    ####
//  ##  ##    ##  ##   #####       ##     ##     ##  ##
//  ##  ##    ##  ##  ##  ##    #####     ##     ######
//  ##  ##    #####   ##  ##   ##  ##     ## ##  ##
//   ######   ##       ######   #####      ###    #####
//           ####

void ClientWorldState::update(float dt)
{
    m_clientTime += dt;

    double renderTime = m_clientTime - m_renderDelay;

    SimulationSnapshot* older = nullptr;
    SimulationSnapshot* newer = nullptr;

    if (m_snapshotBuffer.size() >= 2)
    {
        for (size_t i = 0; i + 1 < m_snapshotBuffer.size(); ++i)
        {
            if (m_snapshotBuffer[i].serverTime <= renderTime &&
                m_snapshotBuffer[i+1].serverTime >= renderTime)
            {
                older = &m_snapshotBuffer[i];
                newer = &m_snapshotBuffer[i+1];
                break;
            }
        }
    }

    for (auto& [id, ship] : m_ships)
    {
        // ===== 1️⃣ ИГРОК — ВСЕГДА PREDICTION =====
        if (ship.role == ShipRole::Player)
        {
            ship.renderTransform =
                smoothShipRenderTransform(
                    ship.renderTransform,
                    ship.transform,
                    dt
                );
        }
        else
        {
            // ===== 2️⃣ NPC — INTERPOLATION =====
            if (older && newer)
            {
                double span = newer->serverTime - older->serverTime;

                if (span > 0.0)
                {
                    float t = float((renderTime - older->serverTime) / span);
                    t = glm::clamp(t, 0.0f, 1.0f);

                    auto itOld = std::find_if(
                        older->ships.begin(),
                        older->ships.end(),
                        [&](const ShipSnapshot& s){ return s.id.value == id; });

                    auto itNew = std::find_if(
                        newer->ships.begin(),
                        newer->ships.end(),
                        [&](const ShipSnapshot& s){ return s.id.value == id; });

                    if (itOld != older->ships.end() &&
                        itNew != newer->ships.end())
                    {
                        const glm::dvec3 delta =
                        world::coordinates::relativeMeters(
                            itNew->transform.worldPosition,
                            itOld->transform.worldPosition
                        );

                    const auto interpolatedWorld =
                        world::coordinates::translated(
                            itOld->transform.worldPosition,
                            delta * static_cast<double>(t)
                        );

                    ship.renderTransform.setWorldPosition(interpolatedWorld);
                    ship.renderTransform.orientation =
                        smoothOrientationMatrix(
                            itOld->transform.orientation,
                            itNew->transform.orientation,
                            t
                        );
                    }
                    else
                    {
                        ship.renderTransform = ship.transform;
                    }
                }
                else
                {
                    ship.renderTransform = ship.transform;
                }
            }
            else
            {
                ship.renderTransform = ship.transform;
            }
        }

        // ===== 3️⃣ PRESENTATION — ВСЕГДА =====
        ship.signalPresentation.update(
            dt,
            ship.receptions
        );
    }


    for (auto& [id, obj] : m_objects)
    {
        obj.renderWorldPosition = obj.worldPosition;
        obj.renderPosition = world::coordinates::legacyFloatMeters(obj.worldPosition);
        obj.renderOrientation = obj.orientation;

       
    }
}









//                                ###     ##                ##
//                                 ##                       ##
//  ######   ######    ####        ##    ###      ####     #####
//   ##  ##   ##  ##  ##  ##    #####     ##     ##  ##     ##
//   ##  ##   ##      ######   ##  ##     ##     ##         ##
//   #####    ##      ##       ##  ##     ##     ##  ##     ## ##
//   ##      ####      #####    ######   ####     ####       ###
//  ####


void ClientWorldState::predict(
    EntityId id,
    const ShipControlState& control,
    const WorldParams& world,
    float dt)
{
    auto it = m_ships.find(id.value);
    if (it == m_ships.end())
        return;

    auto& ship = it->second;

    SharedShipPhysics::integrate(
        ship.transform,
        ship.descriptor->physics,
        control,
        world,
        dt
    );
}



//    ###                                                            ##                ##
//   ## ##                                                           ##                ##
//    #       ####    ######    ####     ####              #####    #####    ####     #####    ####
//  ####     ##  ##    ##  ##  ##  ##   ##  ##            ##         ##         ##     ##     ##  ##
//   ##      ##  ##    ##      ##       ######             #####     ##      #####     ##     ######
//   ##      ##  ##    ##      ##  ##   ##                     ##    ## ##  ##  ##     ## ##  ##
//  ####      ####    ####      ####     #####            ######      ###    #####      ###    #####


void ClientWorldState::forceState(
    const SimulationSnapshot& snapshot)
{
    for (const auto& s : snapshot.ships)
    {
        auto it = m_ships.find(s.id.value);
        if (it == m_ships.end())
            continue;

        auto& state = it->second;

        // Полная синхронизация
        state.transform       = s.transform;
        state.renderTransform = s.transform;
        state.role            = s.role;
    }
}



void ClientWorldState::applySoftCorrection(
    EntityId id,
    const world::coordinates::WorldPosition& authoritativeWorldPosition
)
{
    auto it = m_ships.find(id.value);
    if (it == m_ships.end())
        return;

    auto& ship = it->second;

    const glm::dvec3 delta =
        world::coordinates::relativeMeters(
            authoritativeWorldPosition,
            ship.transform.worldPosition
        );

    const double error = glm::length(delta);

    if (error <= 0.001)
        return;

    const world::coordinates::WorldPosition corrected =
        world::coordinates::translated(
            ship.transform.worldPosition,
            delta * 0.1
        );

    ship.transform.setWorldPosition(corrected);
    ship.renderTransform.setWorldPosition(corrected);
}