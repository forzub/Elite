// GameSimulation: replication snapshot published to clients.

#include "src/game/simulation/GameSimulationInternal.h"



SimulationSnapshot GameSimulation::buildReplicationSnapshot(
    std::uint64_t serverTick
)
{
    // Replication is a publication concern, not part of the 50 Hz simulation
    // step. Build/copy DTO state only when GameServer actually publishes it.
    // Besides saving work, this guarantees dirty payload flags are consumed by
    // a snapshot that can really leave the authoritative runtime.
    SimulationSnapshot snapshot;
    snapshot.metadata.serverTimeSeconds = m_serverTimelineClock.timeSeconds();
    snapshot.metadata.serverTick = serverTick;
    snapshot.ships.clear();
    snapshot.objects.clear();
    snapshot.hubs.clear();
    snapshot.signals = m_worldSignals;


    // ----- тут собираем snapshot для кораблей ----------

    for (auto& [id, shipPtr] : m_ships)
    {
        Ship& ship = *shipPtr;
        ShipSnapshot s;

        const auto& productionTransform =
            ship.core().transform();

        const ShipTransform tr =
            presentationShipTransform(id);

        s.id = id;
        s.instanceId = ship.core().registry().instanceId;
        s.typeId = ship.core().desc().typeId;
        s.physicalControlModel =
            ship.core().state().construction.physicalControlModel;
        s.role = ship.core().role();
        s.motionLabKind = hubMotionLabActorKind(id);
        {
            const auto lifecycleIt = m_shipSimulationLifecycle.find(id);
            if (lifecycleIt != m_shipSimulationLifecycle.end())
            {
                s.simulationTier = lifecycleIt->second.tier;
                s.motionFidelity =
                    lifecycleIt->second.motionFidelity;
                s.materializedFidelity =
                    lifecycleIt->second.materializedFidelity;
            }
        }

        const auto navigationExecutionIt =
            m_npcNavigationExecutionSnapshots.find(id);
        if (navigationExecutionIt !=
            m_npcNavigationExecutionSnapshots.end() &&
            navigationExecutionIt->second.valid)
        {
            const auto& execution = navigationExecutionIt->second;
            game::simulation::NavigationExecutionSnapshot replicated;

            replicated.valid = true;
            replicated.intentRevision = execution.intentRevision;
            replicated.activeTargetRevision =
                execution.activeTargetRevision;

            replicated.idealLinearAccelerationDemandSystemMps2 = {
                execution.idealLinearAccelerationDemandSystemMps2.x,
                execution.idealLinearAccelerationDemandSystemMps2.y,
                execution.idealLinearAccelerationDemandSystemMps2.z
            };
            replicated.idealAngularAccelerationDemandSystemRadPerSec2 = {
                execution.idealAngularAccelerationDemandSystemRadPerSec2.x,
                execution.idealAngularAccelerationDemandSystemRadPerSec2.y,
                execution.idealAngularAccelerationDemandSystemRadPerSec2.z
            };
            replicated.executedLinearAccelerationDemandSystemMps2 = {
                execution.executedLinearAccelerationDemandSystemMps2.x,
                execution.executedLinearAccelerationDemandSystemMps2.y,
                execution.executedLinearAccelerationDemandSystemMps2.z
            };
            replicated.executedAngularAccelerationDemandSystemRadPerSec2 = {
                execution.executedAngularAccelerationDemandSystemRadPerSec2.x,
                execution.executedAngularAccelerationDemandSystemRadPerSec2.y,
                execution.executedAngularAccelerationDemandSystemRadPerSec2.z
            };

            replicated.emergency = execution.emergency;
            replicated.hazardUrgency01 = execution.hazardUrgency01;
            replicated.reactionBlocked = execution.reactionBlocked;
            replicated.decisionSampled = execution.decisionSampled;
            replicated.queuedCommandApplied =
                execution.queuedCommandApplied;
            replicated.pendingCommandCount = static_cast<std::uint32_t>(
                std::min<std::size_t>(
                    execution.pendingCommandCount,
                    std::numeric_limits<std::uint32_t>::max()
                )
            );

            s.navigationExecution = std::move(replicated);
        }

        s.transform = tr;
        // The working propulsion axis is a CURRENT equipment/CoM capability,
        // not a stale actuator sample carried through presentation transforms.
        // The engine executor itself uses ShipCore::effectivePhysics().
        // Publish that SAME server-computed reference into the authoritative
        // ship snapshot, even when this tick did not execute translation.
        if (game::ship::plant::familyOf(
                s.physicalControlModel) ==
                game::ship::plant::PhysicalControlFamily::Assisted)
        {
            const ShipParams flight=ship.core().effectivePhysics();
            s.transform.motion.assistedWorkingThrustAxisEmergency =
                flight.assistedWorkingThrustAxisEmergency;
            s.transform.motion.assistedWorkingThrustAxisBody =
                flight.assistedWorkingThrustAxisBody;
        }

        s.referenceFrame.systemId = tr.motion.systemId;
        s.referenceFrame.frameId = tr.motion.travelFrame.frameId;
        s.referenceFrame.matchedToReferenceFrame =
            tr.motion.matchedToReferenceFrame;
        s.referenceFrame.type = tr.motion.mode;
        s.referenceFrame.bodyId = tr.motion.parentBodyId;
        s.referenceFrame.hubId = tr.motion.hubId;
        s.referenceFrame.localPositionMeters =
            tr.motion.localPositionMeters;
        s.referenceFrame.localVelocityMetersPerSecond =
            tr.motion.localVelocityMps;
        s.referenceFrame.universeTimeSeconds =
            m_orbitalUniverseTimeSeconds;

        if (tr.motion.mode == game::navigation::MotionMode::HubTactical &&
            tr.motion.travelFrame.valid)
        {
            const auto& frame = tr.motion.travelFrame;
            s.referenceFrame.systemId = frame.systemId;
            s.referenceFrame.frameId = frame.frameId;
            s.referenceFrame.originMeters = frame.originMeters;
            s.referenceFrame.velocityMetersPerSecond =
                frame.linearVelocityMps;
            s.referenceFrame.accelerationMetersPerSecond2 =
                frame.linearAccelerationMps2;
            s.referenceFrame.angularVelocityWorldRadPerSecond =
                frame.angularVelocityWorldRadPerSecond;
            s.referenceFrame.angularAccelerationWorldRadPerSecond2 =
                frame.angularAccelerationWorldRadPerSecond2;
            s.referenceFrame.progradeAxis = frame.localToWorldBasis[0];
            s.referenceFrame.radialAxis = frame.localToWorldBasis[1];
            s.referenceFrame.normalAxis = frame.localToWorldBasis[2];
            s.referenceFrame.valid = true;

            // Hub identity remains target/reference metadata. Kinematics above
            // come from the ship-owned travel frame even while the two frames
            // are currently matched.
            const auto* hubFrame =
                hubNavigationFrame(tr.motion.hubId);
            if (hubFrame && hubFrame->valid)
            {
                s.referenceFrame.bodyId = hubFrame->parentBodyId;
                s.referenceFrame.hubId = hubFrame->hubId;
                s.referenceFrame.moduleId = hubFrame->primeModuleId;
            }
        }



        const bool detailedReplication =
            shipUsesDetailedSimulation(id);

        if (detailedReplication)
        {
            s.receptions = ship.core().signalResults();
            s.radarContacts = ship.core().radar().getContacts();
        }

        s.shipCoreStatus = ship.core().getCoreStatus();
        // Publish physical actuator capability on every ship snapshot even
        // when its large ObjectGraphSnapshot is sparse or suppressed.
        s.controlAuthorityTelemetry =
            ship.core().controlAuthorityTelemetry();

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

        const bool motionLabProbe = isHubMotionLabShip(id);
        const bool hadDetailedGraph =
            m_initializedShipGraphIds.find(id) !=
            m_initializedShipGraphIds.end();

        // This MUST be a const read: mutable modules() invalidates the
        // revision even when merely reading, causing continuous replication.
        const auto& readOnlyModules =
            static_cast<const world::modules::ObjectModuleRuntime&>(
                ship.core().moduleRuntime()
            );
        const std::uint64_t moduleRevision = readOnlyModules.revision();
        const auto lastModulePublication =
            m_lastPublishedShipModuleRevision.find(id);
        const bool moduleStateChanged =
            lastModulePublication ==
                m_lastPublishedShipModuleRevision.end() ||
            lastModulePublication->second != moduleRevision;

        if (!detailedReplication)
        {
            // If this client-visible entity was detailed before, publish one
            // explicit empty graph so canonical snapshot merging cannot retain
            // stale heavy state forever after demotion.
            m_shipGraphPayloadPublicationsRemaining.erase(id);
            m_lastPublishedShipModuleRevision.erase(id);

            if (hadDetailedGraph)
            {
                graph.hasModules = true;
                graph.hasStructuralLinks = true;
                graph.hasAssemblyModules = true;
                graph.hasDebugHitVolumes = true;
                graph.modules.clear();
                graph.structuralLinks.clear();
                graph.assemblyModules.clear();
                graph.debugHitVolumes.clear();

                m_initializedShipGraphIds.erase(id);
            }

            // Response rendering uses static descriptor assembly. Runtime
            // module/link/hit state remains server-side until Detailed.
            if (ship.core().hitVolumesDirty())
                ship.core().clearHitVolumesDirty();
        }
        else
        {
            auto resendIt =
                m_shipGraphPayloadPublicationsRemaining.find(id);
            const bool sendStructuralGraph =
                !motionLabProbe &&
                (!hadDetailedGraph ||
                 resendIt !=
                    m_shipGraphPayloadPublicationsRemaining.end());

            if (sendStructuralGraph)
            {
                graph.hasModules = true;
                graph.hasStructuralLinks = true;
                graph.hasAssemblyModules = true;
                graph.hasDebugHitVolumes = true;

                const auto& runtimeModules = readOnlyModules.modules();
                const auto& descModules =
                    ship.core().desc().moduleDescriptors();

                std::unordered_map<
                    std::string,
                    const world::modules::ObjectModuleState*
                > runtimeById;
                runtimeById.reserve(runtimeModules.size());
                for (const auto& mod : runtimeModules)
                    runtimeById[mod.moduleId] = &mod;

                graph.modules.reserve(descModules.size());
                for (const auto& descMod : descModules)
                {
                    const auto itRt =
                        runtimeById.find(descMod.moduleId);
                    const auto* rt =
                        itRt != runtimeById.end()
                            ? itRt->second
                            : nullptr;
                    graph.modules.push_back(
                        buildModuleSnapshot(descMod, rt)
                    );
                }

                const auto& links =
                    ship.core().structuralLinkRuntime().links();
                graph.structuralLinks.reserve(links.size());
                for (const auto& link : links)
                {
                    graph.structuralLinks.push_back(
                        buildStructuralLinkSnapshot(link)
                    );
                }

                graph.assemblyModules =
                    ship.core().assemblyRuntime().buildSnapshots();

                graph.debugHitVolumes =
                    world::modules::HitVolumeSnapshotBuilder::build(
                        ship.core().hitComponent()
                    );

                m_initializedShipGraphIds.insert(id);
                m_lastPublishedShipModuleRevision[id] = moduleRevision;

                if (resendIt !=
                    m_shipGraphPayloadPublicationsRemaining.end())
                {
                    --resendIt->second;
                    if (resendIt->second <= 0)
                    {
                        m_shipGraphPayloadPublicationsRemaining.erase(
                            resendIt
                        );
                    }
                }

                ship.core().clearHitVolumesDirty();
            }
            else
            {
                if (moduleStateChanged)
                {
                    // Preserve real module damage on motion-lab probes as
                    // well. They skip large structural and mesh graphs, but
                    // installed-equipment state must reach the client.
                    // Thermal protection, engine wear and replace-only annulus
                    // work all change the authoritative module revision.
                    // Publish only small module rows: structural links, mesh
                    // assembly and debug hit geometry stay sparse.
                    graph.hasModules = true;
                    const auto& definitions =
                        ship.core().desc().moduleDescriptors();
                    std::unordered_map<
                        std::string,
                        const world::modules::ObjectModuleState*
                    > runtimeById;
                    runtimeById.reserve(readOnlyModules.modules().size());
                    for (const auto& mod : readOnlyModules.modules())
                        runtimeById[mod.moduleId] = &mod;
                    graph.modules.reserve(definitions.size());
                    for (const auto& definition : definitions)
                    {
                        const auto it =
                            runtimeById.find(definition.moduleId);
                        graph.modules.push_back(buildModuleSnapshot(
                            definition,
                            it != runtimeById.end() ? it->second : nullptr
                        ));
                    }
                    m_lastPublishedShipModuleRevision[id] = moduleRevision;
                }

                // Independent debug geometry may change without module state.
                if (ship.core().hitVolumesDirty())
                {
                    graph.hasDebugHitVolumes = true;
                    graph.debugHitVolumes =
                        world::modules::HitVolumeSnapshotBuilder::build(
                            ship.core().hitComponent()
                        );
                    ship.core().clearHitVolumesDirty();
                }
            }
        }

        // Motion-lab probes omit the heavy damage/repair *structure*, but
        // MUST replicate module state above (engine faults/thermal authority).
        // Without module rows, the client cockpit and MotherComputer have
        // no factual equipment graph to reconcile with server thrust.
        if (!motionLabProbe && detailedReplication)
        {
            auto detachedFragments =
                ship.core().detachedFragmentRuntime().buildSnapshots(
                    tr.worldPosition
                );

            const bool hadDetachedFragments =
                m_shipsWithDetachedFragmentPayload.find(id) !=
                    m_shipsWithDetachedFragmentPayload.end();

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

            if (m_universeDiagnosticTrajectories.find(id))
            {
                const auto rebaseWorldPosition =
                    [&](const world::coordinates::WorldPosition& p)
                    {
                        return world::coordinates::translated(
                            tr.worldPosition,
                            world::coordinates::relativeMeters(
                                p,
                                productionTransform.worldPosition
                            )
                        );
                    };

                for (auto& job : repairJobs)
                {
                    job.droneWorldPosition =
                        rebaseWorldPosition(job.droneWorldPosition);
                    job.fragmentWorldPosition =
                        rebaseWorldPosition(job.fragmentWorldPosition);
                    job.homeWorldPosition =
                        rebaseWorldPosition(job.homeWorldPosition);
                }
            }

            const bool hadRepairJobs =
                m_shipsWithRepairJobPayload.find(id) !=
                    m_shipsWithRepairJobPayload.end();

            if (!repairJobs.empty() || hadRepairJobs)
            {
                graph.hasRepairJobs = true;
                graph.repairJobs = std::move(repairJobs);

                if (graph.repairJobs.empty())
                    m_shipsWithRepairJobPayload.erase(id);
                else
                    m_shipsWithRepairJobPayload.insert(id);
            }
        }

        snapshot.ships.push_back(s);
    }

    // Lightweight shells publish only identity + transform. Client hydration
    // resolves descriptor/assembly by typeId and receives no heavy graph.
    for (const auto& [id, shell] : m_shipShells)
    {
        if (!shell.valid())
            continue;

        ShipSnapshot s;
        s.id = id;
        s.instanceId = shell.registry.instanceId;
        s.role = ShipRole::NPC;
        s.typeId = shell.definition->typeId;
        s.simulationTier = shell.lifecycle.tier;
        s.motionFidelity =
            shell.lifecycle.motionFidelity;
        s.materializedFidelity =
            shell.lifecycle.materializedFidelity;
        s.transform = shell.transform;

        s.referenceFrame.systemId = shell.transform.motion.systemId;
        s.referenceFrame.frameId = shell.transform.motion.travelFrame.frameId;
        s.referenceFrame.type = shell.transform.motion.mode;
        s.referenceFrame.localPositionMeters =
            shell.transform.motion.localPositionMeters;
        s.referenceFrame.localVelocityMetersPerSecond =
            shell.transform.motion.localVelocityMps;
        s.referenceFrame.universeTimeSeconds =
            m_orbitalUniverseTimeSeconds;

        if (shell.transform.motion.travelFrame.valid)
        {
            const auto& frame = shell.transform.motion.travelFrame;
            s.referenceFrame.originMeters = frame.originMeters;
            s.referenceFrame.velocityMetersPerSecond =
                frame.linearVelocityMps;
            s.referenceFrame.accelerationMetersPerSecond2 =
                frame.linearAccelerationMps2;
            s.referenceFrame.angularVelocityWorldRadPerSecond =
                frame.angularVelocityWorldRadPerSecond;
            s.referenceFrame.angularAccelerationWorldRadPerSecond2 =
                frame.angularAccelerationWorldRadPerSecond2;
            s.referenceFrame.progradeAxis = frame.localToWorldBasis[0];
            s.referenceFrame.radialAxis = frame.localToWorldBasis[1];
            s.referenceFrame.normalAxis = frame.localToWorldBasis[2];
            s.referenceFrame.valid = true;
        }

        snapshot.ships.push_back(std::move(s));
    }


    // ----- тут собираем snapshot для объектов ----------

    for (auto& [id, obj] : m_staticObjects)
    {
        ObjectSnapshot o;

        o.id = id;
        o.type = obj.type;
        o.systemId = obj.systemId;

        // Теперь obj.worldPosition — это настоящая double-позиция
        // (её нужно будет добавить в структуру StaticObjectData)
        // Истина — obj.worldPosition.
        // position оставляем только как legacy mirror.
        o.worldPosition = obj.worldPosition;
        o.setWorldPosition(obj.worldPosition);
        o.orientation = obj.orientation;
        o.linearVelocityMps = glm::dvec3(obj.linearVelocity);
        o.angularVelocityWorldRadPerSecond = glm::dvec3(obj.angularVelocity);
        o.displayName = obj.displayName;
        o.ownerName = obj.ownerName;
        o.navigationVisible = obj.systemMapVisible;
        o.navigationParentBodyId = obj.mapParentBodyId;
        o.orbitalMotion = obj.orbitalMotion;

        if (obj.attachedToHub &&
            obj.systemId >= 0 &&
            !obj.hubId.empty())
        {
            o.hubAttachment.systemId = obj.systemId;
            o.hubAttachment.hubId = obj.hubId;
            o.hubAttachment.moduleId = obj.hubModuleId;
            o.hubAttachment.localOffsetMeters = obj.hubLocalOffsetMeters;
            o.hubAttachment.localRotationDeg = obj.hubLocalRotationDeg;
            o.hubAttachment.localAngularVelocityDegPerSecond =
                obj.hubLocalAngularVelocityDegPerSecond;
            o.hubAttachment.inheritHubOrientation = obj.inheritHubOrientation;
            o.hubAttachment.valid = true;
        }

        // Вращение станции меняет только assemblyModules.
        // Тяжелые статические payload'ы не пересобираем и не шлем каждый fixed tick.
        bool sendStaticPayload = obj.staticSnapshotPayloadDirty;

        if (obj.staticSnapshotPayloadDirty)
        {
            obj.cachedModuleSnapshots.clear();

            const auto& objDesc = ObjectDescriptorRegistry::get(obj.type);
            const auto& runtimeModules =
                static_cast<const world::modules::ObjectModuleRuntime&>(
                    obj.moduleRuntime).modules();
            const auto& descModules = objDesc.moduleDescriptors();

            std::unordered_map<std::string, const world::modules::ObjectModuleState*> runtimeById;
            runtimeById.reserve(runtimeModules.size());
            for (const auto& mod : runtimeModules)
            {
                runtimeById[mod.moduleId] = &mod;
            }

            for (const auto& descMod : descModules)
            {
                const auto itRt = runtimeById.find(descMod.moduleId);
                const auto* rt =
                    itRt == runtimeById.end() ? nullptr : itRt->second;
                // SAME builder for ship and non-ship module snapshots:
                // material passports stay static; dynamic thermal/emitter
                // parameters cannot silently disappear from one code path.
                obj.cachedModuleSnapshots.push_back(
                    buildModuleSnapshot(descMod, rt)
                );
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

        snapshot.objects.push_back(o);
    }

    // Composite hubs participate in the same ordinary authoritative
    // replication stream as ships/static objects. System/Detail/Hub maps must
    // compose presentation from these facts instead of asking the server to
    // manufacture a second map-specific hub DTO.
    snapshot.hubs.reserve(m_orbitalHubs.size());
    for (const auto& [hubId, hub] : m_orbitalHubs)
    {
        (void)hubId;
        game::simulation::OrbitalHubSnapshot h;
        h.id = hub.id;
        h.name = hub.name;
        h.owner = hub.owner;
        h.systemId = hub.systemId;
        h.parentBodyId = hub.parentBodyId;
        h.worldPosition = hub.worldPosition;
        const auto hubVelocityIt = m_hubVelocityMetersPerSecond.find(hub.id);
        if (hubVelocityIt != m_hubVelocityMetersPerSecond.end())
            h.worldVelocityMps = hubVelocityIt->second;

        if (const auto* frame = hubNavigationFrame(hub.id);
            frame && frame->valid && frame->systemId == hub.systemId)
        {
            h.worldVelocityMps = frame->velocityMetersPerSecond;
            h.angularVelocityWorldRadPerSecond =
                frame->angularVelocityWorldRadPerSecond;
            h.primeModuleId = frame->primeModuleId;
        }

        h.orientation = hub.orientation;
        h.motion = hub.motion;
        snapshot.hubs.push_back(std::move(h));
    }


    return snapshot;

}
