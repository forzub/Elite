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

    const float error =
        glm::length(target.position - current.position);

    // Если это телепорт/первичная инициализация — не размазываем.
    if (error > 500.0f)
    {
        out.setWorldPosition(target.worldPosition);
        out.orientation = target.orientation;
        return out;
    }

    const float posAlpha =
        renderSmoothingAlpha(dt, 18.0f);

    const float rotAlpha =
        renderSmoothingAlpha(dt, 22.0f);

    out.position =
        glm::mix(
            current.position,
            target.position,
            posAlpha
        );

    out.orientation =
        smoothOrientationMatrix(
            current.orientation,
            target.orientation,
            rotAlpha
        );

    out.setWorldPosition(target.worldPosition);

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

            state.position = o.position;

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

            state.renderPosition = o.position;
            
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

            state.position = o.position;
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
                        ship.renderTransform.position =
                            glm::mix(
                                itOld->transform.position,
                                itNew->transform.position,
                                t
                            );

                        ship.renderTransform.worldPosition = itNew->transform.worldPosition;

                        ship.renderTransform.worldPosition = itNew->transform.worldPosition;
                        ship.renderTransform.orientation = itOld->transform.orientation;
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
        obj.renderPosition = obj.position;
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
    const glm::vec3& authoritativePos)
{
    auto it = m_ships.find(id.value);
    if (it == m_ships.end())
        return;

    auto& ship = it->second;

    glm::vec3 delta =
        authoritativePos - ship.transform.position;

    ship.transform.position += delta * 0.1f;
}