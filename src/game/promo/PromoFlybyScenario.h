#pragma once

#include <array>
#include <vector>

#include <glm/glm.hpp>

#include "src/scene/EntityID.h"

class GameSimulation;
class Ship;

namespace game::promo
{

class PromoFlybyScenario
{
public:
    static constexpr bool Enabled = false;

    void setup(GameSimulation& sim);
    void update(GameSimulation& sim, float dt);

    bool active() const { return m_active; }

private:
    struct PromoShip
    {
        EntityId id {};
        int groupIndex = 0;
        int slotIndex = 0;
    };

private:
    std::vector<PromoShip> m_ships;

    bool m_active = false;

    float m_time = 0.0f;

    glm::vec3 m_stationPos {0.0f, 0.0f, -1000.0f};

    float m_orbitRadius = 620.0f;
    float m_verticalSpread = 170.0f;

    float m_speed = 250.0f;
    float m_angularSpeed = 0.35f;

private:
    void spawnPromoShips(GameSimulation& sim);

    void applyShipPose(
        Ship& ship,
        const glm::vec3& position,
        const glm::vec3& forward,
        const glm::vec3& up,
        float speed
    ) const;

    glm::mat4 makeOrientation(
        const glm::vec3& forward,
        const glm::vec3& upHint
    ) const;

    glm::vec3 formationOffset(int groupIndex, int slotIndex) const;

    glm::vec3 approachCenter(float t) const;
    glm::vec3 rejoinCenter(float t) const;

    glm::vec3 orbitGroupCenter(
        int groupIndex,
        float angle
    ) const;

    static float smootherStep(float x);
};

} // namespace game::promo