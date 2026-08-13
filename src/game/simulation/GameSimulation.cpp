#include "GameSimulation.h"
#include "src/game/navigation/HubFrameBasis.h"
#include "src/game/simulation/RuntimeSystemPolicy.h"
#include "src/game/simulation/activation/ActivationSpatialIndex.h"
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
#include "src/game/RuntimeFeatureFlags.h"
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
#include "src/game/navigation/TravelFrameSystem.h"

namespace
{
    glm::dvec3 passiveTrajectoryAcceleration(
        const glm::dvec3& relativePositionMeters,
        double gravitationalParameterM3s2
    )
    {
        const double radiusSquared =
            glm::length2(relativePositionMeters);

        if (gravitationalParameterM3s2 <= 0.0 ||
            radiusSquared <= 1.0)
        {
            return glm::dvec3(0.0);
        }

        const double radius =
            std::sqrt(radiusSquared);

        return
            -gravitationalParameterM3s2 *
            relativePositionMeters /
            (radiusSquared * radius);
    }

    bool isFiniteVector(const glm::dvec3& value)
    {
        return
            std::isfinite(value.x) &&
            std::isfinite(value.y) &&
            std::isfinite(value.z);
    }

    void integratePassiveTrajectoryStep(
        glm::dvec3& relativePositionMeters,
        glm::dvec3& relativeVelocityMps,
        double gravitationalParameterM3s2,
        double dt
    )
    {
        const glm::dvec3 accelerationBefore =
            passiveTrajectoryAcceleration(
                relativePositionMeters,
                gravitationalParameterM3s2
            );

        const glm::dvec3 halfStepVelocity =
            relativeVelocityMps +
            accelerationBefore * (0.5 * dt);

        relativePositionMeters +=
            halfStepVelocity * dt;

        const glm::dvec3 accelerationAfter =
            passiveTrajectoryAcceleration(
                relativePositionMeters,
                gravitationalParameterM3s2
            );

        relativeVelocityMps =
            halfStepVelocity +
            accelerationAfter * (0.5 * dt);
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

        // Only authoritative per-instance state crosses the replication
        // boundary. Static module metadata is available from the same local
        // descriptor library on both server and client.
        ms.moduleId = descMod.moduleId;

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

GameSimulation::GameSimulation(
    game::diagnostics::ServerDiagnostics& diagnostics
)
    : m_diagnostics(diagnostics)
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




void GameSimulation::buildInitialScene(
    const game::world_state::InitialWorldState& initialState
)
{
    m_playerId =
        game::scene::buildInitialScene(*this, initialState);

    if constexpr (game::promo::PromoFlybyScenario::Enabled)
    {
        m_promoFlybyScenario.setup(*this);
    }
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

                const glm::dvec3 rotatedOffset =
                    obj.inheritHubOrientation && hubFrame && hubFrame->valid
                        ? game::navigation::hubVisualLocalToWorldVector(
                            hubFrame->progradeAxis,
                            hubFrame->radialAxis,
                            hubFrame->normalAxis,
                            obj.hubLocalOffsetMeters
                        )
                        : obj.hubLocalOffsetMeters;

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
                }

                obj.linearVelocity =
                    glm::vec3(objectVelocityMetersPerSecond);

                if (obj.inheritHubOrientation)
                {
                    if (hubFrame && hubFrame->valid)
                    {
                        obj.orientation =
                            game::navigation::hubAttachedVisualOrientation(
                                hubFrame->progradeAxis,
                                hubFrame->radialAxis,
                                hubFrame->normalAxis,
                                obj.hubLocalRotationDeg
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



    // === 1. AI / controls / attitude ===
    if (!trajectoryDebugMode)
    {
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

            Ship& ship = *shipPtr;
            if (id == m_playerId)
                continue;

            // Stage 3E is the first production consumer of the activation
            // plan, but only for NPC tactical AI cadence. Physics, control
            // application, HubTactical integration, signals and snapshots
            // remain full-rate until coarse/scheduled motion exists.
            game::simulation::SimulationMode executionMode =
                game::simulation::SimulationMode::Active;

            if constexpr (game::runtime::ActivationNpcAiCadenceEnabled)
            {
                const auto plannerIt =
                    m_activationPlannerDecisions.find(id);

                if (plannerIt != m_activationPlannerDecisions.end())
                {
                    executionMode =
                        plannerIt->second.planUpdate.plannedMode;
                }
            }

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

            ShipControlState aiControl =
                m_npcAiSystem.computeControl(
                    ship,
                    static_cast<float>(cadence.thinkDeltaSeconds)
                );
            ship.setControlState(aiControl);
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

            Ship& ship = *shipPtr;
            ship.updatePhysics(fdt, m_world);
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

            const auto& control = shipPtr->core().control();

            game::navigation::DynamicMotionSystem::applyLocalFrameInput(
                tr.motion,
                tr.motion.travelFrame,
                shipPtr->core().desc().physics,
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
        updateShipReferenceFrames(dt);
        rebuildNavigationGravityContext();
        updateDynamicNavigationContext(dt);

        for (auto& [id, shipPtr] : m_ships)
        {
            if (!shipPtr)
                continue;

            if (isHubMotionLabShip(id))
                continue;

            auto& tr = shipPtr->core().transform();

            if (tr.motion.mode !=
                game::navigation::MotionMode::HubTactical)
            {
                continue;
            }

            if (!tr.motion.travelFrame.valid)
                continue;

            game::navigation::DynamicMotionSystem::updateLocalFrameMotion(
                tr.motion,
                tr.worldPosition,
                tr.motion.travelFrame,
                shipPtr->core().desc().physics,
                dt
            );

            tr.referenceVelocityMetersPerSecond =
                tr.motion.referenceVelocityMps;
        }

        updateDynamicNavigationContext(dt);

        // Controlled server-time samples for the Hub Motion Lab. They are
        // excluded from production AI/physics above, then published through
        // the ordinary authoritative ShipSnapshot path below.
        updateHubMotionLabActors();

        // Stage 3E keeps the 5 Hz physical planner, but NPC tactical AI above
        // is now allowed to consume the stabilized planned mode as a think
        // cadence. Physics, signals and snapshots still ignore plannedMode.
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
        s.typeId = ship.core().desc().typeId;
        s.role = ship.core().role();
        s.motionLabKind = hubMotionLabActorKind(id);

        s.transform = tr;

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

        const bool motionLabProbe = isHubMotionLabShip(id);
        auto resendIt = m_shipGraphPayloadPublicationsRemaining.find(id);
        const bool sendStructuralGraph =
            !motionLabProbe &&
            (m_initializedShipGraphIds.find(id) == m_initializedShipGraphIds.end() ||
             resendIt != m_shipGraphPayloadPublicationsRemaining.end());

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

            if (resendIt != m_shipGraphPayloadPublicationsRemaining.end())
            {
                --resendIt->second;
                if (resendIt->second <= 0)
                    m_shipGraphPayloadPublicationsRemaining.erase(resendIt);
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

        // Motion-lab probes exercise only authoritative transform -> snapshot
        // -> client presentation. Shipping their complete damage/repair graph
        // multiplies startup snapshot memory without testing anything useful.
        if (!motionLabProbe)
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

                // Static descriptor data stays local to each runtime. The
                // replicated module row contains only mutable instance state.
                ms.moduleId = descMod.moduleId;

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
            !(id == m_playerId) && !isHubMotionLabShip(id);
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

        // Production systems are still fully simulated, so currentMode remains
        // truthful Active. Stage 3D keeps a separate persistent plannedMode
        // with demotion hysteresis; no AI/physics/snapshot loop consumes it yet.
        ActivationSpatialQueryResult broadphaseQuery;
        if (!(id == m_playerId))
        {
            broadphaseQuery = spatialIndex.query(
                tr.motion.systemId,
                subject,
                m_activationInteractionPolicy
            );
        }

        auto physicalDecision =
            evaluateActivationShadowCandidates(
                id,
                tr.motion.systemId,
                subject,
                game::simulation::SimulationMode::Active,
                id == m_playerId,
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
    markShipGraphDirty(id);

    return id;
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
    double gravitationalParameterM3s2
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

    m_celestialBodyGravityParameters[bodyId] =
        parameters;
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


void GameSimulation::rebuildHubNavigationFrames(double frameDeltaSeconds)
{
    // Keep the previous kinematic epoch long enough to derive frame
    // acceleration. The current production motion path still consumes the
    // legacy HubNavigationFrame fields; acceleration is shadow state only in
    // this migration stage.
    const auto previousFrames = m_hubNavigationFrames;
    m_hubNavigationFrames.clear();

    for (auto& [hubId, hub] : m_orbitalHubs)
    {
        if (hub.systemId != m_activeCelestialSystemId)
            continue;
        game::navigation::HubNavigationFrame frame;

        frame.systemId = hub.systemId;
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


        




        /*
            frame.velocityMetersPerSecond — полная мировая скорость хаба:

                parent planet world velocity
                +
                hub orbital velocity relative to planet.

            Для построения orbital frame нужна только скорость
            хаба относительно родительской планеты.

            Иначе движение планеты вокруг звезды поворачивает
            prograde/normal и Hub Map получает другую плоскость орбиты,
            чем Planet Details.
        */
        glm::dvec3 parentVelocityMetersPerSecond {
            0.0
        };

        bool parentVelocityResolved =
            false;

        const auto parentVelocityIt =
            m_celestialBodyVelocitiesMetersPerSecond.find(
                hub.parentBodyId
            );

        if (parentVelocityIt !=
            m_celestialBodyVelocitiesMetersPerSecond.end())
        {
            parentVelocityMetersPerSecond =
                parentVelocityIt->second;

            parentVelocityResolved =
                true;
        }

        glm::dvec3 relativeOrbitalVelocityMetersPerSecond;

        if (parentVelocityResolved)
        {
            relativeOrbitalVelocityMetersPerSecond =
                frame.velocityMetersPerSecond -
                parentVelocityMetersPerSecond;
        }
        else
        {
            /*
                При стартовой инициализации velocity cache теоретически
                может быть ещё не заполнен. Аналитическая локальная
                орбитальная скорость остаётся корректным fallback.
            */
            relativeOrbitalVelocityMetersPerSecond =
                world::orbits::
                    computeOrbitVelocityMetersPerSecond(
                        hub.motion,
                        m_orbitalUniverseTimeSeconds
                    );
        }

        glm::dvec3 prograde =
            safeNormalizeD(
                relativeOrbitalVelocityMetersPerSecond,
                glm::dvec3(1.0, 0.0, 0.0)
            );

            /*
                Убираем возможную радиальную составляющую.
                Для круговой орбиты она практически нулевая,
                но frame должен оставаться ортогональным.
            */
            prograde =
                safeNormalizeD(
                    prograde -
                        radial *
                        glm::dot(
                            prograde,
                            radial
                        ),

                    glm::dvec3(
                        1.0,
                        0.0,
                        0.0
                    )
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

        const double orbitRadiusMeters =
            glm::length(frame.originMeters - parentMeters);

        const glm::dvec3 tangentialVelocityMetersPerSecond =
            relativeOrbitalVelocityMetersPerSecond -
            radial *
                glm::dot(
                    relativeOrbitalVelocityMetersPerSecond,
                    radial
                );

        const double angularSpeedRadPerSecond =
            orbitRadiusMeters > 1.0
                ? glm::length(tangentialVelocityMetersPerSecond) /
                    orbitRadiusMeters
                : 0.0;

        /*
            Basis convention is X=prograde, Y=radial, Z=normal with
            normal = cross(prograde, radial). Therefore the orbital frame
            rotates around -normal for positive prograde motion.
        */
        frame.angularVelocityWorldRadPerSecond =
            -normal * angularSpeedRadPerSecond;

        const auto previousFrameIt = previousFrames.find(hubId);
        if (std::abs(frameDeltaSeconds) > 1.0e-9 &&
            previousFrameIt != previousFrames.end() &&
            previousFrameIt->second.valid &&
            previousFrameIt->second.systemId == frame.systemId)
        {
            frame.accelerationMetersPerSecond2 =
                (frame.velocityMetersPerSecond -
                 previousFrameIt->second.velocityMetersPerSecond) /
                frameDeltaSeconds;

            frame.angularAccelerationWorldRadPerSecond2 =
                (frame.angularVelocityWorldRadPerSecond -
                 previousFrameIt->second.angularVelocityWorldRadPerSecond) /
                frameDeltaSeconds;
        }
        else if (!m_gravityBodies.empty())
        {
            // First-epoch fallback: for an orbital hub the physical frame
            // acceleration is gravity-driven. Subsequent ticks use the actual
            // derivative of the authoritative frame velocity above.
            frame.accelerationMetersPerSecond2 =
                game::navigation::GravityFieldSystem::sample(
                    frame.originMeters,
                    m_gravityBodies
                ).accelerationMps2;
        }

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





        hub.orientation =
            game::navigation::hubVisualOrientation(
                frame.progradeAxis,
                frame.radialAxis,
                frame.normalAxis
            );









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
        if (hub.systemId != m_activeCelestialSystemId ||
            !hub.motion.enabled)
        {
            continue;
        }

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

        hub.worldPosition =
            world::coordinates::makeWorldPositionFromMeters(
                hubPosMeters
            );

        hub.orientation =
            glm::mat4(1.0f);
    }

    rebuildHubNavigationFrames(0.0);

    /*
        The Stage 3E live AI probe is a real visible NPC, not a presentation
        ghost. Give it a canonical hub-local production frame before the first
        authoritative update. This is also required by accelerated-universe
        diagnostics: every visible ship must be able to seed the alternate
        trajectory branch from one coherent production epoch.
    */
    if constexpr (game::diagnostics::ActivationCadenceLabEnabled)
    {
        if (m_activationCadenceLabShipId.value != 0)
        {
            game::navigation::ReferenceFrame labFrame;
            labFrame.type =
                game::navigation::ReferenceFrameType::OrbitalHub;
            labFrame.systemId = m_activeCelestialSystemId;
            labFrame.hubId =
                game::diagnostics::ActivationCadenceLabHubId;
            labFrame.localOffsetMeters = glm::dvec3(
                game::diagnostics::ActivationCadenceLabLocalOffsetMeters,
                0.0,
                0.0
            );

            if (!placeShipInReferenceFrame(
                    m_activationCadenceLabShipId,
                    labFrame))
            {
                std::cerr
                    << "[ActivationCadenceLab] failed to place live AI probe "
                    << "in hub reference frame\n";
            }
        }
    }

    // Обновляем объекты, прикреплённые к хабам,
    // чтобы станция уже была в правильной мировой позиции
    // до первого snapshot.
    for (auto& [id, obj] : m_staticObjects)
    {
        if (obj.systemId != m_activeCelestialSystemId ||
            !obj.attachedToHub)
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

        const auto* hubFrame =
            hubNavigationFrame(obj.hubId);

        const glm::dvec3 rotatedOffset =
            obj.inheritHubOrientation && hubFrame && hubFrame->valid
                ? game::navigation::hubVisualLocalToWorldVector(
                    hubFrame->progradeAxis,
                    hubFrame->radialAxis,
                    hubFrame->normalAxis,
                    obj.hubLocalOffsetMeters
                )
                : obj.hubLocalOffsetMeters;

        obj.setWorldPositionMeters(
            hubMeters + rotatedOffset
        );

        if (obj.inheritHubOrientation)
        {
            if (hubFrame && hubFrame->valid)
            {
                obj.orientation =
                    game::navigation::hubAttachedVisualOrientation(
                        hubFrame->progradeAxis,
                        hubFrame->radialAxis,
                        hubFrame->normalAxis,
                        obj.hubLocalRotationDeg
                    );
            }
        }
    }

    rebuildNavigationGravityContext();
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





bool GameSimulation::resolveCelestialBodyMeters(
    int systemId,
    const std::string& bodyId,
    glm::dvec3& outCenterMeters,
    double& outRadiusMeters
) const
{
    if (systemId < 0 || systemId != m_activeCelestialSystemId)
        return false;

    auto it = m_celestialBodyPositionsAu.find(bodyId);

    if (it == m_celestialBodyPositionsAu.end())
        return false;

    outCenterMeters =
        it->second * world::celestial::MetersPerAu;

    const auto gravityIt =
        m_celestialBodyGravityParameters.find(bodyId);

    outRadiusMeters =
        gravityIt != m_celestialBodyGravityParameters.end()
        ? gravityIt->second.radiusMeters
        : 0.0;

    return true;
}








bool GameSimulation::resolveCelestialBodyVelocityMetersPerSecond(
    int systemId,
    const std::string& bodyId,
    glm::dvec3& outVelocityMetersPerSecond
) const
{
    if (systemId < 0 || systemId != m_activeCelestialSystemId)
    {
        outVelocityMetersPerSecond = glm::dvec3(0.0);
        return false;
    }

    const auto it =
        m_celestialBodyVelocitiesMetersPerSecond.find(
            bodyId
        );

    if (it ==
        m_celestialBodyVelocitiesMetersPerSecond.end())
    {
        outVelocityMetersPerSecond =
            glm::dvec3(0.0);

        return false;
    }

    outVelocityMetersPerSecond =
        it->second;

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

        if (frame.systemId >= 0 &&
            frame.systemId != hubFrame->systemId)
        {
            return result;
        }

        result.systemId = hubFrame->systemId;
        result.positionMeters =
            hubFrame->originMeters;

        result.velocityMetersPerSecond =
            hubFrame->velocityMetersPerSecond;

        result.orientation =
            game::navigation::hubVisualOrientation(
                hubFrame->progradeAxis,
                hubFrame->radialAxis,
                hubFrame->normalAxis
            );

        result.positionMeters =
            hubFrame->localToWorldPosition(
                frame.localOffsetMeters
            );

        result.velocityMetersPerSecond =
            hubFrame->localToWorldVelocity(
                frame.localOffsetMeters,
                glm::dvec3(0.0)
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

    tr.motion.systemId = resolved.systemId;

    tr.motion.hubId =
        frame.hubId;

    if (hubFrame && hubFrame->valid)
    {
        const std::string ownedTravelFrameId =
            "ship_travel_" + std::to_string(shipId.value);

        game::navigation::TravelFrameSystem::matchToReference(
            tr.motion,
            hubFrame->kinematicFrame(),
            ownedTravelFrameId,
            hubFrame->hubId
        );
    }

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

        // External reference membership is authoritative only while the
        // ship-owned travel frame is explicitly matched to it. A detached
        // travel frame (future J transit) must not be silently pulled back to
        // the hub's system/domain by a stale binding.
        if (tr.motion.matchedToReferenceFrame)
            tr.motion.systemId = resolved.systemId;

        // Обновляем только скорость системы отсчёта.
        // Позицию свободного корабля не телепортируем.
        glm::dvec3 referenceVelocityMetersPerSecond =
            resolved.velocityMetersPerSecond;

        if (tr.motion.mode ==
                game::navigation::MotionMode::HubTactical)
        {
            const auto* hubFrame =
                hubNavigationFrame(tr.motion.hubId);

            if (hubFrame && hubFrame->valid)
            {
                if (tr.motion.travelFrame.valid)
                {
                    game::navigation::TravelFrameSystem::refreshMatchedReference(
                        tr.motion,
                        hubFrame->kinematicFrame(),
                        hubFrame->hubId
                    );
                }
                else
                {
                    game::navigation::TravelFrameSystem::matchToReference(
                        tr.motion,
                        hubFrame->kinematicFrame(),
                        "ship_travel_" + std::to_string(shipId.value),
                        hubFrame->hubId
                    );
                }
            }

            if (tr.motion.travelFrame.valid)
            {
                referenceVelocityMetersPerSecond =
                    tr.motion.travelFrame.localToWorldVelocity(
                        tr.motion.localPositionMeters,
                        glm::dvec3(0.0)
                    );
            }
        }

        tr.referenceVelocityMetersPerSecond =
            referenceVelocityMetersPerSecond;

        tr.motion.referenceVelocityMps =
            referenceVelocityMetersPerSecond;

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

    if (m_activeCelestialSystemId < 0)
        return;

    /*
        Navigation gravity is derived from the active system context instead of
        one named body. Every body whose catalog supplied a radius and GM may
        participate in the field. A zero influence radius means "do not apply
        an artificial cutoff"; primary-body selection still chooses the
        strongest local acceleration.
    */
    for (const auto& [bodyId, parameters] :
         m_celestialBodyGravityParameters)
    {
        const auto positionIt =
            m_celestialBodyPositionsAu.find(bodyId);

        if (positionIt == m_celestialBodyPositionsAu.end() ||
            parameters.radiusMeters <= 0.0 ||
            parameters.gravitationalParameterM3s2 <= 0.0)
        {
            continue;
        }

        game::navigation::GravityBody body;
        body.id = bodyId;
        body.centerMeters =
            positionIt->second * world::celestial::MetersPerAu;
        body.radiusMeters = parameters.radiusMeters;
        body.gravitationalParameterM3s2 =
            parameters.gravitationalParameterM3s2;

        // Atmosphere policy belongs to environment/gameplay data, not to a
        // hard-coded Earth special case in the gravity resolver.
        body.atmosphereRadiusMeters = body.radiusMeters;
        body.influenceRadiusMeters = 0.0;

        m_gravityBodies.push_back(std::move(body));
    }

    /*
        Corridors are runtime navigation policy around authored orbital hubs.
        They are generated for the active system only; a ship from another
        system can therefore never be classified against a foreign hub.
    */
    for (const auto& [hubId, hub] : m_orbitalHubs)
    {
        if (hub.systemId != m_activeCelestialSystemId ||
            !hub.motion.enabled ||
            hub.parentBodyId.empty())
        {
            continue;
        }

        const auto bodyIt =
            std::find_if(
                m_gravityBodies.begin(),
                m_gravityBodies.end(),
                [&](const game::navigation::GravityBody& body)
                {
                    return body.id == hub.parentBodyId;
                }
            );

        if (bodyIt == m_gravityBodies.end())
            continue;

        game::navigation::OrbitalCorridor corridor;
        corridor.id = hub.id + "_corridor";
        corridor.parentBodyId = hub.parentBodyId;
        corridor.hubId = hub.id;
        corridor.targetAltitudeMeters = hub.motion.altitudeMeters;
        corridor.halfWidthMeters = 50000.0;
        corridor.dangerBelowMeters = 50000.0;
        corridor.escapeAboveMeters = 50000.0;

        m_orbitalCorridors.push_back(std::move(corridor));
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

        if (tr.motion.systemId != m_activeCelestialSystemId)
            continue;

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






        if (m_diagnostics.settings.gravityCsv)
            debugLogGravitySample(*shipPtr);












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