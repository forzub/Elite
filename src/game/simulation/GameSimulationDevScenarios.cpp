// GameSimulation: DEV scenarios and diagnostics: ship-state flight test, 500-ship herd, tier transition test, universe trajectory diagnostic, hub-motion / interplanetary-transfer labs, promo tracking, debug logs.

#include "src/game/simulation/GameSimulationInternal.h"



void GameSimulation::initializeShipStateFlightTest()
{
    if (!m_shipStateFlightTestRequested ||
        m_shipStateFlightTestShipId.value != 0 ||
        m_activeCelestialSystemId < 0)
    {
        return;
    }

    glm::dvec3 origin(0.0);
    if (const Ship* player = playerShip())
    {
        origin =
            world::coordinates::fullMeters(
                player->core().transform().worldPosition
            );
    }

    const glm::dvec3 position =
        origin + glm::dvec3(0.0, 600.0, 1800.0);

    ShipInitData init;
    init.visual.shipType = "Cobra MK1";
    init.visual.shipName = "Ship State Flight Test";
    init.registry.instanceId = 950000;
    init.registry.ownerName = "diagnostics";
    init.registry.registrationId = "STATE-FLIGHT";
    init.registry.shipRole = ShipRoleType::Civilian;

    const auto& descriptor =
        EliteCobraMk1::EliteCobraMk1Descriptor();

    const EntityId id =
        spawnShip(
            ShipRole::NPC,
            m_activeCelestialSystemId,
            descriptor,
            position,
            init,
            glm::mat4(1.0f)
        );
    if (id.value == 0)
        return;

    Ship* ship = getShip(id);
    if (!ship)
        return;

    auto& tr = ship->core().transform();
    tr.motion.mode = game::navigation::MotionMode::HubTactical;
    tr.motion.travelFrame =
        makeDiagnosticIdentityFrame(
            m_activeCelestialSystemId,
            "ship-state-flight-test",
            origin
        );
    tr.motion.localPositionMeters =
        tr.motion.travelFrame.worldToLocalPosition(position);
    tr.motion.localVelocityMps = glm::dvec3(0.0);
    tr.motion.worldVelocityMps = glm::dvec3(0.0);
    tr.motion.referenceVelocityMps = glm::dvec3(0.0);
    tr.referenceVelocityMetersPerSecond = glm::dvec3(0.0);

    m_shipStateFlightTestShipId = id;
    m_shipStateFlightTestStartSeconds =
        m_serverTimelineClock.timeSeconds();
    m_shipStateFlightTestLastLogSeconds = -1.0;
    m_shipStateFlightTestPhase =
        static_cast<std::size_t>(-1);

    std::ofstream reset("ship_state_flight.csv", std::ios::trunc);
    if (reset)
    {
        reset
            << "time_s,cycle,phase,model,damage_profile,damage_name,"
            << "speed_mps,omega_rad_s,"
            << "px,py,pz,vx,vy,vz,"
            << "main_cmd,pitch_cmd,yaw_cmd,roll_cmd,"
            << "strafe_cmd,lift_cmd,gimbal_r,gimbal_u,"
            << "gimbal_yaw_deg,gimbal_pitch_deg,"
            << "engine_ax,engine_ay,engine_az,control_state\n";
    }

    std::cout
        << "[ShipStateFlight] spawned id=" << id.value
        << " continuous=1 reset_kinematics=0\n";
}

bool GameSimulation::isShipStateFlightTestShip(
    EntityId id
) const noexcept
{
    return m_shipStateFlightTestShipId.value != 0 &&
        id == m_shipStateFlightTestShipId;
}

void GameSimulation::updateShipStateFlightTest()
{
    if (!m_shipStateFlightTestRequested)
        return;

    if (m_shipStateFlightTestShipId.value == 0)
    {
        initializeShipStateFlightTest();
        return;
    }

    Ship* ship = getShip(m_shipStateFlightTestShipId);
    if (!ship)
        return;

    using Model = game::ship::plant::PhysicalControlModelId;
    static constexpr std::array<Model, 6> Models {
        Model::AssistedI,
        Model::AssistedY,
        Model::AssistedX,
        Model::NewtonianI,
        Model::NewtonianY,
        Model::NewtonianX
    };

    constexpr double PhaseSeconds = 8.0;
    constexpr std::size_t DamageProfiles = 7;
    constexpr std::size_t PhaseCount =
        Models.size() * DamageProfiles;
    constexpr double CycleSeconds =
        PhaseSeconds * static_cast<double>(PhaseCount);

    const double now = m_serverTimelineClock.timeSeconds();
    const double elapsed =
        std::max(0.0, now - m_shipStateFlightTestStartSeconds);
    const std::uint64_t cycle =
        static_cast<std::uint64_t>(
            std::floor(elapsed / CycleSeconds)
        );
    const double withinCycle =
        std::fmod(elapsed, CycleSeconds);
    const std::size_t phase =
        std::min(
            PhaseCount - 1,
            static_cast<std::size_t>(
                withinCycle / PhaseSeconds
            )
        );
    const double phaseTime =
        withinCycle -
        static_cast<double>(phase) * PhaseSeconds;
    const Model model = Models[phase % Models.size()];
    const std::size_t damageProfile = phase / Models.size();

    if (phase != m_shipStateFlightTestPhase)
    {
        if (!ship->core().selectPhysicalConstructionForDiagnostic(model))
            return;

        auto& state = ship->core().state();
        const auto& slots =
            game::ship::plant::physicalControlSlots(model);

        const bool assisted =
            game::ship::plant::familyOf(model) ==
            game::ship::plant::PhysicalControlFamily::Assisted;

        const auto firstSlotWithRole =
            [&](game::ship::plant::EngineSlotRole role)
                -> game::ship::plant::EngineSlotState*
            {
                for (std::size_t i = 0; i < slots.size(); ++i)
                {
                    if (slots[i].role == role)
                        return &state.actuators.propulsionSlots[i];
                }
                return nullptr;
            };

        if (damageProfile == 1 || damageProfile == 2)
        {
            if (auto* actuator = firstSlotWithRole(
                    game::ship::plant::EngineSlotRole::AftMain))
            {
                actuator->thrust01 =
                    damageProfile == 1 ? 0.50 : 0.0;
                if (damageProfile == 2)
                {
                    actuator->operational = false;
                    actuator->vectoring01 = 0.0;
                }
            }
        }
        else if (damageProfile == 3 || damageProfile == 4)
        {
            const double vectoring =
                damageProfile == 3 ? 0.50 : 0.0;

            for (std::size_t i = 0; i < slots.size(); ++i)
            {
                if (slots[i].role !=
                    game::ship::plant::EngineSlotRole::ManoeuvreCluster)
                {
                    state.actuators.propulsionSlots[i].vectoring01 =
                        vectoring;
                }
            }
        }
        else if (damageProfile == 5 || damageProfile == 6)
        {
            const auto role =
                assisted
                    ? game::ship::plant::EngineSlotRole::ForeMain
                    : game::ship::plant::EngineSlotRole::ManoeuvreCluster;

            if (auto* actuator = firstSlotWithRole(role))
            {
                actuator->thrust01 =
                    damageProfile == 5 ? 0.50 : 0.0;
                if (damageProfile == 6)
                {
                    actuator->operational = false;
                    actuator->vectoring01 = 0.0;
                }
            }
        }

        state.actuators.touch();

        m_shipStateFlightTestPhase = phase;
        std::cout
            << "[ShipStateFlight] cycle=" << cycle
            << " phase=" << phase
            << " model=" << physicalModelName(model)
            << " damage=" << damageProfile
            << " damage_name="
            << physicalDamageProfileName(model, damageProfile)
            << " position_preserved=1 velocity_preserved=1\n";
    }

    ShipControlState control;
    control.controlTick = ++m_shipStateFlightControlTick;

    if (phaseTime < 1.5)
    {
        // Positive main-thrust response.
        control.targetSpeedRate = 0.28f;
    }
    else if (phaseTime < 3.0)
    {
        // Main + attitude + TVC coupling.
        control.targetSpeedRate = 0.18f;
        control.pitchInput = 0.18f;
        control.yawInput = 0.32f;
        control.mainGimbalRightInput = 0.35f;
        control.mainGimbalUpInput = -0.20f;
    }
    else if (phaseTime < 4.5)
    {
        // Lateral/vertical authority and opposite TVC demand.
        control.targetSpeedRate = 0.08f;
        control.strafeInput = 0.30f;
        control.liftInput = 0.18f;
        control.mainGimbalRightInput = -0.25f;
        control.mainGimbalUpInput = 0.25f;
    }
    else if (phaseTime < 6.0)
    {
        // Assisted must use its real fore-main authority. Newtonian has no
        // reverse main and therefore deliberately ignores this sign.
        control.targetSpeedRate = -0.25f;
    }
    // Final two seconds are neutral to expose coast/stabilization.

    ship->setControlState(control);

    if (m_shipStateFlightTestLastLogSeconds >= 0.0 &&
        now - m_shipStateFlightTestLastLogSeconds < 0.20)
    {
        return;
    }
    m_shipStateFlightTestLastLogSeconds = now;

    const auto& tr = ship->core().transform();
    const auto capability = ship->core().capabilities();
    const double speed = glm::length(tr.motion.localVelocityMps);
    const double omega =
        std::sqrt(
            static_cast<double>(tr.pitchRate) * tr.pitchRate +
            static_cast<double>(tr.yawRate) * tr.yawRate +
            static_cast<double>(tr.rollRate) * tr.rollRate
        );

    std::ofstream out("ship_state_flight.csv", std::ios::app);
    if (out)
    {
        out
            << std::fixed << std::setprecision(6)
            << now << ','
            << cycle << ','
            << phase << ','
            << physicalModelName(model) << ','
            << damageProfile << ','
            << physicalDamageProfileName(model, damageProfile) << ','
            << speed << ','
            << omega << ','
            << tr.motion.localPositionMeters.x << ','
            << tr.motion.localPositionMeters.y << ','
            << tr.motion.localPositionMeters.z << ','
            << tr.motion.localVelocityMps.x << ','
            << tr.motion.localVelocityMps.y << ','
            << tr.motion.localVelocityMps.z << ','
            << control.targetSpeedRate << ','
            << control.pitchInput << ','
            << control.yawInput << ','
            << control.rollInput << ','
            << control.strafeInput << ','
            << control.liftInput << ','
            << control.mainGimbalRightInput << ','
            << control.mainGimbalUpInput << ','
            << tr.motion.mainGimbalYawRad * 57.29577951308232 << ','
            << tr.motion.mainGimbalPitchRad * 57.29577951308232 << ','
            << tr.motion.engineAccelerationMps2.x << ','
            << tr.motion.engineAccelerationMps2.y << ','
            << tr.motion.engineAccelerationMps2.z << ','
            << static_cast<int>(capability.controlState)
            << '\n';
    }
}

void GameSimulation::initializeFiveHundredShipHerd()
{
    if ((!m_fiveHundredShipHerdRequested &&
         !m_fiveHundredMaterializedShipHerdRequested &&
         !m_fiveHundredFullMaterializedStressRequested) ||
        !m_fiveHundredShipHerd.empty() ||
        m_activeCelestialSystemId < 0)
    {
        return;
    }

    glm::dvec3 anchor(0.0);
    glm::dvec3 forward(0.0, 0.0, -1.0);
    glm::dvec3 up(0.0, 1.0, 0.0);

    if (const Ship* player = playerShip())
    {
        anchor =
            world::coordinates::fullMeters(
                player->core().transform().worldPosition
            );
        forward = glm::normalize(
            glm::dvec3(player->core().transform().forward())
        );
        up = glm::normalize(
            glm::dvec3(player->core().transform().up())
        );
    }

    // The proving ground must be visible without turning around. The old
    // fixed +Z placement put the herd behind the default Cobra (-Z forward).
    m_fiveHundredShipHerdCenterMeters =
        anchor + forward * 5000.0 + up * 450.0;
    m_fiveHundredShipHerdStartSeconds =
        m_serverTimelineClock.timeSeconds();
    m_fiveHundredShipHerdLastLogSeconds = -1.0;

    const auto& descriptor =
        EliteCobraMk1::EliteCobraMk1Descriptor();
    const auto frame =
        makeDiagnosticIdentityFrame(
            m_activeCelestialSystemId,
            "500-ships-loop",
            m_fiveHundredShipHerdCenterMeters
        );

    for (std::size_t ordinal = 0;
         ordinal < FiveHundredShipCount;
         ++ordinal)
    {
        const std::size_t flightIndex =
            ordinal / HerdFlightSize;
        const std::size_t slotInFlight =
            ordinal % HerdFlightSize;

        const auto sample =
            sampleHerdRoute(
                m_fiveHundredShipHerdCenterMeters,
                flightIndex,
                slotInFlight,
                0.0
            );

        ShipInitData init;
        init.visual.shipType = "Cobra MK1";
        init.visual.shipName =
            "500 ships F" +
            std::to_string(flightIndex + 1) +
            "-" +
            std::to_string(slotInFlight + 1);
        init.registry.instanceId =
            static_cast<std::uint64_t>(500000 + ordinal);
        init.registry.ownerName = "500 ships";
        init.registry.registrationId =
            "HERD-" + std::to_string(ordinal + 1);
        init.registry.shipRole = ShipRoleType::Civilian;

        EntityId id {};
        const bool materializeThisMember =
            m_fiveHundredFullMaterializedStressRequested ||
            (m_fiveHundredMaterializedShipHerdRequested &&
             ordinal < HerdMaterializedInteractionCount);

        if (materializeThisMember)
        {
            id =
                spawnShip(
                    ShipRole::NPC,
                    m_activeCelestialSystemId,
                    descriptor,
                    sample.positionMeters,
                    init,
                    orientationFromVelocity(sample.velocityMps)
                );

            if (Ship* ship = getShip(id))
            {
                auto& tr = ship->core().transform();
                tr.motion.mode = game::navigation::MotionMode::HubTactical;
                tr.motion.travelFrame = frame;
                tr.motion.localPositionMeters =
                    frame.worldToLocalPosition(sample.positionMeters);
                tr.motion.localVelocityMps =
                    frame.worldToLocalVector(sample.velocityMps);
                tr.motion.worldVelocityMps = sample.velocityMps;
                tr.motion.referenceVelocityMps = glm::dvec3(0.0);
                tr.referenceVelocityMetersPerSecond = glm::dvec3(0.0);
            }
        }
        else
        {
            id =
                spawnTrafficShell(
                    m_activeCelestialSystemId,
                    descriptor,
                    sample.positionMeters,
                    sample.velocityMps,
                    init,
                    orientationFromVelocity(sample.velocityMps),
                    static_cast<std::uint32_t>(flightIndex + 1),
                    static_cast<std::uint16_t>(slotInFlight)
                );

            auto shellIt = m_shipShells.find(id);
            if (shellIt != m_shipShells.end())
            {
                auto& tr = shellIt->second.transform;
                tr.motion.mode = game::navigation::MotionMode::HubTactical;
                tr.motion.travelFrame = frame;
                tr.motion.localPositionMeters =
                    frame.worldToLocalPosition(sample.positionMeters);
                tr.motion.localVelocityMps =
                    frame.worldToLocalVector(sample.velocityMps);
                tr.motion.worldVelocityMps = sample.velocityMps;
            }
        }

        if (id.value == 0)
            continue;

        m_fiveHundredShipHerd.emplace(
            id,
            FiveHundredShipHerdMember {
                ordinal,
                flightIndex,
                slotInFlight
            }
        );

    }

    std::ofstream reset("herd_500.csv", std::ios::trunc);
    if (reset)
    {
        reset
            << "time_s,entities,shells,materialized,state_valid,"
            << "route_only,kinematic,response,full_control,"
            << "active,prewarm,coarse,other,"
            << "mean_slot_error_m,max_slot_error_m,mean_speed_mps\n";
    }

    std::cout
        << "[500Ships] spawned=" << m_fiveHundredShipHerd.size()
        << " requested=" << FiveHundredShipCount
        << " flights=" << HerdFlightCount
        << " flight_size=3"
        << " speed_mps=" << HerdSpeedMetersPerSecond
        << " radius_m=" << HerdRadiusMeters
        << " tier="
        << (m_fiveHundredFullMaterializedStressRequested
                ? "full_materialized_stress"
                : (m_fiveHundredMaterializedShipHerdRequested
                    ? "mixed_24_materialized_476_shell"
                    : "traffic_shell"))
        << "\n";
}

void GameSimulation::initializeShipTierTransitionTest()
{
    if (!m_shipTierTransitionTestRequested ||
        m_shipTierTransitionProbeId.value != 0 ||
        m_activeCelestialSystemId < 0)
    {
        return;
    }

    glm::dvec3 anchor(0.0);
    glm::dvec3 forward(0.0, 0.0, -1.0);
    glm::dvec3 up(0.0, 1.0, 0.0);

    if (const Ship* player = playerShip())
    {
        anchor =
            world::coordinates::fullMeters(
                player->core().transform().worldPosition
            );
        forward = glm::normalize(
            glm::dvec3(player->core().transform().forward())
        );
        up = glm::normalize(
            glm::dvec3(player->core().transform().up())
        );
    }

    const auto& descriptor =
        EliteCobraMk1::EliteCobraMk1Descriptor();

    ShipInitData init;
    init.visual.shipType = "Cobra MK1";
    init.visual.shipName = "Tier Transition Probe";
    init.registry.instanceId = 990001;
    init.registry.ownerName = "diagnostics";
    init.registry.registrationId = "TIER-PROBE";
    init.registry.shipRole = ShipRoleType::Civilian;

    const glm::dvec3 position =
        anchor + forward * 900.0 + up * 120.0;
    const glm::dvec3 velocity =
        forward * 60.0;

    m_shipTierTransitionProbeId =
        spawnTrafficShell(
            m_activeCelestialSystemId,
            descriptor,
            position,
            velocity,
            init,
            orientationFromVelocity(velocity)
        );

    if (m_shipTierTransitionProbeId.value == 0)
        return;

    m_shipTierTransitionStartSeconds =
        m_serverTimelineClock.timeSeconds();
    m_shipTierTransitionStage = 0;

    std::cout
        << "[ShipTierTest] stage=traffic_shell id="
        << m_shipTierTransitionProbeId.value
        << " distance_m=900 standalone=1\n";
}



void GameSimulation::updateShipTierTransitionTest()
{
    if (!m_shipTierTransitionTestRequested)
        return;

    if (m_shipTierTransitionProbeId.value == 0)
    {
        initializeShipTierTransitionTest();
        if (m_shipTierTransitionProbeId.value == 0)
            return;
    }

    const double elapsed =
        std::max(
            0.0,
            m_serverTimelineClock.timeSeconds() -
                m_shipTierTransitionStartSeconds
        );

    if (m_shipTierTransitionStage == 0 && elapsed >= 2.0)
    {
        if (materializeShipShell(m_shipTierTransitionProbeId))
        {
            m_shipTierTransitionStage = 1;
            std::cout
                << "[ShipTierTest] stage=materialized id="
                << m_shipTierTransitionProbeId.value
                << " preserved_entity_id=1\n";
        }
    }

    if (m_shipTierTransitionStage == 1 && elapsed >= 5.0)
    {
        if (ejectShipCockpitCapsule(m_shipTierTransitionProbeId))
        {
            m_shipTierTransitionStage = 2;
            std::cout
                << "[ShipTierTest] stage=derelict id="
                << m_shipTierTransitionProbeId.value
                << " formation_removed=1 ai_removed=1\n";
        }
    }

    if (m_shipTierTransitionStage == 2 && elapsed >= 8.0)
    {
        if (dematerializeShip(
                m_shipTierTransitionProbeId,
                game::simulation::ShipSimulationTier::WreckShell))
        {
            m_shipTierTransitionStage = 3;
            std::cout
                << "[ShipTierTest] stage=wreck_shell id="
                << m_shipTierTransitionProbeId.value
                << " preserved_entity_id=1\n";
        }
    }
}


void GameSimulation::updateFiveHundredShipHerdDiagnostics()
{
    if (!m_fiveHundredShipHerdRequested &&
        !m_fiveHundredMaterializedShipHerdRequested &&
        !m_fiveHundredFullMaterializedStressRequested)
        return;

    if (m_fiveHundredShipHerd.empty())
    {
        initializeFiveHundredShipHerd();
        return;
    }

    const double now = m_serverTimelineClock.timeSeconds();
    if (m_fiveHundredShipHerdLastLogSeconds >= 0.0 &&
        now - m_fiveHundredShipHerdLastLogSeconds < 0.50)
    {
        return;
    }
    m_fiveHundredShipHerdLastLogSeconds = now;

    const double elapsed =
        std::max(0.0, now - m_fiveHundredShipHerdStartSeconds);

    std::size_t live = 0;
    std::size_t shells = 0;
    std::size_t materialized = 0;
    std::size_t stateValid = 0;
    std::size_t active = 0;
    std::size_t prewarm = 0;
    std::size_t coarse = 0;
    std::size_t other = 0;
    std::size_t routeOnly = 0;
    std::size_t kinematic = 0;
    std::size_t response = 0;
    std::size_t fullControl = 0;
    double errorSum = 0.0;
    double maximumError = 0.0;
    double speedSum = 0.0;

    for (const auto& [id, member] : m_fiveHundredShipHerd)
    {
        const Ship* ship = getShip(id);
        const auto* shell = shipShell(id);
        if (!ship && !shell)
            continue;

        ++live;

        switch (shipMotionFidelity(id))
        {
            case game::simulation::ShipMotionFidelity::RouteOnly:
                ++routeOnly;
                break;
            case game::simulation::ShipMotionFidelity::Kinematic:
                ++kinematic;
                break;
            case game::simulation::ShipMotionFidelity::Response:
                ++response;
                break;
            case game::simulation::ShipMotionFidelity::FullControl:
                ++fullControl;
                break;
        }

        if (ship)
        {
            ++materialized;
            if (ship->core().state().valid())
                ++stateValid;
        }
        else
        {
            ++shells;
        }

        const auto target =
            sampleHerdRoute(
                m_fiveHundredShipHerdCenterMeters,
                member.flightIndex,
                member.slotInFlight,
                elapsed
            );
        const ShipTransform& actualTransform =
            ship ? ship->core().transform() : shell->transform;
        const glm::dvec3 actualPosition =
            world::coordinates::fullMeters(actualTransform.worldPosition);
        const double error =
            glm::length(actualPosition - target.positionMeters);
        errorSum += error;
        maximumError = std::max(maximumError, error);
        speedSum +=
            glm::length(actualTransform.motion.worldVelocityMps);

        if (!ship)
            continue;

        switch (activationExecutionMode(id))
        {
            case game::simulation::SimulationMode::Active:
                ++active;
                break;
            case game::simulation::SimulationMode::Prewarm:
                ++prewarm;
                break;
            case game::simulation::SimulationMode::Coarse:
                ++coarse;
                break;
            default:
                ++other;
                break;
        }
    }

    const double denominator =
        live > 0 ? static_cast<double>(live) : 1.0;
    const double meanError = errorSum / denominator;
    const double meanSpeed = speedSum / denominator;

    std::ofstream out("herd_500.csv", std::ios::app);
    if (out)
    {
        out
            << std::fixed << std::setprecision(6)
            << now << ','
            << live << ','
            << shells << ','
            << materialized << ','
            << stateValid << ','
            << routeOnly << ','
            << kinematic << ','
            << response << ','
            << fullControl << ','
            << active << ','
            << prewarm << ','
            << coarse << ','
            << other << ','
            << meanError << ','
            << maximumError << ','
            << meanSpeed << '\n';
    }

    std::cout
        << "[500Ships] entities=" << live
        << " shells=" << shells
        << " materialized=" << materialized
        << " motion={route:" << routeOnly
        << ",kin:" << kinematic
        << ",response:" << response
        << ",full:" << fullControl
        << "} mean_error_m=" << meanError
        << "\n";

    if (materialized != stateValid)
    {
        std::cerr
            << "[500Ships] invalid_materialized_state="
            << (materialized - stateValid)
            << " materialized=" << materialized
            << " shells=" << shells << '\n';
    }
}

bool GameSimulation::buildFiveHundredShipHerdGoal(
    EntityId id,
    NpcNavigationGoal& outGoal
) const
{
    const auto it = m_fiveHundredShipHerd.find(id);
    if (it == m_fiveHundredShipHerd.end() || !getShip(id))
        return false;

    const double elapsed =
        std::max(
            0.0,
            m_serverTimelineClock.timeSeconds() -
                m_fiveHundredShipHerdStartSeconds
        );
    const auto sample =
        sampleHerdRoute(
            m_fiveHundredShipHerdCenterMeters,
            it->second.flightIndex,
            it->second.slotInFlight,
            elapsed
        );

    outGoal = {};
    outGoal.revision =
        static_cast<std::uint64_t>(
            std::floor(elapsed * 10.0)
        ) + 1;
    outGoal.mode = NpcNavigationGoalMode::TrackFormationSlot;
    outGoal.desiredPositionSystemMeters = sample.positionMeters;
    outGoal.desiredVelocitySystemMps = sample.velocityMps;
    outGoal.positionResponsePerSecond = 0.45;
    outGoal.maximumPositionCorrectionSpeedMps = 55.0;
    outGoal.velocityResponsePerSecond = 0.90;
    outGoal.angularDampingPerSecond = 2.0;
    return true;
}


bool GameSimulation::beginUniverseTrajectoryDiagnostic(
    double startUniverseTimeSeconds
)
{
    m_universeDiagnosticTrajectories.begin(
        startUniverseTimeSeconds
    );

    bool playerEntered = false;
    std::size_t eligibleShipCount = 0;
    std::size_t seededShipCount = 0;
    bool rejectedEligibleShip = false;

    for (const auto& [id, shipPtr] : m_ships)
    {
        if (!shipPtr)
            continue;

        /*
            Every ship published while the accelerated universe timeline is
            active must have a transform from the same timeline revision.
            Diagnostic/render probes are not exempt: keeping a visible Hub
            Motion Lab ship on the frozen production branch while hubs and
            celestial bodies advance would create a mixed-epoch snapshot.
        */
        const auto& tr = shipPtr->core().transform();
        const auto& motion = tr.motion;

        if (motion.mode ==
            game::navigation::MotionMode::Destroyed)
        {
            continue;
        }

        ++eligibleShipCount;

        std::string parentBodyId;
        const game::navigation::HubNavigationFrame* sourceHubFrame = nullptr;

        if (motion.systemId < 0)
        {
            std::cerr
                << "[UniverseDiagnosticTrajectory] rejected ship=" << id
                << " epoch=" << startUniverseTimeSeconds
                << " reason=missing_system_membership\n";

            rejectedEligibleShip = true;
            continue;
        }

        if (motion.systemId != m_activeCelestialSystemId)
        {
            std::cerr
                << "[UniverseDiagnosticTrajectory] rejected ship=" << id
                << " epoch=" << startUniverseTimeSeconds
                << " reason=inactive_system_context"
                << " ship_system=" << motion.systemId
                << " active_system=" << m_activeCelestialSystemId
                << "\n";

            rejectedEligibleShip = true;
            continue;
        }

        if (!motion.hubId.empty())
        {
            sourceHubFrame = hubNavigationFrame(motion.hubId);

            if (sourceHubFrame && sourceHubFrame->valid)
            {
                if (sourceHubFrame->systemId != motion.systemId)
                {
                    std::cerr
                        << "[UniverseDiagnosticTrajectory] rejected ship=" << id
                        << " epoch=" << startUniverseTimeSeconds
                        << " reason=hub_system_mismatch"
                        << " ship_system=" << motion.systemId
                        << " hub_system=" << sourceHubFrame->systemId
                        << "\n";

                    rejectedEligibleShip = true;
                    continue;
                }

                parentBodyId = sourceHubFrame->parentBodyId;
            }
        }

        if (parentBodyId.empty())
            parentBodyId = motion.primaryGravityBodyId;

        if (parentBodyId.empty())
            parentBodyId = motion.parentBodyId;

        /*
            Diagnostic entry must be self-contained. A freshly materialized
            visible ship may not have cached its navigation primary yet even
            though the current gravity field can resolve one immediately.
            Never cancel the entire user-visible fast-universe mode merely
            because that cache has not been populated by a prior gameplay tick.
        */
        if (parentBodyId.empty())
        {
            const auto gravitySample =
                game::navigation::GravityFieldSystem::sample(
                    tr.fullWorldMeters(),
                    m_gravityBodies
                );

            parentBodyId = gravitySample.primaryBodyId;
        }

        const auto gravityIt =
            m_celestialBodyGravityParameters.find(parentBodyId);

        if (gravityIt ==
                m_celestialBodyGravityParameters.end() ||
            gravityIt->second.gravitationalParameterM3s2 <= 0.0 ||
            gravityIt->second.radiusMeters <= 0.0)
        {
            std::cerr
                << "[UniverseDiagnosticTrajectory] rejected ship=" << id
                << " epoch=" << startUniverseTimeSeconds
                << " reason=missing_parent_gravity"
                << " parent=" << parentBodyId << "\n";

            rejectedEligibleShip = true;
            continue;
        }

        /*
            Capture one coherent production epoch. Local tactical motion is
            canonical in the ship-owned travel frame, so reconstruct its world
            seed through that frame (including omega x r). The external hub is
            metadata/reference only; all other modes use their production
            world transform directly.
        */
        const auto parentPositionIt =
            m_celestialBodyPositionsAu.find(parentBodyId);

        const auto parentVelocityIt =
            m_celestialBodyVelocitiesMetersPerSecond.find(
                parentBodyId
            );

        if (parentPositionIt ==
                m_celestialBodyPositionsAu.end() ||
            parentVelocityIt ==
                m_celestialBodyVelocitiesMetersPerSecond.end())
        {
            std::cerr
                << "[UniverseDiagnosticTrajectory] rejected ship=" << id
                << " epoch=" << startUniverseTimeSeconds
                << " reason=incomplete_parent_kinematics"
                << " parent=" << parentBodyId << "\n";

            rejectedEligibleShip = true;
            continue;
        }

        const glm::dvec3 parentCenterMeters =
            parentPositionIt->second *
            world::celestial::MetersPerAu;

        const glm::dvec3 parentVelocityMps =
            parentVelocityIt->second;

        const bool seedFromLocalTravelFrame =
            motion.travelFrame.valid &&
            motion.travelFrame.systemId == motion.systemId &&
            motion.mode ==
                game::navigation::MotionMode::HubTactical;

        const glm::dvec3 shipWorldPositionMeters =
            seedFromLocalTravelFrame
                ? motion.travelFrame.localToWorldPosition(
                    motion.localPositionMeters
                )
                : tr.fullWorldMeters();

        const glm::dvec3 shipWorldVelocityMps =
            seedFromLocalTravelFrame
                ? motion.travelFrame.localToWorldVelocity(
                    motion.localPositionMeters,
                    motion.localVelocityMps
                )
                : motion.worldVelocityMps;

        const glm::dvec3 relativePositionMeters =
            shipWorldPositionMeters - parentCenterMeters;

        const glm::dvec3 relativeVelocityMps =
            shipWorldVelocityMps - parentVelocityMps;

        const double radiusMeters =
            glm::length(relativePositionMeters);

        const bool finiteSeed =
            std::isfinite(startUniverseTimeSeconds) &&
            isFiniteVector(parentCenterMeters) &&
            isFiniteVector(parentVelocityMps) &&
            isFiniteVector(shipWorldPositionMeters) &&
            isFiniteVector(shipWorldVelocityMps) &&
            isFiniteVector(relativePositionMeters) &&
            isFiniteVector(relativeVelocityMps) &&
            std::isfinite(radiusMeters);

        if (!finiteSeed ||
            radiusMeters <= gravityIt->second.radiusMeters)
        {
            std::cerr
                << "[UniverseDiagnosticTrajectory] rejected ship=" << id
                << " epoch=" << startUniverseTimeSeconds
                << " reason=invalid_seed"
                << " parent=" << parentBodyId
                << " radius_m=" << radiusMeters
                << " parent_radius_m="
                << gravityIt->second.radiusMeters
                << "\n";

            rejectedEligibleShip = true;
            continue;
        }

        game::simulation::UniverseDiagnosticTrajectoryState state;
        state.systemId = motion.systemId;
        state.parentBodyId = parentBodyId;
        state.hubId = motion.hubId;
        state.relativePositionMeters = relativePositionMeters;
        state.relativeVelocityMps = relativeVelocityMps;
        state.epochUniverseTimeSeconds = startUniverseTimeSeconds;

        if (!m_universeDiagnosticTrajectories.add(
                id,
                std::move(state)))
        {
            rejectedEligibleShip = true;
            continue;
        }

        ++seededShipCount;

        const double relativeSpeedMps =
            glm::length(relativeVelocityMps);

        const double circularSpeedMps =
            std::sqrt(
                gravityIt->second.gravitationalParameterM3s2 /
                radiusMeters
            );

        std::cout
            << "[UniverseDiagnosticTrajectory] seed ship=" << id
            << " epoch=" << startUniverseTimeSeconds
            << " parent=" << parentBodyId
            << " radius_m=" << radiusMeters
            << " altitude_m="
            << radiusMeters - gravityIt->second.radiusMeters
            << " relative_speed_mps=" << relativeSpeedMps
            << " circular_speed_mps=" << circularSpeedMps
            << " speed_error_mps="
            << relativeSpeedMps - circularSpeedMps
            << "\n";

        if (id == m_playerId)
            playerEntered = true;
    }

    const bool completeBranch =
        playerEntered &&
        !rejectedEligibleShip &&
        seededShipCount == eligibleShipCount;

    if (!completeBranch)
    {
        std::cerr
            << "[UniverseDiagnosticTrajectory] activation rejected"
            << " eligible_ships=" << eligibleShipCount
            << " seeded_ships=" << seededShipCount
            << " player_seeded=" << (playerEntered ? 1 : 0)
            << "\n";

        m_universeDiagnosticTrajectories.discard();
        return false;
    }

    return true;
}

void GameSimulation::endUniverseTrajectoryDiagnostic()
{
    if (!m_universeDiagnosticTrajectories.active())
        return;

    std::cout
        << "[UniverseDiagnosticTrajectory] discard branch ships="
        << m_universeDiagnosticTrajectories.size()
        << "\n";

    /*
        Transaction boundary: accelerated trajectory results are never copied
        back into production ShipTransform/DynamicMotionState. The production
        branch was frozen while this branch existed, so rollback is exactly a
        discard operation for every player/NPC ship in the session.
    */
    m_universeDiagnosticTrajectories.discard();
}

void GameSimulation::advanceUniverseTrajectoryDiagnostic(
    double universeDeltaSeconds
)
{
    if (!m_universeDiagnosticTrajectories.active())
        return;

    constexpr double MaxSubstepSeconds = 5.0;
    constexpr int MaxSubsteps = 512;

    const double absDelta =
        std::abs(universeDeltaSeconds);

    const int substeps =
        absDelta > 0.000001
        ? std::clamp(
            static_cast<int>(
                std::ceil(absDelta / MaxSubstepSeconds)
            ),
            1,
            MaxSubsteps
        )
        : 0;

    const double substepDelta =
        substeps > 0
        ? universeDeltaSeconds /
            static_cast<double>(substeps)
        : 0.0;

    for (auto& [id, state] :
         m_universeDiagnosticTrajectories.states())
    {
        (void)id;

        const auto gravityIt =
            m_celestialBodyGravityParameters.find(
                state.parentBodyId
            );

        if (gravityIt ==
            m_celestialBodyGravityParameters.end())
        {
            continue;
        }

        const double gravitationalParameter =
            gravityIt->second.gravitationalParameterM3s2;

        for (int step = 0; step < substeps; ++step)
        {
            integratePassiveTrajectoryStep(
                state.relativePositionMeters,
                state.relativeVelocityMps,
                gravitationalParameter,
                substepDelta
            );
        }
    }
}

bool GameSimulation::applyDiagnosticTrajectoryTransform(
    EntityId shipId,
    ShipTransform& transform
) const
{
    const auto* state =
        m_universeDiagnosticTrajectories.find(shipId);

    if (!state ||
        state->systemId != m_activeCelestialSystemId)
    {
        return false;
    }

    const auto parentPositionIt =
        m_celestialBodyPositionsAu.find(
            state->parentBodyId
        );

    const auto parentVelocityIt =
        m_celestialBodyVelocitiesMetersPerSecond.find(
            state->parentBodyId
        );

    const auto gravityIt =
        m_celestialBodyGravityParameters.find(
            state->parentBodyId
        );

    if (parentPositionIt == m_celestialBodyPositionsAu.end() ||
        parentVelocityIt == m_celestialBodyVelocitiesMetersPerSecond.end() ||
        gravityIt == m_celestialBodyGravityParameters.end())
    {
        return false;
    }

    const glm::dvec3 parentCenterMeters =
        parentPositionIt->second *
        world::celestial::MetersPerAu;

    const glm::dvec3 parentVelocityMps =
        parentVelocityIt->second;

    const glm::dvec3 worldPositionMeters =
        parentCenterMeters +
        state->relativePositionMeters;

    const glm::dvec3 worldVelocityMps =
        parentVelocityMps +
        state->relativeVelocityMps;

    transform.setWorldPositionMeters(
        worldPositionMeters
    );

    auto& motion = transform.motion;
    motion.mode = game::navigation::MotionMode::PassiveTrajectory;
    motion.systemId = state->systemId;
    motion.parentBodyId = state->parentBodyId;
    motion.hubId = state->hubId;
    motion.worldVelocityMps = worldVelocityMps;
    motion.mainEngineAccelerationMps2 = glm::dvec3(0.0);
    motion.manoeuvreAccelerationMps2 = glm::dvec3(0.0);
    motion.engineAccelerationMps2 = glm::dvec3(0.0);
    motion.desiredTacticalVelocityMps = glm::dvec3(0.0);
    motion.desiredRelativeVelocityMps = glm::dvec3(0.0);

    const glm::dvec3 relativeAcceleration =
        passiveTrajectoryAcceleration(
            state->relativePositionMeters,
            gravityIt->second.gravitationalParameterM3s2
        );

    const double distanceMeters =
        glm::length(
            state->relativePositionMeters
        );

    motion.gravityAccelerationMps2 = relativeAcceleration;
    motion.primaryGravityBodyId = state->parentBodyId;
    motion.primaryGravityDistanceMeters = distanceMeters;
    motion.primaryGravityAltitudeMeters =
        distanceMeters - gravityIt->second.radiusMeters;
    motion.primaryGravityAccelerationMps2 =
        glm::length(relativeAcceleration);

    const auto* frame =
        hubNavigationFrame(state->hubId);

    if (frame &&
        frame->valid &&
        frame->systemId == state->systemId)
    {
        motion.localPositionMeters =
            frame->worldToLocalPosition(
                worldPositionMeters
            );

        motion.localVelocityMps =
            frame->worldToLocalVelocity(
                worldPositionMeters,
                worldVelocityMps
            );

        motion.referenceVelocityMps =
            frame->localToWorldVelocity(
                motion.localPositionMeters,
                glm::dvec3(0.0)
            );

        transform.referenceVelocityMetersPerSecond =
            motion.referenceVelocityMps;
    }
    else
    {
        motion.referenceVelocityMps = parentVelocityMps;
        transform.referenceVelocityMetersPerSecond = parentVelocityMps;
    }

    return true;
}

ShipTransform GameSimulation::presentationShipTransform(
    EntityId shipId
) const
{
    const Ship* ship = getShip(shipId);
    if (!ship)
        return ShipTransform{};

    ShipTransform transform =
        ship->core().transform();

    if (m_universeDiagnosticTrajectories.active())
    {
        applyDiagnosticTrajectoryTransform(
            shipId,
            transform
        );
    }

    return transform;
}



void GameSimulation::updateHubMotionLabActors()
{
    if (m_hubMotionLabShips.empty())
        return;

    glm::dvec3 playerLocalPositionMeters {0.0};
    glm::dvec3 playerLocalVelocityMetersPerSecond {0.0};

    if (const Ship* player = playerShip())
    {
        const auto& playerMotion =
            player->core().transform().motion;

        if (playerMotion.mode ==
                game::navigation::MotionMode::HubTactical &&
            playerMotion.hubId ==
                game::diagnostics::HubMotionLabHubId)
        {
            playerLocalPositionMeters =
                playerMotion.localPositionMeters;
            playerLocalVelocityMetersPerSecond =
                playerMotion.localVelocityMps;
        }
    }

    const double t = m_serverTimelineClock.timeSeconds();

    for (const auto& [shipId, registration] :
         m_hubMotionLabShips)
    {
        Ship* ship = getShip(shipId);
        if (!ship)
            continue;

        const auto* frame =
            hubNavigationFrame(registration.hubId);

        if (!frame || !frame->valid)
            continue;

        const auto localState =
            game::diagnostics::evaluateHubMotionLabActor(
                registration.kind,
                t,
                playerLocalPositionMeters,
                playerLocalVelocityMetersPerSecond
            );

        auto& tr = ship->core().transform();

        tr.motion.mode =
            game::navigation::MotionMode::HubTactical;
        tr.motion.systemId = frame->systemId;
        tr.motion.hubId = registration.hubId;
        tr.motion.parentBodyId = frame->parentBodyId;
        game::navigation::TravelFrameSystem::matchToReference(
            tr.motion,
            frame->kinematicFrame(),
            "ship_travel_" + std::to_string(shipId.value),
            frame->hubId
        );
        tr.motion.localPositionMeters =
            localState.positionMeters;
        tr.motion.localVelocityMps =
            localState.velocityMetersPerSecond;
        tr.motion.referenceVelocityMps =
            tr.motion.travelFrame.localToWorldVelocity(
                localState.positionMeters,
                glm::dvec3(0.0)
            );
        tr.motion.worldVelocityMps =
            tr.motion.travelFrame.localToWorldVelocity(
                localState.positionMeters,
                localState.velocityMetersPerSecond
            );

        tr.referenceVelocityMetersPerSecond =
            tr.motion.referenceVelocityMps;

        tr.setWorldPositionMeters(
            tr.motion.travelFrame.localToWorldPosition(
                localState.positionMeters
            )
        );

        const glm::dvec3 relativeWorldVelocity =
            frame->localToWorldVector(
                localState.velocityMetersPerSecond
            );

        if (glm::length(relativeWorldVelocity) > 0.001)
        {
            tr.orientation =
                makePromoLookOrientation(
                    glm::normalize(glm::vec3(relativeWorldVelocity)),
                    glm::vec3(frame->radialAxis)
                );
        }
    }
}



void GameSimulation::updateInterplanetaryTransferLabActor()
{
    if (m_interplanetaryTransferLabShipId.value == 0 ||
        m_activeCelestialSystemId != 0)
    {
        return;
    }

    Ship* ship = getShip(m_interplanetaryTransferLabShipId);
    if (!ship)
        return;

    const auto sunPositionIt =
        m_celestialBodyPositionsAu.find(
            game::diagnostics::InterplanetaryTransferLabSunBodyId
        );
    const auto sunVelocityIt =
        m_celestialBodyVelocitiesMetersPerSecond.find(
            game::diagnostics::InterplanetaryTransferLabSunBodyId
        );
    const auto marsPositionIt =
        m_celestialBodyPositionsAu.find(
            game::diagnostics::InterplanetaryTransferLabMarsBodyId
        );

    if (sunPositionIt == m_celestialBodyPositionsAu.end() ||
        sunVelocityIt ==
            m_celestialBodyVelocitiesMetersPerSecond.end() ||
        marsPositionIt == m_celestialBodyPositionsAu.end())
    {
        return;
    }

    const double transferElapsedSinceSpawnSeconds = std::max(
        0.0,
        m_orbitalUniverseTimeSeconds -
            m_interplanetaryTransferLabStartUniverseTimeSeconds
    );

    if (!m_interplanetaryTransferLabDeparturePhaseInitialized)
    {
        constexpr double twoPi = 6.28318530717958647692;
        const glm::dvec3 currentMarsRelativeAu =
            marsPositionIt->second - sunPositionIt->second;
        const double currentMarsPhaseRad = std::atan2(
            -currentMarsRelativeAu.z,
            currentMarsRelativeAu.x
        );
        const double syntheticElapsedSeconds =
            game::diagnostics::InterplanetaryInitialElapsedDays *
                game::diagnostics::InterplanetarySecondsPerDay +
            transferElapsedSinceSpawnSeconds;
        const double marsAngularVelocityRadPerSecond =
            twoPi /
            (game::diagnostics::InterplanetaryMarsOrbitPeriodDays *
             game::diagnostics::InterplanetarySecondsPerDay);

        // The actor is spawned partway through the transfer. Recover the Mars
        // phase at that synthetic departure epoch so the diagnostic ellipse is
        // attached to the real Sol-system phase instead of the global +X axis.
        m_interplanetaryTransferLabDeparturePhaseRad =
            currentMarsPhaseRad -
            marsAngularVelocityRadPerSecond * syntheticElapsedSeconds;
        m_interplanetaryTransferLabDeparturePhaseInitialized = true;
    }

    const auto state =
        game::diagnostics::evaluateInterplanetaryTransferLab(
            transferElapsedSinceSpawnSeconds,
            m_interplanetaryTransferLabDeparturePhaseRad
        );

    const glm::dvec3 sunMeters =
        sunPositionIt->second * world::celestial::MetersPerAu;
    const glm::dvec3 worldVelocityMetersPerSecond =
        sunVelocityIt->second +
        state.relativeVelocityMetersPerSecond;

    auto& tr = ship->core().transform();
    tr.setWorldPositionMeters(
        sunMeters + state.relativePositionMeters
    );

    tr.motion.mode =
        game::navigation::MotionMode::PassiveTrajectory;
    tr.motion.systemId = 0;
    tr.motion.parentBodyId =
        game::diagnostics::InterplanetaryTransferLabSunBodyId;
    tr.motion.hubId.clear();
    tr.motion.worldVelocityMps =
        worldVelocityMetersPerSecond;
    tr.motion.referenceVelocityMps = sunVelocityIt->second;
    tr.motion.localPositionMeters =
        state.relativePositionMeters;
    tr.motion.localVelocityMps =
        state.relativeVelocityMetersPerSecond;
    tr.motion.mainEngineAccelerationMps2 = glm::dvec3(0.0);
    tr.motion.manoeuvreAccelerationMps2 = glm::dvec3(0.0);
    tr.motion.engineAccelerationMps2 = glm::dvec3(0.0);
    tr.motion.desiredTacticalVelocityMps = glm::dvec3(0.0);
    tr.motion.desiredRelativeVelocityMps = glm::dvec3(0.0);
    tr.referenceVelocityMetersPerSecond = sunVelocityIt->second;

    if (state.heliocentricSpeedMetersPerSecond > 0.001)
    {
        tr.orientation = makePromoLookOrientation(
            glm::normalize(
                glm::vec3(state.relativeVelocityMetersPerSecond)
            ),
            glm::vec3(0.0f, 1.0f, 0.0f)
        );
    }
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
            static_cast<float>(m_serverTimelineClock.timeSeconds()),
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
































void GameSimulation::debugLogGravitySample(const Ship& ship)
{
    auto& row =
        m_diagnostics.simulation.gravityRows;

    if (row >= 1200)
        return;

    const auto& tr =
        ship.core().transform();

    std::ofstream out(
        "gravity_debug.csv",
        row == 0
            ? std::ios::out
            : std::ios::app
    );

    if (!out.is_open())
        return;

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

    const double gravityMag =
        glm::length(
            tr.motion.gravityAccelerationMps2
        );

    const glm::dvec3 positionMeters =
        world::coordinates::fullMeters(
            tr.worldPosition
        );

    out
        << row << ","
        << positionMeters.x << ","
        << positionMeters.y << ","
        << positionMeters.z << ","

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


void GameSimulation::debugLogServerNavState(double dt)
{
    auto& state =
        m_diagnostics.simulation;

    auto& row =
        state.serverNavigationRows;

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

    bool& hubOrbitDebugInitialized =
        state.serverNavigationHubOrbitInitialized;
    glm::dvec3& hubStartM =
        state.serverNavigationHubStartMeters;
    glm::dvec3& hubStartRadial =
        state.serverNavigationHubStartRadial;
    double& hubStartAngleDeg =
        state.serverNavigationHubStartAngleDeg;

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
    auto& row =
        m_diagnostics.simulation.playerMotionRows;

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
                tr.motion.localPositionMeters,
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
    auto& state =
        m_diagnostics.simulation;

    auto& row =
        state.hubPlayerChainRows;

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

    bool& havePrev =
        state.hubPlayerChainHasPreviousSample;
    glm::dvec3& prevHubM =
        state.hubPlayerChainPreviousHubMeters;
    glm::dvec3& prevPlayerLocal =
        state.hubPlayerChainPreviousPlayerLocalMeters;
    glm::dvec3& prevStationLocal =
        state.hubPlayerChainPreviousStationLocalMeters;

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
            playerM,
            tr.motion.worldVelocityMps
        );

    const glm::dvec3 rotatingVelocityAtPlayer =
        glm::cross(
            frame->angularVelocityWorldRadPerSecond,
            playerM - frame->originMeters
        );

    const glm::dvec3 predictedRelativeLocalV =
        frame->worldToLocalVector(
            relativeWorldV - rotatingVelocityAtPlayer
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
