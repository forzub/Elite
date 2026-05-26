#include "src/game/traffic/StationTrafficSystem.h"

#include <cmath>
#include <string>
#include <algorithm>
#include <fstream>

#include <glm/gtx/norm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "src/game/client/ClientWorldState.h"
#include "src/game/ship/descriptors/EliteCobraMk1.h"
#include "src/game/geometry/AssemblyMeshLibrary.h"

namespace game::traffic
{

namespace
{
constexpr int TrafficShipCount = 1500;

constexpr float TwoPi = 6.28318530718f;

glm::vec3 safeNormalize(
    const glm::vec3& v,
    const glm::vec3& fallback
)
{
    if (glm::length2(v) < 0.000001f)
        return fallback;

    return glm::normalize(v);
}

float pseudoRandom01(uint32_t x)
{
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;

    return float(x & 0x00FFFFFFu) / float(0x01000000u);
}


float smoothingAlpha(float dt, float responsiveness)
{
    if (dt <= 0.0f)
        return 1.0f;

    return 1.0f - std::exp(-responsiveness * dt);
}

glm::mat4 smoothOrientation(
    const glm::mat4& current,
    const glm::mat4& target,
    float alpha
)
{
    glm::quat qCurrent = glm::quat_cast(current);
    glm::quat qTarget  = glm::quat_cast(target);

    glm::quat q =
        glm::slerp(
            glm::normalize(qCurrent),
            glm::normalize(qTarget),
            glm::clamp(alpha, 0.0f, 1.0f)
        );

    return glm::mat4_cast(glm::normalize(q));
}









} // namespace

void StationTrafficSystem::setup(ClientWorldState& world)
{
    if (m_active)
        return;

    spawnTraffic(world);
    m_active = true;
}

void StationTrafficSystem::reset(ClientWorldState& world)
{
    std::cout
    << "[StationTraffic] reset BEGIN visualShips="
    << world.visualShips().size()
    << " trafficShips="
    << m_ships.size()
    << std::endl;


    auto& visualShips = world.visualShips();

    visualShips.erase(
        std::remove_if(
            visualShips.begin(),
            visualShips.end(),
            [](const game::visual::VisualShip& ship)
            {
                return ship.id >= 800000 && ship.id < 900000;
            }
        ),
        visualShips.end()
    );

    std::cout
    << "[StationTraffic] reset AFTER ERASE visualShips="
    << world.visualShips().size()
    << std::endl;

    m_ships.clear();
    m_active = false;
    m_trafficTime = 0.0;

    std::cout
    << "[StationTraffic] reset END active="
    << m_active
    << " trafficShips="
    << m_ships.size()
    << std::endl;
}

void StationTrafficSystem::spawnTraffic(ClientWorldState& world)
{
    auto& visualShips = world.visualShips();

    

    std::cout
        << "[StationTraffic] spawnTraffic BEGIN visualShips="
        << visualShips.size()
        << std::endl;

    const auto& desc =
        EliteCobraMk1::EliteCobraMk1Descriptor();

    const auto& assembly =
        game::ship::geometry::AssemblyMeshLibrary::get(desc.typeId);

    m_ships.clear();
    m_ships.reserve(TrafficShipCount);

    for (int i = 0; i < TrafficShipCount; ++i)
    {
        const uint32_t seed = uint32_t(i + 1);

        TrafficShip traffic;

        traffic.id = 800000u + uint32_t(i);

        traffic.basePhase =
            pseudoRandom01(seed * 31u) * TwoPi;

        traffic.phase = traffic.basePhase;

        traffic.linearSpeed =
            35.0f + pseudoRandom01(seed * 71u) * 65.0f;

        traffic.radius =
            900.0f + pseudoRandom01(seed * 113u) * 4200.0f;

        traffic.height =
            -650.0f + pseudoRandom01(seed * 151u) * 1500.0f;

        traffic.laneTilt =
            -0.35f + pseudoRandom01(seed * 191u) * 0.70f;

        traffic.laneOffset =
            pseudoRandom01(seed * 223u) * TwoPi;

        traffic.routeType =
            int(seed % 3u);

        game::visual::VisualShip visual;

        visual.id = traffic.id;
        visual.shipType = "Cobra MK1";
        visual.shipName =
            "Station Traffic " + std::to_string(i + 1);

        visual.descriptor = &desc;
        visual.assembly = &assembly;
        visual.visible = true;

        visual.visualScale =
            0.85f + pseudoRandom01(seed * 271u) * 0.35f;

        const glm::vec3 pos =
            computePosition(traffic);

        const glm::vec3 forward =
            computeForward(traffic);


               
        visual.transform.setWorldPositionMeters(glm::dvec3(pos));

        visual.renderTransform.setWorldPosition(visual.transform.worldPosition);

        visual.transform.orientation =
            makeOrientation(forward, {0.0f, 1.0f, 0.0f});

        visual.renderTransform = visual.transform;
        visual.velocity = forward * traffic.linearSpeed;

        traffic.visualIndex = visualShips.size();
        visualShips.push_back(visual);
        m_ships.push_back(traffic);

    }


    std::cout
    << "[StationTraffic] spawnTraffic END visualShips="
    << visualShips.size()
    << " trafficShips="
    << m_ships.size()
    << std::endl;
}

void StationTrafficSystem::update(ClientWorldState& world, float dt)
{
    if (!m_active)
        return;

    dt = std::clamp(dt, 0.0f, 0.05f);
m_trafficTime += static_cast<double>(dt);

auto& visualShips = world.visualShips();

for (auto& traffic : m_ships)
{
    if (traffic.visualIndex >= visualShips.size())
        continue;

    auto& visual =
        visualShips[traffic.visualIndex];

    // Защита на случай, если visualShips был пересобран/почищен,
    // а индексы traffic ещё не обновлены.
    if (visual.id != traffic.id)
        continue;

    const float safeRadius =
        std::max(traffic.radius, 1.0f);

    const float angularSpeed =
        traffic.linearSpeed / safeRadius;

    traffic.phase +=
        static_cast<double>(angularSpeed) *
        static_cast<double>(dt);

    const glm::vec3 pos =
        computePosition(traffic);

    const glm::vec3 forward =
        computeForward(traffic);

    const glm::mat4 targetOrientation =
        makeOrientation(
            forward,
            {0.0f, 1.0f, 0.0f}
        );

    visual.transform.setWorldPositionMeters(
        glm::dvec3(pos)
    );

    visual.transform.orientation =
        targetOrientation;

    visual.renderTransform =
        visual.transform;

    visual.velocity =
        forward * traffic.linearSpeed;
}

}

glm::vec3 StationTrafficSystem::computePosition(
    const TrafficShip& ship
) const
{
    const double a =
        ship.phase + ship.laneOffset;

    const double x =
    std::cos(a) * static_cast<double>(ship.radius);

const double z =
    std::sin(a) * static_cast<double>(ship.radius);

double y =
    static_cast<double>(ship.height) +
    std::sin(a * 2.0 + static_cast<double>(ship.laneTilt)) * 180.0;

if (ship.routeType == 1)
{
    y += std::sin(a * 0.5) * 420.0;
}
else if (ship.routeType == 2)
{
    y += std::cos(a * 0.75) * 260.0;
}

return {
    static_cast<float>(x),
    static_cast<float>(2150.0 + y),
    static_cast<float>(2450.0 + z)
};
}

glm::vec3 StationTrafficSystem::computeForward(
    const TrafficShip& ship
) const
{
    const double  a =
        ship.phase + ship.laneOffset;

    glm::vec3 tangent {
        static_cast<float>(-std::sin(a)),
        static_cast<float>(0.05 * std::cos(a * 2.0 + static_cast<double>(ship.laneTilt))),
        static_cast<float>(std::cos(a))
    };

    if (ship.routeType == 1)
    {
        tangent.y += 0.10f * std::cos(a * 0.5f);
    }
    else if (ship.routeType == 2)
    {
        tangent.y -= 0.08f * std::sin(a * 0.75f);
    }

    return safeNormalize(tangent, {0.0f, 0.0f, -1.0f});
}

glm::mat4 StationTrafficSystem::makeOrientation(
    const glm::vec3& forward,
    const glm::vec3& upHint
) const
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

    m[0] = glm::vec4(r, 0.0f);
    m[1] = glm::vec4(u, 0.0f);
    m[2] = glm::vec4(-f, 0.0f);
    m[3] = glm::vec4(0, 0, 0, 1);

    return m;
}

} // namespace game::traffic