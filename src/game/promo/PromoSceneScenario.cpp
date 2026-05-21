#include "game/promo/PromoSceneScenario.h"
#include "game/promo/PromoSceneTuning.h"

#include <algorithm>
#include <cmath>
#include <string>

#include <glm/gtx/norm.hpp>

#include "src/game/client/ClientWorldState.h"
#include "src/game/geometry/AssemblyMeshLibrary.h"
#include "src/game/ship/descriptors/EliteCobraMk1.h"

#include "src/world/coordinates/WorldPosition.h"
namespace game::promo
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

glm::vec3 lerpVec3(
    const glm::vec3& a,
    const glm::vec3& b,
    float t
)
{
    return a + (b - a) * t;
}

bool isPromoVisualShip(game::visual::VisualShipId id)
{
    return id >= 900000u && id < 910000u;
}

} // namespace

float PromoSceneScenario::smootherStep(float x)
{
    x = std::clamp(x, 0.0f, 1.0f);
    return x * x * x * (x * (x * 6.0f - 15.0f) + 10.0f);
}

void PromoSceneScenario::setup(ClientWorldState& world)
{
    if (m_active)
        return;

    const auto& desc =
        EliteCobraMk1::EliteCobraMk1Descriptor();

    // Скорость настраиваемая, но пока берём безопасную часть от ТТХ.
    m_wingSpeed =
    std::clamp(
        desc.physics.maxCombatSpeed * tuning::WingSpeedFactor,
        tuning::WingSpeedMin,
        tuning::WingSpeedMax
    );

    m_flightDir =
        safeNormalize(m_flightDir, {0.0f, 0.0f, -1.0f});

    spawnWing(world);

    m_time = 0.0f;
    m_active = true;
}

void PromoSceneScenario::reset(ClientWorldState& world)
{
    // PromoSceneScenario owns only client-side visual promo ships.
    // When returning to normal game mode, remove them and let regular
    // replicated ships/rendering take over.
    auto& ships = world.visualShips();

    ships.erase(
        std::remove_if(
            ships.begin(),
            ships.end(),
            [](const game::visual::VisualShip& ship)
            {
                return isPromoVisualShip(ship.id);
            }
        ),
        ships.end()
    );

    m_wing.clear();
    m_time = 0.0f;
    m_active = false;
    m_lastCameraTarget = glm::vec3(0.0f, 100.0f, 0.0f);
}

void PromoSceneScenario::spawnWing(ClientWorldState& world)
{
    auto& visualShips =
        world.visualShips();

    visualShips.erase(
        std::remove_if(
            visualShips.begin(),
            visualShips.end(),
            [](const game::visual::VisualShip& ship)
            {
                return isPromoVisualShip(ship.id);
            }
        ),
        visualShips.end()
    );

    m_wing.clear();

    const auto& desc =
        EliteCobraMk1::EliteCobraMk1Descriptor();

    const auto& assembly =
        game::ship::geometry::AssemblyMeshLibrary::get(desc.typeId);

    game::visual::VisualShipId nextId = 900000;

    for (int group = 0; group < 3; ++group)
    {
        for (int slot = 0; slot < 3; ++slot)
        {
            game::visual::VisualShip ship;

            ship.id = nextId++;
            ship.shipType = "Cobra MK1";
            ship.shipName =
                "Promo Cobra " +
                std::to_string(group + 1) +
                "-" +
                std::to_string(slot + 1);

            ship.descriptor = &desc;
            ship.assembly = &assembly;

            const glm::vec3 pos =
                computeShipPosition(group, slot, 0.0f);

            ship.transform.setWorldPositionMeters(glm::dvec3(pos));
            ship.transform.orientation =
                makeOrientation(
                    m_flightDir,
                    {0.0f, 1.0f, 0.0f}
                );

            ship.renderTransform = ship.transform;
            ship.velocity = m_flightDir * m_wingSpeed;

            visualShips.push_back(ship);

            m_wing.push_back({
                ship.id,
                group,
                slot
            });
        }
    }
}

glm::vec3 PromoSceneScenario::formationOffset(
    int groupIndex,
    int slotIndex
) const
{
    // Широкий клин:
    // groupIndex: левая/центр/правая группа
    // slotIndex: место в мини-звене
    const float groupX =
        (static_cast<float>(groupIndex) - 1.0f) * 92.0f;

    const float slotX =
        (static_cast<float>(slotIndex) - 1.0f) * 30.0f;

    const float slotZ =
        -std::abs(static_cast<float>(slotIndex) - 1.0f) * 36.0f;

    const float groupY =
        (1.0f - static_cast<float>(groupIndex)) * 8.0f;

    return {
        groupX + slotX,
        groupY,
        slotZ
    };
}




glm::vec3 PromoSceneScenario::computeShipPosition(
    int groupIndex,
    int slotIndex,
    float t
) const
{
    const float forwardDist =
        -m_spawnDistance + m_wingSpeed * t;

    glm::vec3 base =
        m_playerPos + m_flightDir * forwardDist;

    base.y =
        m_playerPos.y + m_passHeight;

    glm::vec3 offset =
        formationOffset(groupIndex, slotIndex);

    // split начинается только после того, как звено прошло игрока.
    float splitT =
        (forwardDist - m_splitAfterPlayerDistance) / tuning::SplitDurationDistance;;

    splitT =
        smootherStep(splitT);

    glm::vec3 splitOffset {0.0f};

    if (groupIndex == 0)
    {
        // Левая группа: влево, чуть вверх, немного вперёд.
        splitOffset =
            glm::vec3(
                -m_sideSplitDistance,
                180.0f,
                -420.0f
            ) * splitT;
    }
    else if (groupIndex == 1)
    {
        // Центральная группа остаётся в центре кадра:
        // вперёд + вверх.
        splitOffset =
            glm::vec3(
                0.0f,
                m_centerClimb,
                -620.0f
            ) * splitT;
    }
    else
    {
        // Правая группа.
        splitOffset =
            glm::vec3(
                m_sideSplitDistance,
                180.0f,
                -420.0f
            ) * splitT;
    }

    return base + offset + splitOffset;
}





glm::mat4 PromoSceneScenario::makeOrientation(
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

    // Engine convention:
    // +X right, +Y up, -Z forward
    m[0] = glm::vec4(r, 0.0f);
    m[1] = glm::vec4(u, 0.0f);
    m[2] = glm::vec4(-f, 0.0f);
    m[3] = glm::vec4(0, 0, 0, 1);

    return m;
}

void PromoSceneScenario::update(
    ClientWorldState& world,
    float dt
)
{
    if (!m_active)
        return;

    m_time += dt;

    // Зацикливаем пролёт, чтобы звено возвращалось.
    const float cycleDuration = 36.0f;
    const float localT =
        std::fmod(m_time, cycleDuration);

    auto& ships =
        world.visualShips();

    for (auto& ship : ships)
    {
        auto it =
            std::find_if(
                m_wing.begin(),
                m_wing.end(),
                [&](const WingShipRef& ref)
                {
                    return ref.id == ship.id;
                }
            );

        if (it == m_wing.end())
            continue;

        const glm::vec3 posNow =
            computeShipPosition(
                it->groupIndex,
                it->slotIndex,
                localT
            );

        const glm::vec3 posAhead =
            computeShipPosition(
                it->groupIndex,
                it->slotIndex,
                localT + 0.05f
            );

        const glm::vec3 forward =
            safeNormalize(
                posAhead - posNow,
                m_flightDir
            );

        ship.transform.setWorldPositionMeters(glm::dvec3(posNow));
        ship.transform.orientation =
            makeOrientation(
                forward,
                {0.0f, 1.0f, 0.0f}
            );

        ship.renderTransform = ship.transform;
        ship.velocity = forward * m_wingSpeed;
    }

    m_lastCameraTarget =
        cameraTarget(world);
}





bool PromoSceneScenario::cameraTargetValid(
    const ClientWorldState& world
) const
{
    if (!m_active)
        return false;

    const auto& ships =
        world.visualShips();

    for (const auto& ship : ships)
    {
        if (ship.visible)
            return true;
    }

    return false;
}

glm::vec3 PromoSceneScenario::cameraTarget(
    const ClientWorldState& world
) const
{
    const auto& ships =
        world.visualShips();

    glm::vec3 sum {0.0f};
    int count = 0;

    for (const auto& ship : ships)
    {
        if (!ship.visible)
            continue;

        // Камера следит за центральной группой.
        // По текущей схеме groupIndex == 1.
        for (const WingShipRef& ref : m_wing)
        {
            if (ref.id == ship.id && ref.groupIndex == 1)
            {
                sum += world::coordinates::legacyFloatMeters(
                    ship.renderTransform.worldPosition
                );
                ++count;
                break;
            }
        }
    }

    if (count <= 0)
        return m_lastCameraTarget;

    return sum / static_cast<float>(count);
}






} // namespace game::promo