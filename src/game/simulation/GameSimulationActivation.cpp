// GameSimulation: activation claims and the activation shadow (simulation-tier cadence).

#include "src/game/simulation/GameSimulationInternal.h"



void GameSimulation::registerActivationCadenceLabShip(EntityId shipId)
{
    if (shipId.value == 0)
        return;

    m_activationCadenceLabShipId = shipId;
}

bool GameSimulation::isActivationCadenceLabShip(
    EntityId shipId
) const noexcept
{
    return
        m_activationCadenceLabShipId.value != 0 &&
        shipId == m_activationCadenceLabShipId;
}

void GameSimulation::updateActivationCadenceLabClaim(
    double serverTimeSeconds
)
{
    using namespace game::simulation::activation;

    if constexpr (!game::diagnostics::ActivationCadenceLabEnabled)
        return;

    if (m_activationCadenceLabShipId.value == 0)
        return;

    // The diagnostic actor owns only its self-sourced lab claim, so clearing
    // this source cannot remove real claims owned by other systems.
    clearActivationClaimsFromSource(m_activationCadenceLabShipId);

    const auto demand =
        game::diagnostics::activationCadenceLabDemand(serverTimeSeconds);

    if (!demand.hasClaim)
        return;

    ActivationClaim claim;
    claim.subjectId = m_activationCadenceLabShipId;
    claim.sourceId = m_activationCadenceLabShipId;
    claim.systemId = m_activeCelestialSystemId;
    claim.minimumMode = demand.minimumMode;
    claim.kind = ActivationClaimKind::ScriptedCritical;
    claim.expiresAtServerTimeSeconds = serverTimeSeconds + 0.5;

    upsertActivationClaim(claim);
}

void GameSimulation::upsertActivationClaim(
    const game::simulation::activation::ActivationClaim& claim
)
{
    using namespace game::simulation::activation;

    if (claim.subjectId.value == 0 ||
        claim.sourceId.value == 0 ||
        claim.kind == ActivationClaimKind::None ||
        !activationClaimCanRaise(claim))
    {
        return;
    }

    for (auto& existing : m_activationClaims)
    {
        if (existing.subjectId == claim.subjectId &&
            existing.sourceId == claim.sourceId &&
            existing.kind == claim.kind)
        {
            existing = claim;
            return;
        }
    }

    m_activationClaims.push_back(claim);
}

void GameSimulation::clearActivationClaimsFromSource(EntityId sourceId)
{
    m_activationClaims.erase(
        std::remove_if(
            m_activationClaims.begin(),
            m_activationClaims.end(),
            [sourceId](const auto& claim)
            {
                return claim.sourceId == sourceId;
            }
        ),
        m_activationClaims.end()
    );
}

void GameSimulation::debugLogActivationShadow(double dt)
{
    using namespace game::simulation::activation;

    m_activationShadowCsvAccumulatorSeconds += std::max(0.0, dt);
    if (m_activationShadowCsvAccumulatorSeconds < 0.25)
        return;

    m_activationShadowCsvAccumulatorSeconds = 0.0;

    const char* path = "simulation_activation_shadow.csv";

    if (!m_activationShadowCsvInitialized)
    {
        std::ofstream reset(path, std::ios::trunc);
        if (!reset)
            return;

        reset
            << "server_time_s,entity_id,role,current_mode,desired_mode,reason,"
            << "requested_mode,planned_mode,plan_transition,"
            << "transition_serial,last_transition,last_transition_time_s,"
            << "claim_kind,claim_source_id,"
            << "broadphase_candidates,broadphase_comparable,broadphase_fallback,"
            << "broadphase_query_radius_m,broadphase_visited_cells,"
            << "broadphase_subject_residual_speed_mps,"
            << "broadphase_max_anchor_residual_speed_mps,"
            << "npc_ai_eligible,npc_ai_lab,npc_ai_lab_phase,"
            << "npc_ai_interval_s,npc_ai_time_since_think_s,"
            << "npc_ai_think_count,npc_ai_skipped_frames,"
            << "npc_ai_last_think_time_s,"
            << "ship_motion_control_interval_s,ship_motion_control_time_since_update_s,"
            << "ship_motion_control_update_count,ship_motion_control_skipped_frames,"
            << "ship_systems_interval_s,ship_systems_time_since_update_s,"
            << "ship_systems_update_count,ship_systems_skipped_frames,"
            << "ship_maintenance_interval_s,ship_maintenance_time_since_update_s,"
            << "ship_maintenance_update_count,ship_maintenance_skipped_frames,"
            << "anchor_id,anchor_kind,current_center_distance_m,"
            << "current_surface_distance_m,time_to_closest_s,"
            << "closest_center_distance_m,closest_surface_distance_m,"
            << "interaction_envelope_m,within_envelope,enters_horizon\n";

        m_activationShadowCsvInitialized = true;
    }

    std::ofstream out(path, std::ios::app);
    if (!out)
        return;

    out << std::fixed << std::setprecision(6);

    for (const auto& [id, planner] : m_activationPlannerDecisions)
    {
        const Ship* ship = getShip(id);
        if (!ship)
            continue;

        const auto& decision = planner.physicalDecision;
        const auto& claim = planner.claimDecision;
        const auto& plan = planner.planUpdate;

        const bool npcAiEligible =
            !isPlayerControlled(id) &&
            ship->core().role() != ShipRole::Player &&
            !isHubMotionLabShip(id) &&
            !isInterplanetaryTransferLabShip(id);
        const bool npcAiLab = isActivationCadenceLabShip(id);
        const auto npcAiLabDemand =
            npcAiLab
                ? game::diagnostics::activationCadenceLabDemand(
                    m_serverTimelineClock.timeSeconds()
                )
                : game::diagnostics::ActivationCadenceLabDemand{};
        const auto cadenceIt = m_npcAiCadenceStates.find(id);
        const bool hasCadence =
            npcAiEligible && cadenceIt != m_npcAiCadenceStates.end();
        const game::simulation::activation::ActivationCadenceState* cadenceState =
            hasCadence ? &cadenceIt->second : nullptr;
        const double aiIntervalSeconds =
            npcAiEligible
                ? game::simulation::activation::npcAiIntervalSeconds(
                    plan.plannedMode,
                    m_activationExecutionPolicy
                )
                : 0.0;

        const auto motionIt = m_shipMotionControlCadenceStates.find(id);
        const auto systemsIt = m_shipSystemsCadenceStates.find(id);
        const auto maintenanceIt = m_shipMaintenanceCadenceStates.find(id);
        const auto* motionState =
            motionIt != m_shipMotionControlCadenceStates.end()
                ? &motionIt->second
                : nullptr;
        const auto* systemsState =
            systemsIt != m_shipSystemsCadenceStates.end()
                ? &systemsIt->second
                : nullptr;
        const auto* maintenanceState =
            maintenanceIt != m_shipMaintenanceCadenceStates.end()
                ? &maintenanceIt->second
                : nullptr;

        const double motionIntervalSeconds =
            game::simulation::activation::shipMotionControlIntervalSeconds(
                plan.plannedMode,
                m_activationExecutionPolicy
            );
        const double systemsIntervalSeconds =
            game::simulation::activation::shipSystemsIntervalSeconds(
                plan.plannedMode,
                m_activationExecutionPolicy
            );
        const double maintenanceIntervalSeconds =
            game::simulation::activation::shipMaintenanceIntervalSeconds(
                plan.plannedMode,
                m_activationExecutionPolicy
            );

        out
            << m_serverTimelineClock.timeSeconds() << ','
            << id.value << ','
            << static_cast<int>(ship->core().role()) << ','
            << simulationModeName(decision.currentMode) << ','
            << simulationModeName(decision.desiredMode) << ','
            << activationReasonName(decision.reason) << ','
            << simulationModeName(claim.requestedMode) << ','
            << simulationModeName(plan.plannedMode) << ','
            << activationPlanTransitionName(plan.transition) << ','
            << plan.transitionSerial << ','
            << activationPlanTransitionName(plan.lastTransition) << ','
            << plan.lastTransitionServerTimeSeconds << ','
            << (claim.hasClaim
                    ? activationClaimKindName(claim.kind)
                    : "none") << ','
            << (claim.hasClaim ? claim.sourceId.value : 0u) << ','
            << decision.candidateAnchorCount << ','
            << decision.comparableAnchorCount << ','
            << (decision.broadphaseFallback ? 1 : 0) << ','
            << decision.broadphaseQueryRadiusMeters << ','
            << decision.broadphaseVisitedCellCount << ','
            << decision.broadphaseSubjectResidualSpeedMetersPerSecond << ','
            << decision.broadphaseMaxAnchorResidualSpeedMetersPerSecond << ','
            << (npcAiEligible ? 1 : 0) << ','
            << (npcAiLab ? 1 : 0) << ','
            << (npcAiLab ? npcAiLabDemand.phase : "none") << ','
            << (std::isfinite(aiIntervalSeconds) ? aiIntervalSeconds : -1.0) << ','
            << (cadenceState ? cadenceState->timeSinceLastExecutionSeconds : 0.0) << ','
            << (cadenceState ? cadenceState->executionCount : 0u) << ','
            << (cadenceState ? cadenceState->skippedFrameCount : 0u) << ','
            << (cadenceState ? cadenceState->lastExecutionServerTimeSeconds : 0.0) << ','
            << (std::isfinite(motionIntervalSeconds) ? motionIntervalSeconds : -1.0) << ','
            << (motionState ? motionState->timeSinceLastExecutionSeconds : 0.0) << ','
            << (motionState ? motionState->executionCount : 0u) << ','
            << (motionState ? motionState->skippedFrameCount : 0u) << ','
            << (std::isfinite(systemsIntervalSeconds) ? systemsIntervalSeconds : -1.0) << ','
            << (systemsState ? systemsState->timeSinceLastExecutionSeconds : 0.0) << ','
            << (systemsState ? systemsState->executionCount : 0u) << ','
            << (systemsState ? systemsState->skippedFrameCount : 0u) << ','
            << (std::isfinite(maintenanceIntervalSeconds) ? maintenanceIntervalSeconds : -1.0) << ','
            << (maintenanceState ? maintenanceState->timeSinceLastExecutionSeconds : 0.0) << ','
            << (maintenanceState ? maintenanceState->executionCount : 0u) << ','
            << (maintenanceState ? maintenanceState->skippedFrameCount : 0u) << ','
            << (decision.hasAnchor ? decision.anchorId.value : 0u) << ','
            << (decision.hasAnchor
                    ? activationAnchorKindName(decision.anchorKind)
                    : "none") << ','
            << decision.prediction.currentCenterDistanceMeters << ','
            << decision.prediction.currentSurfaceDistanceMeters << ','
            << decision.prediction.timeToClosestSeconds << ','
            << decision.prediction.closestCenterDistanceMeters << ','
            << decision.prediction.closestSurfaceDistanceMeters << ','
            << decision.prediction.interactionEnvelopeMeters << ','
            << (decision.prediction.currentlyWithinEnvelope ? 1 : 0) << ','
            << (decision.prediction.entersEnvelopeWithinHorizon ? 1 : 0)
            << '\n';
    }
}


void GameSimulation::updateActivationShadow()
{
    using namespace game::simulation::activation;

    m_activationPlannerDecisions.clear();

    const double serverTimeSeconds =
        m_serverTimelineClock.timeSeconds();

    updateActivationCadenceLabClaim(serverTimeSeconds);

    // Expired gameplay claims are cheap to prune at the 5 Hz planner cadence.
    m_activationClaims.erase(
        std::remove_if(
            m_activationClaims.begin(),
            m_activationClaims.end(),
            [serverTimeSeconds](const ActivationClaim& claim)
            {
                return
                    claim.expiresAtServerTimeSeconds > 0.0 &&
                    serverTimeSeconds > claim.expiresAtServerTimeSeconds;
            }
        ),
        m_activationClaims.end()
    );

    std::vector<ActivationAnchor> anchors;
    anchors.reserve(m_ships.size() + m_staticObjects.size());

    for (const auto& [id, shipPtr] : m_ships)
    {
        if (!shipPtr)
            continue;

        const auto& core = shipPtr->core();
        const auto& tr = core.transform();

        if (!game::simulation::sameRuntimeSystem(
                tr.motion.systemId,
                m_activeCelestialSystemId))
        {
            continue;
        }

        const glm::dvec3 positionMeters =
            world::coordinates::fullMeters(tr.worldPosition);

        ActivationAnchor anchor;
        anchor.id = id;
        anchor.kind = ActivationAnchorKind::Ship;
        anchor.systemId = tr.motion.systemId;
        anchor.kinematics.positionMeters = {
            positionMeters.x,
            positionMeters.y,
            positionMeters.z
        };
        anchor.kinematics.velocityMetersPerSecond = {
            tr.motion.worldVelocityMps.x,
            tr.motion.worldVelocityMps.y,
            tr.motion.worldVelocityMps.z
        };
        anchor.kinematics.bounds =
            makeSpatialBounds(core.descriptor().logicalDimensions());

        anchors.push_back(anchor);
    }

    // Static infrastructure is an interaction anchor, not yet an activation
    // subject. This catches cases such as a ship approaching a multi-kilometre
    // station even when the player is elsewhere. Whole-station activation is
    // intentionally deferred; large stations will later activate by sectors.
    for (const auto& [id, obj] : m_staticObjects)
    {
        if (!game::simulation::sameRuntimeSystem(
                obj.systemId,
                m_activeCelestialSystemId))
        {
            continue;
        }

        const auto& descriptor =
            ObjectDescriptorRegistry::get(obj.type);

        const glm::dvec3 positionMeters =
            world::coordinates::fullMeters(obj.worldPosition);

        ActivationAnchor anchor;
        anchor.id = id;
        anchor.kind = ActivationAnchorKind::StaticObject;
        anchor.systemId = obj.systemId;
        anchor.kinematics.positionMeters = {
            positionMeters.x,
            positionMeters.y,
            positionMeters.z
        };
        anchor.kinematics.velocityMetersPerSecond = {
            static_cast<double>(obj.linearVelocity.x),
            static_cast<double>(obj.linearVelocity.y),
            static_cast<double>(obj.linearVelocity.z)
        };
        anchor.kinematics.bounds =
            makeSpatialBounds(descriptor.logicalDimensions());

        anchors.push_back(anchor);
    }

    // Stage 3D replaces the temporary all-pairs subject/anchor traversal with
    // a conservative spatial broad-phase. Exact CPA remains authoritative for
    // the decision; the grid only prunes anchors that provably cannot enter the
    // interaction envelope within the configured look-ahead window.
    ActivationSpatialIndex spatialIndex;
    spatialIndex.rebuild(anchors);

    for (const auto& [id, shipPtr] : m_ships)
    {
        if (!shipPtr)
            continue;

        const auto& core = shipPtr->core();
        const auto& tr = core.transform();

        if (!game::simulation::sameRuntimeSystem(
                tr.motion.systemId,
                m_activeCelestialSystemId))
        {
            continue;
        }

        const glm::dvec3 positionMeters =
            world::coordinates::fullMeters(tr.worldPosition);

        KinematicPoint subject;
        subject.positionMeters = {
            positionMeters.x,
            positionMeters.y,
            positionMeters.z
        };
        subject.velocityMetersPerSecond = {
            tr.motion.worldVelocityMps.x,
            tr.motion.worldVelocityMps.y,
            tr.motion.worldVelocityMps.z
        };
        subject.bounds =
            makeSpatialBounds(core.descriptor().logicalDimensions());

        // currentMode reports the allocation actually consumed by the Stage 4A
        // execution lanes. The tactical planner still bottoms out at Coarse;
        // Scheduled materialization/collapse belongs to a later persistent-world
        // slice.
        ActivationSpatialQueryResult broadphaseQuery;
        if (!isPlayerControlled(id))
        {
            broadphaseQuery = spatialIndex.query(
                tr.motion.systemId,
                subject,
                m_activationInteractionPolicy
            );
        }

        const auto currentExecutionMode = activationExecutionMode(id);

        auto physicalDecision =
            evaluateActivationShadowCandidates(
                id,
                tr.motion.systemId,
                subject,
                currentExecutionMode,
                isPlayerControlled(id),
                anchors,
                broadphaseQuery.candidateIndices,
                m_activationInteractionPolicy,
                broadphaseQuery.usedFallback
            );

        physicalDecision.broadphaseQueryRadiusMeters =
            broadphaseQuery.conservativeQueryRadiusMeters;
        physicalDecision.broadphaseVisitedCellCount =
            broadphaseQuery.visitedCellCount;
        physicalDecision.broadphaseSubjectResidualSpeedMetersPerSecond =
            broadphaseQuery.subjectResidualSpeedMetersPerSecond;
        physicalDecision.broadphaseMaxAnchorResidualSpeedMetersPerSecond =
            broadphaseQuery.maxAnchorResidualSpeedMetersPerSecond;

        auto [stateIt, inserted] =
            m_activationPlanStates.try_emplace(id);

        if (inserted)
        {
            stateIt->second.plannedMode =
                game::simulation::SimulationMode::Active;
            stateIt->second.modeEnteredServerTimeSeconds =
                serverTimeSeconds;
            stateIt->second.releaseNotBeforeServerTimeSeconds =
                serverTimeSeconds +
                std::max(
                    0.0,
                    m_activationHysteresisPolicy.activeReleaseDelaySeconds
                );
        }

        m_activationPlannerDecisions[id] =
            evaluateActivationPlan(
                stateIt->second,
                physicalDecision,
                tr.motion.systemId,
                m_activationClaims,
                serverTimeSeconds,
                m_activationHysteresisPolicy
            );
    }
}
