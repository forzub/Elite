#include "game/scene/GameSceneSetup.h"

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

    const glm::vec3 playerPos =
        {0.0f, 50.0f, 0.0f};

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
    // Игрок смотрит в +Z.
    // Звено появляется со стороны +Z и летит через игрока в -Z.
    // Станция стоит высоко впереди по +Z, чтобы попадать в кадр
    // во время начального сопровождения звена.
    const glm::vec3 stationPos =
        {0.0f, 2150.0f, 2450.0f};

    // Пока identity. Если длинная ось станции окажется не вдоль Z,
    // позже добавим поворот здесь, а не в GameSimulation.
    const glm::mat4 stationOrientation =
        glm::mat4(1.0f);

    sim.spawnStation(
        ObjectType::Station,
        stationPos,
        stationOrientation
    );
}


EntityId buildGameScene(GameSimulation& sim)
{
    EntityId playerId =
        spawnPromoPlayer(sim);

    // Игровой режим: два NPC сразу в поле зрения игрока.
    // Игрок сейчас стоит в {0, 50, 0} и смотрит в +Z,
    // поэтому NPC ставим впереди по +Z, с небольшим разносом по X/Y.
    const glm::vec3 playerPos =
        {0.0f, 50.0f, 0.0f};

    ShipVisualIdentity visualIdentity {
        .shipType = "Cobra MK3",
        .shipName = "Scarlet Hawk Moth"
    };

    ShipRegistry registry {
        .instanceId      = 2,
        .ownerName       = "Scarlet Hawk Moth",
        .ownerActor      = ActorIds::Player(),
        .registrationId  = "NPC-0002",
        .homePort        = "Lave",
        .shipRole        = ShipRoleType::Civilian
    };

    auto* npc1Card =
        new CryptoCard(
            generateActorCode(),
            "Player Access Card"
        );

    npc1Card->actor =
        ActorIds::Player();

    ShipInitData initData;
    initData.visual = visualIdentity;
    initData.registry = registry;
    initData.initialInventory = {npc1Card};

    const glm::vec3 npc1Pos =
        {-65.0f, 35.0f, 260.0f};

    sim.spawnShip(
        ShipRole::NPC,
        EliteCobraMk1::EliteCobraMk1Descriptor(),
        npc1Pos,
        initData,
        makeLookOrientation(playerPos - npc1Pos)
    );

    visualIdentity.shipType = "Cobra MK3";
    visualIdentity.shipName = "Hooded snake";

    registry.instanceId = 3;
    registry.ownerName = "Hooded snake";
    registry.registrationId = "NPC-0003";

    auto* npc2Card =
        new CryptoCard(
            generateActorCode(),
            "Player Access Card"
        );

    npc2Card->actor =
        ActorIds::Player();

    initData.visual = visualIdentity;
    initData.registry = registry;
    initData.initialInventory = {npc2Card};

    const glm::vec3 npc2Pos =
        {80.0f, 85.0f, 340.0f};

    sim.spawnShip(
        ShipRole::NPC,
        EliteCobraMk1::EliteCobraMk1Descriptor(),
        npc2Pos,
        initData,
        makeLookOrientation(playerPos - npc2Pos)
    );

    spawnPromoStation(sim);

    return playerId;
}

EntityId buildPromoScene(GameSimulation& sim)
{
    EntityId playerId =
        spawnPromoPlayer(sim);

    spawnPromoStation(sim);

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