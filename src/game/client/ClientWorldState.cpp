#include "src/game/geometry/CatalogShipAssembly.h"
#include "ClientWorldState.h"
#include "src/game/client/ShipBreachNodes.h"
#include "src/game/client/ClientSpatialDomain.h"
#include "src/game/client/ReferenceFramePresentation.h"
#include "src/game/client/ClientHubTacticalPrediction.h"
#include "src/game/shared/SharedShipPhysics.h"
#include "src/game/client/SnapshotPresentationWindow.h"
#include "src/game/network/ReplicationSnapshotMerge.h"
#include "src/game/client/presentation/LocalPredictedPresentation.h"
#include "src/game/client/ClientRemoteShipPresentation.h"
#include "src/game/ship/control/ShipControlExecutionGateway.h"
#include "src/game/ship/ShipPropulsionState.h"
#include <iostream>
#include "src/game/ship/ShipDescriptorRegistry.h"
#include "src/game/ship/refit/RepairRefit.h"
#include "src/world/descriptors/ObjectDescriptorRegistry.h"
#include "src/game/geometry/AssemblyMeshLibrary.h"
#include "src/game/geometry/ObjectAssemblyRegistry.h"
#include <algorithm>
#include <chrono>

#include <cmath>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/common.hpp>

#include "src/world/coordinates/WorldPosition.h"
#include <fstream>
#include <iomanip>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <thread>
#include <unordered_map>
#include "src/game/navigation/HubAttachedPose.h"
#include <unordered_set>

namespace
{
    // The snapshot history holds canonical (full-set) snapshots: in Sol ~1400 objects with their module graphs
    // each. Freeing one took ~25 ms, the whole 20-entry history ~0.5 s - on the jump into another system (the
    // system fence drops the history) that was one 0.5 s frame, and the fixed-step server, advanced by the frame
    // time, fell 0.5 s behind and caught up over the next frames (measured 2026-11-19, Sol -> Eps Eri). Dropped
    // snapshots are plain data (vectors / strings, no GL, no registry back-references): one background thread
    // frees them. DEV A/B: ELITE_DEV_SYNC_SNAPSHOT_RELEASE=1 frees them inline as before.
    class SnapshotHistoryReleaser
    {
    public:
        static SnapshotHistoryReleaser& instance()
        {
            static SnapshotHistoryReleaser releaser;
            return releaser;
        }

        static bool inline_()
        {
            static const bool sync = [] {
                const char* v = std::getenv("ELITE_DEV_SYNC_SNAPSHOT_RELEASE");
                return v && *v == '1';
            }();
            return sync;
        }

        void release(SimulationSnapshot&& snapshot)
        {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_pending.push_back(std::move(snapshot));
            }
            m_wake.notify_one();
        }

        ~SnapshotHistoryReleaser()
        {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_stop = true;
            }
            m_wake.notify_one();
            if (m_worker.joinable())
                m_worker.join();
        }

    private:
        SnapshotHistoryReleaser() : m_worker([this] { run(); }) {}

        void run()
        {
            std::vector<SimulationSnapshot> batch;
            for (;;)
            {
                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    m_wake.wait(lock, [this] { return m_stop || !m_pending.empty(); });
                    if (m_pending.empty() && m_stop)
                        return;
                    batch.swap(m_pending);
                }
                batch.clear();   // the frees, off the main thread
            }
        }

        std::mutex m_mutex;
        std::condition_variable m_wake;
        std::vector<SimulationSnapshot> m_pending;
        bool m_stop = false;
        std::thread m_worker;   // last: started after the members above exist
    };

    // moves a snapshot out of the history to the releaser (or frees it here with the DEV A/B switch)
    void releaseSnapshot(SimulationSnapshot& snapshot)
    {
        if (SnapshotHistoryReleaser::inline_())
        {
            SimulationSnapshot dropped = std::move(snapshot);
            (void)dropped;
            return;
        }
        SnapshotHistoryReleaser::instance().release(std::move(snapshot));
    }

    // The history (m_snapshotBuffer) is read only for poses: update()'s interpolation (ship transform /
    // referenceFrame, object worldPosition / orientation / systemId + graph.assemblyModules) and the map samplers
    // (ids, poses, names, hubAttachment, orbitalMotion, traffic, hubs). Nobody reads the module graphs, structural
    // links, fragments, repair jobs, hit volumes, city building states or the ships' sensor / damage event lists
    // from it - the live state (m_ships / m_objects) owns those. Keeping them in every one of the 20 samples made
    // each sparse apply deep-copy the whole retained set (Sol: ~1400 objects with their graphs, 52-105 ms canonical
    // per snapshot, measured 2026-11-19). The stripped payload goes to the releaser (no free on this thread).
    // DEV A/B: ELITE_DEV_FULL_SNAPSHOT_HISTORY=1 keeps the full samples as before.
    bool keepFullSnapshotHistory()
    {
        static const bool full = [] {
            const char* v = std::getenv("ELITE_DEV_FULL_SNAPSHOT_HISTORY");
            return v && *v == '1';
        }();
        return full;
    }

    void stripHistoryOnlyGraph(game::simulation::ObjectGraphSnapshot& graph, game::simulation::ObjectGraphSnapshot& garbage)
    {
        // assemblyModules stay: update() interpolates the station / city assembly rotation from the history
        graph.hasModules = false;
        graph.hasStructuralLinks = false;
        graph.hasDetachedFragments = false;
        graph.hasRepairJobs = false;
        graph.hasDebugHitVolumes = false;
        garbage.modules = std::move(graph.modules);
        garbage.structuralLinks = std::move(graph.structuralLinks);
        garbage.detachedFragments = std::move(graph.detachedFragments);
        garbage.repairJobs = std::move(graph.repairJobs);
        garbage.debugHitVolumes = std::move(graph.debugHitVolumes);
        graph.modules.clear();
        graph.structuralLinks.clear();
        graph.detachedFragments.clear();
        graph.repairJobs.clear();
        graph.debugHitVolumes.clear();
    }

    template <typename T>
    bool hasHeavyGraph(const T& value)
    {
        const auto& g = value.graph;
        return !g.modules.empty() || !g.structuralLinks.empty() || !g.detachedFragments.empty() ||
               !g.repairJobs.empty() || !g.debugHitVolumes.empty();
    }

    void stripToHistorySample(SimulationSnapshot& sample)
    {
        if (keepFullSnapshotHistory())
            return;
        SimulationSnapshot garbage;
        for (auto& ship : sample.ships)
        {
            if (!hasHeavyGraph(ship) && ship.receptions.empty() && ship.damageEvents.empty())
                continue;
            ShipSnapshot& g = garbage.ships.emplace_back();
            stripHistoryOnlyGraph(ship.graph, g.graph);
            g.receptions = std::move(ship.receptions);
            g.damageEvents = std::move(ship.damageEvents);
            ship.receptions.clear();
            ship.damageEvents.clear();
        }
        for (auto& object : sample.objects)
        {
            if (!hasHeavyGraph(object) && object.buildingStates.empty() && object.materializedVisits.empty() &&
                object.promotedAirSlots.empty())
                continue;
            ObjectSnapshot& g = garbage.objects.emplace_back();
            stripHistoryOnlyGraph(object.graph, g.graph);
            g.buildingStates = std::move(object.buildingStates);
            g.materializedVisits = std::move(object.materializedVisits);
            g.promotedAirSlots = std::move(object.promotedAirSlots);
            object.buildingStates.clear();
            object.materializedVisits.clear();
            object.promotedAirSlots.clear();
        }
        if (!garbage.ships.empty() || !garbage.objects.empty())
            releaseSnapshot(garbage);
    }

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

        // ONE inverse of the server projection (projectShipReferenceFrame): travel frame, local state, reference
        // velocity - the same helper the TCP decode uses
        game::simulation::restoreMotionFromReferenceFrame(transform.motion, frame);
        // frame identity (the projection's metadata). matchedReferenceFrameId is NOT derived here: the server writes
        // it (TravelFrameSystem::matchToReference: the reference hub's id, not necessarily motion.hubId) and the row
        // carries it
        transform.motion.systemId = frame.systemId;
        transform.motion.hubId = frame.hubId;
        transform.motion.parentBodyId = frame.bodyId;
        transform.motion.matchedToReferenceFrame =
            frame.matchedToReferenceFrame;
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

    ObjectType objectTypeOf(const ClientShipState& s) { return s.typeId; }
    ObjectType objectTypeOf(const ClientObjectState& s) { return s.type; }

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

        state.breachedNodeIds.clear();
        for (const auto& modDesc : state.descriptor->moduleDescriptors())
        {
            if (isEffectivelyVisible(modDesc.moduleId))
                continue;

            for (const auto& partId : modDesc.meshPartIds)
                state.hiddenPartIds.insert(partId);
            for (auto& node : game::client::breachNodesForLegacyModule(objectTypeOf(state), modDesc.moduleId))
                state.breachedNodeIds.insert(std::move(node));
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
    using HydrateClock = std::chrono::steady_clock;
    const auto hydrateBegin = HydrateClock::now();

    double shipGraphMs = 0.0;
    double shipAssemblyMs = 0.0;
    double shipHiddenMs = 0.0;
    double objectGraphMs = 0.0;
    double objectAssemblyMs = 0.0;
    double objectHiddenMs = 0.0;
    double hubMs = 0.0;
    double droneMs = 0.0;
    double canonicalMs = 0.0;
    double shipLoopMs = 0.0;      // the whole ship loop (the parts above included)
    double objectNewMs = 0.0;     // objects seen for the first time (whole body)
    double objectUpdateMs = 0.0;  // objects already known (whole body)
    int objectsNew = 0;
    double fenceClearMs = 0.0;    // the snapshot history dropped on a timeline / system change
    double objectRemoveMs = 0.0;  // objects no longer replicated, erased

    const auto elapsedMs = [](const auto& begin) {
        return std::chrono::duration<double, std::milli>(
            HydrateClock::now() - begin
        ).count();
    };

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
        // (the old history is freed off this thread: SnapshotHistoryReleaser)
        const auto phaseBegin = HydrateClock::now();
        for (auto& dropped : m_snapshotBuffer)
            releaseSnapshot(dropped);
        m_snapshotBuffer.clear();
        fenceClearMs = elapsedMs(phaseBegin);
    }

    m_snapshotTimelineRevision = incomingTimelineRevision;
    m_snapshotActiveSystemId = incomingActiveSystemId;

    std::unordered_set<std::uint32_t> authoritativeShipIds;
    authoritativeShipIds.reserve(snapshot.ships.size());

    std::unordered_set<std::uint32_t> authoritativeObjectIds;
    authoritativeObjectIds.reserve(snapshot.objects.size());

    // ------- передача ShipSnapshot ------
    const auto shipLoopBegin = HydrateClock::now();
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

            const auto previousPhysicalControlModel =
                state.physicalControlModel;
            state.physicalControlModel = s.physicalControlModel;
            if (previousPhysicalControlModel != state.physicalControlModel)
            {
                std::cout
                    << "[ShipModel][client] entity=" << s.id.value
                    << " applied="
                    << game::ship::plant::physicalControlModelName(
                           state.physicalControlModel
                       )
                    << " previous="
                    << game::ship::plant::physicalControlModelName(
                           previousPhysicalControlModel
                       )
                    << "\n";
            }

            state.acknowledgedControlTick = s.acknowledgedControlTick;
            state.motionLabKind = s.motionLabKind;
            state.simulationTier = s.simulationTier;
            state.motionFidelity = s.motionFidelity;
            state.materializedFidelity = s.materializedFidelity;
            state.catalogShipId = s.catalogShipId;
            state.descriptor =
                &ShipDescriptorRegistry::resolve(s.typeId, s.catalogShipId);
            // per-instance configuration (refit: another product in a slot): the same configured descriptor the
            // server flies (append-only store, stable pointers)
            if (!s.refit.productOverrides.empty())
                if (const auto* configured =
                        game::ship::refit::configuredDescriptor(*state.descriptor, s.refit.productOverrides))
                    state.descriptor = configured;
                // &ObjectDescriptorRegistry::get(s.typeId);

            state.transform       = s.transform;
            state.referenceFrame  = s.referenceFrame;
            if (const auto* execution =
                    std::get_if<
                        game::simulation::NavigationExecutionSnapshot
                    >(&s.navigationExecution))
            {
                state.navigationExecution = *execution;
            }
            else
            {
                state.navigationExecution = {};
            }
            applyReferenceFrameState(state.transform, state.referenceFrame);
            state.renderTransform = state.transform;
            state.renderReferenceFrame = state.referenceFrame;
            state.receptions = s.receptions;
            state.damageEvents = s.damageEvents;
            state.shipCoreStatus = s.shipCoreStatus;
            state.controlAuthorityTelemetry = s.controlAuthorityTelemetry;
            state.dockDispatcher = s.dockDispatcher;
            state.padService = s.padService;
            state.executedRoute = s.executedRoute;
            state.padNotice = s.padNotice;
            state.landingGear = s.landingGear;
            state.traffic = s.traffic;
            // per-instance configuration: a new override list (an applied refit) re-resolves the descriptor
            const bool configurationChanged = state.refit.productOverrides != s.refit.productOverrides;
            state.refit = s.refit;
            if (configurationChanged)
            {
                const auto* base = &ShipDescriptorRegistry::resolve(state.typeId, state.catalogShipId);
                const auto* configured = state.refit.productOverrides.empty()
                                             ? base
                                             : game::ship::refit::configuredDescriptor(*base, state.refit.productOverrides);
                state.descriptor = configured ? configured : base;
            }
            state.combat = s.combat;
            {
                const auto phaseBegin = HydrateClock::now();
                applyGraphSnapshot(
                    s.graph,
                    state.modules,
                    state.structuralLinks,
                    state.assemblyModules,
                    state.detachedFragments,
                    state.repairJobs,
                    state.debugHitVolumes
                );
                shipGraphMs += elapsedMs(phaseBegin);
            }

            

            // state.gpuMesh = &render::MeshLibrary::get(s.typeId);

            if (!s.catalogShipId.empty())
            {
                // A catalog ship without a mesh: its passport primitive blocks.
                state.assembly =
                    &game::ship::geometry::catalogShipAssembly(*state.descriptor);
                std::cerr << "[CatalogShip] client hydrated id=" << s.id.value << " " << s.catalogShipId
                          << " tier=" << static_cast<int>(s.simulationTier) << " modules=" << state.assembly->modules.size()
                          << " radius=" << state.assembly->boundRadius << '\n';
            }
            else if (!game::ship::geometry::AssemblyMeshLibrary::has(s.typeId))
            {
                throw std::runtime_error(
                    "[ClientWorldState] missing assembly for ship typeId=" +
                    std::to_string(static_cast<int>(s.typeId))
                );
            }

            if (s.catalogShipId.empty())
            {
                const auto phaseBegin = HydrateClock::now();
                state.assembly =
                    &game::ship::geometry::AssemblyMeshLibrary::get(s.typeId);
                const double phaseMs = elapsedMs(phaseBegin);
                shipAssemblyMs += phaseMs;
                if (phaseMs >= 50.0)
                {
                    std::cerr
                        << "[M8E-HYDRATE] ship-assembly type="
                        << static_cast<unsigned int>(s.typeId)
                        << " duration_ms=" << phaseMs
                        << "\n";
                }
            }

            {
                const auto phaseBegin = HydrateClock::now();
                rebuildHiddenPartIds(state);
                shipHiddenMs += elapsedMs(phaseBegin);
            }

            
        }
        else
        {
            auto& state = it->second;

            state.instanceId = s.instanceId;
            state.role = s.role;
            state.typeId = s.typeId;
            state.physicalControlModel = s.physicalControlModel;
            state.acknowledgedControlTick = s.acknowledgedControlTick;
            state.motionLabKind = s.motionLabKind;
            state.simulationTier = s.simulationTier;
            state.motionFidelity = s.motionFidelity;
            state.materializedFidelity = s.materializedFidelity;
            state.transform = s.transform;
            state.referenceFrame = s.referenceFrame;
            if (isLocalControlledEntity(EntityId {s.id}))
            {
                // B4: the authoritative attitude layer, raw (before any replay / prediction rewrites state.transform)
                m_ownAttitude.valid = true;
                m_ownAttitude.snapshot = s.transform;
                m_ownAttitude.snapshotServerTimeSeconds = snapshot.metadata.serverTimeSeconds;
            }
            if (const auto* execution =
                    std::get_if<
                        game::simulation::NavigationExecutionSnapshot
                    >(&s.navigationExecution))
            {
                state.navigationExecution = *execution;
            }
            else
            {
                state.navigationExecution = {};
            }
            applyReferenceFrameState(state.transform, state.referenceFrame);
            state.receptions = s.receptions;
            state.damageEvents = s.damageEvents;
            state.shipCoreStatus = s.shipCoreStatus;
            state.controlAuthorityTelemetry = s.controlAuthorityTelemetry;
            state.dockDispatcher = s.dockDispatcher;
            state.padService = s.padService;
            state.executedRoute = s.executedRoute;
            state.padNotice = s.padNotice;
            state.landingGear = s.landingGear;
            state.traffic = s.traffic;
            // per-instance configuration: a new override list (an applied refit) re-resolves the descriptor
            const bool configurationChanged = state.refit.productOverrides != s.refit.productOverrides;
            state.refit = s.refit;
            if (configurationChanged)
            {
                const auto* base = &ShipDescriptorRegistry::resolve(state.typeId, state.catalogShipId);
                const auto* configured = state.refit.productOverrides.empty()
                                             ? base
                                             : game::ship::refit::configuredDescriptor(*base, state.refit.productOverrides);
                state.descriptor = configured ? configured : base;
            }
            state.combat = s.combat;
            {
                const auto phaseBegin = HydrateClock::now();
                applyGraphSnapshot(
                    s.graph,
                    state.modules,
                    state.structuralLinks,
                    state.assemblyModules,
                    state.detachedFragments,
                    state.repairJobs,
                    state.debugHitVolumes
                );
                shipGraphMs += elapsedMs(phaseBegin);
            }

            {
                const auto phaseBegin = HydrateClock::now();
                rebuildHiddenPartIds(state);
                shipHiddenMs += elapsedMs(phaseBegin);
            }
        }

    }


    shipLoopMs = elapsedMs(shipLoopBegin);
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
        const auto objectBegin = HydrateClock::now();
        const bool objectNew = it == m_objects.end();
        struct ObjectTimer
        {
            double& sink;
            std::chrono::steady_clock::time_point begin;
            ~ObjectTimer() { sink += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count(); }
        } objectTimer {objectNew ? objectNewMs : objectUpdateMs, objectBegin};
        objectsNew += objectNew ? 1 : 0;

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
            state.angularVelocityWorldRadPerSecond =
                o.angularVelocityWorldRadPerSecond;
            state.renderOrientation = o.orientation;
            state.renderLinearVelocityMps = o.linearVelocityMps;
            state.renderAngularVelocityWorldRadPerSecond = o.angularVelocityWorldRadPerSecond;
            state.hubAttachment = o.hubAttachment;
            state.displayName = o.displayName;
            state.ownerName = o.ownerName;
            state.navigationVisible = o.navigationVisible;
            state.navigationParentBodyId = o.navigationParentBodyId;
            state.orbitalMotion = o.orbitalMotion;
            if (state.buildingStates.size() != o.buildingStates.size())
                std::cerr << "[CityDamage] client: object " << o.id.value << " " << o.buildingStates.size()
                          << " damaged buildings" << std::endl;
            state.buildingStates = o.buildingStates;
            state.materializedVisits = o.materializedVisits;
            state.promotedAirSlots = o.promotedAirSlots;
            state.occupiedBerths = o.occupiedBerths;

            {
                const auto phaseBegin = HydrateClock::now();
                applyGraphSnapshot(
                    o.graph,
                    state.modules,
                    state.structuralLinks,
                    state.assemblyModules,
                    state.detachedFragments,
                    state.debugHitVolumes
                );
                objectGraphMs += elapsedMs(phaseBegin);
            }
            // a new object with no static payload (audit SRV-3: objects coming back after a jump had none - no
            // obstacle geometry for the planner); counted, logged in summary
            if (!o.graph.hasModules && !o.graph.hasDebugHitVolumes)
            {
                static int bare = 0;
                if (++bare <= 3 || bare % 50 == 0)
                    std::cerr << "[Replication] object " << o.id.value << " '" << o.displayName
                              << "' arrived WITHOUT its static payload (" << bare << " so far)" << std::endl;
            }

             state.renderAssemblyModules = state.assemblyModules; 
            
            const auto& desc = ObjectDescriptorRegistry::get(o.type);
            
            if (!game::ship::geometry::AssemblyMeshLibrary::has(o.type))
            {
                throw std::runtime_error(
                    "[ClientWorldState] missing assembly for object type=" +
                    std::to_string(static_cast<int>(o.type))
                );
            }

            {
                const auto phaseBegin = HydrateClock::now();
                state.assembly =
                    &game::ship::geometry::AssemblyMeshLibrary::get(o.type);
                const double phaseMs = elapsedMs(phaseBegin);
                objectAssemblyMs += phaseMs;
                if (phaseMs >= 50.0)
                {
                    std::cerr
                        << "[M8E-HYDRATE] object-assembly type="
                        << static_cast<unsigned int>(o.type)
                        << " duration_ms=" << phaseMs
                        << "\n";
                }
            }

            {
                const auto phaseBegin = HydrateClock::now();
                rebuildHiddenPartIds(state);
                objectHiddenMs += elapsedMs(phaseBegin);
            }
        }
        else
        {
            auto& state = it->second;
            state.systemId = o.systemId;

            // state.worldPosition = o.worldPosition;
            state.setWorldPosition(o.worldPosition);
            state.orientation = o.orientation;
            state.linearVelocityMps = o.linearVelocityMps;
            state.angularVelocityWorldRadPerSecond =
                o.angularVelocityWorldRadPerSecond;
            state.hubAttachment = o.hubAttachment;
            state.displayName = o.displayName;
            state.ownerName = o.ownerName;
            state.navigationVisible = o.navigationVisible;
            state.navigationParentBodyId = o.navigationParentBodyId;
            state.orbitalMotion = o.orbitalMotion;
            if (state.buildingStates.size() != o.buildingStates.size())
                std::cerr << "[CityDamage] client: object " << o.id.value << " " << o.buildingStates.size()
                          << " damaged buildings" << std::endl;
            state.buildingStates = o.buildingStates;
            state.materializedVisits = o.materializedVisits;
            state.promotedAirSlots = o.promotedAirSlots;
            state.occupiedBerths = o.occupiedBerths;

            // Static object payload is sparse: for a rotating station the server sends
            // only graph.assemblyModules every tick. Heavy module/debug payload arrives
            // only on first snapshot or after structural changes.
            const bool modulesChanged = o.graph.hasModules;

            {
                const auto phaseBegin = HydrateClock::now();
                applyGraphSnapshot(
                    o.graph,
                    state.modules,
                    state.structuralLinks,
                    state.assemblyModules,
                    state.detachedFragments,
                    state.debugHitVolumes
                );
                objectGraphMs += elapsedMs(phaseBegin);
            }

            state.renderAssemblyModules = state.assemblyModules;

            if (modulesChanged)
            {
                const auto phaseBegin = HydrateClock::now();
                rebuildHiddenPartIds(state);
                objectHiddenMs += elapsedMs(phaseBegin);
            }
        }
    }

    const auto objectRemoveBegin = HydrateClock::now();
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
    objectRemoveMs = elapsedMs(objectRemoveBegin);

    std::unordered_set<std::string> authoritativeHubIds;
    authoritativeHubIds.reserve(snapshot.hubs.size());

    const auto hubBegin = HydrateClock::now();
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
        state.angularVelocityWorldRadPerSecond = hub.angularVelocityWorldRadPerSecond;
        state.orientation = hub.orientation;
        state.motion = hub.motion;
    }

    // the hub-attached objects this packet did not carry (the server sends a derivable one only on a change): their pose
    // follows from their hub's row (HubAttachedPose.h - the server's law), at this snapshot's universe time
    {
        std::unordered_set<std::uint32_t> carried;
        for (const auto& object : snapshot.objects)
            carried.insert(object.id.value);
        for (auto& [oid, object] : m_objects)
        {
            if (carried.count(oid) || !object.hubAttachment.valid || !object.hubAttachment.inheritHubOrientation)
                continue;
            bool turning = false;
            for (const auto& m : object.assemblyModules)
                turning = turning || m.rotationAngleRad != 0.0f;
            if (turning)
                continue;
            const auto hubIt = m_hubs.find(object.hubAttachment.hubId);
            if (hubIt == m_hubs.end())
                continue;
            const auto& hub = hubIt->second;
            const auto frame = game::navigation::makeReplicatedHubKinematicFrame(
                hub.systemId, hub.id, world::coordinates::fullMeters(hub.worldPosition), hub.worldVelocityMps,
                hub.angularVelocityWorldRadPerSecond, hub.orientation);
            if (!frame.valid)
                continue;
            const auto pose = game::navigation::poseHubAttached(
                frame.originMeters, frame.linearVelocityMps, frame.localToWorldBasis[0], frame.localToWorldBasis[1],
                frame.localToWorldBasis[2], frame.angularVelocityWorldRadPerSecond, object.hubAttachment,
                snapshot.metadata.universeTimeSeconds);
            if (!pose.valid)
                continue;
            object.worldPosition = world::coordinates::makeWorldPositionFromMeters(pose.worldMeters);
            object.orientation = pose.orientation;
            object.linearVelocityMps = pose.linearVelocityMps;
            object.angularVelocityWorldRadPerSecond = pose.angularVelocityWorldRadPerSecond;
        }
    }

    hubMs += elapsedMs(hubBegin);

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

    const auto droneBegin = HydrateClock::now();
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









    droneMs += elapsedMs(droneBegin);

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
            object.renderLinearVelocityMps = object.linearVelocityMps;
            object.renderAngularVelocityWorldRadPerSecond = object.angularVelocityWorldRadPerSecond;
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

    {
        const auto phaseBegin = HydrateClock::now();
        m_snapshotBuffer.push_back(
            game::network::materializeCanonicalReplicationSnapshot(
                previousCanonical,
                snapshot
            )
        );
        // a pose-only history sample (see stripToHistorySample): the next sparse apply copies only that
        stripToHistorySample(m_snapshotBuffer.back());
        canonicalMs += elapsedMs(phaseBegin);
    }

    double historyTrimMs = 0.0;
    {
        const auto phaseBegin = HydrateClock::now();
        while (m_snapshotBuffer.size() > 20)
        {
            releaseSnapshot(m_snapshotBuffer.front());
            m_snapshotBuffer.pop_front();
        }
        historyTrimMs = elapsedMs(phaseBegin);
    }

    const double totalMs = elapsedMs(hydrateBegin);
    if (static const bool entryTiming = [] {
            const char* v = std::getenv("ELITE_DEV_JUMP_ENTRY_TIMING");
            return v && *v == '1';
        }();
        entryTiming && (timelineRevisionChanged || activeSystemChanged))
        std::cerr << "[JumpEntryTiming] client fence (history dropped): apply-snapshot " << totalMs << " ms, of it the history release "
                  << fenceClearMs << " ms" << (SnapshotHistoryReleaser::inline_() ? " (inline, DEV A/B)" : " (off-thread)") << std::endl;
    // DEV: ELITE_DEV_HYDRATE_LOG_ALL=1 - the line below for every apply (the canonical_ms distribution), not only >= 100 ms
    static const bool hydrateLogAll = [] {
        const char* v = std::getenv("ELITE_DEV_HYDRATE_LOG_ALL");
        return v && *v == '1';
    }();
    if (totalMs >= 100.0 || hydrateLogAll)
    {
        std::cerr
            << "[M8E-HYDRATE] apply-snapshot total_ms=" << totalMs
            << " ships=" << snapshot.ships.size()
            << " objects=" << snapshot.objects.size()
            << " hubs=" << snapshot.hubs.size()
            << " ship_graph_ms=" << shipGraphMs
            << " ship_assembly_ms=" << shipAssemblyMs
            << " ship_hidden_ms=" << shipHiddenMs
            << " object_graph_ms=" << objectGraphMs
            << " object_assembly_ms=" << objectAssemblyMs
            << " object_hidden_ms=" << objectHiddenMs
            << " hub_ms=" << hubMs
            << " drone_ms=" << droneMs
            << " canonical_ms=" << canonicalMs
            << " ship_loop_ms=" << shipLoopMs
            << " object_new_ms=" << objectNewMs << " (" << objectsNew << " new)"
            << " object_update_ms=" << objectUpdateMs
            << " fence_clear_ms=" << fenceClearMs
            << " object_remove_ms=" << objectRemoveMs
            << " history_trim_ms=" << historyTrimMs
            << "\n";
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

void ClientWorldState::update(
    float dt,
    bool authoritativePlayerRendering,
    double renderServerTimeSeconds,
    double estimatedServerTimeNowSeconds
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

    // id -> sample of the interpolation bracket, built once a frame (a linear find_if per ship / object made the
    // presentation O(N^2): ~1400 objects in Sol)
    std::unordered_map<std::uint32_t, const ShipSnapshot*> olderShipById;
    std::unordered_map<std::uint32_t, const ShipSnapshot*> newerShipById;
    std::unordered_map<std::uint32_t, const ObjectSnapshot*> olderObjectById;
    std::unordered_map<std::uint32_t, const ObjectSnapshot*> newerObjectById;
    if (older && newer)
    {
        const auto indexShips = [](const SimulationSnapshot& sample, auto& out) {
            out.reserve(sample.ships.size());
            for (const ShipSnapshot& s : sample.ships)
                out.emplace(static_cast<std::uint32_t>(s.id.value), &s);   // first wins, as find_if did
        };
        const auto indexObjects = [](const SimulationSnapshot& sample, auto& out) {
            out.reserve(sample.objects.size());
            for (const ObjectSnapshot& o : sample.objects)
                out.emplace(static_cast<std::uint32_t>(o.id.value), &o);
        };
        indexShips(*older, olderShipById);
        indexShips(*newer, newerShipById);
        indexObjects(*older, olderObjectById);
        indexObjects(*newer, newerObjectById);
    }
    const auto lookup = [](const auto& index, std::uint32_t id) {
        const auto it = index.find(id);
        return it == index.end() ? nullptr : it->second;
    };

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

            const ShipSnapshot* oldIt = lookup(olderShipById, static_cast<std::uint32_t>(shipId));
            const ShipSnapshot* newIt = lookup(newerShipById, static_cast<std::uint32_t>(shipId));

            if (!oldIt || !newIt)
            {
                return fallback;
            }

            auto frame = game::client::interpolateReferenceFramePresentation(
                oldIt->referenceFrame,
                newIt->referenceFrame,
                presentationWindow.interpolationAlpha
            );
            // A frame change between the two snapshots: there is nothing to
            // interpolate, the helper returns the new frame as it was at the NEWER
            // snapshot - ahead of the render epoch by (1 - alpha) * span. At orbital /
            // heliocentric speed that put the ship 1.2-1.5 km off the station drawn
            // at render time for a few frames (the hub jumped on J on / off). Carry
            // the new frame back to the render epoch along its own velocity.
            if (!game::client::sameReferenceFrameIdentity(oldIt->referenceFrame, newIt->referenceFrame))
            {
                const double back =
                    (1.0 - std::clamp(presentationWindow.interpolationAlpha, 0.0, 1.0)) * span;
                frame.originMeters -= frame.velocityMetersPerSecond * back;
            }
            return frame;
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

            const ShipTransform previousRender = ship.renderTransform;
            const auto previousRenderFrame = ship.renderReferenceFrame;

            ship.renderTransform =
                smoothShipRenderTransform(
                    ship.renderTransform,
                    *presentationTarget,
                    ship.renderReferenceFrame,
                    ship.referenceFrame,
                    dt
                );
            // B4: the own ship's ORIENTATION from its layers (ELITE_DEV_ATTITUDE_LEGACY=1: the old exponential chase, A/B)
            static const bool attitudeLegacy = [] {
                const char* v = std::getenv("ELITE_DEV_ATTITUDE_LEGACY");
                return v && *v == '1';
            }();
            auto& L = m_ownAttitude;
            if (!attitudeLegacy && L.valid && estimatedServerTimeNowSeconds > 0.0 &&
                L.snapshot.motion.systemId == ship.transform.motion.systemId)
            {
                constexpr double MaxHorizonSeconds = 0.30, CorrectionTauSeconds = 0.12, SnapDeg = 25.0;
                const double now = std::max(estimatedServerTimeNowSeconds, L.lastNowSeconds);   // never backwards
                L.lastNowSeconds = now;
                ShipTransform predicted = L.snapshot;
                const double horizon = std::clamp(now - L.snapshotServerTimeSeconds, 0.0, MaxHorizonSeconds);
                if (horizon > 0.0)
                    SharedShipPhysics::propagateOrientation(predicted, static_cast<float>(horizon));
                const glm::quat qPred = glm::normalize(glm::quat_cast(glm::mat3(predicted.orientation)));
                if (L.consumedSnapshotTime != L.snapshotServerTimeSeconds)
                {
                    // a new authoritative state: what was presented, expressed against the new prediction
                    L.consumedSnapshotTime = L.snapshotServerTimeSeconds;
                    L.correction = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                    if (L.presentedValid)
                    {
                        const glm::quat err = glm::normalize(L.presented * glm::inverse(qPred));
                        const float angle = 2.0f * std::acos(std::clamp(std::abs(err.w), 0.0f, 1.0f));
                        if (glm::degrees(angle) < SnapDeg)
                            L.correction = err;
                    }
                    L.correctionAtSeconds = now;
                }
                const float keep = static_cast<float>(std::exp(-(now - L.correctionAtSeconds) / CorrectionTauSeconds));
                const glm::quat corr = glm::slerp(glm::quat(1.0f, 0.0f, 0.0f, 0.0f), L.correction, keep);
                const glm::quat presented = glm::normalize(corr * qPred);
                L.presented = presented;
                L.presentedValid = true;
                const glm::mat3 R = glm::mat3_cast(presented);
                glm::mat4 o(R);
                o[3] = ship.renderTransform.orientation[3];
                ship.renderTransform.orientation = o;
            }
            else
                L.presentedValid = false;

            if (ship.referenceFrame.valid)
            {
                const auto presentationFrame =
                    sampleRenderReferenceFrame(id);

                // A frame change (e.g. J free flight re-bases the ship into its
                // own frame) reaches the newest snapshot before the render
                // epoch: the world is still drawn ~one interpolation window
                // in the past, in the previous frame. Presenting the ship in
                // the newest frame then put it at "now" while the hub stayed
                // at "then" - kilometres apart at orbital speed (the hub
                // vanished for ~0.3 s). Until the render epoch crosses the
                // change, keep the ship in the frame the world is drawn in.
                if (presentationFrame.valid &&
                    !game::client::sameReferenceFrameIdentity(presentationFrame, ship.referenceFrame) &&
                    game::client::sameReferenceFrameIdentity(presentationFrame, previousRenderFrame))
                {
                    const glm::dvec3 local =
                        previousRender.motion.localPositionMeters +
                        previousRender.motion.localVelocityMps * static_cast<double>(dt);
                    const glm::mat4 orientation = ship.renderTransform.orientation;
                    ship.renderTransform = previousRender;
                    ship.renderTransform.orientation = orientation;
                    ship.renderTransform.motion.localPositionMeters = local;
                    ship.renderReferenceFrame = presentationFrame;
                    ship.renderReferenceFrame.localPositionMeters = local;
                    ship.renderReferenceFrame.localVelocityMetersPerSecond = previousRender.motion.localVelocityMps;
                    ship.renderTransform.setWorldPositionMeters(presentationFrame.localToWorldPosition(local));
                    ship.signalPresentation.update(dt, ship.receptions);
                    continue;
                }

                // The render epoch has just crossed the change: the smoothing above
                // blended a local position of the OLD frame (kilometres from the hub)
                // with one of the NEW frame - for one frame the ship (camera) stood
                // ~1.5-2 km off and the hub jumped. Enter the new frame at its own
                // local state, no blend across frames.
                if (presentationFrame.valid && previousRenderFrame.valid &&
                    game::client::sameReferenceFrameIdentity(presentationFrame, ship.referenceFrame) &&
                    !game::client::sameReferenceFrameIdentity(presentationFrame, previousRenderFrame))
                {
                    ship.renderTransform.motion.localPositionMeters = presentationTarget->motion.localPositionMeters;
                    ship.renderTransform.motion.localVelocityMps = presentationTarget->motion.localVelocityMps;
                }

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

                    const ShipSnapshot* itOld = lookup(olderShipById, static_cast<std::uint32_t>(id));
                    const ShipSnapshot* itNew = lookup(newerShipById, static_cast<std::uint32_t>(id));

                    if (itOld && itNew)
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
                            const bool clientDeadReckoning =
                                ship.motionFidelity ==
                                    game::simulation::ShipMotionFidelity::RouteOnly ||
                                ship.motionFidelity ==
                                    game::simulation::ShipMotionFidelity::Kinematic;

                            if (clientDeadReckoning)
                            {
                                // Background truth may arrive only at 2-10 Hz.
                                // The client owns smooth presentation between
                                // authoritative epochs, never combat/collision
                                // truth. A new server snapshot always corrects
                                // this presentation path.
                                const auto deadReckoning =
                                    game::client::
                                        interpolateRemoteShipPresentation(
                                            *itOld,
                                            older->metadata.serverTimeSeconds,
                                            *itNew,
                                            newer->metadata.serverTimeSeconds,
                                            renderTime
                                        );
                                ship.renderTransform = itOld->transform;
                                ship.renderTransform.orientation =
                                    smoothOrientationMatrix(
                                        itOld->transform.orientation,
                                        itNew->transform.orientation,
                                        t
                                    );

                                if (sameReferenceFrame(
                                        itOld->referenceFrame,
                                        itNew->referenceFrame
                                    ))
                                {
                                    const glm::dvec3 localPosition =
                                        deadReckoning.localPositionMeters;

                                    ship.renderReferenceFrame =
                                        game::client::
                                            interpolateReferenceFramePresentation(
                                                itOld->referenceFrame,
                                                itNew->referenceFrame,
                                                static_cast<double>(t)
                                            );
                                    ship.renderReferenceFrame
                                        .localPositionMeters = localPosition;
                                    ship.renderReferenceFrame
                                        .localVelocityMetersPerSecond =
                                            deadReckoning.localVelocityMetersPerSecond;

                                    ship.renderTransform.motion
                                        .localPositionMeters = localPosition;
                                    ship.renderTransform.motion
                                        .localVelocityMps =
                                            deadReckoning.localVelocityMetersPerSecond;
                                    ship.renderTransform.setWorldPositionMeters(
                                        ship.renderReferenceFrame
                                            .localToWorldPosition(localPosition)
                                    );
                                }
                                else
                                {
                                    ship.renderTransform.setWorldPosition(
                                        deadReckoning.worldPosition
                                    );
                                }
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
                                ship.renderTransform.setWorldPosition(
                                    interpolatedWorld
                                );

                                if (sameReferenceFrame(
                                        itOld->referenceFrame,
                                        itNew->referenceFrame
                                    ))
                                {
                                    const glm::dvec3 localPosition =
                                        itOld->referenceFrame
                                            .localPositionMeters +
                                        (itNew->referenceFrame
                                             .localPositionMeters -
                                         itOld->referenceFrame
                                             .localPositionMeters) *
                                            static_cast<double>(t);

                                    const glm::dvec3 localVelocity =
                                        itOld->referenceFrame
                                            .localVelocityMetersPerSecond +
                                        (itNew->referenceFrame
                                             .localVelocityMetersPerSecond -
                                         itOld->referenceFrame
                                             .localVelocityMetersPerSecond) *
                                            static_cast<double>(t);

                                    ship.renderTransform.motion
                                        .localPositionMeters = localPosition;
                                    ship.renderTransform.motion
                                        .localVelocityMps = localVelocity;
                                    ship.renderReferenceFrame =
                                        game::client::
                                            interpolateReferenceFramePresentation(
                                                itOld->referenceFrame,
                                                itNew->referenceFrame,
                                                static_cast<double>(t)
                                            );
                                    ship.renderReferenceFrame
                                        .localPositionMeters = localPosition;
                                    ship.renderReferenceFrame
                                        .localVelocityMetersPerSecond =
                                            localVelocity;

                                    ship.renderTransform.setWorldPositionMeters(
                                        ship.renderReferenceFrame
                                            .localToWorldPosition(localPosition)
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

                const ObjectSnapshot* itOld = lookup(olderObjectById, static_cast<std::uint32_t>(id));
                const ObjectSnapshot* itNew = lookup(newerObjectById, static_cast<std::uint32_t>(id));

                if (itOld && itNew)
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
                        obj.renderLinearVelocityMps = obj.linearVelocityMps;
                        obj.renderAngularVelocityWorldRadPerSecond = obj.angularVelocityWorldRadPerSecond;
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
                        // DEV: ELITE_DEV_HUB_POSE_TRACE - is the station's pose refreshed every snapshot?
                        {
                            static const bool trace = std::getenv("ELITE_DEV_HUB_POSE_TRACE") != nullptr;
                            static double lastOld = -1.0;
                            if (trace && id == 1 && older->metadata.serverTimeSeconds != lastOld)
                            {
                                lastOld = older->metadata.serverTimeSeconds;
                                std::fprintf(stderr, "[HubPoseSnap] station span %.4f s, |delta| %.1f m (%.0f m/s), alpha %.3f\n",
                                             span, glm::length(delta), span > 0.0 ? glm::length(delta) / span : 0.0, t);
                            }
                        }

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
                        obj.renderLinearVelocityMps =
                            itOld->linearVelocityMps + (itNew->linearVelocityMps - itOld->linearVelocityMps) * static_cast<double>(t);
                        obj.renderAngularVelocityWorldRadPerSecond =
                            itOld->angularVelocityWorldRadPerSecond +
                            (itNew->angularVelocityWorldRadPerSecond - itOld->angularVelocityWorldRadPerSecond) * static_cast<double>(t);

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
                    obj.renderLinearVelocityMps = obj.linearVelocityMps;
                    obj.renderAngularVelocityWorldRadPerSecond = obj.angularVelocityWorldRadPerSecond;
                    obj.renderAssemblyModules = obj.assemblyModules;
                    usedFallback = true;
                }
            }
            else
            {
                obj.renderWorldPosition = obj.worldPosition;
                obj.renderOrientation = obj.orientation;
                obj.renderLinearVelocityMps = obj.linearVelocityMps;
                obj.renderAngularVelocityWorldRadPerSecond = obj.angularVelocityWorldRadPerSecond;
                obj.renderAssemblyModules = obj.assemblyModules;
                usedFallback = true;
            }
        }
        else
        {
            obj.renderWorldPosition = obj.worldPosition;
            obj.renderOrientation = obj.orientation;
            obj.renderLinearVelocityMps = obj.linearVelocityMps;
            obj.renderAngularVelocityWorldRadPerSecond = obj.angularVelocityWorldRadPerSecond;
            obj.renderAssemblyModules = obj.assemblyModules;
            usedFallback = true;
        }






        




    }

    // A ship COUPLED to an object (standing on / carried by a hub city's pad service - author 2026-11-25: «он имеет
    // сцепку с площадкой ... стоял ровно без дрожи»): drawn from the object's pose AS DRAWN (the same interpolation epoch)
    // times its pose in the object's frame, interpolated between the same two snapshots. The predicted own ship ran
    // ~0.2 s ahead of the drawn city: on a turning ring ~20 m off its pad, jittering ~0.2 m a frame.
    for (auto& [id, ship] : m_ships)
    {
        const ShipSnapshot* sn = lookup(newerShipById, static_cast<std::uint32_t>(id));
        if (!sn)
            continue;
        const auto& mn = sn->transform.motion;
        if (mn.attachedObjectId == 0)
            continue;
        const auto objIt = m_objects.find(mn.attachedObjectId);
        if (objIt == m_objects.end())
            continue;
        glm::dvec3 local = mn.attachedLocalPosition, up = mn.attachedLocalUp, back = mn.attachedLocalBack;
        if (const ShipSnapshot* so = lookup(olderShipById, static_cast<std::uint32_t>(id));
            so && so->transform.motion.attachedObjectId == mn.attachedObjectId)
        {
            const double t = std::clamp(presentationWindow.interpolationAlpha, 0.0, 1.0);
            local = glm::mix(so->transform.motion.attachedLocalPosition, local, t);
            up = glm::mix(so->transform.motion.attachedLocalUp, up, t);
            back = glm::mix(so->transform.motion.attachedLocalBack, back, t);
        }
        if (glm::length(up) < 1.0e-6 || glm::length(back) < 1.0e-6)
            continue;
        up = glm::normalize(up);
        back = glm::normalize(back - up * glm::dot(back, up));
        const glm::dmat3 L(glm::cross(up, back), up, back);
        const glm::dmat3 Ro(glm::mat3(objIt->second.renderOrientation));
        const glm::dvec3 world = world::coordinates::fullMeters(objIt->second.renderWorldPosition) + Ro * local;
        ship.renderTransform.setWorldPositionMeters(world);
        ship.renderTransform.orientation = glm::mat4(glm::mat3(Ro * L));
        // the renderer resolves a hub ship from its local place in the frame it is drawn in
        const auto& rf = ship.renderReferenceFrame;
        if (rf.valid)
        {
            const glm::dmat3 B(rf.progradeAxis, rf.radialAxis, rf.normalAxis);
            ship.renderTransform.motion.localPositionMeters = glm::transpose(B) * (world - rf.originMeters);
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
    (void)control;
    (void)world;
    clearLocalPredictedPresentation();

    auto it=m_ships.find(id.value);
    if(it==m_ships.end() || !isLocalControlledEntity(id))
        return;

    // Fractional presentation must use the SAME law as the fixed client
    // prediction: authoritative kinematics only. No second engine solver,
    // including in healthy Assisted, Newtonian, or damaged layouts.
    m_localPredictedPresentationTarget=it->second.transform;
    m_localPredictionRemainderSeconds=
        std::clamp(fractionalStepSeconds,0.0f,
                   std::max(0.0f,fixedStepSeconds));
    (void)game::client::advanceReplicatedHubKinematics(
        m_localPredictedPresentationTarget,
        it->second.referenceFrame,
        m_localPredictionRemainderSeconds);

    m_localPredictedPresentationShipId=id.value;
    m_hasLocalPredictedPresentationTarget=true;
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
    (void)control;
    (void)world;

    auto it=m_ships.find(id.value);
    if(it==m_ships.end() || !isLocalControlledEntity(id))
        return;

    // The simulation/engine/thermal/rigid-body solver runs ONCE, on the
    // authoritative server, for every player, NPC and collision.
    // Prediction advances only the last replicated motion state. It never
    // replays commands through a divergent slot-only engine approximation.
    // This is also the replay law for unacknowledged numbered inputs.
    (void)game::client::advanceReplicatedHubKinematics(
        it->second.transform,
        it->second.referenceFrame,
        dt);
}


//    ###                                                            ##                ##
//   ## ##                                                           ##                ##
//    #       ####    ######    ####     ####              #####    #####    ####     #####    ####
//  ####     ##  ##    ##  ##  ##  ##   ##  ##            ##         ##         ##     ##     ##  ##
//   ##      ##  ##    ##      ##       ######             #####     ##      #####     ##     ######
//   ##      ##  ##    ##      ##  ##   ##                     ##    ## ##  ##  ##     ## ##  ##
//  ####      ####    ####      ####     #####            ######      ###    #####      ###    #####
