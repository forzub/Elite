#include "game/scene/GameSceneSetup.h"

#include <cmath>
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
    const std::string& parentBodyId,
    glm::dvec3& centerMeters,
    double& parentRadiusMeters
)
{
    return sim.resolveCelestialBodyMeters(
        parentBodyId,
        centerMeters,
        parentRadiusMeters
    );
}




    void spawnOrbitalHubFromInitialState(
            GameSimulation& sim,
            const game::world_state::InitialWorldStateOrbitalHub& hub
        )
        {
            glm::dvec3 parentCenterMeters {0.0};
            double parentRadiusMeters = 0.0;

            if (!resolveParentBodyForInitialWorldState(
                    sim,
                    hub.parentBodyId,
                    parentCenterMeters,
                    parentRadiusMeters
                ))
            {
                return;
            }

            world::orbits::OrbitalMotion motion;

motion.enabled = true;
motion.centerMeters = parentCenterMeters;
motion.parentRadiusMeters = parentRadiusMeters;
motion.altitudeMeters =
    hub.motion.altitudeKm * 1000.0;

motion.orbitalPeriodSeconds =
    hub.motion.orbitalPeriodSeconds;

motion.selfRotationPeriodSeconds =
    hub.motion.selfRotationPeriodSeconds;

motion.inclinationDeg =
    hub.motion.inclinationDeg;

motion.longitudeOfAscendingNodeDeg =
    hub.motion.longitudeOfAscendingNodeDeg;

motion.argumentOfPeriapsisDeg =
    hub.motion.argumentOfPeriapsisDeg;

motion.initialPhaseDeg =
    hub.motion.initialPhaseDeg;

motion.epochSeconds =
    hub.motion.epochSeconds;

const glm::dvec3 hubCenterMeters =
    world::orbits::computeOrbitPositionMeters(
        motion,
        0.0
    );

const glm::mat4 hubOrientation =
    world::orbits::computeSelfRotation(
        motion,
        0.0
    );

world::hubs::OrbitalHubRuntime runtimeHub;

runtimeHub.id = hub.id;
runtimeHub.name = hub.name;
runtimeHub.owner = hub.owner;
runtimeHub.systemId = hub.systemId;
runtimeHub.parentBodyId = hub.parentBodyId;
runtimeHub.motion = motion;

runtimeHub.worldPosition =
    world::coordinates::makeWorldPositionFromMeters(
        hubCenterMeters
    );

runtimeHub.orientation =
    hubOrientation;

sim.registerOrbitalHub(runtimeHub);




            for (const auto& module : hub.modules)
            {
                if (!module.exists)
                    continue;

                

                ObjectType objectType =
                    ObjectType::Station;

                // Первый слой: command_station -> Station.
                // Позже добавим ObjectType::Buoy, Relay, Mine, Dock и т.д.
                if (module.type == "command_station")
                    objectType = ObjectType::Station;

                const glm::dvec3 modulePositionMeters =
                    hubCenterMeters + module.offsetMeters;

                const EntityId objectId =
                    sim.spawnStation(
                        objectType,
                        modulePositionMeters,
                        hubOrientation
                    );

                const bool isMapRepresentative =
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
                        hub.systemId,
                        hub.parentBodyId,
                        hub.id,
                        module.id
                    );
                }

               sim.attachStaticObjectToHub(
                    objectId,
                    hub.id,
                    module.id,
                    module.offsetMeters,
                    module.localRotationDeg,
                    true
                );
            }
        }





bool spawnInitialWorldStateObjects(
    GameSimulation& sim
)
{
    game::world_state::InitialWorldState state;

    if (!game::world_state::loadInitialWorldStateWithFallbacks(state))
        return false;

    for (const auto& hub : state.orbitalHubs)
    {
        if (hub.systemId != 0)
            continue;

        spawnOrbitalHubFromInitialState(
            sim,
            hub
        );
    }

    return true;
}






bool findEarthHighOrbitalPositionMeters(
    const GameSimulation& sim,
    glm::dvec3& outPositionMeters
)
{
    for (const auto& [id, obj] : sim.staticObjects())
    {
        if (obj.hubId != "earth_orbital_hub")
            continue;

        if (obj.hubModuleId != "earth_high_orbital")
            continue;

        outPositionMeters =
            world::coordinates::fullMeters(
                obj.worldPosition
            );

        return true;
    }

    return false;
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
            stationPos,
            stationOrientation
        );

    sim.setStaticObjectMapInfo(
        stationId,
        "Earth High Orbital",
        "Sol Authority",
        0,
        "system_0.Sol.Земля",
        "earth_orbital_hub",
        "earth_high_orbital"
    );

    sim.setStaticObjectOrbitalMotion(
        stationId,
        motion
    );
}


EntityId buildGameScene(GameSimulation& sim)
{
    if (!spawnInitialWorldStateObjects(sim))
    {
        spawnPromoStation(sim);
    }

    glm::dvec3 stationPos =
        SolarTestScene::StationPositionM;

    findEarthHighOrbitalPositionMeters(
        sim,
        stationPos
    );

    const glm::dvec3 playerPos =
        stationPos + glm::dvec3(0.0, 2500.0, -9000.0);

    ShipVisualIdentity playerVisual {
        .shipType = "Cobra MK1",
        .shipName = "Jeraya"
    };

    ShipRegistry playerRegistry {
        .instanceId      = 1,
        .ownerName       = "Jeraya",
        .ownerActor      = ActorIds::Player(),
        .registrationId  = "PL-0001",
        .homePort        = "Earth High Orbital",
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
            EliteCobraMk1::EliteCobraMk1Descriptor(),
            playerPos,
            playerInitData,
            makeLookOrientation(glm::vec3(stationPos - playerPos))
        );

    return playerId;
}
















EntityId buildPromoScene(GameSimulation& sim)
{
    EntityId playerId =
        spawnPromoPlayer(sim);

    if (!spawnInitialWorldStateObjects(sim))
    {
        spawnPromoStation(sim);
    }

    return playerId;
}

} // namespace

EntityId buildInitialScene(GameSimulation& sim)
{
    if constexpr (GameSceneSetupConfig::PromoScene)
    {
        return buildPromoScene(sim);
    }

    return buildGameScene(sim);
}

} // namespace game::scene