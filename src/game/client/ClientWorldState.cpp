#include "ClientWorldState.h"
#include "src/game/shared/SharedShipPhysics.h"
#include <iostream>
#include "src/game/ship/ShipDescriptorRegistry.h"



void ClientWorldState::applySnapshot(const SimulationSnapshot& snapshot)
{
    for (const auto& s : snapshot.ships)
    {
        auto it = m_ships.find(s.id.value);

        if (it == m_ships.end())
        {
            ClientShipState state;

            state.id   = s.id;
            state.role = s.role;

            state.descriptor =
                &ShipDescriptorRegistry::get(s.typeId);

            state.transform       = s.transform;
            state.renderTransform = s.transform;

            state.labels = s.labels;

            m_ships[s.id.value] = state;
        }
        else
        {
            auto& state = it->second;

            state.transform = s.transform;
            state.labels    = s.labels;
        }
    }
}









void ClientWorldState::update(float dt)
{
    const float interpSpeed = 12.0f;

    for (auto& [id, ship] : m_ships)
    {
        ship.renderTransform.position =
            glm::mix(
                ship.renderTransform.position,
                ship.transform.position,
                dt * interpSpeed
            );

        // ориентацию пока жёстко копируем
        ship.renderTransform.orientation =
            ship.transform.orientation;
    }
}






const std::unordered_map<uint32_t, ClientShipState>&
ClientWorldState::ships() const
{
    return m_ships;
}




void ClientWorldState::predict(
    EntityId id,
    const ShipControlState& control,
    const WorldParams& world,
    float dt)
{
    auto it = m_ships.find(id.value);
    if (it == m_ships.end())
        return;

    auto& ship = it->second;

    SharedShipPhysics::integrate(
        ship.transform,
        ship.descriptor->physics,
        control,
        world,
        dt
    );
}




void ClientWorldState::forceState(
    const SimulationSnapshot& snapshot)
{
    for (const auto& s : snapshot.ships)
    {
        auto it = m_ships.find(s.id.value);
        if (it == m_ships.end())
            continue;

        auto& state = it->second;

        // Полная синхронизация
        state.transform       = s.transform;
        state.renderTransform = s.transform;
        state.role            = s.role;
    }
}



void ClientWorldState::applySoftCorrection(
    EntityId id,
    const glm::vec3& authoritativePos)
{
    auto it = m_ships.find(id.value);
    if (it == m_ships.end())
        return;

    auto& ship = it->second;

    glm::vec3 delta =
        authoritativePos - ship.transform.position;

    ship.transform.position += delta * 0.1f;
}