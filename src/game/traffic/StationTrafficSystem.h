#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>

#include <glm/glm.hpp>

#include "src/game/visual/VisualShip.h"

class ClientWorldState;

namespace game::traffic
{

class StationTrafficSystem
{
public:
    void setup(ClientWorldState& world);
    void update(ClientWorldState& world, float dt);
    void reset(ClientWorldState& world);

    bool active() const { return m_active; }

private:
    struct TrafficShip
    {
        game::visual::VisualShipId id = 0;
        std::size_t visualIndex = 0;

        // Стартовая фаза маршрута.
        // Не накапливаем phase += dt бесконечно — это даёт микродрожание.
        double  basePhase = 0.0f;

        // Текущая фаза, вычисляется от общего времени.
        double  phase = 0.0f;
        // Желаемая линейная скорость в world units/sec.
        // Угловая скорость вычисляется как linearSpeed / radius.
        float linearSpeed = 80.0f;

        float radius = 1000.0f;
        float height = 0.0f;

        float laneTilt = 0.0f;
        double  laneOffset = 0.0f;

        int routeType = 0;
    };

private:
    bool m_active = false;

    double m_trafficTime = 0.0;

    std::vector<TrafficShip> m_ships;

private:
    void spawnTraffic(ClientWorldState& world);

    glm::vec3 computePosition(const TrafficShip& ship) const;
    glm::vec3 computeForward(const TrafficShip& ship) const;

    glm::mat4 makeOrientation(
        const glm::vec3& forward,
        const glm::vec3& upHint
    ) const;
};

} // namespace game::traffic