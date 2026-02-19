#include "GameSimulation.h"


#include "game/ship/ShipInitData.h"
#include "game/ship/ShipRoleType.h"
#include "game/ship/descriptors/EliteCobraMk1.h"

#include "game/items/cryptocard/CryptoCard.h"
#include "game/player/ActorCodeGenerator.h"
#include "src/game/player/ActorIdProvider.h"
#include "game/ship/ShipRegistry.h"
#include "game/ship/ShipVisualIdentity.h"

GameSimulation::GameSimulation()
{
    // ========================= PLAYER =========================

    ShipVisualIdentity visualIdentity = {
        .shipType = "Cobra MK1",
        .shipName = "Jeraya"
    };

    ShipRegistry registry = {
        .instanceId      = 1,
        .ownerName       = "Jeraya",
        .ownerActor      = ActorIds::Player(),
        .registrationId  = "PL-0001",
        .homePort        = "Lave",
        .shipRole        = ShipRoleType::Civilian
    };

    auto* playerCard = new CryptoCard(
        generateActorCode(),
        "Player Access Card"
    );
    playerCard->actor = ActorIds::Player();

    ShipInitData initData;
    initData.visual = visualIdentity;
    initData.registry = registry;
    initData.initialInventory = {playerCard};

    m_playerId = spawnShip(
        ShipRole::Player,
        EliteCobraMk1::EliteCobraMk1Descriptor(),
        {0.0f, 50.0f, 10.0f},
        initData
    );

    // ========================= NPC #1 =========================

    visualIdentity = {
        .shipType = "Cobra MK3",
        .shipName = "Scarlet Hawk Moth"
    };

    registry.instanceId = 2;

    auto* npc1Card = new CryptoCard(
        generateActorCode(),
        "Player Access Card"
    );
    npc1Card->actor = ActorIds::Player();

    initData.visual = visualIdentity;
    initData.registry = registry;
    initData.initialInventory = {npc1Card};

    EntityId npcId1 = spawnShip(
        ShipRole::NPC,
        EliteCobraMk1::EliteCobraMk1Descriptor(),
        {20.0f, 0.0f, -50.0f},
        initData
    );

    // ========================= NPC #2 =========================

    visualIdentity.shipName = "Hooded snake";
    registry.instanceId = 3;

    auto* npc2Card = new CryptoCard(
        generateActorCode(),
        "Player Access Card"
    );
    npc2Card->actor = ActorIds::Player();

    initData.visual = visualIdentity;
    initData.registry = registry;
    initData.initialInventory = {npc2Card};

    EntityId npcId2 =spawnShip(
        ShipRole::NPC,
        EliteCobraMk1::EliteCobraMk1Descriptor(),
        {-20.0f, 50.0f, -50.0f},
        initData
    );
}



//                       ###              ##
//                        ##              ##
//  ##  ##   ######       ##    ####     #####    ####
//  ##  ##    ##  ##   #####       ##     ##     ##  ##
//  ##  ##    ##  ##  ##  ##    #####     ##     ######
//  ##  ##    #####   ##  ##   ##  ##     ## ##  ##
//   ######   ##       ######   #####      ###    #####
//           ####

void GameSimulation::update(double dt)
{

    
    float fdt = static_cast<float>(dt);
    m_serverTime += dt;

    // === 1. AI phase ===
    for (auto& [id, ship] : m_ships)
    {        
        if (id == m_playerId)
            continue;

        ShipControlState aiControl = m_npcAiSystem.computeControl(ship, fdt);
        ship.setControlState(aiControl);
    }

    for (auto& [id, ship] : m_ships)
        ship.applyControl();

    for (auto& [id, ship] : m_ships)
        ship.updatePhysics(fdt, m_world);

    std::vector<Ship*> signalShips;
    for (auto& [id, ship] : m_ships)
        signalShips.push_back(&ship);

    WorldSignalTxSystem::collectFromShips(signalShips, m_worldSignals);

    for (auto& [id, ship] : m_ships)
        ship.updateSignals(fdt, m_worldSignals, m_planets, m_interferenceSources);

    for (auto& [id, ship] : m_ships)
        ship.updatePerception(fdt);


    m_snapshot.serverTime = m_serverTime;
    m_snapshot.ships.clear();
    m_snapshot.signals = m_worldSignals;


    for (auto& [id, ship] : m_ships)
    {
        ShipSnapshot s;

        const auto& tr = ship.core().transform();

        s.id = id;
        s.typeId = ship.core().desc().typeId;
        s.role = ship.core().role();

        s.transform = tr;


 
        const auto& receptions = ship.core().signalResults();
        s.receptions = receptions;
        s.receptions = ship.core().signalResults();
        
        m_snapshot.ships.push_back(s);
    }
}











void GameSimulation::setTick(uint32_t tick)
{
    m_snapshot.snapshotTick = tick;
}



EntityId GameSimulation::generateEntityId()
{
    EntityId id;
    id.value = m_nextEntityId++;
    return id;
}



EntityId GameSimulation::spawnShip(
    ShipRole role,
    const ShipDescriptor& descriptor,
    glm::vec3 position,
    const ShipInitData& initData
)
{
    Ship ship;

    EntityId id = generateEntityId();
    ship.setId(id);

    ship.init(role, descriptor, position, initData);

    m_ships[id] = std::move(ship);

    return id;
}


void GameSimulation::applyControl(EntityId id, const ShipControlState& control)
{
    Ship* ship = getShip(id);
    if (!ship)
        return;

    ship->setControlState(control);
}
















WorldParams& GameSimulation::world()
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
    return &it->second;
}

const Ship* GameSimulation::getShip(EntityId id) const
{
    auto it = m_ships.find(id);
    if (it == m_ships.end())
        return nullptr;
    return &it->second;
}

Ship* GameSimulation::playerShip()
{
    return getShip(m_playerId);
}

const Ship* GameSimulation::playerShip() const
{
    return getShip(m_playerId);
}

std::unordered_map<EntityId, Ship>& GameSimulation::ships()
{
    return m_ships;
}

void GameSimulation::setPlayerControl(const ShipControlState& control)
{
    m_playerControlState = control;
}

const SimulationSnapshot& GameSimulation::snapshot() const
{
    return m_snapshot;
}