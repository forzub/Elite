#include "ClientWorldState.h"
#include "src/game/client/ClientSpatialDomain.h"
#include "src/game/client/ReferenceFramePresentation.h"
#include "src/game/client/ClientHubTacticalPrediction.h"
#include "src/game/client/SnapshotPresentationWindow.h"
#include "src/game/network/ReplicationSnapshotMerge.h"
#include "src/game/client/presentation/LocalPredictedPresentation.h"
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
    void appendHubMotionLabPresentationCsv(
        const game::diagnostics::HubMotionLabPresentationSample& sample
    )
    {
        if constexpr (!game::diagnostics::HubMotionLabTelemetryCsvEnabled)
            return;

        static std::ofstream out(
            game::diagnostics::HubMotionLabTelemetryCsvPath,
            std::ios::out | std::ios::trunc
        );
        static bool headerWritten = false;

        if (!out)
            return;

        if (!headerWritten)
        {
            out
                << "frame,frame_dt_ms,requested_render_s,actual_render_s,"
                << "oldest_snapshot_s,newest_snapshot_s,requested_minus_newest_ms,"
                << "clamped_oldest,clamped_newest,has_bracket,"
                << "older_tick,newer_tick,alpha,"
                << "slow_error_m,fast_error_m,"
                << "match_vs_predicted_player_delta_m,"
                << "match_vs_delayed_player_error_m,"
                << "player_prediction_remainder_ms,"
                << "player_fixed_to_fractional_m,"
                << "player_render_to_fractional_m,"
                << "player_render_step_m\n";
            headerWritten = true;
        }

        out
            << sample.frameIndex << ','
            << std::setprecision(12) << sample.frameDtMilliseconds << ','
            << sample.requestedRenderTimeSeconds << ','
            << sample.actualRenderTimeSeconds << ','
            << sample.oldestSnapshotTimeSeconds << ','
            << sample.newestSnapshotTimeSeconds << ','
            << sample.requestedMinusNewestMilliseconds << ','
            << (sample.clampedToOldest ? 1 : 0) << ','
            << (sample.clampedToNewest ? 1 : 0) << ','
            << (sample.hasInterpolationBracket ? 1 : 0) << ','
            << sample.olderServerTick << ','
            << sample.newerServerTick << ','
            << sample.interpolationAlpha << ','
            << sample.slowNpcLocalErrorMeters << ','
            << sample.fastNpcLocalErrorMeters << ','
            << sample.matchVsPredictedPlayerDistanceDeltaMeters << ','
            << sample.matchVsDelayedPlayerErrorMeters << ','
            << sample.playerPredictionRemainderMilliseconds << ','
            << sample.playerFixedToFractionalTargetMeters << ','
            << sample.playerRenderToFractionalTargetMeters << ','
            << sample.playerRenderStepMeters
            << '\n';
    }

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
        transform.motion.travelFrame = frame.kinematicFrame();
        transform.motion.matchedToReferenceFrame =
            frame.matchedToReferenceFrame;
        transform.motion.matchedReferenceFrameId =
            frame.matchedToReferenceFrame ? frame.hubId : std::string{};
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
            /*
                The reference-frame snapshot describes the frame geometry.
                The entity's predicted hub-local motion belongs to ShipTransform.
                Using targetFrame.localPositionMeters here silently discarded
                client linear prediction and turned the camera translation into
                a low-pass response to 16.7 Hz authoritative snapshots.
            */
            const glm::dvec3 smoothedLocal =
                current.motion.localPositionMeters +
                (target.motion.localPositionMeters -
                 current.motion.localPositionMeters) *
                    static_cast<double>(posAlpha);

            out.motion.localPositionMeters = smoothedLocal;
            out.motion.localVelocityMps = target.motion.localVelocityMps;
            out.setWorldPositionMeters(
                targetFrame.localToWorldPosition(smoothedLocal)
            );
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
        m_snapshotTimelineRevision != 0 &&
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
            state.instanceId = s.instanceId;
            state.role = s.role;
            state.typeId = s.typeId;
            state.acknowledgedControlTick = s.acknowledgedControlTick;
            state.motionLabKind = s.motionLabKind;
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

            state.instanceId = s.instanceId;
            state.role = s.role;
            state.typeId = s.typeId;
            state.acknowledgedControlTick = s.acknowledgedControlTick;
            state.motionLabKind = s.motionLabKind;
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


    const bool fullAuthoritativeEntitySet =
        snapshot.replication.entitySetMode ==
        game::network::ReplicatedEntitySetMode::FullAuthoritativeSet;

    if (fullAuthoritativeEntitySet)
    {
        for (auto it = m_ships.begin(); it != m_ships.end();)
        {
            if (authoritativeShipIds.find(it->first) == authoritativeShipIds.end())
                it = m_ships.erase(it);
            else
                ++it;
        }
    }
    else
    {
        // Sparse omission means retain. Only an explicit lifecycle removal may
        // delete a ship from the replicated client world.
        for (const auto id : snapshot.replication.removedShipIds)
            m_ships.erase(id.value);
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
            state.linearVelocityMps = o.linearVelocityMps;
            state.renderOrientation = o.orientation;
            state.hubAttachment = o.hubAttachment;
            state.displayName = o.displayName;
            state.ownerName = o.ownerName;
            state.navigationVisible = o.navigationVisible;
            state.navigationParentBodyId = o.navigationParentBodyId;
            state.orbitalMotion = o.orbitalMotion;

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
            state.linearVelocityMps = o.linearVelocityMps;
            state.hubAttachment = o.hubAttachment;
            state.displayName = o.displayName;
            state.ownerName = o.ownerName;
            state.navigationVisible = o.navigationVisible;
            state.navigationParentBodyId = o.navigationParentBodyId;
            state.orbitalMotion = o.orbitalMotion;

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

    if (fullAuthoritativeEntitySet)
    {
        for (auto it = m_objects.begin(); it != m_objects.end();)
        {
            if (authoritativeObjectIds.find(it->first) == authoritativeObjectIds.end())
                it = m_objects.erase(it);
            else
                ++it;
        }
    }
    else
    {
        for (const auto id : snapshot.replication.removedObjectIds)
            m_objects.erase(id.value);
    }

    std::unordered_set<std::string> authoritativeHubIds;
    authoritativeHubIds.reserve(snapshot.hubs.size());

    for (const auto& hub : snapshot.hubs)
    {
        authoritativeHubIds.insert(hub.id);
        auto& state = m_hubs[hub.id];
        state.id = hub.id;
        state.name = hub.name;
        state.owner = hub.owner;
        state.systemId = hub.systemId;
        state.parentBodyId = hub.parentBodyId;
        state.worldPosition = hub.worldPosition;
        state.worldVelocityMps = hub.worldVelocityMps;
        state.orientation = hub.orientation;
        state.motion = hub.motion;
    }

    if (fullAuthoritativeEntitySet)
    {
        for (auto it = m_hubs.begin(); it != m_hubs.end();)
        {
            if (authoritativeHubIds.find(it->first) == authoritativeHubIds.end())
                it = m_hubs.erase(it);
            else
                ++it;
        }
    }
    else
    {
        for (const auto& id : snapshot.replication.removedHubIds)
            m_hubs.erase(id);
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
    // Presentation/map samplers require a canonical full entity set at every
    // retained epoch even after transport publication becomes sparse. Live
    // state above applies only actual incoming updates/removals; the history
    // sample below materializes omitted entities from the previous baseline.
    const SimulationSnapshot* previousCanonical =
        m_snapshotBuffer.empty() ? nullptr : &m_snapshotBuffer.back();

    m_snapshotBuffer.push_back(
        game::network::materializeCanonicalReplicationSnapshot(
            previousCanonical,
            snapshot
        )
    );

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
    m_presentationServerTimeSeconds = renderServerTimeSeconds;

    auto& labTelemetry = m_hubMotionLabPresentationSample;
    labTelemetry = {};
    labTelemetry.frameIndex = ++m_hubMotionLabFrameIndex;
    labTelemetry.frameDtMilliseconds = static_cast<double>(dt) * 1000.0;
    labTelemetry.requestedRenderTimeSeconds = renderServerTimeSeconds;

    const auto presentationWindow =
        game::client::resolveSnapshotPresentationWindow(
            m_snapshotBuffer,
            renderServerTimeSeconds,
            [](const SimulationSnapshot& snapshot)
            {
                return snapshot.metadata.serverTimeSeconds;
            }
        );

    const double renderTime =
        presentationWindow.renderTimeSeconds;

    labTelemetry.hasSnapshots =
        presentationWindow.hasSnapshots;
    labTelemetry.oldestSnapshotTimeSeconds =
        presentationWindow.oldestSnapshotTimeSeconds;
    labTelemetry.newestSnapshotTimeSeconds =
        presentationWindow.newestSnapshotTimeSeconds;
    labTelemetry.requestedMinusNewestMilliseconds =
        presentationWindow.hasSnapshots
            ? (renderServerTimeSeconds -
               presentationWindow.newestSnapshotTimeSeconds) * 1000.0
            : 0.0;
    labTelemetry.clampedToOldest =
        presentationWindow.clampedToOldest;
    labTelemetry.clampedToNewest =
        presentationWindow.clampedToNewest;
    labTelemetry.actualRenderTimeSeconds =
        renderTime;
    labTelemetry.hasInterpolationBracket =
        presentationWindow.hasInterpolationBracket;
    labTelemetry.interpolationAlpha =
        presentationWindow.interpolationAlpha;

    SimulationSnapshot* older = nullptr;
    SimulationSnapshot* newer = nullptr;

    if (presentationWindow.hasInterpolationBracket)
    {
        older = &m_snapshotBuffer[presentationWindow.olderIndex];
        newer = &m_snapshotBuffer[presentationWindow.newerIndex];

        labTelemetry.olderServerTick =
            older->metadata.serverTick;
        labTelemetry.newerServerTick =
            newer->metadata.serverTick;
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

            return game::client::interpolateReferenceFramePresentation(
                oldIt->referenceFrame,
                newIt->referenceFrame,
                presentationWindow.interpolationAlpha
            );
        };

    for (auto& [id, ship] : m_ships)
    {
        const bool usePredictedPlayerPresentation =
            isLocalControlledEntity(ship.id) &&
            !authoritativePlayerRendering;

        if (usePredictedPlayerPresentation)
        {
            const ShipTransform* presentationTarget = &ship.transform;

            if (m_hasLocalPredictedPresentationTarget &&
                m_localPredictedPresentationShipId == id)
            {
                presentationTarget =
                    &m_localPredictedPresentationTarget;
            }

            ship.renderTransform =
                smoothShipRenderTransform(
                    ship.renderTransform,
                    *presentationTarget,
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
                    const float t =
                        static_cast<float>(
                            presentationWindow.interpolationAlpha
                        );

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

    if constexpr (game::diagnostics::HubMotionLabEnabled)
    {
        const ClientShipState* playerState = nullptr;

        const auto localPlayerIt =
            m_ships.find(m_localControlledEntityId.value);
        if (localPlayerIt != m_ships.end())
            playerState = &localPlayerIt->second;

        game::simulation::ShipReferenceFrameSnapshot delayedPlayerFrame;
        bool haveDelayedPlayerFrame = false;

        if (playerState)
        {
            delayedPlayerFrame =
                sampleRenderReferenceFrame(playerState->id.value);

            haveDelayedPlayerFrame =
                delayedPlayerFrame.valid &&
                delayedPlayerFrame.systemId ==
                    playerState->renderTransform.motion.systemId;

            if (m_hasLocalPredictedPresentationTarget &&
                m_localPredictedPresentationShipId == playerState->id.value)
            {
                labTelemetry.playerPredictionRemainderMilliseconds =
                    static_cast<double>(m_localPredictionRemainderSeconds) *
                    1000.0;

                labTelemetry.playerFixedToFractionalTargetMeters =
                    glm::length(
                        m_localPredictedPresentationTarget.motion.localPositionMeters -
                        playerState->transform.motion.localPositionMeters
                    );

                labTelemetry.playerRenderToFractionalTargetMeters =
                    glm::length(
                        m_localPredictedPresentationTarget.motion.localPositionMeters -
                        playerState->renderTransform.motion.localPositionMeters
                    );
            }

            if (m_hasPreviousLabPlayerRenderLocal)
            {
                labTelemetry.playerRenderStepMeters =
                    glm::length(
                        playerState->renderTransform.motion.localPositionMeters -
                        m_previousLabPlayerRenderLocalMeters
                    );
            }

            m_previousLabPlayerRenderLocalMeters =
                playerState->renderTransform.motion.localPositionMeters;
            m_hasPreviousLabPlayerRenderLocal = true;
        }
        else
        {
            m_hasPreviousLabPlayerRenderLocal = false;
        }

        for (const auto& [id, ship] : m_ships)
        {
            (void)id;

            if (ship.motionLabKind ==
                    game::diagnostics::HubMotionLabActorKind::SlowOrbit ||
                ship.motionLabKind ==
                    game::diagnostics::HubMotionLabActorKind::FastOrbit)
            {
                const auto expected =
                    game::diagnostics::evaluateHubMotionLabActor(
                        ship.motionLabKind,
                        renderTime
                    );

                const double errorMeters = glm::length(
                    ship.renderTransform.motion.localPositionMeters -
                    expected.positionMeters
                );

                if (ship.motionLabKind ==
                    game::diagnostics::HubMotionLabActorKind::SlowOrbit)
                {
                    labTelemetry.slowNpcLocalErrorMeters = errorMeters;
                }
                else
                {
                    labTelemetry.fastNpcLocalErrorMeters = errorMeters;
                }
            }
            else if (ship.motionLabKind ==
                         game::diagnostics::HubMotionLabActorKind::MatchPlayer &&
                     playerState)
            {
                const glm::dvec3 relativeToPredictedPlayer =
                    ship.renderTransform.motion.localPositionMeters -
                    playerState->renderTransform.motion.localPositionMeters;

                const auto* spec =
                    game::diagnostics::hubMotionLabSpec(
                        game::diagnostics::HubMotionLabActorKind::MatchPlayer
                    );

                if (spec)
                {
                    const double expectedDistance = std::sqrt(
                        spec->radiusMeters * spec->radiusMeters +
                        spec->radialOffsetMeters * spec->radialOffsetMeters
                    );

                    /*
                        Remote actors intentionally render on the delayed
                        presentation timeline while the local player is
                        predicted toward server-now. The resulting distance
                        delta is therefore a cross-timeline diagnostic, not a
                        remote interpolation error.
                    */
                    labTelemetry.matchVsPredictedPlayerDistanceDeltaMeters =
                        std::abs(
                            glm::length(relativeToPredictedPlayer) -
                            expectedDistance
                        );
                }

                if (haveDelayedPlayerFrame)
                {
                    const auto expectedAtRenderEpoch =
                        game::diagnostics::evaluateHubMotionLabActor(
                            game::diagnostics::HubMotionLabActorKind::MatchPlayer,
                            renderTime,
                            delayedPlayerFrame.localPositionMeters,
                            delayedPlayerFrame.localVelocityMetersPerSecond
                        );

                    labTelemetry.matchVsDelayedPlayerErrorMeters =
                        glm::length(
                            ship.renderTransform.motion.localPositionMeters -
                            expectedAtRenderEpoch.positionMeters
                        );
                }
            }
        }

        appendHubMotionLabPresentationCsv(labTelemetry);
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
                const float t =
                    static_cast<float>(
                        presentationWindow.interpolationAlpha
                    );

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


void ClientWorldState::prepareLocalPredictedPresentation(
    EntityId id,
    const ShipControlState& control,
    const WorldParams& world,
    float fractionalStepSeconds,
    float fixedStepSeconds
)
{
    clearLocalPredictedPresentation();

    auto it = m_ships.find(id.value);
    if (it == m_ships.end())
        return;

    const auto& ship = it->second;
    if (!isLocalControlledEntity(id) || !ship.descriptor)
        return;

    m_localPredictedPresentationTarget =
        game::client::presentation::sampleLocalPredictedPresentationTarget(
            ship.transform,
            ship.referenceFrame,
            ship.descriptor->physics,
            control,
            world,
            fractionalStepSeconds,
            fixedStepSeconds
        );

    m_localPredictedPresentationShipId = id.value;
    m_localPredictionRemainderSeconds =
        std::clamp(
            fractionalStepSeconds,
            0.0f,
            std::max(0.0f, fixedStepSeconds)
        );
    m_hasLocalPredictedPresentationTarget = true;
}

void ClientWorldState::clearLocalPredictedPresentation() noexcept
{
    m_hasLocalPredictedPresentationTarget = false;
    m_localPredictedPresentationShipId = 0;
    m_localPredictionRemainderSeconds = 0.0f;
}


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
    if (!isLocalControlledEntity(id) || !ship.descriptor)
        return;

    SharedShipPhysics::integrate(
        ship.transform,
        ship.descriptor->physics,
        control,
        world,
        dt
    );

    /*
        SharedShipPhysics currently owns attitude prediction only. The server
        advances HubTactical translation through DynamicMotionSystem, so the
        client must run that same deterministic motion step as part of input
        prediction. Otherwise localPositionMeters changes only when a server
        snapshot arrives and nearby co-frame infrastructure appears to pulse
        even though the hub frame itself is smooth.
    */
    (void)game::client::predictHubTacticalMotion(
        ship.transform,
        ship.referenceFrame,
        ship.descriptor->physics,
        control,
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
