### передача snapshot:
- src\game\simulation\SimulationSnapshot.h - вносим глобальные данные для переноса.
- src\game\simulation\ShipSnapshot.h  - данные, которые касаются напрямую корабля.
- void GameSimulation::update(double dt)  - формирование snapshot
- src\game\client\ClientWorldState.h  - по сути - дублируем данные которые хотим передать для корабля.
- void ClientWorldState::applySnapshot(const SimulationSnapshot& snapshot) - обязательно сделать присвоение данных
``` cpp
if (it == m_ships.end())
        {
            ClientShipState state;
            // для объекта, который появляется на клиенте
            state.id   = s.id;
            state.role = s.role;

            state.descriptor =
                &ShipDescriptorRegistry::get(s.typeId);

            state.transform       = s.transform;
            state.renderTransform = s.transform;
            state.radarContacts = s.radarContacts;

            m_ships[s.id.value] = state;
            state.receptions = s.receptions;
            state.shipCoreStatus = s.shipCoreStatus;
        }
        else
        {
            // обновление данных для объекта, который есть на клиенте
            auto& state = it->second;
            state.transform = s.transform;
            state.receptions = s.receptions;
            state.radarContacts   = s.radarContacts;
            state.shipCoreStatus = s.shipCoreStatus;
        }
  ```
### передача сообщение с клиента на сервер
- src\game\network\ClientShipCommand.h
- src\game\network\ClientMessage.h  - описание создания сообщения.
- void SpaceState::update(float dt) - отправка сообщений:
```cpp
// --------------------------------------------------
    // DEBUG: Обрабатываем отладочные команды из очереди
    // --------------------------------------------------
    {
        std::lock_guard<std::mutex> lock(m_debugCommandsMutex);
        for (const auto& cmd : m_debugCommands)
        {
             
            switch (cmd.type)
            {
                case ClientShipCommand::DamageRadiator:
                {
                    ClientShipCommand ship_cmd;
                    ship_cmd.type = ClientShipCommand::DamageRadiator;
                    ship_cmd.index = cmd.index;
                    ship_cmd.amount = cmd.amount;

                    game::network::ClientMessage  msg;
                    msg.clientTick = 0;
                    msg.type = game::network::ClientMessageType::ClientShipCommand;
                    msg.payload = ship_cmd;
                    m_client->sendMessage(msg);

                    break;
                }
                case ClientShipCommand::RepairAllPanels:
                {
                    ClientShipCommand ship_cmd;
                    ship_cmd.type = ClientShipCommand::RepairAllPanels;

                    game::network::ClientMessage  msg;
                    msg.clientTick = 0;
                    msg.type = game::network::ClientMessageType::ClientShipCommand;
                    msg.payload = ship_cmd;
                    m_client->sendMessage(msg);
                    break;
                }
                // ... другие команды
            }
        }
        m_debugCommands.clear();
    }

```
- void GameServer::update(double dt)
``` cpp
// --- Обработка событийных ShipCommand ---
        auto cmdIt = m_pendingClientShipCommands.find(id.value);
        if (cmdIt != m_pendingClientShipCommands.end())
        {
            auto& cmdQueue = cmdIt->second;

            while (!cmdQueue.empty())
            {
                const auto& shipCmd = cmdQueue.front();

                std::cout << "GameServer::update  - ClientShipCommand received: "
                        << shipCmd.type << "\n";

                ship.applyCommand(shipCmd);

                cmdQueue.pop_front();
            }
        }
```
