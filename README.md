передача snapshot:
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
