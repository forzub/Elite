#include "GameClient.h"
#include "src/game/server/GameServer.h"
#include "src/game/client/ClientWorldState.h"
#include <iostream>

// GameClient::GameClient(GameServer* server, EntityId playerId)
GameClient::GameClient(ITransport* transport, EntityId playerId)
    : m_transport(transport)
    , m_playerId(playerId)
{
}






void GameClient::submitInput(const ShipControlState& control)
{
    
    ShipControlState c = control;

    m_clientTick++;
    c.controlTick = m_clientTick;

    m_pendingInputs.push_back({ m_clientTick, c });

    // m_server->submitCommand(m_playerId, c);
    m_transport->sendInput(m_playerId, c);

}







void GameClient::update(
    float dt,
    const WorldParams& world,
    float fixedDt)
{
    m_accumulator += dt;

SimulationSnapshot snapshot;

while (m_transport->receiveSnapshot(snapshot))
{
    m_world.applySnapshot(snapshot);

    while (!m_pendingInputs.empty() &&
           m_pendingInputs.front().controlTick <= snapshot.snapshotTick)
    {
        m_pendingInputs.pop_front();
    }
}

while (m_accumulator >= fixedDt)
{
    if (!m_pendingInputs.empty())
    {
        const auto& last = m_pendingInputs.back();

        m_world.predict(
            m_playerId,
            last.control,
            world,
            fixedDt
        );
    }

    m_accumulator -= fixedDt;
}

m_world.update(dt);
}






// void GameClient::reconcile(const SimulationSnapshot& snapshot)
// {
//     while (!m_pendingInputs.empty() &&
//            m_pendingInputs.front().controlTick <= snapshot.snapshotTick)
//     {
//         m_pendingInputs.pop_front();
//     }
// }


void GameClient::reconcile(
    const SimulationSnapshot& snapshot,
    const WorldParams& world,
    float fixedDt)
{
    // 1️⃣ Удаляем подтверждённые инпуты
    while (!m_pendingInputs.empty() &&
           m_pendingInputs.front().controlTick <= snapshot.snapshotTick)
    {
        m_pendingInputs.pop_front();
    }

    // 2️⃣ Найти authoritative позицию игрока в snapshot
    glm::vec3 authoritativePos{};
    bool found = false;

    for (const auto& s : snapshot.ships)
    {
        if (s.id == m_playerId)
        {
            authoritativePos = s.transform.position;
            found = true;
            break;
        }
    }

    if (!found)
        return;

    // 3️⃣ Найти текущий клиентский корабль
    const auto& ships = m_world.ships();
    auto it = ships.find(m_playerId.value);
    if (it == ships.end())
        return;

    const auto& clientShip = it->second;

    // 4️⃣ Посчитать ошибку
    

    float error = glm::distance(
        clientShip.transform.position,
        authoritativePos
        );

        if (error > 0.01f)
        {
            m_world.applySoftCorrection(
                m_playerId,
                authoritativePos
            );
        }




    // 6️⃣ Переигрываем неподтверждённые инпуты
    for (const auto& input : m_pendingInputs)
    {
        m_world.predict(
            m_playerId,
            input.control,
            world,
            fixedDt
        );
    }
}




void GameClient::replayPendingInputs(
    const WorldParams& world,
    float fixedDt)
{
    for (const auto& input : m_pendingInputs)
    {
        m_world.predict(
            m_playerId,
            input.control,
            world,
            fixedDt
        );
    }
}






const ClientWorldState& GameClient::world() const
{
    return m_world;
}

ClientWorldState& GameClient::world()
{
    return m_world;
}


