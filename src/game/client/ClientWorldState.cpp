#include "ClientWorldState.h"
#include "src/game/client/ClientSpatialDomain.h"
#include "src/game/client/ReferenceFramePresentation.h"
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
#include <fstream>
#include <iomanip>

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

    bool sameReferenceFrame(
        const game::simulation::ShipReferenceFrameSnapshot& a,
        const game::simulation::ShipReferenceFrameSnapshot& b
    )
    {
        // Membership is part of frame identity: a.systemId == b.systemId.
        return game::client::sameReferenceFrameIdentity(a, b);
    }

    void applyReferenceFrameState(
        ShipTransform& transform,
        const game::simulation::ShipReferenceFrameSnapshot& frame
    )
    {
        if (!frame.valid)
            return;

        transform.motion.systemId = frame.systemId;
        transform.motion.hubId = frame.hubId;
        transform.motion.parentBodyId = frame.bodyId;
        transform.motion.localPositionMeters = frame.localPositionMeters;
        transform.motion.localVelocityMps = frame.localVelocityMetersPerSecond;
        const glm::dvec3 referenceVelocity =
            frame.localToWorldVelocity(
                frame.localPositionMeters,
                glm::dvec3(0.0)
            );

        transform.motion.referenceVelocityMps = referenceVelocity;
        transform.referenceVelocityMetersPerSecond = referenceVelocity;
        transform.setWorldPositionMeters(
            frame.localToWorldPosition(frame.localPositionMeters)
        );
    }

    ShipTransform smoothShipRenderTransform(
        const ShipTransform& current,
        const ShipTransform& target,
        const game::simulation::ShipReferenceFrameSnapshot& currentFrame,
        const game::simulation::ShipReferenceFrameSnapshot& targetFrame,
        float dt
    )
    {
        // A system transfer is a spatial discontinuity. WorldPosition values
        // on the two sides are expressed in different system-local domains.
        if (!game::client::canInterpolateSystemLocalState(
                current.motion.systemId,
                target.motion.systemId
            ))
        {
            return target;
        }

        ShipTransform out = target;
        const float posAlpha = renderSmoothingAlpha(dt, 18.0f);
        const float rotAlpha = renderSmoothingAlpha(dt, 22.0f);

        if (sameReferenceFrame(currentFrame, targetFrame))
        {
            const glm::dvec3 smoothedLocal =
                currentFrame.localPositionMeters +
                (targetFrame.localPositionMeters - currentFrame.localPositionMeters) *
                static_cast<double>(posAlpha);

            out.motion.localPositionMeters = smoothedLocal;
            out.motion.localVelocityMps = targetFrame.localVelocityMetersPerSecond;
            out.setWorldPositionMeters(targetFrame.localToWorldPosition(smoothedLocal));
        }
        else
        {
            const glm::dvec3 worldDelta = world::coordinates::relativeMeters(
                target.worldPosition,
                current.worldPosition
            );

            if (glm::length(worldDelta) > 500.0)
                out.setWorldPosition(target.worldPosition);
            else
                out.setWorldPosition(
                    world::coordinates::translated(
                        current.worldPosition,
                        worldDelta * static_cast<double>(posAlpha)
                    )
                );
        }

        out.orientation = smoothOrientationMatrix(
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





const game::simulation::ObjectAssemblyModuleSnapshot* findAssemblyModuleById(
    const std::vector<game::simulation::ObjectAssemblyModuleSnapshot>& modules,
    const std::string& moduleId
)
{
    for (const auto& m : modules)
    {
        if (m.moduleId == moduleId)
            return &m;
    }

    return nullptr;
}

float interpolateAngleRad(
    float from,
    float to,
    float t
)
{
    const float delta =
        std::atan2(
            std::sin(to - from),
            std::cos(to - from)
        );

    return from + delta * glm::clamp(t, 0.0f, 1.0f);
}

std::vector<game::simulation::ObjectAssemblyModuleSnapshot>
interpolateAssemblyModules(
    const std::vector<game::simulation::ObjectAssemblyModuleSnapshot>& olderModules,
    const std::vector<game::simulation::ObjectAssemblyModuleSnapshot>& newerModules,
    const std::vector<game::simulation::ObjectAssemblyModuleSnapshot>& fallbackModules,
    float t
)
{
    std::vector<game::simulation::ObjectAssemblyModuleSnapshot> out =
        fallbackModules.empty()
            ? newerModules
            : fallbackModules;

    for (auto& dst : out)
    {
        const auto* oldModule =
            findAssemblyModuleById(
                olderModules,
                dst.moduleId
            );

        const auto* newModule =
            findAssemblyModuleById(
                newerModules,
                dst.moduleId
            );

        if (!oldModule || !newModule)
            continue;

        dst.rotationAngleRad =
            interpolateAngleRad(
                oldModule->rotationAngleRad,
                newModule->rotationAngleRad,
                t
            );
    }

    return out;
}






static float findAssemblyAngleRad(
    const std::vector<game::simulation::ObjectAssemblyModuleSnapshot>& modules,
    const std::string& moduleId
)
{
    for (const auto& m : modules)
    {
        if (m.moduleId == moduleId)
            return m.rotationAngleRad;
    }

    return 0.0f;
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
    const std::uint64_t incomingTimelineRevision =
        snapshot.metadata.universeTimelineRevision;

    const bool timelineRevisionChanged =
        m_snapshotTimelineRevision != 0 &&
        incomingTimelineRevision != m_snapshotTimelineRevision;

    const int incomingActiveSystemId =
        snapshot.session.playerNavigation.currentSystemId;

    const bool activeSystemChanged =
        m_snapshotActiveSystemId >= 0 &&
        incomingActiveSystemId != m_snapshotActiveSystemId;

    if (timelineRevisionChanged || activeSystemChanged)
    {
        // Both events are hard presentation discontinuities. Server time stays
        // monotonic, so without this fence snapshots from different universe
        // branches or different system-local coordinate domains look adjacent.
        m_snapshotBuffer.clear();
    }

    m_snapshotTimelineRevision = incomingTimelineRevision;
    m_snapshotActiveSystemId = incomingActiveSystemId;

    std::unordered_set<std::uint32_t> authoritativeShipIds;
    authoritativeShipIds.reserve(snapshot.ships.size());

    std::unordered_set<std::uint32_t> authoritativeObjectIds;
    authoritativeObjectIds.reserve(snapshot.objects.size());

    // ------- передача ShipSnapshot ------
    for (const auto& s : snapshot.ships)
    {
        authoritativeShipIds.insert(s.id.value);
        auto it = m_ships.find(s.id.value);

        if (it == m_ships.end())
        {
            auto& state = m_ships[s.id.value];
            state.id   = s.id;
            state.role = s.role;
            state.typeId = s.typeId;
            state.descriptor =
                &ShipDescriptorRegistry::get(s.typeId);
                // &ObjectDescriptorRegistry::get(s.typeId);

            state.transform       = s.transform;
            state.referenceFrame  = s.referenceFrame;
            applyReferenceFrameState(state.transform, state.referenceFrame);
            state.renderTransform = state.transform;
            state.renderReferenceFrame = state.referenceFrame;
            state.receptions = s.receptions;
            state.radarContacts = s.radarContacts;
            state.damageEvents = s.damageEvents;
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

            state.role = s.role;
            state.typeId = s.typeId;
            state.transform = s.transform;
            state.referenceFrame = s.referenceFrame;
            applyReferenceFrameState(state.transform, state.referenceFrame);
            state.receptions = s.receptions;
            state.radarContacts = s.radarContacts;
            state.damageEvents = s.damageEvents;
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


    for (auto it = m_ships.begin(); it != m_ships.end();)
    {
        if (authoritativeShipIds.find(it->first) == authoritativeShipIds.end())
            it = m_ships.erase(it);
        else
            ++it;
    }

    // ------- передача ObjectSnapshot ------
    for (const auto& o : snapshot.objects)
    {
        authoritativeObjectIds.insert(o.id.value);
        auto it = m_objects.find(o.id.value);

        if (it == m_objects.end())
        {
            auto& state = m_objects[o.id.value];
            state.id   = o.id;
            state.type = o.type;
            state.systemId = o.systemId;
            state.descriptor = &ObjectDescriptorRegistry::get(o.type);

            // Новая истинная позиция
            // state.worldPosition = o.worldPosition;

            // Legacy mirror — пересчитываем через relativeMetersFloat
            state.setWorldPosition(o.worldPosition);

            state.orientation = o.orientation;
            state.renderOrientation = o.orientation;
            state.hubAttachment = o.hubAttachment;

            applyGraphSnapshot(
                o.graph,
                state.modules,
                state.structuralLinks,
                state.assemblyModules,
                state.detachedFragments,
                state.debugHitVolumes
            );

             state.renderAssemblyModules = state.assemblyModules; 
            
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
            state.systemId = o.systemId;

            // state.worldPosition = o.worldPosition;
            state.setWorldPosition(o.worldPosition);
            state.orientation = o.orientation;
            state.hubAttachment = o.hubAttachment;

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

            state.renderAssemblyModules = state.assemblyModules;

            if (modulesChanged)
                rebuildHiddenPartIds(state);
        }
    }

    for (auto it = m_objects.begin(); it != m_objects.end();)
    {
        if (authoritativeObjectIds.find(it->first) == authoritativeObjectIds.end())
            it = m_objects.erase(it);
        else
            ++it;
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

            drone.transform.motion.systemId =
                ship.transform.motion.systemId;
            drone.transform.setWorldPosition(job.droneWorldPosition);
            drone.renderTransform = drone.transform;

            drone.visible = true;
            drone.visualScale = 1.0f;

            m_visualDrones.push_back(drone);
        }
    }









    if (timelineRevisionChanged)
    {
        for (auto& [id, ship] : m_ships)
        {
            (void)id;
            ship.renderTransform = ship.transform;
            ship.renderReferenceFrame = ship.referenceFrame;
        }

        for (auto& [id, object] : m_objects)
        {
            (void)id;
            object.renderWorldPosition = object.worldPosition;
            object.renderOrientation = object.orientation;
            object.renderAssemblyModules = object.assemblyModules;
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

void ClientWorldState::update(
    float dt,
    bool authoritativePlayerRendering,
    double renderServerTimeSeconds
)
{
    double renderTime = renderServerTimeSeconds;

    if (!m_snapshotBuffer.empty())
    {
        const double oldest = m_snapshotBuffer.front().metadata.serverTimeSeconds;
        const double newest = m_snapshotBuffer.back().metadata.serverTimeSeconds;

        // Если renderTime вылетает за диапазон —
        // зажимаем его внутрь буфера
        if (renderTime < oldest)
            renderTime = oldest;

        if (renderTime > newest)
            renderTime = newest;
    }

    SimulationSnapshot* older = nullptr;
    SimulationSnapshot* newer = nullptr;

    if (m_snapshotBuffer.size() >= 2)
    {
        for (size_t i = 0; i + 1 < m_snapshotBuffer.size(); ++i)
        {
            if (m_snapshotBuffer[i].metadata.serverTimeSeconds <= renderTime &&
                m_snapshotBuffer[i+1].metadata.serverTimeSeconds >= renderTime)
            {
                older = &m_snapshotBuffer[i];
                newer = &m_snapshotBuffer[i+1];
                break;
            }
        }
    }

    auto sampleRenderReferenceFrame =
        [&](std::uint32_t shipId)
            -> game::simulation::ShipReferenceFrameSnapshot
        {
            auto currentIt = m_ships.find(shipId);
            if (currentIt == m_ships.end())
                return {};

            const auto& fallback = currentIt->second.referenceFrame;

            if (!older || !newer)
                return fallback;

            const double span =
                newer->metadata.serverTimeSeconds -
                older->metadata.serverTimeSeconds;

            if (span <= 0.0)
                return fallback;

            const auto oldIt = std::find_if(
                older->ships.begin(),
                older->ships.end(),
                [&](const ShipSnapshot& snapshot)
                {
                    return snapshot.id.value == shipId;
                }
            );

            const auto newIt = std::find_if(
                newer->ships.begin(),
                newer->ships.end(),
                [&](const ShipSnapshot& snapshot)
                {
                    return snapshot.id.value == shipId;
                }
            );

            if (oldIt == older->ships.end() ||
                newIt == newer->ships.end())
            {
                return fallback;
            }

            const double alpha =
                (renderTime - older->metadata.serverTimeSeconds) / span;

            return game::client::interpolateReferenceFramePresentation(
                oldIt->referenceFrame,
                newIt->referenceFrame,
                alpha
            );
        };

    for (auto& [id, ship] : m_ships)
    {
        const bool usePredictedPlayerPresentation =
            ship.role == ShipRole::Player &&
            !authoritativePlayerRendering;

        if (usePredictedPlayerPresentation)
        {
            ship.renderTransform =
                smoothShipRenderTransform(
                    ship.renderTransform,
                    ship.transform,
                    ship.renderReferenceFrame,
                    ship.referenceFrame,
                    dt
                );

            if (ship.referenceFrame.valid)
            {
                const auto presentationFrame =
                    sampleRenderReferenceFrame(id);

                ship.renderReferenceFrame =
                    game::client::sameReferenceFrameIdentity(
                        presentationFrame,
                        ship.referenceFrame
                    )
                        ? presentationFrame
                        : ship.referenceFrame;

                ship.renderReferenceFrame.localPositionMeters =
                    ship.renderTransform.motion.localPositionMeters;
                ship.renderReferenceFrame.localVelocityMetersPerSecond =
                    ship.renderTransform.motion.localVelocityMps;

                /*
                    The camera/player and every hub-attached object must use
                    one reference-frame sample. Rebase the smoothed local ship
                    position through the render-time frame instead of through
                    the newest discrete authoritative frame.
                */
                ship.renderTransform.setWorldPositionMeters(
                    ship.renderReferenceFrame.localToWorldPosition(
                        ship.renderTransform.motion.localPositionMeters
                    )
                );
            }
        }
        else
        {
            // NPCs and debug passive trajectories are rendered only from
            // authoritative server snapshots.
            if (older && newer)
            {
                double span = newer->metadata.serverTimeSeconds - older->metadata.serverTimeSeconds;

                if (span > 0.0)
                {
                    float t = float((renderTime - older->metadata.serverTimeSeconds) / span);
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
                        if (!game::client::canInterpolateSystemLocalState(
                                itOld->transform.motion.systemId,
                                itNew->transform.motion.systemId
                            ))
                        {
                            // Never draw a path between two star systems.
                            ship.renderTransform = ship.transform;
                        }
                        else
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

                            ship.renderTransform = itNew->transform;
                            ship.renderTransform.setWorldPosition(interpolatedWorld);

                            if (sameReferenceFrame(
                                    itOld->referenceFrame,
                                    itNew->referenceFrame
                                ))
                            {
                                const glm::dvec3 localPosition =
                                    itOld->referenceFrame.localPositionMeters +
                                    (itNew->referenceFrame.localPositionMeters -
                                     itOld->referenceFrame.localPositionMeters) *
                                        static_cast<double>(t);

                                const glm::dvec3 localVelocity =
                                    itOld->referenceFrame.localVelocityMetersPerSecond +
                                    (itNew->referenceFrame.localVelocityMetersPerSecond -
                                     itOld->referenceFrame.localVelocityMetersPerSecond) *
                                        static_cast<double>(t);

                                ship.renderTransform.motion.localPositionMeters =
                                    localPosition;
                                ship.renderTransform.motion.localVelocityMps =
                                    localVelocity;
                                ship.renderReferenceFrame =
                                    game::client::interpolateReferenceFramePresentation(
                                        itOld->referenceFrame,
                                        itNew->referenceFrame,
                                        static_cast<double>(t)
                                    );
                                ship.renderReferenceFrame.localPositionMeters =
                                    localPosition;
                                ship.renderReferenceFrame.localVelocityMetersPerSecond =
                                    localVelocity;

                                ship.renderTransform.setWorldPositionMeters(
                                    ship.renderReferenceFrame.localToWorldPosition(
                                        localPosition
                                    )
                                );
                            }

                            ship.renderTransform.orientation =
                                smoothOrientationMatrix(
                                    itOld->transform.orientation,
                                    itNew->transform.orientation,
                                    t
                                );
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

        bool usedInterpolation = false;
        bool usedFallback = false;
        float debugT = -1.0f;



        if (older && newer)
        {
            double span = newer->metadata.serverTimeSeconds - older->metadata.serverTimeSeconds;

            if (span > 0.0)
            {
                float t = float((renderTime - older->metadata.serverTimeSeconds) / span);
                t = glm::clamp(t, 0.0f, 1.0f);

                debugT = t;

                auto itOld = std::find_if(
                    older->objects.begin(),
                    older->objects.end(),
                    [&](const ObjectSnapshot& s)
                    {
                        return s.id.value == id;
                    }
                );

                auto itNew = std::find_if(
                    newer->objects.begin(),
                    newer->objects.end(),
                    [&](const ObjectSnapshot& s)
                    {
                        return s.id.value == id;
                    }
                );

                if (itOld != older->objects.end() &&
                    itNew != newer->objects.end())
                {
                    if (!game::client::canInterpolateSystemLocalState(
                            itOld->systemId,
                            itNew->systemId
                        ))
                    {
                        // Object coordinates are system-local too. A transfer
                        // is a hard snap, not a short numerical interpolation.
                        obj.renderWorldPosition = obj.worldPosition;
                        obj.renderOrientation = obj.orientation;
                        obj.renderAssemblyModules = obj.assemblyModules;
                        usedFallback = true;
                    }
                    else
                    {
                        const glm::dvec3 delta =
                            world::coordinates::relativeMeters(
                                itNew->worldPosition,
                                itOld->worldPosition
                            );

                        obj.renderWorldPosition =
                            world::coordinates::translated(
                                itOld->worldPosition,
                                delta * static_cast<double>(t)
                            );

                        obj.renderOrientation =
                            smoothOrientationMatrix(
                                itOld->orientation,
                                itNew->orientation,
                                t
                            );

                        if (itOld->graph.hasAssemblyModules &&
                            itNew->graph.hasAssemblyModules)
                        {
                            obj.renderAssemblyModules =
                                interpolateAssemblyModules(
                                    itOld->graph.assemblyModules,
                                    itNew->graph.assemblyModules,
                                    obj.assemblyModules,
                                    t
                                );

                            usedInterpolation = true;
                        }
                        else
                        {
                            obj.renderAssemblyModules = obj.assemblyModules;
                        }
                    }

                }
                else
                {
                    obj.renderWorldPosition = obj.worldPosition;
                    obj.renderOrientation = obj.orientation;
                    obj.renderAssemblyModules = obj.assemblyModules;
                    usedFallback = true;
                }
            }
            else
            {
                obj.renderWorldPosition = obj.worldPosition;
                obj.renderOrientation = obj.orientation;
                obj.renderAssemblyModules = obj.assemblyModules;
                usedFallback = true;
            }
        }
        else
        {
            obj.renderWorldPosition = obj.worldPosition;
            obj.renderOrientation = obj.orientation;
            obj.renderAssemblyModules = obj.assemblyModules;
            usedFallback = true;
        }






        




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
