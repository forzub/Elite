#include <stdexcept>
#include "game/scene/GameSceneSetup.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>

#include "game/simulation/GameSimulation.h"

#include "game/ship/ShipInitData.h"
#include "game/ship/ShipRegistry.h"
#include "game/ship/ShipRoleType.h"
#include "game/ship/ShipVisualIdentity.h"
#include "game/ship/descriptors/EliteCobraMk1.h"

#include "game/items/cryptocard/CryptoCard.h"
#include "game/player/ActorCodeGenerator.h"
#include "src/game/player/ActorIdProvider.h"
#include "src/game/diagnostics/HubMotionLab.h"
#include "src/game/diagnostics/ActivationCadenceLab.h"
#include "src/game/diagnostics/InterplanetaryTransferLab.h"

#include "src/world/types/ObjectType.h"
#include "src/world/orbits/OrbitalMotion.h"
#include "src/world/hubs/OrbitalHubRuntime.h"
#include "src/world/coordinates/WorldPosition.h"

namespace game::scene
{

namespace
{

glm::vec3 safeNormalize(
    const glm::vec3& v,
    const glm::vec3& fallback
)
{
    if (glm::length2(v) < 0.000001f)
        return fallback;

    return glm::normalize(v);
}

glm::mat4 makeLookOrientation(
    const glm::vec3& forward,
    const glm::vec3& upHint = glm::vec3(0.0f, 1.0f, 0.0f)
)
{
    const glm::vec3 f =
        safeNormalize(forward, {0.0f, 0.0f, -1.0f});

    glm::vec3 r =
        glm::cross(f, upHint);

    if (glm::length2(r) < 0.000001f)
        r = glm::cross(f, {0.0f, 0.0f, 1.0f});

    r = glm::normalize(r);

    const glm::vec3 u =
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



namespace SolarTestScene
{
    constexpr double AU = 149597870700.0;

    constexpr double EarthRadiusM = 6371000.0;
    constexpr double MoonDistanceM = 384400000.0;
    constexpr double StationAltitudeM = 420000.0;

    constexpr double DegToRad = 3.14159265358979323846 / 180.0;

    constexpr double EarthAxialTiltDeg = 23.439281;
    constexpr double IssInclinationDeg = 51.64;

    const glm::dvec3 SunPositionM =
        glm::dvec3(0.0, 0.0, 0.0);

    const glm::dvec3 EarthPositionM =
        glm::dvec3(AU, 0.0, 0.0);

    // Вектор оси Земли.
    // Земная орбита лежит в XZ, нормаль к эклиптике — +Y.
    // Наклоняем земную ось на 23.44°.
    const glm::dvec3 EarthAxis =
        glm::normalize(glm::dvec3(
            0.0,
            std::cos(EarthAxialTiltDeg * DegToRad),
            std::sin(EarthAxialTiltDeg * DegToRad)
        ));

    // Условная позиция Луны в плоскости земной орбиты.
    // Потом сделаем полноценную лунную орбиту.
    const glm::dvec3 MoonPositionM =
        EarthPositionM + glm::dvec3(0.0, 0.0, MoonDistanceM);

    // Точка на орбите МКС.
    // Не полюс. Орбита наклонена к экватору Земли на 51.64°.
    inline glm::dvec3 makeIssLikeOrbitDirection()
    {
        const double inc = IssInclinationDeg * DegToRad;

        glm::dvec3 equatorX = glm::normalize(glm::dvec3(1.0, 0.0, 0.0));
        glm::dvec3 equatorZ = glm::normalize(glm::cross(EarthAxis, equatorX));

        // Берём не 0°, чтобы станция не висела на “идеальной математической точке”.
        const double phase = 35.0 * DegToRad;

        glm::dvec3 inEquator =
            std::cos(phase) * equatorX +
            std::sin(phase) * equatorZ;

        glm::dvec3 tilted =
            std::cos(inc) * inEquator +
            std::sin(inc) * EarthAxis;

        return glm::normalize(tilted);
    }

    const glm::dvec3 StationPositionM =
        EarthPositionM +
        makeIssLikeOrbitDirection() *
            (EarthRadiusM + StationAltitudeM);

    const glm::dvec3 PlayerPositionM =
        StationPositionM + glm::dvec3(0.0, 2500.0, -9000.0);

    const glm::dvec3 Npc1PositionM =
        StationPositionM + glm::dvec3(-900.0, 2700.0, -8200.0);

    const glm::dvec3 Npc2PositionM =
        StationPositionM + glm::dvec3(1100.0, 2900.0, -8600.0);


    
}



bool resolveParentBodyForInitialWorldState(
    const GameSimulation& sim,
    int systemId,
    const std::string& parentBodyId,
    glm::dvec3& centerMeters,
    double& parentRadiusMeters
)
{
    return sim.resolveCelestialBodyMeters(
        systemId,
        parentBodyId,
        centerMeters,
        parentRadiusMeters
    );
}




bool spawnOrbitalHubFromInitialState(
    GameSimulation& sim,
    const game::world_state::InitialWorldStateOrbitalHub& hub
)
{
    glm::dvec3 parentCenterMeters {0.0};
    double parentRadiusMeters = 0.0;

    if (!resolveParentBodyForInitialWorldState(
            sim,
            hub.systemId,
            hub.parentBodyId,
            parentCenterMeters,
            parentRadiusMeters))
    {
        std::cerr
            << "[InitialWorldState] cannot resolve parent body for hub "
            << hub.id << ": " << hub.parentBodyId << "\n";
        return false;
    }

    world::orbits::OrbitalMotion motion;
    motion.enabled = true;
    motion.centerMeters = parentCenterMeters;
    motion.parentRadiusMeters = parentRadiusMeters;
    motion.altitudeMeters = hub.motion.altitudeKm * 1000.0;
    motion.orbitalPeriodSeconds = hub.motion.orbitalPeriodSeconds;
    motion.orbitalPeriodPolicy = hub.motion.orbitalPeriodPolicy;
    motion.selfRotationPeriodSeconds = hub.motion.selfRotationPeriodSeconds;
    motion.inclinationDeg = hub.motion.inclinationDeg;
    motion.longitudeOfAscendingNodeDeg =
        hub.motion.longitudeOfAscendingNodeDeg;
    motion.argumentOfPeriapsisDeg = hub.motion.argumentOfPeriapsisDeg;
    motion.initialPhaseDeg = hub.motion.initialPhaseDeg;
    motion.epochSeconds = hub.motion.epochSeconds;

    const glm::dvec3 hubCenterMeters =
        world::orbits::computeOrbitPositionMeters(motion, 0.0);
    const glm::mat4 hubOrientation =
        world::orbits::computeSelfRotation(motion, 0.0);

    world::hubs::OrbitalHubRuntime runtimeHub;
    runtimeHub.id = hub.id;
    runtimeHub.name = hub.name;
    runtimeHub.owner = hub.owner;
    runtimeHub.systemId = hub.systemId;
    runtimeHub.parentBodyId = hub.parentBodyId;
    runtimeHub.motion = motion;
    runtimeHub.worldPosition =
        world::coordinates::makeWorldPositionFromMeters(hubCenterMeters);
    runtimeHub.orientation = hubOrientation;

    sim.registerOrbitalHub(runtimeHub);

    for (const auto& module : hub.modules)
    {
        if (!module.exists)
            continue;

        // Schema validation currently admits only command_station.  New
        // authoritative module kinds must get an explicit mapping here rather
        // than silently becoming a station.
        const ObjectType objectType = ObjectType::Station;
        const glm::dvec3 modulePositionMeters =
            hubCenterMeters + module.offsetMeters;

        const EntityId objectId =
            sim.spawnStation(
                objectType,
                hub.systemId,
                modulePositionMeters,
                hubOrientation
            );

        const bool isMapRepresentative =
            module.mapVisible &&
            !hub.mapObjectModuleId.empty() &&
            module.id == hub.mapObjectModuleId;

        if (isMapRepresentative)
        {
            std::string mapName = module.name;
            if (hub.modules.size() > 1)
                mapName += " (Hub)";

            sim.setStaticObjectMapInfo(
                objectId,
                mapName,
                hub.owner,
                hub.parentBodyId,
                hub.id,
                module.id
            );
        }

        if (!sim.attachStaticObjectToHub(
                objectId,
                hub.id,
                module.id,
                module.offsetMeters,
                module.localRotationDeg,
                true))
        {
            std::cerr
                << "[InitialWorldState] failed to attach module "
                << module.id << " to hub " << hub.id << "\n";
            return false;
        }
    }

    return true;
}





bool spawnInitialWorldStateObjects(
    GameSimulation& sim,
    const game::world_state::InitialWorldState& state
)
{
    const int activeSystemId =
        sim.activeCelestialSystemId();

    bool spawnedForActiveSystem = false;

    for (const auto& hub : state.orbitalHubs)
    {
        if (hub.systemId != activeSystemId)
            continue;

        if (!spawnOrbitalHubFromInitialState(sim, hub))
            return false;

        spawnedForActiveSystem = true;
    }

    return spawnedForActiveSystem;
}






bool findHubMapObjectPositionMeters(
    const GameSimulation& sim,
    const std::string& hubId,
    const std::string& moduleId,
    glm::dvec3& outPositionMeters
)
{
    if (hubId.empty())
        return false;

    for (const auto& [id, obj] : sim.staticObjects())
    {
        if (obj.hubId != hubId)
            continue;

        if (!moduleId.empty() && obj.hubModuleId != moduleId)
            continue;

        outPositionMeters =
            world::coordinates::fullMeters(
                obj.worldPosition
            );

        return true;
    }

    return false;
}

const game::world_state::InitialWorldStateOrbitalHub*
findInitialHub(
    const game::world_state::InitialWorldState& state,
    const std::string& hubId
)
{
    for (const auto& hub : state.orbitalHubs)
    {
        if (hub.id == hubId)
            return &hub;
    }

    return nullptr;
}
















EntityId spawnPromoPlayer(GameSimulation& sim)
{
    ShipVisualIdentity visualIdentity {
        .shipType = "Cobra MK1",
        .shipName = "Jeraya"
    };

    ShipRegistry registry {
        .instanceId      = 1,
        .ownerName       = "Jeraya",
        .ownerActor      = ActorIds::Player(),
        .registrationId  = "PL-0001",
        .homePort        = "Promo Scene",
        .shipRole        = ShipRoleType::Civilian
    };

    auto* playerCard =
        new CryptoCard(
            generateActorCode(),
            "Player Access Card"
        );

    playerCard->actor =
        ActorIds::Player();

    ShipInitData initData;
    initData.visual = visualIdentity;
    initData.registry = registry;
    initData.initialInventory = {playerCard};

    const glm::dvec3 playerPos =
        SolarTestScene::PlayerPositionM;

    // Игрок смотрит туда, откуда прилетают кобры.
    // Кобры летят с +Z в сторону -Z.
    // Значит игрок сначала смотрит в +Z.
    const glm::vec3 lookDir =
        {0.0f, 0.0f, 1.0f};

    return sim.spawnShip(
        ShipRole::Player,
        0,
        EliteCobraMk1::EliteCobraMk1Descriptor(),
        playerPos,
        initData,
        makeLookOrientation(lookDir)
    );
}

void spawnPromoStation(GameSimulation& sim)
{
    world::orbits::OrbitalMotion motion;

    motion.enabled = true;

    motion.centerMeters =
        SolarTestScene::EarthPositionM;

    motion.parentRadiusMeters =
        SolarTestScene::EarthRadiusM;

    motion.altitudeMeters =
        SolarTestScene::StationAltitudeM;

    // Примерно низкая орбита, около МКС.
    motion.orbitalPeriodSeconds = 5400.0;

    // Собственное вращение станции.
    // Пока условно: один оборот за 180 секунд.
    motion.selfRotationPeriodSeconds = 180.0;

    motion.inclinationDeg = 51.64;
    motion.longitudeOfAscendingNodeDeg = 0.0;
    motion.argumentOfPeriapsisDeg = 0.0;
    motion.initialPhaseDeg = 35.0;

    motion.epochSeconds = 0.0;

    const glm::dvec3 stationPos =
        world::orbits::computeOrbitPositionMeters(
            motion,
            0.0
        );

    const glm::mat4 stationOrientation =
        world::orbits::computeSelfRotation(
            motion,
            0.0
        );

    const EntityId stationId =
        sim.spawnStation(
            ObjectType::Station,
            0,
            stationPos,
            stationOrientation
        );

    sim.setStaticObjectMapInfo(
        stationId,
        "Earth High Orbital",
        "Sol Authority",
        "system_0.Sol.Земля",
        "earth_orbital_hub",
        "earth_high_orbital"
    );

    sim.setStaticObjectOrbitalMotion(
        stationId,
        "system_0.Sol.Земля",
        motion
    );
}


EntityId spawnHubMotionLabNpc(
    GameSimulation& sim,
    game::diagnostics::HubMotionLabActorKind kind,
    std::uint64_t instanceId,
    const glm::dvec3& stationPos
)
{
    const auto* spec =
        game::diagnostics::hubMotionLabSpec(kind);

    if (!spec)
        return EntityId{};

    ShipVisualIdentity visual {
        .shipType = "Cobra MK1",
        .shipName = spec->label
    };

    ShipRegistry registry {
        .instanceId = instanceId,
        .ownerName = "Hub Motion Lab",
        .ownerActor = ActorIds::Unknown(),
        .registrationId =
            "LAB-" + std::to_string(instanceId),
        .homePort = "Earth High Orbital",
        .shipRole = ShipRoleType::Civilian
    };

    ShipInitData initData;
    initData.visual = visual;
    initData.registry = registry;

    const EntityId id =
        sim.spawnShip(
            ShipRole::NPC,
            0, // Sol; explicit membership is part of the spawn contract.
            EliteCobraMk1::EliteCobraMk1Descriptor(),
            stationPos,
            initData,
            glm::mat4(1.0f)
        );

    sim.registerHubMotionLabShip(
        id,
        kind,
        std::string(game::diagnostics::HubMotionLabHubId)
    );

    return id;
}

void spawnHubMotionLabNpcs(
    GameSimulation& sim,
    const glm::dvec3& stationPos
)
{
    spawnHubMotionLabNpc(
        sim,
        game::diagnostics::HubMotionLabActorKind::SlowOrbit,
        9001,
        stationPos
    );

    spawnHubMotionLabNpc(
        sim,
        game::diagnostics::HubMotionLabActorKind::FastOrbit,
        9002,
        stationPos
    );

    spawnHubMotionLabNpc(
        sim,
        game::diagnostics::HubMotionLabActorKind::MatchPlayer,
        9003,
        stationPos
    );
}

void spawnHubGuidanceTestModules(
    GameSimulation& sim,
    int systemId,
    const std::string& hubId,
    const glm::dvec3& hubReferencePositionMeters
)
{
    struct Spec
    {
        ObjectType type;
        const char* moduleId;
        const char* displayName;
        glm::dvec3 localOffsetMeters;
        glm::dvec3 localRotationDeg;
        glm::dvec3 localAngularVelocityDegPerSecond;
    };

    // Hub visual-local basis is X=normal, Y=radial, Z=-prograde. Both test
    // meshes have their through corridor along local Z, so their docking axis
    // is collinear with orbital prograde as requested. They are deliberately
    // several kilometres apart to create a useful short-range guidance lab.
    const Spec specs[] = {
        {
            ObjectType::GuidanceDockCube,
            "guidance_dock_cube_a",
            "GUIDANCE DOCK CUBE A",
            glm::dvec3(3000.0, 350.0, 0.0),
            glm::dvec3(0.0),
            glm::dvec3(0.0, 0.0, 2.0)
        },
        {
            ObjectType::GuidanceDockCylinder,
            "guidance_dock_cylinder_b",
            "GUIDANCE DOCK CYLINDER B",
            glm::dvec3(-3000.0, -250.0, 0.0),
            glm::dvec3(0.0),
            glm::dvec3(0.0, 0.0, -1.5)
        }
    };

    for (const Spec& spec : specs)
    {
        const EntityId id = sim.spawnStation(
            spec.type,
            systemId,
            hubReferencePositionMeters,
            glm::mat4(1.0f)
        );

        if (id.value == 0)
            continue;

        sim.setStaticObjectIdentity(id, spec.displayName, "Hub Motion Lab");

        if (!sim.attachStaticObjectToHub(
                id,
                hubId,
                spec.moduleId,
                spec.localOffsetMeters,
                spec.localRotationDeg,
                true,
                spec.localAngularVelocityDegPerSecond))
        {
            std::cerr
                << "[HubGuidanceLab] failed to attach "
                << spec.moduleId << " to " << hubId << "\n";
        }
    }
}


EntityId spawnInterplanetaryTransferLabNpc(GameSimulation& sim)
{
    using namespace game::diagnostics;

    glm::dvec3 sunCenterMeters {0.0};
    double sunRadiusMeters = 0.0;
    if (!sim.resolveCelestialBodyMeters(
            0,
            InterplanetaryTransferLabSunBodyId,
            sunCenterMeters,
            sunRadiusMeters))
    {
        std::cerr
            << "[InterplanetaryTransferLab] cannot resolve Sol body\n";
        return EntityId{};
    }

    const auto state =
        evaluateInterplanetaryTransferLab(0.0);

    ShipVisualIdentity visual {
        .shipType = "Cobra MK1",
        .shipName = InterplanetaryTransferLabLabel
    };

    ShipRegistry registry {
        .instanceId = InterplanetaryTransferLabInstanceId,
        .ownerName = "Interplanetary Flight Test",
        .ownerActor = ActorIds::Unknown(),
        .registrationId = "TRN-ME-9020",
        .homePort = "Mars departure corridor",
        .shipRole = ShipRoleType::Civilian
    };

    ShipInitData initData;
    initData.visual = visual;
    initData.registry = registry;

    const glm::dvec3 spawnPosition =
        sunCenterMeters + state.relativePositionMeters;
    const glm::vec3 lookDirection =
        glm::normalize(glm::vec3(state.relativeVelocityMetersPerSecond));

    const EntityId id =
        sim.spawnShip(
            ShipRole::NPC,
            0,
            EliteCobraMk1::EliteCobraMk1Descriptor(),
            spawnPosition,
            initData,
            makeLookOrientation(lookDirection)
        );

    glm::dvec3 sunVelocityMetersPerSecond {0.0};
    sim.resolveCelestialBodyVelocityMetersPerSecond(
        0,
        InterplanetaryTransferLabSunBodyId,
        sunVelocityMetersPerSecond
    );

    if (Ship* ship = sim.getShip(id))
    {
        auto& tr = ship->core().transform();
        tr.motion.mode = game::navigation::MotionMode::PassiveTrajectory;
        tr.motion.systemId = 0;
        tr.motion.parentBodyId = InterplanetaryTransferLabSunBodyId;
        tr.motion.worldVelocityMps =
            sunVelocityMetersPerSecond +
            state.relativeVelocityMetersPerSecond;
        tr.motion.referenceVelocityMps = sunVelocityMetersPerSecond;
        tr.motion.localPositionMeters = state.relativePositionMeters;
        tr.motion.localVelocityMps = state.relativeVelocityMetersPerSecond;
        tr.referenceVelocityMetersPerSecond = sunVelocityMetersPerSecond;
    }

    sim.registerInterplanetaryTransferLabShip(id);

    std::cout
        << "[InterplanetaryTransferLab] spawned Mars->Earth test ship"
        << " radius_au="
        << state.heliocentricRadiusMeters / InterplanetaryAuMeters
        << " speed_km_s="
        << state.heliocentricSpeedMetersPerSecond / 1000.0
        << " transfer_day="
        << state.transferElapsedSeconds / InterplanetarySecondsPerDay
        << "/"
        << state.transferDurationSeconds / InterplanetarySecondsPerDay
        << "\n";

    return id;
}


EntityId spawnActivationCadenceLabNpc(
    GameSimulation& sim,
    const glm::dvec3& stationPos
)
{
    using namespace game::diagnostics;

    ShipVisualIdentity visual {
        .shipType = "Cobra MK1",
        .shipName = ActivationCadenceLabLabel
    };

    ShipRegistry registry {
        .instanceId = ActivationCadenceLabInstanceId,
        .ownerName = "Activation Diagnostics",
        .ownerActor = ActorIds::Unknown(),
        .registrationId = "ACT-AI-9010",
        .homePort = "Earth High Orbital",
        .shipRole = ShipRoleType::Civilian
    };

    ShipInitData initData;
    initData.visual = visual;
    initData.registry = registry;

    // Keep the actor well outside the station interaction envelope so the
    // diagnostic claim sequence, rather than geometry, drives Coarse /
    // Prewarm / Active transitions.
    const glm::dvec3 spawnPosition =
        stationPos + glm::dvec3(
            game::diagnostics::ActivationCadenceLabLocalOffsetMeters,
            0.0,
            0.0
        );

    const EntityId id =
        sim.spawnShip(
            ShipRole::NPC,
            0,
            EliteCobraMk1::EliteCobraMk1Descriptor(),
            spawnPosition,
            initData,
            glm::mat4(1.0f)
        );

    sim.registerActivationCadenceLabShip(id);
    return id;
}



EntityId buildGameScene(
    GameSimulation& sim,
    const game::world_state::InitialWorldState& initialState
)
{
    if (!spawnInitialWorldStateObjects(sim, initialState))
    {
        throw std::runtime_error(
            "initial world contains no orbital hub for the active system"
        );
    }

    const auto* playerHub =
        findInitialHub(
            initialState,
            initialState.playerStart.hubId
        );

    if (!playerHub)
    {
        throw std::runtime_error(
            "validated player_start hub disappeared during scene bootstrap"
        );
    }

    const int initialSystemId =
        initialState.playerStart.systemId;

    glm::dvec3 stationPos {0.0};
    if (!findHubMapObjectPositionMeters(
            sim,
            playerHub->id,
            playerHub->mapObjectModuleId,
            stationPos))
    {
        throw std::runtime_error(
            "player_start hub has no spawned map representative"
        );
    }

    // This is only a bootstrap position until GameServer resolves the authored
    // reference frame after hub frames have been prepared.
    const glm::dvec3 playerPos =
        stationPos + initialState.playerStart.localOffsetMeters;

    ShipVisualIdentity playerVisual {
        .shipType = "Cobra MK1",
        .shipName = "Jeraya"
    };

    ShipRegistry playerRegistry {
        .instanceId      = 1,
        .ownerName       = "Jeraya",
        .ownerActor      = ActorIds::Player(),
        .registrationId  = "PL-0001",
        .homePort        = playerHub->name,
        .shipRole        = ShipRoleType::Civilian
    };

    auto* playerCard =
        new CryptoCard(
            generateActorCode(),
            "Player Access Card"
        );

    playerCard->actor =
        ActorIds::Player();

    ShipInitData playerInitData;
    playerInitData.visual = playerVisual;
    playerInitData.registry = playerRegistry;
    playerInitData.initialInventory = {playerCard};

    const EntityId playerId =
        sim.spawnShip(
            ShipRole::Player,
            initialSystemId,
            EliteCobraMk1::EliteCobraMk1Descriptor(),
            playerPos,
            playerInitData,
            makeLookOrientation(glm::vec3(stationPos - playerPos))
        );

    const bool diagnosticHubAvailable =
        playerHub->id == game::diagnostics::HubMotionLabHubId;

    if constexpr (game::diagnostics::HubMotionLabEnabled)
    {
        if (diagnosticHubAvailable)
        {
            spawnHubMotionLabNpcs(sim, stationPos);
            spawnHubGuidanceTestModules(
                sim,
                initialSystemId,
                playerHub->id,
                stationPos
            );
        }
    }

    if constexpr (game::diagnostics::ActivationCadenceLabEnabled)
    {
        if (diagnosticHubAvailable)
            spawnActivationCadenceLabNpc(sim, stationPos);
    }

    if constexpr (game::diagnostics::InterplanetaryTransferLabEnabled)
    {
        if (initialSystemId == 0)
            spawnInterplanetaryTransferLabNpc(sim);
    }

    return playerId;
}














EntityId buildPromoScene(
    GameSimulation& sim,
    const game::world_state::InitialWorldState& initialState
)
{
    EntityId playerId =
        spawnPromoPlayer(sim);

    if (!spawnInitialWorldStateObjects(sim, initialState))
    {
        spawnPromoStation(sim);
    }

    return playerId;
}

} // namespace

EntityId buildInitialScene(
    GameSimulation& sim,
    const game::world_state::InitialWorldState& initialState
)
{
    if constexpr (GameSceneSetupConfig::PromoScene)
    {
        return buildPromoScene(sim, initialState);
    }

    return buildGameScene(sim, initialState);
}

} // namespace game::scene