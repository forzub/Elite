#include "src/game/simulation/GameSimulationInternal.h"


GameSimulation::GameSimulation(
    game::diagnostics::ServerDiagnostics& diagnostics
)
    : m_diagnostics(diagnostics)
{
    // ===================== ObjectDescriptor =========================

    ObjectDescriptorRegistry::ensureInitialized();
    game::ship::geometry::ObjectAssemblyRegistry::ensureInitialized();

    m_shipStateFlightTestRequested =
        environmentFlagEnabled("ELITE_SHIP_STATE_FLIGHT_TEST");
    m_physicalCorridorHotStartRequested =
        environmentFlagEnabled("ELITE_PHYSICAL_CORRIDOR_HOT_START");
    m_fiveHundredShipHerdRequested =
        environmentFlagEnabled("ELITE_500_SHIPS_TEST");
    m_fiveHundredMaterializedShipHerdRequested =
        environmentFlagEnabled("ELITE_500_MATERIALIZED_SHIPS_TEST") ||
        environmentFlagEnabled("ELITE_500_INTERACTION_TEST");
    m_fiveHundredFullMaterializedStressRequested =
        environmentFlagEnabled("ELITE_500_FULL_MATERIALIZED_STRESS");
    m_shipAutomaticDetailDisabled =
        environmentFlagEnabled("ELITE_DISABLE_AUTO_SHIP_DETAIL");
    m_shipTierTransitionTestRequested =
        environmentFlagEnabled("ELITE_SHIP_TIER_TRANSITION_TEST");



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




void GameSimulation::buildInitialScene(
    const game::world_state::InitialWorldState& initialState
)
{
    m_playerId =
        game::scene::buildInitialScene(*this, initialState);

    // Live physical-corridor diagnostics must switch the actual ship
    // construction, not merely relabel the stress overlay. This environment
    // variable is only set by the dedicated six-model corridor runner.
    if (const char* requestedModel =
            std::getenv("ELITE_SHIP_PHYSICAL_TEST_MODEL");
        requestedModel && *requestedModel)
    {
        game::ship::plant::PhysicalControlModelId model;
        if (physicalModelFromLabel(requestedModel, model))
        {
            if (Ship* player = playerShip())
            {
                if (player->core().
                        selectPhysicalConstructionForDiagnostic(model))
                {
                    std::cout
                        << "[PhysicalCorridor] player model override="
                        << physicalModelName(model)
                        << " law="
                        << game::navigation::localFlightControlLawName(
                               player->core().transform().
                                   motion.localControlLaw
                           )
                        << "\n";
                }
                else
                {
                    std::cerr
                        << "[PhysicalCorridor] failed player model override="
                        << requestedModel << "\n";
                }
            }
        }
        else
        {
            std::cerr
                << "[PhysicalCorridor] unknown model label="
                << requestedModel << "\n";
        }
    }

    // m_playerId is only the authored primary spawn compatibility alias.
    // Human control authority is established later by GameServer from
    // PlayerRegistry -> ControlRegistry -> ServerSessionRegistry. A dedicated
    // server may therefore boot with zero connected/player-controlled ships.

    if constexpr (game::promo::PromoFlybyScenario::Enabled)
    {
        m_promoFlybyScenario.setup(*this);
    }

    initializeShipStateFlightTest();
    initializeFiveHundredShipHerd();
    initializeShipTierTransitionTest();
}


void GameSimulation::applyPhysicalCorridorHotStart()
{
    if (!m_physicalCorridorHotStartRequested ||
        m_physicalCorridorHotStartApplied)
    {
        return;
    }

    Ship* player = playerShip();
    if (!player)
        return;

    auto& tr = player->core().transform();
    auto& motion = tr.motion;

    if (motion.mode != game::navigation::MotionMode::HubTactical ||
        !motion.travelFrame.valid)
    {
        return;
    }

    const glm::dvec3 oldForward =
        safeNormalizeD(
            glm::dvec3(tr.forward()),
            glm::dvec3(0.0, 0.0, -1.0)
        );
    const glm::dvec3 oldUp =
        safeNormalizeD(
            glm::dvec3(tr.up()),
            glm::dvec3(0.0, 1.0, 0.0)
        );
    const glm::dvec3 oldRight =
        safeNormalizeD(
            glm::dvec3(tr.right()),
            glm::dvec3(1.0, 0.0, 0.0)
        );

    // Diagnostic hot entry: the ship is already moving on its old course,
    // while the hull is deliberately yawed away from that course. This makes
    // route construction consume a real non-zero velocity boundary and proves
    // that navigation course is not reconstructed from hull nose.
    //
    // The ordinary matrix keeps the 300 m/s default. A dedicated high-speed
    // planner probe may override it without changing production physics.
    double startSpeedMps = 300.0;
    if (const char* requestedSpeed =
            std::getenv("ELITE_PHYSICAL_CORRIDOR_HOT_START_SPEED_MPS"))
    {
        char* end = nullptr;
        const double parsed = std::strtod(requestedSpeed, &end);
        if (end != requestedSpeed &&
            std::isfinite(parsed) &&
            parsed > 0.0)
        {
            startSpeedMps = parsed;
        }
    }

    double noseOffsetDeg = 60.0;
    if (const char* requestedOffset =
            std::getenv("ELITE_PHYSICAL_CORRIDOR_HOT_START_NOSE_OFFSET_DEG"))
    {
        char* end = nullptr;
        const double parsed = std::strtod(requestedOffset, &end);
        if (end != requestedOffset && std::isfinite(parsed))
            noseOffsetDeg = parsed;
    }
    const double noseOffsetRad =
        noseOffsetDeg * 0.0174532925199432957692;

    const glm::dvec3 newForward =
        safeNormalizeD(
            oldForward * std::cos(noseOffsetRad) +
            oldRight * std::sin(noseOffsetRad),
            oldForward
        );
    glm::dvec3 newRight =
        safeNormalizeD(
            glm::cross(newForward, oldUp),
            oldRight
        );
    const glm::dvec3 newUp =
        safeNormalizeD(
            glm::cross(newRight, newForward),
            oldUp
        );
    newRight =
        safeNormalizeD(
            glm::cross(newForward, newUp),
            newRight
        );

    tr.orientation = glm::mat4(1.0f);
    tr.orientation[0] =
        glm::vec4(glm::vec3(newRight), 0.0f);
    tr.orientation[1] =
        glm::vec4(glm::vec3(newUp), 0.0f);
    tr.orientation[2] =
        glm::vec4(glm::vec3(-newForward), 0.0f);

    const glm::dvec3 relativeWorldVelocity =
        oldForward * startSpeedMps;
    motion.localVelocityMps =
        motion.travelFrame.worldToLocalVector(
            relativeWorldVelocity
        );
    motion.worldVelocityMps =
        motion.referenceVelocityMps +
        relativeWorldVelocity;
    tr.referenceVelocityMetersPerSecond =
        motion.referenceVelocityMps;
    motion.forwardSpeedMps =
        glm::dot(relativeWorldVelocity, newForward);

    // Keep the diagnostic entry genuinely high-speed until the route request
    // is issued. Without this hold, ordinary Assisted stabilization immediately
    // bleeds the deliberately off-nose velocity and the planner ends up seeing
    // ~25-40 m/s instead of the requested 300 m/s hot start.
    motion.targetForwardSpeedMps = startSpeedMps;
    motion.assistedTargetSpeedHold = true;
    motion.assistedThrottleTrimWasActive = false;
    motion.velocityAlignmentMode =
        game::navigation::VelocityAlignmentMode::None;

    tr.pitchRate = 0.0f;
    tr.yawRate = 0.0f;
    tr.rollRate = 0.0f;

    m_physicalCorridorHotStartApplied = true;

    std::cout
        << "[PhysicalCorridor] hot_start=1"
        << " speed_mps=" << startSpeedMps
        << " nose_course_offset_deg=" << noseOffsetDeg
        << "\n";
}

void GameSimulation::updateShipShells(double dt)
{
    if (dt <= 0.0)
        return;

    const double now = m_serverTimelineClock.timeSeconds();
    const double herdElapsed =
        std::max(
            0.0,
            now - m_fiveHundredShipHerdStartSeconds
        );

    for (auto& [id, shell] : m_shipShells)
    {
        shell.authoritativeMotionAccumulatorSeconds += dt;

        const double interval =
            game::simulation::shellMotionIntervalSeconds(
                shell.lifecycle.motionFidelity,
                m_shipMotionFidelityPolicy
            );

        if (shell.authoritativeMotionAccumulatorSeconds + 1.0e-9 <
            interval)
        {
            continue;
        }

        const double authoritativeDt =
            std::max(
                dt,
                shell.authoritativeMotionAccumulatorSeconds
            );
        shell.authoritativeMotionAccumulatorSeconds = 0.0;

        auto herdIt = m_fiveHundredShipHerd.find(id);
        if (herdIt != m_fiveHundredShipHerd.end() &&
            shell.lifecycle.tier ==
                game::simulation::ShipSimulationTier::TrafficShell)
        {
            // RouteOnly/Kinematic traffic is evaluated analytically only at
            // its authoritative cadence. Clients smooth presentation between
            // these snapshots; the server does not integrate 50 tiny steps.
            const auto sample =
                sampleHerdRoute(
                    m_fiveHundredShipHerdCenterMeters,
                    herdIt->second.flightIndex,
                    herdIt->second.slotInFlight,
                    herdElapsed
                );

            shell.transform.setWorldPositionMeters(sample.positionMeters);
            shell.transform.orientation =
                orientationFromVelocity(sample.velocityMps);
            shell.transform.motion.worldVelocityMps = sample.velocityMps;

            if (shell.transform.motion.travelFrame.valid)
            {
                shell.transform.motion.localPositionMeters =
                    shell.transform.motion.travelFrame.worldToLocalPosition(
                        sample.positionMeters
                    );
                shell.transform.motion.localVelocityMps =
                    shell.transform.motion.travelFrame.worldToLocalVector(
                        sample.velocityMps
                    );
            }
            else
            {
                shell.transform.motion.localVelocityMps = sample.velocityMps;
            }
            continue;
        }

        // Non-route shells/wrecks need only one inertial step per cadence.
        shell.transform.addWorldMeters(
            shell.transform.motion.worldVelocityMps * authoritativeDt
        );
        shell.transform.motion.localPositionMeters +=
            shell.transform.motion.localVelocityMps * authoritativeDt;
    }
}

void GameSimulation::updateShipSimulationDetail(double dt)
{
    if (dt <= 0.0 || m_shipAutomaticDetailDisabled)
        return;

    m_shipMotionFidelityEvaluationAccumulatorSeconds += dt;
    if (m_shipMotionFidelityEvaluationAccumulatorSeconds + 1.0e-9 <
        m_shipMotionFidelityPolicy.evaluationIntervalSeconds)
    {
        return;
    }
    m_shipMotionFidelityEvaluationAccumulatorSeconds = 0.0;

    const double now = m_serverTimelineClock.timeSeconds();

    std::vector<game::simulation::ShipObserverView> observers;
    observers.reserve(
        std::max<std::size_t>(1, m_playerControlledShipIds.size())
    );

    std::unordered_set<EntityId> observedPlayerIds;
    const auto appendObserver =
        [&](EntityId observerId)
        {
            if (observerId.value == 0 ||
                observedPlayerIds.find(observerId) !=
                    observedPlayerIds.end())
            {
                return;
            }

            const Ship* observer = getShip(observerId);
            if (!observer)
                return;

            const auto& tr = observer->core().transform();
            game::simulation::ShipObserverView view;
            view.observerId = observerId;
            view.systemId = tr.motion.systemId;
            view.positionMeters =
                world::coordinates::fullMeters(tr.worldPosition);
            view.forward = glm::dvec3(tr.forward());
            view.valid = true;
            observers.push_back(view);
            observedPlayerIds.insert(observerId);
        };

    for (const EntityId id : m_playerControlledShipIds)
        appendObserver(id);

    // Single-player compatibility: session ownership may be established one
    // frame after the authored primary player already exists.
    if (observers.empty())
        appendObserver(m_playerId);

    // Client camera knowledge is useful for deciding whether a background ship
    // deserves smoother server motion, but it is never combat authority. Each
    // hint is short-lived, same-system/distance validated, and capped at
    // Response. Strongest demand across all connected observers wins.
    std::unordered_map<
        EntityId,
        game::simulation::ShipMotionFidelity
    > clientObservationDemand;

    for (auto it = m_shipClientObservationInterests.begin();
         it != m_shipClientObservationInterests.end();)
    {
        const EntityId observerId = it->first;
        const auto& interest = it->second;

        const Ship* observer = getShip(observerId);
        if (!interest.live(now) ||
            !isPlayerControlled(observerId) ||
            !observer)
        {
            it = m_shipClientObservationInterests.erase(it);
            continue;
        }

        const auto& observerTransform = observer->core().transform();
        const int observerSystemId = observerTransform.motion.systemId;
        const glm::dvec3 observerPosition =
            world::coordinates::fullMeters(
                observerTransform.worldPosition
            );

        for (const auto& entry : interest.entries)
        {
            int targetSystemId = -1;
            glm::dvec3 targetPosition {0.0};
            bool targetFound = false;

            if (const Ship* target = getShip(entry.targetId))
            {
                const auto& tr = target->core().transform();
                targetSystemId = tr.motion.systemId;
                targetPosition =
                    world::coordinates::fullMeters(tr.worldPosition);
                targetFound = true;
            }
            else if (const auto* target = shipShell(entry.targetId))
            {
                targetSystemId = target->transform.motion.systemId;
                targetPosition =
                    world::coordinates::fullMeters(
                        target->transform.worldPosition
                    );
                targetFound = true;
            }

            if (!targetFound ||
                targetSystemId != observerSystemId)
            {
                continue;
            }

            const double distanceMeters =
                glm::length(targetPosition - observerPosition);
            if (!game::simulation::clientObservationDemandAllowed(
                    entry.requested,
                    distanceMeters,
                    m_shipMotionFidelityPolicy))
            {
                continue;
            }

            const auto sanitized =
                game::simulation::sanitizeClientObservationDemand(
                    entry.requested
                );

            auto [demandIt, inserted] =
                clientObservationDemand.emplace(
                    entry.targetId,
                    sanitized
                );
            if (!inserted)
            {
                demandIt->second =
                    game::simulation::strongerMotionFidelity(
                        demandIt->second,
                        sanitized
                    );
            }
        }

        ++it;
    }

    const auto mergeClientObservationDemand =
        [&](EntityId id,
            game::simulation::ShipMotionFidelity serverDemand) noexcept
        {
            const auto it = clientObservationDemand.find(id);
            if (it == clientObservationDemand.end())
                return serverDemand;

            return game::simulation::strongerMotionFidelity(
                serverDemand,
                it->second
            );
        };

    std::vector<EntityId> toMaterialize;
    std::vector<std::pair<EntityId, game::simulation::ShipSimulationTier>>
        toDematerialize;

    const auto forcedFullStress =
        [&](EntityId id) noexcept
        {
            return
                m_fiveHundredFullMaterializedStressRequested &&
                m_fiveHundredShipHerd.find(id) !=
                    m_fiveHundredShipHerd.end();
        };

    for (auto& [id, shell] : m_shipShells)
    {
        if (m_shipTierTransitionTestRequested &&
            id == m_shipTierTransitionProbeId)
        {
            continue;
        }

        auto& runtime = m_shipMotionFidelityStates[id];
        if (runtime.transitionSerial == 0 &&
            runtime.planned != shell.lifecycle.motionFidelity)
        {
            runtime.planned = shell.lifecycle.motionFidelity;
        }

        game::simulation::ShipMotionFidelity requested =
            game::simulation::ShipMotionFidelity::RouteOnly;

        if (forcedFullStress(id))
        {
            requested =
                game::simulation::ShipMotionFidelity::FullControl;
        }
        else
        {
            requested =
                game::simulation::evaluateShipMotionFidelityDemand(
                    id,
                    shell.transform.motion.systemId,
                    world::coordinates::fullMeters(
                        shell.transform.worldPosition
                    ),
                    observers,
                    m_activationClaims,
                    now,
                    m_shipMotionFidelityPolicy
                ).requested;
            requested =
                mergeClientObservationDemand(id, requested);
        }

        const auto update =
            game::simulation::updateShipMotionFidelityPlan(
                runtime,
                requested,
                now,
                m_shipMotionFidelityPolicy
            );

        shell.lifecycle.setMotionFidelity(update.planned);

        // Traffic-origin Response remains a compact shell. At this level the
        // player can observe smooth believable motion, but there is still no
        // reason to allocate ShipCore/module/reactor/AI state. FullControl is
        // the materialization boundary.
        if (update.planned ==
            game::simulation::ShipMotionFidelity::FullControl)
        {
            toMaterialize.push_back(id);
        }
    }

    for (auto& [id, shipPtr] : m_ships)
    {
        if (!shipPtr)
            continue;

        if (m_shipTierTransitionTestRequested &&
            id == m_shipTierTransitionProbeId)
        {
            continue;
        }

        auto lifecycleIt = m_shipSimulationLifecycle.find(id);
        if (lifecycleIt == m_shipSimulationLifecycle.end())
            continue;

        auto& lifecycle = lifecycleIt->second;
        auto& runtime = m_shipMotionFidelityStates[id];

        const bool pinnedFull =
            isPlayerControlled(id) ||
            shipPtr->core().role() == ShipRole::Player ||
            isHubMotionLabShip(id) ||
            isNavigationRuntimeLabShip(id) ||
            isShipStateFlightTestShip(id) ||
            forcedFullStress(id);

        game::simulation::ShipMotionFidelity requested =
            game::simulation::ShipMotionFidelity::FullControl;

        if (!pinnedFull)
        {
            const auto& tr = shipPtr->core().transform();
            requested =
                game::simulation::evaluateShipMotionFidelityDemand(
                    id,
                    tr.motion.systemId,
                    world::coordinates::fullMeters(tr.worldPosition),
                    observers,
                    m_activationClaims,
                    now,
                    m_shipMotionFidelityPolicy
                ).requested;
            requested =
                mergeClientObservationDemand(id, requested);
        }

        const auto update =
            game::simulation::updateShipMotionFidelityPlan(
                runtime,
                requested,
                now,
                m_shipMotionFidelityPolicy
            );

        lifecycle.setMotionFidelity(update.planned);

        if (pinnedFull)
        {
            if (shipPtr->core().role() == ShipRole::Player ||
                isPlayerControlled(id))
            {
                lifecycle.promoteDetailed();
            }
            continue;
        }

        if (game::simulation::shipMotionFidelityPriority(update.planned) >
            game::simulation::shipMotionFidelityPriority(
                game::simulation::ShipMotionFidelity::Response))
        {
            continue;
        }

        // Exact module history is cheap to retain in memory but must not be
        // silently destroyed by a LOD transition. A pristine inactive ship can
        // collapse to a shell; a damaged ship remains materialized at a coarse
        // motion budget until a future compressed damage signature exists.
        bool persistentDetailedHistory =
            shipPtr->core().activeRepairJobCount() > 0 ||
            !shipPtr->core().detachedFragmentRuntime().fragments().empty();

        if (!persistentDetailedHistory)
        {
            for (const auto& module :
                 static_cast<const world::modules::ObjectModuleRuntime&>(
                     shipPtr->core().moduleRuntime()).modules())
            {
                if (module.maxHealth > 0.0f &&
                    module.health + 1.0e-4f < module.maxHealth)
                {
                    persistentDetailedHistory = true;
                    break;
                }
            }
        }

        if (!persistentDetailedHistory)
        {
            for (const auto& link :
                 shipPtr->core().structuralLinkRuntime().links())
            {
                if (link.destroyed ||
                    link.health + 1.0e-4f < link.maxHealth)
                {
                    persistentDetailedHistory = true;
                    break;
                }
            }
        }

        lifecycle.demoteToResponseInternals();

        if (persistentDetailedHistory || !lifecycle.shellEligible)
            continue;

        toDematerialize.push_back({
            id,
            lifecycle.pilotPresent
                ? game::simulation::ShipSimulationTier::TrafficShell
                : game::simulation::ShipSimulationTier::WreckShell
        });
    }

    for (const EntityId id : toMaterialize)
    {
        const auto stateIt = m_shipMotionFidelityStates.find(id);
        const auto planned =
            stateIt != m_shipMotionFidelityStates.end()
                ? stateIt->second.planned
                : game::simulation::ShipMotionFidelity::Response;

        if (materializeShipShell(id))
        {
            auto lifecycleIt = m_shipSimulationLifecycle.find(id);
            if (lifecycleIt != m_shipSimulationLifecycle.end())
                lifecycleIt->second.setMotionFidelity(planned);
        }
    }

    for (const auto& [id, targetTier] : toDematerialize)
    {
        const auto stateIt = m_shipMotionFidelityStates.find(id);
        const auto planned =
            stateIt != m_shipMotionFidelityStates.end()
                ? stateIt->second.planned
                : game::simulation::ShipMotionFidelity::Kinematic;

        if (dematerializeShip(id, targetTier))
        {
            auto shellIt = m_shipShells.find(id);
            if (shellIt != m_shipShells.end())
                shellIt->second.lifecycle.setMotionFidelity(planned);
        }
    }
}


bool GameSimulation::updateNpcNavigationControl(
    EntityId id,
    Ship& ship,
    const NpcNavigationGoal& goal,
    double executionTimeSeconds,
    double executionDeltaSeconds
)
{
    using Bridge = game::navigation::NavigationRuntimeControlBridge;

    if (!std::isfinite(executionTimeSeconds) ||
        !std::isfinite(executionDeltaSeconds) ||
        executionDeltaSeconds <= 0.0)
    {
        return false;
    }

    auto bridgeIt = m_npcNavigationControlBridges.find(id);
    if (bridgeIt == m_npcNavigationControlBridges.end())
    {
        auto bridge = std::make_unique<Bridge>(
            m_npcAiSystem.pilotSkillProfile(ship)
        );

        Bridge::Intent neutral;
        neutral.revision = 0;

        const double initialTime =
            executionTimeSeconds - executionDeltaSeconds;

        if (!bridge->reset(initialTime, neutral))
            return false;

        bridgeIt = m_npcNavigationControlBridges
            .emplace(id, std::move(bridge))
            .first;

        m_npcNavigationLastExecutionTimeSeconds[id] = initialTime;
    }

    const auto& tr = ship.core().transform();

    game::navigation::NpcNavigationKinematicState navigationState;
    navigationState.positionSystemMeters =
        world::coordinates::fullMeters(tr.worldPosition);
    navigationState.relativeSystemVelocityMps =
        tr.motion.travelFrame.valid
            ? tr.motion.travelFrame.localToWorldVector(
                  tr.motion.localVelocityMps
              )
            : tr.motion.worldVelocityMps;
    navigationState.forwardSystem = glm::dvec3(tr.forward());
    navigationState.rightSystem = glm::dvec3(tr.right());
    navigationState.upSystem = glm::dvec3(tr.up());
    navigationState.pitchRateRadPerSec =
        static_cast<double>(tr.pitchRate);
    navigationState.yawRateRadPerSec =
        static_cast<double>(tr.yawRate);
    navigationState.rollRateRadPerSec =
        static_cast<double>(tr.rollRate);

    Bridge::Intent intent;
    if (isNavigationRuntimeLabShip(id))
    {
        if (!buildNavigationRuntimeLabIntent(id, ship, intent))
            return false;
    }
    else
    {
        intent =
            game::navigation::NpcNavigationIntentController::buildIntent(
                navigationState,
                goal
            );
    }

    double& lastTime =
        m_npcNavigationLastExecutionTimeSeconds[id];

    if (executionTimeSeconds + 1.0e-9 < lastTime)
        return false;

    Bridge::StepResult latest;
    bool stepped = false;

    // Activation may sleep a distant NPC longer than PilotSkillExecutor's
    // maximum single step. Advance the exact elapsed interval in bounded
    // deterministic pieces instead of clamping or lying about dt.
    while (lastTime < executionTimeSeconds - 1.0e-9)
    {
        const double dt = std::min(
            bridgeIt->second->maximumStepSeconds(),
            executionTimeSeconds - lastTime
        );
        const double nextTime = lastTime + dt;
        latest = bridgeIt->second->step(nextTime, dt, intent);
        stepped = true;

        if (latest.status != Bridge::PilotExecutor::Status::Ok)
            return false;

        lastTime = nextTime;
    }

    if (!stepped)
        return false;

    if (isNavigationRuntimeLabShip(id))
    {
        // The runtime lab intentionally preserves the legacy direct-demand
        // seam as a diagnostic specimen until its dedicated migration. It is
        // not production NPC/autopilot behavior.
        ship.setControlState(latest.control);
    }
    else
    {
        game::navigation::autopilot::AutopilotControlRequest
            pilotRequest;
        pilotRequest.desiredVelocityMapMps =
            game::navigation::NpcNavigationIntentController::
                desiredVelocityForGoal(
                    navigationState,
                    goal
                );
        pilotRequest.desiredLinearAccelerationMapMps2 =
            latest.snapshot.executedLinearAccelerationDemandSystemMps2;

        const double desiredSpeed =
            glm::length(pilotRequest.desiredVelocityMapMps);
        pilotRequest.desiredForwardMap =
            desiredSpeed > 1.0e-9
                ? pilotRequest.desiredVelocityMapMps / desiredSpeed
                : navigationState.forwardSystem;
        pilotRequest.desiredUpMap = navigationState.upSystem;

        pilotRequest.actualVelocityMapMps =
            navigationState.relativeSystemVelocityMps;
        pilotRequest.forwardMap = navigationState.forwardSystem;
        pilotRequest.rightMap = navigationState.rightSystem;
        pilotRequest.upMap = navigationState.upSystem;
        pilotRequest.pitchRateRadPerSec =
            navigationState.pitchRateRadPerSec;
        pilotRequest.yawRateRadPerSec =
            navigationState.yawRateRadPerSec;
        pilotRequest.rollRateRadPerSec =
            navigationState.rollRateRadPerSec;
        pilotRequest.stopRequested =
            goal.mode == NpcNavigationGoalMode::Hold;
        pilotRequest.deltaSeconds = executionDeltaSeconds;

        auto& pilotController =
            m_npcAutopilotLawControllers[id];
        ship.setControlState(
            pilotController.update(
                tr.motion.localControlLaw,
                pilotRequest,
                game::ship::telemetry::applyAuthorityToControlParams(
                    ship.core().effectivePhysics(),
                    ship.core().controlAuthorityTelemetry()
                )
            )
        );
    }

    m_npcNavigationExecutionSnapshots[id] = latest.snapshot;

    if (isNavigationRuntimeLabShip(id) && latest.snapshot.valid)
    {
        auto& observation = m_navigationRuntimeLabObservation;
        ++observation.executionCount;
        observation.executionSeen = true;

        // The control bridge is explicitly System-space. Navigation-v2 safety
        // prediction is NavLocal-only, so executed system acceleration may
        // re-enter Navigation v2 only through NavigationFrameBoundary.
        const auto& executed =
            latest.snapshot.executedLinearAccelerationDemandSystemMps2;
        const glm::dvec3 executedWorldVector(
            executed.x,
            executed.y,
            executed.z
        );
        const double executedMagnitude =
            glm::length(executedWorldVector);

        glm::dvec3 executedMapVector(0.0);
        bool executedMapVectorValid = false;
        double lateralMagnitude = 0.0;
        if (const auto* hubFrame =
                hubNavigationFrame(m_navigationRuntimeLabHubId);
            hubFrame && hubFrame->valid)
        {
            const auto executionBoundary =
                makeNavigationRuntimeLabBoundary(*hubFrame);
            if (executionBoundary.valid())
            {
                executedMapVector =
                    executionBoundary.toNavigationVector(
                        game::navigation::NavigationFrameBoundary::SystemVector {
                            executedWorldVector
                        }
                    ).value;
                executedMapVectorValid = true;
            }

            const glm::dvec3 routeVectorMap =
                game::diagnostics::NavigationRuntimeLabGoalVisualLocalMeters -
                game::diagnostics::NavigationRuntimeLabStartVisualLocalMeters;

            const glm::dvec3 routeVectorWorld =
                hubFrame->normalAxis * routeVectorMap.x +
                hubFrame->radialAxis * routeVectorMap.y -
                hubFrame->progradeAxis * routeVectorMap.z;

            const double routeLength =
                glm::length(routeVectorWorld);
            if (routeLength > 1.0e-12)
            {
                const glm::dvec3 routeDirectionWorld =
                    routeVectorWorld / routeLength;
                const glm::dvec3 lateral =
                    executedWorldVector -
                    routeDirectionWorld *
                        glm::dot(
                            executedWorldVector,
                            routeDirectionWorld
                        );
                lateralMagnitude = glm::length(lateral);
            }
        }

        if (executedMapVectorValid)
        {
            observation.lastExecutedLinearDemandMapMps2 =
                executedMapVector;

            if (observation.firstVisibleHorizonBypassCaptured &&
                !observation.firstVisibleHorizonBypassExecutedCaptured &&
                observation.firstVisibleHorizonBypassSegmentRevision ==
                    m_navigationRuntimeLabAcceptedSegment.revision)
            {
                observation.firstVisibleHorizonBypassExecutedCaptured = true;
                observation.firstVisibleHorizonBypassExecutedAccelerationMapMps2 =
                    executedMapVector;
            }
        }
        else
        {
            // Do not let a stale map-space command survive a broken reference
            // frame and contaminate the next exact-static prediction.
            observation.lastExecutedLinearDemandMapMps2 =
                glm::dvec3(0.0);
        }
        observation.maximumExecutedLinearDemandMps2 =
            std::max(
                observation.maximumExecutedLinearDemandMps2,
                executedMagnitude
            );
        observation.maximumExecutedLateralDemandMps2 =
            std::max(
                observation.maximumExecutedLateralDemandMps2,
                lateralMagnitude
            );
        observation.nonZeroExecutedDemandSeen =
            observation.nonZeroExecutedDemandSeen ||
            executedMagnitude > 1.0e-6;
        observation.lateralExecutedDemandSeen =
            observation.lateralExecutedDemandSeen ||
            lateralMagnitude > 1.0e-4;

        observation.movingPassageExecutionActive =
            observation.movingPassageAuthorityActive;

        if (observation.movingPassageExecutionActive &&
            executedMagnitude > 1.0e-6)
        {
            observation.movingPassageExecutedSeen = true;
            observation.maximumMovingPassageExecutedDemandMps2 =
                std::max(
                    observation.maximumMovingPassageExecutedDemandMps2,
                    executedMagnitude
                );
            observation.lastMovingPassageExecutedWorldMps2 =
                executedWorldVector;
        }
    }

    return latest.snapshot.valid;
}


game::simulation::SimulationMode
GameSimulation::activationExecutionMode(EntityId shipId) const noexcept
{
    using game::simulation::SimulationMode;

    // The locally controlled player and diagnostic reference/proving actors
    // remain fully materialized. Their purpose is to measure one subsystem
    // deterministically; activation decimation would mix a second variable
    // into the result.
    if (isPlayerControlled(shipId) ||
        isHubMotionLabShip(shipId) ||
        isNavigationRuntimeLabShip(shipId))
    {
        return SimulationMode::Active;
    }

    const auto stateIt = m_activationPlanStates.find(shipId);
    if (stateIt != m_activationPlanStates.end())
        return stateIt->second.plannedMode;

    // Before the first 5 Hz planner evaluation, preserve legacy full-rate
    // behavior rather than demoting an entity speculatively.
    return SimulationMode::Active;
}


game::simulation::SimulationMode
GameSimulation::motionExecutionMode(EntityId shipId) const noexcept
{
    using game::simulation::ShipMotionFidelity;
    using game::simulation::SimulationMode;

    if (isPlayerControlled(shipId) ||
        isHubMotionLabShip(shipId) ||
        isNavigationRuntimeLabShip(shipId) ||
        isShipStateFlightTestShip(shipId))
    {
        return SimulationMode::Active;
    }

    switch (shipMotionFidelity(shipId))
    {
        case ShipMotionFidelity::FullControl:
            return SimulationMode::Active;

        case ShipMotionFidelity::Response:
            // 10 Hz AI and 25 Hz control using the existing Prewarm cadences.
            return SimulationMode::Prewarm;

        case ShipMotionFidelity::Kinematic:
        case ShipMotionFidelity::RouteOnly:
            return SimulationMode::Coarse;
    }

    return SimulationMode::Coarse;
}


//                       ###              ##
//                        ##              ##
//  ##  ##   ######       ##    ####     #####    ####
//  ##  ##    ##  ##   #####       ##     ##     ##  ##
//  ##  ##    ##  ##  ##  ##    #####     ##     ######
//  ##  ##    #####   ##  ##   ##  ##     ## ##  ##
//   ######   ##       ######   #####      ###    #####
//           ####

void GameSimulation::update(
    const game::server::ServerTimeContext& time
)
{
    const bool trajectoryDebugMode =
        time.universeTimeSimulation;

    // Normal gameplay systems remain frozen while universe time is being
    // accelerated for trajectory diagnostics. Only passive orbital/ballistic
    // motion advances by universeDeltaSeconds.
    const double dt =
        std::max(0.0, time.gameplayDeltaSeconds);

    const double trajectoryDeltaSeconds = trajectoryDebugMode
        ? time.universeDeltaSeconds
        : time.gameplayDeltaSeconds;

    m_orbitalUniverseTimeSeconds = time.universeTimeSeconds;
    m_npcRepairThinkTimerSeconds += dt;

    const bool npcRepairThinkTick =
        m_npcRepairThinkTimerSeconds >= 15.0;

    if (npcRepairThinkTick)
        m_npcRepairThinkTimerSeconds = 0.0;

    // Режим 1: нормальная мощность

    float fdt = static_cast<float>(dt);
    m_serverTimelineClock.advance(time.serverDeltaSeconds);


// Celestial positions and velocities are injected atomically by GameServer
// from the server-owned CelestialRuntimeRegistry before this update.














    for (auto& [id, ship] : m_ships)
    {
        if (!ship ||
            !game::simulation::sameRuntimeSystem(
                ship->core().transform().motion.systemId,
                m_activeCelestialSystemId))
        {
            continue;
        }

        const auto maintenanceMode = motionExecutionMode(id);

        if constexpr (game::runtime::ActivationShipMaintenanceCadenceEnabled)
        {
            auto& maintenanceState = m_shipMaintenanceCadenceStates[id];
            const auto maintenanceCadence =
                game::simulation::activation::advanceShipMaintenanceCadence(
                    maintenanceState,
                    maintenanceMode,
                    dt,
                    m_serverTimelineClock.timeSeconds(),
                    m_activationExecutionPolicy
                );

            if (maintenanceCadence.execute)
            {
                const double maintenanceDt =
                    maintenanceCadence.executionDeltaSeconds;
                ship->core().updateAssemblyRuntime(maintenanceDt);
                ship->core().updateDetachedFragments(
                    static_cast<float>(maintenanceDt)
                );
                ship->core().updateRepairJobs(
                    static_cast<float>(maintenanceDt)
                );
            }
        }
        else
        {
            ship->core().updateAssemblyRuntime(dt);
            ship->core().updateDetachedFragments(fdt);
            ship->core().updateRepairJobs(fdt);
        }


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
        if (hub.systemId != m_activeCelestialSystemId ||
            !hub.motion.enabled)
        {
            continue;
        }

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









        hub.worldPosition =
            world::coordinates::makeWorldPositionFromMeters(
                hubPosMeters
            );

        // Хаб НЕ вращается вокруг собственной оси.
        // Его orientation задаётся позже из HubNavigationFrame:
        // X = normal, Y = radial, Z = -prograde.
        hub.orientation = glm::mat4(1.0f);

    }




    rebuildHubNavigationFrames(trajectoryDeltaSeconds);
    initializeNavigationRuntimeLab();

    // HubNavigationFrame above now belongs to the current fixed-step epoch.
    // Refresh every matched ship before AI/navigation consumes worldPosition.
    // Otherwise a ship keeps the previous epoch's world pose while the planner
    // measures it against the current hub origin/basis, producing an apparent
    // offset of roughly frameVelocity * dt (hundreds of metres at orbital
    // world speeds).
    if (!trajectoryDebugMode)
        updateShipReferenceFrames(dt);

    if (!trajectoryDebugMode)
        endUniverseTrajectoryDiagnostic();



    for (auto& [id, obj] : m_staticObjects)
    {
        if (obj.systemId != m_activeCelestialSystemId)
            continue;

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

                const auto* hubFrame =
                    hubNavigationFrame(obj.hubId);

                const bool movingGapBoundary =
                    game::diagnostics::
                        isNavigationRuntimeLabMovingGapBoundary(
                            obj.displayName
                        );
                const glm::dvec3 localLinearVelocityVisualMps =
                    movingGapBoundary
                        ? game::diagnostics::
                            NavigationRuntimeLabMovingGapVelocityVisualMps
                        : glm::dvec3(0.0);
                const glm::dvec3 effectiveLocalOffsetMeters =
                    obj.hubLocalOffsetMeters +
                    localLinearVelocityVisualMps *
                        m_serverTimelineClock.timeSeconds();

                const glm::dvec3 rotatedOffset =
                    obj.inheritHubOrientation && hubFrame && hubFrame->valid
                        ? game::navigation::hubVisualLocalToWorldVector(
                            hubFrame->progradeAxis,
                            hubFrame->radialAxis,
                            hubFrame->normalAxis,
                            effectiveLocalOffsetMeters
                        )
                        : effectiveLocalOffsetMeters;

                obj.setWorldPositionMeters(
                    hubMeters + rotatedOffset
                );

                /*
                    Static infrastructure can still be kinematically moving.
                    Keep its derived world velocity coherent with the same hub
                    frame used for position so map interpolation/extrapolation
                    never sees a moving object with a zero velocity.
                */
                glm::dvec3 objectVelocityMetersPerSecond {0.0};

                const auto hubVelocityIt =
                    m_hubVelocityMetersPerSecond.find(obj.hubId);

                if (hubVelocityIt !=
                    m_hubVelocityMetersPerSecond.end())
                {
                    objectVelocityMetersPerSecond =
                        hubVelocityIt->second;
                }

                if (hubFrame && hubFrame->valid)
                {
                    objectVelocityMetersPerSecond +=
                        glm::cross(
                            hubFrame->angularVelocityWorldRadPerSecond,
                            rotatedOffset
                        );

                    if (movingGapBoundary)
                    {
                        objectVelocityMetersPerSecond +=
                            game::navigation::hubVisualLocalToWorldVector(
                                hubFrame->progradeAxis,
                                hubFrame->radialAxis,
                                hubFrame->normalAxis,
                                localLinearVelocityVisualMps
                            );
                    }
                }
                else if (movingGapBoundary)
                {
                    objectVelocityMetersPerSecond +=
                        localLinearVelocityVisualMps;
                }

                obj.linearVelocity =
                    glm::vec3(objectVelocityMetersPerSecond);

                glm::dvec3 objectAngularVelocityWorldRadPerSecond {0.0};
                if (hubFrame && hubFrame->valid)
                {
                    const glm::dvec3 currentLocalRotationDeg =
                        obj.hubLocalRotationDeg +
                        obj.hubLocalAngularVelocityDegPerSecond *
                            m_orbitalUniverseTimeSeconds;
                    objectAngularVelocityWorldRadPerSecond =
                        hubFrame->angularVelocityWorldRadPerSecond;
                    objectAngularVelocityWorldRadPerSecond +=
                        game::navigation::hubAttachedAngularVelocityWorld(
                            hubFrame->progradeAxis,
                            hubFrame->radialAxis,
                            hubFrame->normalAxis,
                            currentLocalRotationDeg,
                            obj.hubLocalAngularVelocityDegPerSecond
                        );
                }
                obj.angularVelocity =
                    glm::vec3(objectAngularVelocityWorldRadPerSecond);

                if (obj.inheritHubOrientation)
                {
                    if (hubFrame && hubFrame->valid)
                    {
                        const glm::dvec3 currentLocalRotationDeg =
                            obj.hubLocalRotationDeg +
                            obj.hubLocalAngularVelocityDegPerSecond *
                                m_orbitalUniverseTimeSeconds;
                        obj.orientation =
                            game::navigation::hubAttachedVisualOrientation(
                                hubFrame->progradeAxis,
                                hubFrame->radialAxis,
                                hubFrame->normalAxis,
                                currentLocalRotationDeg
                            );
                    }
                }







            }
        }
        else if (obj.orbitalMotion.enabled)
        {
                if (!obj.orbitalParentBodyId.empty())
                {
                    auto parentIt =
                        m_celestialBodyPositionsAu.find(obj.orbitalParentBodyId);

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

                glm::dvec3 objectVelocityMetersPerSecond =
                    world::orbits::computeOrbitVelocityMetersPerSecond(
                        obj.orbitalMotion,
                        m_orbitalUniverseTimeSeconds
                    );

                if (!obj.orbitalParentBodyId.empty())
                {
                    const auto parentVelocityIt =
                        m_celestialBodyVelocitiesMetersPerSecond.find(
                            obj.orbitalParentBodyId
                        );

                    if (parentVelocityIt !=
                        m_celestialBodyVelocitiesMetersPerSecond.end())
                    {
                        objectVelocityMetersPerSecond +=
                            parentVelocityIt->second;
                    }
                }

                obj.linearVelocity =
                    glm::vec3(objectVelocityMetersPerSecond);

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



    // HitVolumes and hub-attached transforms above are now authoritative for
    // this fixed step. Publish the exact static NavigationSpace geometry only
    // after both have been updated, never from stale first-frame poses.
    publishNavigationRuntimeLabStaticGeometry();

    // === 1. AI / controls / attitude ===
    if (!trajectoryDebugMode)
    {
        applyPhysicalCorridorHotStart();
        updateShipStateFlightTest();
        updateShipShells(dt);
        updateShipSimulationDetail(dt);
        updateShipTierTransitionTest();
        updateFiveHundredShipHerdDiagnostics();
        // Stage 4B uses one cadence decision for both attitude/control-force
        // evaluation and HubTactical engine-command refresh. Reusing the same
        // accumulated dt prevents the two halves of ship motion from drifting
        // onto different coarse epochs.
        m_shipMotionControlStepDecisions.clear();

        for (auto& [id, shipPtr] : m_ships)
        {
            if (!shipPtr ||
                !game::simulation::sameRuntimeSystem(
                    shipPtr->core().transform().motion.systemId,
                    m_activeCelestialSystemId))
            {
                continue;
            }

            if (isHubMotionLabShip(id))
                continue;

            if (isInterplanetaryTransferLabShip(id))
                continue;

            Ship& ship = *shipPtr;
            if (isShipStateFlightTestShip(id))
                continue;

            if (shipSimulationTier(id) ==
                game::simulation::ShipSimulationTier::Derelict)
            {
                ship.setControlState(ShipControlState {});
                m_npcNavigationControlBridges.erase(id);
                m_npcNavigationExecutionSnapshots.erase(id);
                m_npcNavigationLastExecutionTimeSeconds.erase(id);
                m_npcAutopilotLawControllers.erase(id);
                continue;
            }

            if (isPlayerControlled(id) ||
                ship.core().role() == ShipRole::Player)
            {
                m_npcNavigationControlBridges.erase(id);
                m_npcNavigationExecutionSnapshots.erase(id);
                m_npcNavigationLastExecutionTimeSeconds.erase(id);
                m_npcAutopilotLawControllers.erase(id);
                continue;
            }

            // Tactical AI is one of the activation-controlled materialized
            // execution lanes. Stage 4B also decimates motion-control
            // evaluation, while translation propagation remains fixed-step.
            game::simulation::SimulationMode executionMode =
                motionExecutionMode(id);

            if constexpr (!game::runtime::ActivationNpcAiCadenceEnabled)
                executionMode = game::simulation::SimulationMode::Active;

            auto& cadenceState = m_npcAiCadenceStates[id];
            const auto cadence =
                game::simulation::activation::advanceNpcAiCadence(
                    cadenceState,
                    executionMode,
                    dt,
                    m_serverTimelineClock.timeSeconds(),
                    m_activationExecutionPolicy
                );

            if (!cadence.execute)
                continue;

            NpcNavigationGoal goal;
            if (!buildFiveHundredShipHerdGoal(id, goal))
            {
                goal =
                    m_npcAiSystem.computeGoal(
                        ship,
                        static_cast<float>(cadence.thinkDeltaSeconds)
                    );
            }

            if (!updateNpcNavigationControl(
                    id,
                    ship,
                    goal,
                    m_serverTimelineClock.timeSeconds(),
                    cadence.thinkDeltaSeconds))
            {
                // Fail closed at the ownership seam. Never fall back to the
                // retired direct steering path, or a runtime error would
                // silently reintroduce two motion authorities.
                ship.setControlState(ShipControlState {});
                m_npcNavigationControlBridges.erase(id);
                m_npcNavigationExecutionSnapshots.erase(id);
                m_npcNavigationLastExecutionTimeSeconds.erase(id);
                m_npcAutopilotLawControllers.erase(id);
            }
        }

        for (auto& [id, shipPtr] : m_ships)
        {
            if (!shipPtr ||
                !game::simulation::sameRuntimeSystem(
                    shipPtr->core().transform().motion.systemId,
                    m_activeCelestialSystemId))
            {
                continue;
            }

            if (isHubMotionLabShip(id))
                continue;

            if (isInterplanetaryTransferLabShip(id))
                continue;

            Ship& ship = *shipPtr;
            ship.applyControl();
        }

        for (auto& [id, shipPtr] : m_ships)
        {
            if (!shipPtr ||
                !game::simulation::sameRuntimeSystem(
                    shipPtr->core().transform().motion.systemId,
                    m_activeCelestialSystemId))
            {
                continue;
            }

            if (isHubMotionLabShip(id))
                continue;

            if (isInterplanetaryTransferLabShip(id))
                continue;

            Ship& ship = *shipPtr;

            game::simulation::activation::ActivationCadenceDecision
                motionControlCadence;

            if constexpr (
                game::runtime::ActivationShipMotionControlCadenceEnabled)
            {
                const auto motionMode = motionExecutionMode(id);
                auto& motionState = m_shipMotionControlCadenceStates[id];
                motionControlCadence =
                    game::simulation::activation::advanceShipMotionControlCadence(
                        motionState,
                        motionMode,
                        dt,
                        m_serverTimelineClock.timeSeconds(),
                        m_activationExecutionPolicy
                    );
            }
            else
            {
                motionControlCadence.execute = true;
                motionControlCadence.executionDeltaSeconds = dt;
                motionControlCadence.thinkDeltaSeconds = dt;
                motionControlCadence.intervalSeconds = 0.0;
            }

            m_shipMotionControlStepDecisions[id] = motionControlCadence;

            if (motionControlCadence.execute)
            {
                ship.updateMotionControl(
                    static_cast<float>(
                        motionControlCadence.executionDeltaSeconds
                    ),
                    m_world
                );
            }

            // Non-tactical or frame-unbound ships keep ordinary attitude
            // propagation here. Tactical ships must apply ENGINE-produced
            // torque before this step's orientation integration; moving this
            // motion sample after translation control avoids a whole fixed
            // tick of extra feedback delay for pitch/yaw/roll.
            const auto& angularMotion = ship.core().transform().motion;
            if (angularMotion.mode !=
                    game::navigation::MotionMode::HubTactical ||
                !angularMotion.travelFrame.valid)
            {
                ship.propagateMotionOrientation(fdt);
            }

            if (shipUsesDetailedSimulation(id))
            {
                if constexpr (
                    game::runtime::ActivationShipSystemsCadenceEnabled)
                {
                    const auto systemsMode = motionExecutionMode(id);
                    auto& systemsState = m_shipSystemsCadenceStates[id];
                    const auto systemsCadence =
                        game::simulation::activation::
                            advanceShipSystemsCadence(
                                systemsState,
                                systemsMode,
                                dt,
                                m_serverTimelineClock.timeSeconds(),
                                m_activationExecutionPolicy
                            );

                    if (systemsCadence.execute)
                    {
                        ship.updateSystems(
                            static_cast<float>(
                                systemsCadence.executionDeltaSeconds
                            )
                        );
                    }
                }
                else
                {
                    ship.updateSystems(fdt);
                }
            }
        }

        for (auto& [id, shipPtr] : m_ships)
        {
            if (!shipPtr ||
                !game::simulation::sameRuntimeSystem(
                    shipPtr->core().transform().motion.systemId,
                    m_activeCelestialSystemId))
            {
                continue;
            }

            if (isHubMotionLabShip(id))
                continue;

            if (isInterplanetaryTransferLabShip(id))
                continue;

            auto& tr = shipPtr->core().transform();

            if (tr.motion.mode !=
                game::navigation::MotionMode::HubTactical)
            {
                continue;
            }

            if (tr.motion.matchedToReferenceFrame)
            {
                const auto* matchedHubFrame =
                    hubNavigationFrame(tr.motion.matchedReferenceFrameId);

                if (matchedHubFrame && matchedHubFrame->valid)
                {
                    game::navigation::TravelFrameSystem::refreshMatchedReference(
                        tr.motion,
                        matchedHubFrame->kinematicFrame(),
                        matchedHubFrame->hubId
                    );
                }
            }

            if (!tr.motion.travelFrame.valid)
                continue;

            const auto decisionIt =
                m_shipMotionControlStepDecisions.find(id);

            if (decisionIt == m_shipMotionControlStepDecisions.end() ||
                !decisionIt->second.execute)
            {
                // A coarse control cadence holds the last rates, but the
                // physics still integrates attitude every fixed tick.
                shipPtr->propagateMotionOrientation(fdt);
                continue;
            }

            const float motionControlDt = static_cast<float>(
                decisionIt->second.executionDeltaSeconds
            );

            // GameSimulation owns cadence/reference-frame orchestration only.
            // ShipControlState semantics and actuator/backend selection are
            // isolated behind the ship control execution gateway.
            shipPtr->updateTranslationControl(motionControlDt);
            // Rotation now uses the ACTUAL physical engine r x F of the
            // current tick, not the previous tick's solver output.
            shipPtr->propagateMotionOrientation(fdt);
        }
    }

    /*
        Accelerated trajectory diagnostics are a read-only branch over frozen
        production ships. Do not update production reference-frame or gravity
        fields from the accelerated celestial epoch.
    */
    if (trajectoryDebugMode)
    {
        advanceUniverseTrajectoryDiagnostic(
            trajectoryDeltaSeconds
        );
    }
    else
    {
        rebuildNavigationGravityContext();
        updateDynamicNavigationContext(dt);

        for (auto& [id, shipPtr] : m_ships)
        {
            if (!shipPtr)
                continue;

            if (isHubMotionLabShip(id))
                continue;

            if (isInterplanetaryTransferLabShip(id))
                continue;

            auto& tr = shipPtr->core().transform();

            if (tr.motion.mode !=
                game::navigation::MotionMode::HubTactical)
            {
                continue;
            }

            if (!tr.motion.travelFrame.valid)
                continue;

            (void)shipPtr->propagateHubTacticalTranslation(dt);

            if (isNavigationRuntimeLabShip(id))
            {
                auto& observation =
                    m_navigationRuntimeLabObservation;

                const glm::dvec3 appliedAcceleration =
                    tr.motion.engineAccelerationMps2;
                const double appliedAccelerationMagnitude =
                    glm::length(appliedAcceleration);

                observation.maximumAppliedEngineAccelerationMps2 =
                    std::max(
                        observation.maximumAppliedEngineAccelerationMps2,
                        appliedAccelerationMagnitude
                    );

                if (observation.movingPassageExecutionActive &&
                    appliedAccelerationMagnitude > 1.0e-6)
                {
                    observation.movingPassageAppliedAccelerationSeen = true;
                    observation.maximumMovingPassageAppliedAccelerationMps2 =
                        std::max(
                            observation.maximumMovingPassageAppliedAccelerationMps2,
                            appliedAccelerationMagnitude
                        );
                }

                observation.maximumRelativeSpeedMps =
                    std::max(
                        observation.maximumRelativeSpeedMps,
                        glm::length(tr.motion.localVelocityMps)
                    );

                if (const auto* hubFrame =
                        hubNavigationFrame(
                            m_navigationRuntimeLabHubId
                        );
                    hubFrame && hubFrame->valid)
                {
                    if (observation.firstVisibleHorizonBypassCaptured &&
                        !observation.firstVisibleHorizonBypassAppliedCaptured &&
                        observation.firstVisibleHorizonBypassSegmentRevision ==
                            m_navigationRuntimeLabAcceptedSegment.revision)
                    {
                        const auto boundary =
                            makeNavigationRuntimeLabBoundary(*hubFrame);
                        if (boundary.valid())
                        {
                            observation.firstVisibleHorizonBypassAppliedCaptured =
                                true;
                            observation.
                                firstVisibleHorizonBypassAppliedMainAccelerationMapMps2 =
                                boundary.toNavigationVector(
                                    game::navigation::NavigationFrameBoundary::
                                        SystemVector {
                                            tr.motion.mainEngineAccelerationMps2
                                        }
                                ).value;
                            observation.
                                firstVisibleHorizonBypassAppliedRcsAccelerationMapMps2 =
                                boundary.toNavigationVector(
                                    game::navigation::NavigationFrameBoundary::
                                        SystemVector {
                                            tr.motion.manoeuvreAccelerationMps2
                                        }
                                ).value;
                            observation.
                                firstVisibleHorizonBypassAppliedTotalAccelerationMapMps2 =
                                boundary.toNavigationVector(
                                    game::navigation::NavigationFrameBoundary::
                                        SystemVector {
                                            tr.motion.engineAccelerationMps2
                                        }
                                ).value;
                        }
                    }

                    const glm::dvec3 routeVectorMap =
                        game::diagnostics::
                            NavigationRuntimeLabGoalVisualLocalMeters -
                        game::diagnostics::
                            NavigationRuntimeLabStartVisualLocalMeters;

                    const glm::dvec3 routeVectorWorld =
                        hubFrame->normalAxis * routeVectorMap.x +
                        hubFrame->radialAxis * routeVectorMap.y -
                        hubFrame->progradeAxis * routeVectorMap.z;

                    const double routeLength =
                        glm::length(routeVectorWorld);
                    if (routeLength > 1.0e-12)
                    {
                        const glm::dvec3 routeDirectionWorld =
                            routeVectorWorld / routeLength;
                        const glm::dvec3 lateralApplied =
                            appliedAcceleration -
                            routeDirectionWorld *
                                glm::dot(
                                    appliedAcceleration,
                                    routeDirectionWorld
                                );

                        observation.
                            maximumAppliedLateralAccelerationMps2 =
                            std::max(
                                observation.
                                    maximumAppliedLateralAccelerationMps2,
                                glm::length(lateralApplied)
                            );
                    }
                }
            }

            tr.referenceVelocityMetersPerSecond =
                tr.motion.referenceVelocityMps;
        }

        // The interplanetary diagnostic actor owns an analytic Sun-centered
        // transfer trajectory. Place it before the final navigation-context
        // sample so gravity/primary-body metadata belongs to this exact pose.
        updateInterplanetaryTransferLabActor();
        updateDynamicNavigationContext(dt);

        // Controlled server-time samples for the Hub Motion Lab. They are
        // excluded from production AI/physics above, then published through
        // the ordinary authoritative ShipSnapshot path below.
        updateHubMotionLabActors();

        // Stage 4B keeps the 5 Hz physical planner. NPC tactical AI, motion
        // control, internal systems and structural/repair maintenance consume
        // the stabilized mode as work cadences. Cheap kinematic propagation,
        // signals and entity presence in snapshots remain full-rate for now.
        m_activationShadowEvaluationAccumulatorSeconds += dt;
        if (!m_activationShadowEvaluated ||
            m_activationShadowEvaluationAccumulatorSeconds >= 0.20)
        {
            m_activationShadowEvaluationAccumulatorSeconds = 0.0;
            updateActivationShadow();
            m_activationShadowEvaluated = true;
        }

        if constexpr (game::runtime::ActivationShadowDiagnosticsEnabled)
            debugLogActivationShadow(dt);
    }










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








    if (m_diagnostics.settings.hubPlayerChainCsv)
        debugLogHubPlayerChain(dt);

    if (m_diagnostics.settings.serverNavigationCsv)
        debugLogServerNavState(dt);

    if (m_diagnostics.settings.playerMotionCsv)
        debugLogPlayerMotion(dt);





    if constexpr (game::promo::PromoFlybyScenario::Enabled)
    {
        if (!trajectoryDebugMode)
            m_promoFlybyScenario.update(*this, fdt);
    }





    /*
        Signals and radar are gameplay state derived from production ship
        transforms. During accelerated trajectory diagnostics the production
        world is frozen, while presentation transforms live on an alternate
        branch. Recomputing sensors against one branch and publishing ships
        from the other would create a mixed-epoch snapshot, so keep the last
        normal sensor state unchanged until normal gameplay resumes.
    */
    if (!trajectoryDebugMode)
    {
        std::vector<ITransmitterSource*> signalSources;
        for (auto& [id, shipPtr] : m_ships)
            signalSources.push_back(shipPtr.get());

        WorldSignalTxSystem::collectFromSources(
            signalSources,
            m_worldSignals
        );

        std::unordered_map<int, std::vector<WorldSignal>>
            signalsBySystem;

        for (const auto& signal : m_worldSignals)
        {
            if (signal.systemId >= 0)
                signalsBySystem[signal.systemId].push_back(signal);
        }

        std::unordered_map<int, std::vector<Planet>>
            planetsBySystem;
        std::unordered_map<int, std::vector<InterferenceSource>>
            interferenceBySystem;

        for (const auto& planet : m_planets)
        {
            if (planet.systemId >= 0)
                planetsBySystem[planet.systemId].push_back(planet);
        }

        for (const auto& source : m_interferenceSources)
        {
            if (source.systemId >= 0)
                interferenceBySystem[source.systemId].push_back(source);
        }

        static const std::vector<WorldSignal> EmptySignals;
        static const std::vector<Planet> EmptyPlanets;
        static const std::vector<InterferenceSource> EmptyInterference;

        for (auto& [id, shipPtr] : m_ships)
        {
            if (!shipPtr)
                continue;

            // Response NPCs do not run an individual receiver over the entire
            // system signal list. They use navigation/combat broad-phase data;
            // player/explicit detailed actors keep the rich receiver model.
            if (!shipUsesDetailedSimulation(id))
                continue;

            Ship& ship = *shipPtr;
            const int shipSystemId =
                ship.core().transform().motion.systemId;

            if (!game::simulation::sameRuntimeSystem(
                    shipSystemId,
                    m_activeCelestialSystemId))
            {
                continue;
            }

            const auto signalIt =
                signalsBySystem.find(shipSystemId);
            const auto planetIt =
                planetsBySystem.find(shipSystemId);
            const auto interferenceIt =
                interferenceBySystem.find(shipSystemId);

            ship.updateSignals(
                fdt,
                signalIt != signalsBySystem.end()
                    ? signalIt->second
                    : EmptySignals,
                planetIt != planetsBySystem.end()
                    ? planetIt->second
                    : EmptyPlanets,
                interferenceIt != interferenceBySystem.end()
                    ? interferenceIt->second
                    : EmptyInterference
            );
        }

        if constexpr (game::runtime::RadarSimulationEnabled)
        {
            for (auto& [id, shipPtr] : m_ships)
            {
                if (!shipPtr)
                    continue;

                if (!shipUsesDetailedSimulation(id))
                    continue;

                Ship& ship = *shipPtr;
                const int observerSystemId =
                    ship.core().transform().motion.systemId;

                if (!game::simulation::sameRuntimeSystem(
                        observerSystemId,
                        m_activeCelestialSystemId))
                {
                    continue;
                }

                std::vector<world::RadarContactInput> inputs;
                inputs.reserve(
                    m_ships.size() > 0
                        ? m_ships.size() - 1
                        : 0
                );

                for (auto& [otherId, otherPtr] : m_ships)
                {
                    if (id == otherId || !otherPtr)
                        continue;

                    Ship& other = *otherPtr;
                    const auto& otherTransform =
                        other.core().transform();

                    if (!game::simulation::sameRuntimeSystem(
                            observerSystemId,
                            otherTransform.motion.systemId))
                    {
                        continue;
                    }

                    inputs.push_back({
                        otherId,
                        otherTransform.worldPosition,
                        other.core().desc().radarCrossSection
                    });
                }

                ship.updatePerception(fdt, inputs);
            }
        }
    }


}











void GameSimulation::markShipGraphDirty(EntityId id)
{
    // Structural graph redundancy is counted in snapshots that are actually
    // published, not in simulation ticks. Two publications preserve the old
    // delivery redundancy without coupling GameSimulation to server cadence.
    m_shipGraphPayloadPublicationsRemaining[id] = 2;
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



EntityId GameSimulation::generateEntityId()
{
    EntityId id;
    id.value = m_nextEntityId++;
    return id;
}

void GameSimulation::registerHubMotionLabShip(
    EntityId shipId,
    game::diagnostics::HubMotionLabActorKind kind,
    const std::string& hubId
)
{
    if (shipId.value == 0 ||
        kind == game::diagnostics::HubMotionLabActorKind::None ||
        hubId.empty())
    {
        return;
    }

    m_hubMotionLabShips[shipId] = {kind, hubId};
}

bool GameSimulation::isHubMotionLabShip(
    EntityId shipId
) const noexcept
{
    return m_hubMotionLabShips.find(shipId) !=
        m_hubMotionLabShips.end();
}

game::diagnostics::HubMotionLabActorKind
GameSimulation::hubMotionLabActorKind(
    EntityId shipId
) const noexcept
{
    const auto it = m_hubMotionLabShips.find(shipId);
    return it == m_hubMotionLabShips.end()
        ? game::diagnostics::HubMotionLabActorKind::None
        : it->second.kind;
}

void GameSimulation::registerInterplanetaryTransferLabShip(EntityId shipId)
{
    if (shipId.value == 0)
        return;

    m_interplanetaryTransferLabShipId = shipId;
    m_interplanetaryTransferLabStartUniverseTimeSeconds =
        m_orbitalUniverseTimeSeconds;
    m_interplanetaryTransferLabDeparturePhaseInitialized = false;
}

bool GameSimulation::isInterplanetaryTransferLabShip(
    EntityId shipId
) const noexcept
{
    return
        m_interplanetaryTransferLabShipId.value != 0 &&
        shipId == m_interplanetaryTransferLabShipId;
}


void GameSimulation::registerNavigationRuntimeLabShip(
    EntityId shipId,
    const std::string& hubId
)
{
    if (shipId.value == 0 || hubId.empty())
        return;

    m_navigationRuntimeLabShipId = shipId;
    m_navigationRuntimeLabHubId = hubId;
    m_navigationRuntimeLabInitialized = false;
    m_navigationRuntimeLabStaticGeometryPublished = false;
    m_navigationRuntimeLabHasPreviousExactSafetyPosition = false;
    m_navigationRuntimeLabPreviousExactSafetyPositionMap = glm::dvec3(0.0);
    m_navigationRuntimeLabMap.reset();
    m_navigationRuntimeLabSpace.reset();
    m_navigationRuntimeLabSourceRevision = 0;
    m_navigationRuntimeLabLastPlan = {};
    m_navigationRuntimeLabAcceptedSegment = {};
    m_navigationRuntimeLabWorkScheduler =
        game::navigation::NavigationWorkScheduler {};
    m_navigationRuntimeLabNextPlannerJobRevision = 1;
    m_navigationRuntimeLabCapabilityRevision = 1;
    m_navigationRuntimeLabSchedulerCapabilityInitialized = false;
    m_navigationRuntimeLabSchedulerCapability = {};
    m_navigationRuntimeLabNextSegmentRevision = 1;
    m_navigationRuntimeLabObservation = {};
    m_navigationRuntimeLabObservation.shipEntityId = shipId.value;
}

bool GameSimulation::isNavigationRuntimeLabShip(
    EntityId shipId
) const noexcept
{
    return
        m_navigationRuntimeLabShipId.value != 0 &&
        shipId == m_navigationRuntimeLabShipId;
}

const game::navigation::NavigationRuntimePlanner::Result*
GameSimulation::navigationRuntimeLabLastPlan() const noexcept
{
    if (!m_navigationRuntimeLabInitialized ||
        m_navigationRuntimeLabShipId.value == 0)
    {
        return nullptr;
    }

    return &m_navigationRuntimeLabLastPlan;
}


EntityId GameSimulation::spawnShip(
    ShipRole role,
    int systemId,
    const ShipDescriptor& descriptor,
    const glm::dvec3& positionMeters,
    const ShipInitData& initData,
    const glm::mat4& orientation
)
{
    if (!game::simulation::canCreateInActiveRuntimeSystem(
            systemId,
            m_activeCelestialSystemId))
    {
        return EntityId{};
    }

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
    ship->core().transform().motion.systemId = systemId;

    m_ships[id] = std::move(ship);

    game::simulation::ShipSimulationLifecycleState lifecycle;
    lifecycle.tier =
        game::simulation::ShipSimulationTier::MaterializedShip;
    lifecycle.actorKind =
        role == ShipRole::Player
            ? game::simulation::ShipActorKind::Player
            : game::simulation::ShipActorKind::NpcPilot;
    lifecycle.motionFidelity =
        role == ShipRole::Player
            ? game::simulation::ShipMotionFidelity::FullControl
            : game::simulation::ShipMotionFidelity::Response;
    lifecycle.materializedFidelity =
        role == ShipRole::Player
            ? game::simulation::ShipMaterializedFidelity::Detailed
            : game::simulation::ShipMaterializedFidelity::Response;
    lifecycle.pilotPresent = true;

    game::simulation::ShipMotionFidelityRuntimeState motionState;
    motionState.planned = lifecycle.motionFidelity;
    m_shipMotionFidelityStates[id] = motionState;
    m_shipSimulationLifecycle[id] = lifecycle;

    markShipGraphDirty(id);

    return id;
}


EntityId GameSimulation::spawnTrafficShell(
    int systemId,
    const ShipDescriptor& descriptor,
    const glm::dvec3& positionMeters,
    const glm::dvec3& velocityMetersPerSecond,
    const ShipInitData& initData,
    const glm::mat4& orientation,
    std::uint32_t formationId,
    std::uint16_t formationSlot
)
{
    if (!game::simulation::canCreateInActiveRuntimeSystem(
            systemId,
            m_activeCelestialSystemId))
    {
        return EntityId{};
    }

    const EntityId id = generateEntityId();

    game::simulation::ShipShellState shell;
    shell.id = id;
    shell.definition = &descriptor;
    shell.visual = initData.visual;
    shell.registry = initData.registry;
    shell.transform.setWorldPositionMeters(positionMeters);
    shell.transform.orientation = orientation;
    shell.transform.motion.systemId = systemId;
    shell.transform.motion.worldVelocityMps = velocityMetersPerSecond;
    shell.transform.motion.localVelocityMps = velocityMetersPerSecond;
    shell.transform.referenceVelocityMetersPerSecond = glm::dvec3(0.0);

    shell.lifecycle.tier =
        game::simulation::ShipSimulationTier::TrafficShell;
    shell.lifecycle.actorKind =
        game::simulation::ShipActorKind::TrafficAutopilot;
    shell.lifecycle.motionFidelity =
        game::simulation::ShipMotionFidelity::RouteOnly;
    shell.lifecycle.pilotPresent = true;
    shell.lifecycle.shellEligible = true;
    shell.lifecycle.formationId = formationId;
    shell.lifecycle.formationSlot = formationSlot;

    m_shipShells.emplace(id, std::move(shell));

    game::simulation::ShipMotionFidelityRuntimeState motionState;
    motionState.planned =
        game::simulation::ShipMotionFidelity::RouteOnly;
    m_shipMotionFidelityStates[id] = motionState;

    return id;
}


const game::simulation::ShipShellState*
GameSimulation::shipShell(EntityId id) const
{
    const auto it = m_shipShells.find(id);
    return it == m_shipShells.end() ? nullptr : &it->second;
}


game::simulation::ShipSimulationTier
GameSimulation::shipSimulationTier(EntityId id) const noexcept
{
    const auto shellIt = m_shipShells.find(id);
    if (shellIt != m_shipShells.end())
        return shellIt->second.lifecycle.tier;

    const auto it = m_shipSimulationLifecycle.find(id);
    if (it != m_shipSimulationLifecycle.end())
        return it->second.tier;

    return game::simulation::ShipSimulationTier::MaterializedShip;
}

game::simulation::ShipMotionFidelity
GameSimulation::shipMotionFidelity(EntityId id) const noexcept
{
    const auto shellIt = m_shipShells.find(id);
    if (shellIt != m_shipShells.end())
        return shellIt->second.lifecycle.motionFidelity;

    const auto it = m_shipSimulationLifecycle.find(id);
    if (it != m_shipSimulationLifecycle.end())
        return it->second.motionFidelity;

    return game::simulation::ShipMotionFidelity::Response;
}


game::simulation::ShipMaterializedFidelity
GameSimulation::shipMaterializedFidelity(EntityId id) const noexcept
{
    const auto shellIt = m_shipShells.find(id);
    if (shellIt != m_shipShells.end())
        return shellIt->second.lifecycle.materializedFidelity;

    const auto it = m_shipSimulationLifecycle.find(id);
    if (it != m_shipSimulationLifecycle.end())
        return it->second.materializedFidelity;

    return game::simulation::ShipMaterializedFidelity::Response;
}


bool GameSimulation::shipUsesDetailedSimulation(EntityId id) const noexcept
{
    const auto it = m_shipSimulationLifecycle.find(id);
    return
        it != m_shipSimulationLifecycle.end() &&
        it->second.usesDetailedSimulation();
}


bool GameSimulation::promoteShipToDetailedSimulation(EntityId id)
{
    if (!getShip(id))
        return false;

    auto it = m_shipSimulationLifecycle.find(id);
    if (it == m_shipSimulationLifecycle.end())
        return false;

    if (it->second.materializedFidelity !=
        game::simulation::ShipMaterializedFidelity::Detailed)
    {
        it->second.promoteDetailed();

        // Detailed promotion is the publication boundary for module/link/hit
        // state. Response actors intentionally do not ship that graph.
        markShipGraphDirty(id);
    }

    return true;
}


bool GameSimulation::submitClientObservationInterest(
    EntityId observerId,
    std::uint64_t serial,
    const std::vector<
        game::simulation::ShipClientObservationInterestEntry>& entries
)
{
    if (observerId.value == 0 ||
        !isPlayerControlled(observerId) ||
        !getShip(observerId) ||
        entries.size() >
            game::simulation::MaxShipClientObservationInterestEntries)
    {
        return false;
    }

    auto& state = m_shipClientObservationInterests[observerId];
    const double now = m_serverTimelineClock.timeSeconds();

    // A live stream is monotonic. Once the previous hint has expired, a new
    // client/session controlling the same entity may restart its local serial.
    if (state.live(now) && state.serial != 0 && serial <= state.serial)
        return false;

    state.serial = serial;
    state.expiresAtServerTimeSeconds =
        now +
        game::simulation::ShipClientObservationInterestLifetimeSeconds;
    state.entries.clear();
    state.entries.reserve(entries.size());

    std::unordered_map<EntityId, game::simulation::ShipMotionFidelity>
        strongestByTarget;
    strongestByTarget.reserve(entries.size());

    for (const auto& entry : entries)
    {
        if (entry.targetId.value == 0 ||
            entry.targetId == observerId)
        {
            continue;
        }

        const auto sanitized =
            game::simulation::sanitizeClientObservationDemand(
                entry.requested
            );
        if (sanitized ==
            game::simulation::ShipMotionFidelity::RouteOnly)
        {
            continue;
        }

        auto [it, inserted] =
            strongestByTarget.emplace(entry.targetId, sanitized);
        if (!inserted)
        {
            it->second =
                game::simulation::strongerMotionFidelity(
                    it->second,
                    sanitized
                );
        }
    }

    for (const auto& [targetId, requested] : strongestByTarget)
    {
        state.entries.push_back({
            targetId,
            requested
        });
    }

    return true;
}



bool GameSimulation::materializeShipShell(EntityId id)
{
    auto shellIt = m_shipShells.find(id);
    if (shellIt == m_shipShells.end())
        return getShip(id) != nullptr;

    const auto shell = shellIt->second;
    if (!shell.valid())
        return false;

    auto ship = std::make_unique<Ship>();
    ship->setId(id);
    ship->setTypeId(shell.definition->typeId);

    ShipInitData init;
    init.visual = shell.visual;
    init.registry = shell.registry;

    ship->init(
        ShipRole::NPC,
        *shell.definition,
        glm::vec3(0.0f),
        init,
        shell.transform.orientation
    );
    ship->core().transform() = shell.transform;

    // Expand cheap aggregate engine damage into the real module graph. This is
    // deliberately approximate: exact hit history begins only after materialization.
    const auto& definitions =
        game::ship::plant::physicalControlSlots(
            shell.definition->physicalControlModel
        );
    const auto& binding =
        shell.definition->physicalControl.slotModuleIds;

    if (binding.size() == definitions.size())
    {
        for (std::size_t i = 0; i < binding.size(); ++i)
        {
            if (binding[i].empty())
                continue;

            auto* module =
                ship->core().moduleRuntime().findModule(binding[i]);
            if (!module)
                continue;

            double authority =
                shell.damage.mainThrustAuthority01;
            if (definitions[i].role ==
                game::ship::plant::EngineSlotRole::ManoeuvreCluster)
            {
                authority = shell.damage.manoeuvreAuthority01;
            }
            else
            {
                authority = std::min(
                    shell.damage.mainThrustAuthority01,
                    shell.damage.mainVectoringAuthority01
                );
            }

            ship->core().moduleRuntime().setModuleHealth(
                binding[i],
                static_cast<float>(
                    module->maxHealth *
                    std::clamp(authority, 0.0, 1.0)
                )
            );
        }
    }

    ship->core().refreshRuntimeState();

    auto lifecycle = shell.lifecycle;
    if (lifecycle.tier ==
        game::simulation::ShipSimulationTier::WreckShell)
    {
        lifecycle.tier =
            game::simulation::ShipSimulationTier::Derelict;
        lifecycle.actorKind =
            game::simulation::ShipActorKind::None;
        lifecycle.pilotPresent = false;
        lifecycle.formationId = 0;
        lifecycle.formationSlot = 0;
        ++lifecycle.revision;
    }
    else
    {
        lifecycle.materializeForInteraction();
    }

    m_shipShells.erase(shellIt);
    m_ships[id] = std::move(ship);
    m_shipSimulationLifecycle[id] = lifecycle;

    auto& motionState = m_shipMotionFidelityStates[id];
    motionState.planned = lifecycle.motionFidelity;
    markShipGraphDirty(id);
    return true;
}


bool GameSimulation::markShipDerelict(EntityId id)
{
    Ship* ship = getShip(id);
    if (!ship)
        return false;

    auto& lifecycle = m_shipSimulationLifecycle[id];
    lifecycle.onPilotEjected();

    ship->setControlState(ShipControlState {});
    m_playerControlledShipIds.erase(id);
    m_npcNavigationControlBridges.erase(id);
    m_npcNavigationExecutionSnapshots.erase(id);
    m_npcNavigationLastExecutionTimeSeconds.erase(id);
    m_npcAutopilotLawControllers.erase(id);
    m_npcAiCadenceStates.erase(id);
    m_shipMotionControlCadenceStates.erase(id);

    // A pilotless or uncontrollable hull is no longer a formation member.
    m_fiveHundredShipHerd.erase(id);
    return true;
}


bool GameSimulation::dematerializeShip(
    EntityId id,
    game::simulation::ShipSimulationTier targetTier
)
{
    if (targetTier != game::simulation::ShipSimulationTier::TrafficShell &&
        targetTier != game::simulation::ShipSimulationTier::WreckShell)
    {
        return false;
    }

    auto shipIt = m_ships.find(id);
    if (shipIt == m_ships.end() || !shipIt->second)
        return false;

    Ship& ship = *shipIt->second;
    auto lifecycleIt = m_shipSimulationLifecycle.find(id);
    auto lifecycle =
        lifecycleIt != m_shipSimulationLifecycle.end()
            ? lifecycleIt->second
            : game::simulation::ShipSimulationLifecycleState{};

    if (targetTier == game::simulation::ShipSimulationTier::TrafficShell &&
        !lifecycle.pilotPresent)
    {
        return false;
    }

    if (targetTier == game::simulation::ShipSimulationTier::WreckShell &&
        lifecycle.pilotPresent)
    {
        return false;
    }

    game::simulation::ShipShellState shell;
    shell.id = id;
    shell.definition = &ship.core().desc();
    shell.visual = ship.core().visualIdentity();
    shell.registry = ship.core().registry();
    shell.transform = ship.core().transform();
    shell.lifecycle = lifecycle;
    shell.lifecycle.tier = targetTier;
    ++shell.lifecycle.revision;

    double structuralSum = 0.0;
    std::size_t structuralCount = 0;
    for (const auto& module :
         static_cast<const world::modules::ObjectModuleRuntime&>(
             ship.core().moduleRuntime()).modules())
    {
        if (module.maxHealth <= 0.0f)
            continue;
        structuralSum +=
            std::clamp(
                static_cast<double>(module.health / module.maxHealth),
                0.0,
                1.0
            );
        ++structuralCount;
    }
    shell.damage.structuralIntegrity01 =
        structuralCount > 0
            ? structuralSum / static_cast<double>(structuralCount)
            : 1.0;

    const auto& definitions =
        game::ship::plant::physicalControlSlots(
            ship.core().state().construction.physicalControlModel
        );
    const auto& slots = ship.core().state().actuators.propulsionSlots;

    double mainThrust = 0.0;
    double mainVectoring = 0.0;
    double manoeuvre = 0.0;
    std::size_t mainCount = 0;
    std::size_t manoeuvreCount = 0;

    const std::size_t count = std::min(definitions.size(), slots.size());
    for (std::size_t i = 0; i < count; ++i)
    {
        if (definitions[i].role ==
            game::ship::plant::EngineSlotRole::ManoeuvreCluster)
        {
            manoeuvre +=
                game::ship::plant::effectiveSlotThrust01(slots[i]);
            ++manoeuvreCount;
        }
        else
        {
            mainThrust +=
                game::ship::plant::effectiveSlotThrust01(slots[i]);
            mainVectoring +=
                game::ship::plant::effectiveSlotVectoring01(slots[i]);
            ++mainCount;
        }
    }

    shell.damage.mainThrustAuthority01 =
        mainCount > 0
            ? mainThrust / static_cast<double>(mainCount)
            : 0.0;
    shell.damage.mainVectoringAuthority01 =
        mainCount > 0
            ? mainVectoring / static_cast<double>(mainCount)
            : 0.0;
    shell.damage.manoeuvreAuthority01 =
        manoeuvreCount > 0
            ? manoeuvre / static_cast<double>(manoeuvreCount)
            : 1.0;

    m_npcNavigationControlBridges.erase(id);
    m_npcNavigationExecutionSnapshots.erase(id);
    m_npcNavigationLastExecutionTimeSeconds.erase(id);
    m_npcAutopilotLawControllers.erase(id);
    m_npcAiCadenceStates.erase(id);
    m_shipMotionControlCadenceStates.erase(id);
    m_shipSystemsCadenceStates.erase(id);
    m_shipMaintenanceCadenceStates.erase(id);

    m_ships.erase(shipIt);
    m_shipSimulationLifecycle.erase(id);
    m_shipShells[id] = std::move(shell);

    auto& motionState = m_shipMotionFidelityStates[id];
    motionState.planned = m_shipShells[id].lifecycle.motionFidelity;

    return true;
}


EntityId GameSimulation::spawnStation(
    ObjectType type,
    int systemId,
    const glm::dvec3& positionMeters,
    const glm::mat4& orientation
)
{
    if (!game::simulation::canCreateInActiveRuntimeSystem(
            systemId,
            m_activeCelestialSystemId))
    {
        return EntityId{};
    }

    EntityId id = generateEntityId();

    auto& obj = m_staticObjects[id];

    obj.id = id;
    obj.type = type;
    obj.systemId = systemId;
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


void GameSimulation::setCelestialBodyKinematicStateAu(
    int systemId,
    const std::unordered_map<std::string, glm::dvec3>& positionsAu,
    const std::unordered_map<std::string, glm::dvec3>& velocitiesAuPerSecond
)
{
    if (systemId < 0)
        return;

    if (m_activeCelestialSystemId != systemId)
    {
        m_activeCelestialSystemId = systemId;
        m_celestialBodyGravityParameters.clear();
        m_hubNavigationFrames.clear();
        m_hubVelocityMetersPerSecond.clear();
        m_gravityBodies.clear();
        m_orbitalCorridors.clear();
    }

    m_celestialBodyPositionsAu = positionsAu;
    m_celestialBodyVelocitiesMetersPerSecond.clear();

    for (const auto& [bodyId, velocityAuPerSecond] : velocitiesAuPerSecond)
    {
        m_celestialBodyVelocitiesMetersPerSecond[bodyId] =
            velocityAuPerSecond * world::celestial::MetersPerAu;
    }
}







void GameSimulation::setCelestialBodyGravityParameters(
    int systemId,
    const std::string& bodyId,
    double radiusMeters,
    double gravitationalParameterM3s2,
    double atmosphereOuterRadiusMeters
)
{
    if (systemId < 0 ||
        systemId != m_activeCelestialSystemId ||
        bodyId.empty())
    {
        return;
    }

    CelestialBodyGravityParameters parameters;
    parameters.radiusMeters =
        std::max(0.0, radiusMeters);
    parameters.gravitationalParameterM3s2 =
        std::max(0.0, gravitationalParameterM3s2);
    parameters.atmosphereOuterRadiusMeters =
        std::max(parameters.radiusMeters, atmosphereOuterRadiusMeters);

    m_celestialBodyGravityParameters[bodyId] =
        parameters;
}


void GameSimulation::setOrbitalUniverseTimeSeconds(double t)
{
    m_orbitalUniverseTimeSeconds = t;
}



















bool GameSimulation::setStaticObjectIdentity(
    EntityId id,
    const std::string& name,
    const std::string& owner
)
{
    auto it = m_staticObjects.find(id);
    if (it == m_staticObjects.end())
        return false;

    it->second.displayName = name;
    it->second.ownerName = owner;
    return true;
}


bool GameSimulation::setStaticObjectMapInfo(
    EntityId id,
    const std::string& name,
    const std::string& owner,
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
    it->second.systemMapVisible = true;
    it->second.mapParentBodyId = parentBodyId;
    it->second.hubId = hubId;
    it->second.hubModuleId = hubModuleId;

    return true;
}


































bool GameSimulation::setStaticObjectOrbitalMotion(
    EntityId id,
    const std::string& parentBodyId,
    const world::orbits::OrbitalMotion& motion
)
{
    auto it = m_staticObjects.find(id);

    if (it == m_staticObjects.end())
        return false;

    if (parentBodyId.empty() ||
        it->second.systemId != m_activeCelestialSystemId ||
        m_celestialBodyPositionsAu.find(parentBodyId) ==
            m_celestialBodyPositionsAu.end())
    {
        return false;
    }

    it->second.orbitalParentBodyId = parentBodyId;
    it->second.orbitalMotion = motion;
    it->second.orbitalMotion.enabled = true;

    return true;
}







bool GameSimulation::registerOrbitalHub(
    const world::hubs::OrbitalHubRuntime& hub
)
{
    if (hub.id.empty() ||
        !game::simulation::canCreateInActiveRuntimeSystem(
            hub.systemId,
            m_activeCelestialSystemId))
    {
        return false;
    }

    m_orbitalHubs[hub.id] = hub;
    return true;
}

bool GameSimulation::attachStaticObjectToHub(
    EntityId objectId,
    const std::string& hubId,
    const std::string& hubModuleId,
    const glm::dvec3& localOffsetMeters,
    const glm::dvec3& localRotationDeg,
    bool inheritHubOrientation,
    const glm::dvec3& localAngularVelocityDegPerSecond
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

    const int hubSystemId = hubIt->second.systemId;
    if (hubSystemId < 0 ||
        obj.systemId != hubSystemId)
    {
        return false;
    }
    obj.attachedToHub = true;
    obj.hubId = hubId;
    obj.hubModuleId = hubModuleId;
    obj.hubLocalOffsetMeters = localOffsetMeters;
    obj.hubLocalRotationDeg = localRotationDeg;
    obj.hubLocalAngularVelocityDegPerSecond =
        localAngularVelocityDegPerSecond;
    obj.inheritHubOrientation = inheritHubOrientation;

    obj.orbitalMotion.enabled = false;

    hubIt->second.modules.push_back(objectId);

    return true;
}













void GameSimulation::updateStaticObjectOrbitParentParameters(
    int systemId,
    const std::string& parentBodyId,
    double parentRadiusMeters,
    double parentGravitationalParameterM3s2
)
{
    for (auto& [id, obj] : m_staticObjects)
    {
        if (!obj.orbitalMotion.enabled)
            continue;

        if (obj.systemId != systemId ||
            obj.orbitalParentBodyId != parentBodyId)
        {
            continue;
        }

        obj.orbitalMotion.parentRadiusMeters =
            parentRadiusMeters;

        if (obj.orbitalMotion.orbitalPeriodPolicy ==
            world::orbits::OrbitalPeriodPolicy::Kepler)
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

        if (hub.systemId != systemId ||
            hub.parentBodyId != parentBodyId)
        {
            continue;
        }

        hub.motion.parentRadiusMeters =
            parentRadiusMeters;

        if (hub.motion.orbitalPeriodPolicy ==
            world::orbits::OrbitalPeriodPolicy::Kepler)
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
    if (!getShip(id) && shipShell(id))
        (void)materializeShipShell(id);

    (void)promoteShipToDetailedSimulation(id);

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

const WorldParams& GameSimulation::world() const
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
    if (!getShip(id) && shipShell(id))
        (void)materializeShipShell(id);

    (void)promoteShipToDetailedSimulation(id);

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
    if (!getShip(id) && shipShell(id))
        (void)materializeShipShell(id);

    (void)promoteShipToDetailedSimulation(id);

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
    if (!getShip(id) && shipShell(id))
        (void)materializeShipShell(id);

    (void)promoteShipToDetailedSimulation(id);

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
    if (!getShip(id) && shipShell(id))
        (void)materializeShipShell(id);

    (void)promoteShipToDetailedSimulation(id);

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
    if (!getShip(id) && shipShell(id))
        (void)materializeShipShell(id);

    (void)promoteShipToDetailedSimulation(id);

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
    {
        // The cockpit fragment is the current physical escape capsule carrier.
        // The abandoned hull immediately loses AI/formation authority and
        // continues only under physics until it may be reduced to WreckShell.
        markShipGraphDirty(id);
        (void)markShipDerelict(id);
    }
    return changed;
}






bool GameSimulation::debugHangShipModule(EntityId id, const std::string& moduleId)
{
    if (!getShip(id) && shipShell(id))
        (void)materializeShipShell(id);

    (void)promoteShipToDetailedSimulation(id);

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
    if (!getShip(id) && shipShell(id))
        (void)materializeShipShell(id);

    (void)promoteShipToDetailedSimulation(id);

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
    if (!getShip(id) && shipShell(id))
        (void)materializeShipShell(id);

    (void)promoteShipToDetailedSimulation(id);

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
    if (!getShip(id) && shipShell(id))
        (void)materializeShipShell(id);

    (void)promoteShipToDetailedSimulation(id);

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



bool GameSimulation::setPlayerControlled(EntityId id, bool controlled)
{
    if (id.value == 0 || !getShip(id))
        return false;

    if (controlled)
    {
        m_playerControlledShipIds.insert(id);
    }
    else
    {
        m_playerControlledShipIds.erase(id);
        m_shipClientObservationInterests.erase(id);
    }

    return true;
}

bool GameSimulation::isPlayerControlled(EntityId id) const noexcept
{
    return m_playerControlledShipIds.find(id) !=
        m_playerControlledShipIds.end();
}

void GameSimulation::setPlayerControl(const ShipControlState& control)
{
    m_playerControlState = control;
}

std::unordered_map<EntityId, std::unique_ptr<Ship>>& GameSimulation::ships()
{
    return m_ships;
}


const std::unordered_map<EntityId, std::unique_ptr<Ship>>&
GameSimulation::ships() const
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

    const int targetSystemId =
        targetShip->core().transform().motion.systemId;

    if (!game::simulation::sameRuntimeSystem(
            targetSystemId,
            m_activeCelestialSystemId))
    {
        return false;
    }

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

        if (!game::simulation::sameRuntimeSystem(
                sourceShip->core().transform().motion.systemId,
                targetSystemId))
        {
            continue;
        }

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