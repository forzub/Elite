#include "game/promo/PromoFlybyScenario.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>

#include "game/simulation/GameSimulation.h"
#include "game/ship/Ship.h"
#include "game/ship/ShipInitData.h"
#include "game/ship/ShipRegistry.h"
#include "game/ship/ShipRoleType.h"
#include "game/ship/ShipVisualIdentity.h"
#include "game/ship/descriptors/EliteCobraMk1.h"
#include "game/player/ActorIdProvider.h"

namespace game::promo
{

namespace
{
constexpr float PI = 3.14159265358979323846f;

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




} // namespace







float PromoFlybyScenario::smootherStep(float x)
{
    x = std::clamp(x, 0.0f, 1.0f);
    return x * x * x * (x * (x * 6.0f - 15.0f) + 10.0f);
}

void PromoFlybyScenario::setup(GameSimulation& sim)
{
    if (m_active)
        return;

    const auto& desc =
        EliteCobraMk1::EliteCobraMk1Descriptor();

    // Для промо берём физику корабля, но не боевую скорость.
    // Камере нужна читаемость, а не "9 мух пролетели через объектив".
    m_speed = std::clamp(
        desc.physics.maxCombatSpeed * 0.28f,
        70.0f,
        120.0f
    );

    // Станция большая, поэтому нужен кинематографический безопасный коридор.
    // Не опираемся напрямую на turnRadius: он слишком мал для красивого облёта станции.
    m_orbitRadius = 1150.0f;
    m_verticalSpread = 260.0f;

    // Медленнее реальной скорости, чтобы манёвр читался.
    m_angularSpeed = 0.18f;

    spawnPromoShips(sim);

    m_time = 0.0f;
    m_active = true;

    std::cout
        << "[PromoFlybyScenario] enabled ships="
        << m_ships.size()
        << " speed=" << m_speed
        << " orbitRadius=" << m_orbitRadius
        << " angularSpeed=" << m_angularSpeed
        << "\n";
}

void PromoFlybyScenario::spawnPromoShips(GameSimulation& sim)
{
    m_ships.clear();

    const auto& desc =
        EliteCobraMk1::EliteCobraMk1Descriptor();

    int instanceId = 1000;

    for (int group = 0; group < 3; ++group)
    {
        for (int slot = 0; slot < 3; ++slot)
        {
            char name[64];
            std::snprintf(
                name,
                sizeof(name),
                "Cobretty Promo %d-%d",
                group + 1,
                slot + 1
            );

            ShipVisualIdentity visualIdentity {
                .shipType = "Cobra MK1",
                .shipName = name
            };

            char reg[64];
            std::snprintf(
                reg,
                sizeof(reg),
                "PR-%02d%02d",
                group + 1,
                slot + 1
            );

            ShipRegistry registry {
                .instanceId = static_cast<ShipInstanceId>(instanceId++),
                .ownerName = "Promo Wing",
                .ownerActor = ActorIds::Unknown(),
                .registrationId = reg,
                .homePort = "Lexie Liu Station",
                .shipRole = ShipRoleType::Civilian
            };

            ShipInitData initData;
            initData.visual = visualIdentity;
            initData.registry = registry;
            initData.initialInventory = {};

            const glm::vec3 start =
                approachCenter(0.0f) +
                formationOffset(group, slot);

            const glm::vec3 forward =
                safeNormalize(
                    m_stationPos - start,
                    {0.0f, 0.0f, -1.0f}
                );

            EntityId id = sim.spawnShip(
                ShipRole::NPC,
                desc,
                start,
                initData,
                makeOrientation(forward, {0.0f, 1.0f, 0.0f})
            );

            m_ships.push_back({
                id,
                group,
                slot
            });
        }
    }
}

glm::vec3 PromoFlybyScenario::formationOffset(
    int groupIndex,
    int slotIndex
) const
{
    // 9 кораблей в широком клине:
    // group = мини-группа / эшелон,
    // slot  = место внутри мини-звена.
    const float groupX =
        (static_cast<float>(groupIndex) - 1.0f) * 42.0f;

    const float groupY =
        (1.0f - static_cast<float>(groupIndex)) * 10.0f;

    const float slotX =
        (static_cast<float>(slotIndex) - 1.0f) * 18.0f;

    const float slotZ =
        -std::abs(static_cast<float>(slotIndex) - 1.0f) * 20.0f;

    return {
        groupX + slotX,
        groupY,
        slotZ
    };
}

glm::vec3 PromoFlybyScenario::approachCenter(float t) const
{
    // Корабли подходят к станции по диагонали,
    // но не целятся в центр станции.
    const glm::vec3 start =
        m_stationPos + glm::vec3(-900.0f, 220.0f, 2300.0f);

    const glm::vec3 end =
        m_stationPos + glm::vec3(-720.0f, 160.0f, 980.0f);

    return lerpVec3(start, end, smootherStep(t));
}

glm::vec3 PromoFlybyScenario::rejoinCenter(float t) const
{
    // Не уводим звено в даль.
    // Точка сборки находится перед станцией на безопасном радиусе,
    // чтобы следующий цикл не выглядел как исчезновение.
    const glm::vec3 start =
        m_stationPos + glm::vec3(720.0f, 165.0f, -980.0f);

    const glm::vec3 end =
        m_stationPos + glm::vec3(-720.0f, 160.0f, 980.0f);

    return lerpVec3(start, end, smootherStep(t));
}

glm::vec3 PromoFlybyScenario::orbitGroupCenter(
    int groupIndex,
    float angle
) const
{
    // Три группы идут по разным дорожкам вокруг станции.
    // Все держат безопасный радиус и не проходят через центр.
    const float laneOffset =
        static_cast<float>(groupIndex - 1);

    const float laneAngle =
        angle + laneOffset * 0.42f;

    const float radius =
        m_orbitRadius + laneOffset * 120.0f;

    const float x =
        std::cos(laneAngle) * radius;

    const float z =
        std::sin(laneAngle) * radius;

    const float y =
        120.0f + laneOffset * m_verticalSpread;

    return m_stationPos + glm::vec3(x, y, z);
}

glm::mat4 PromoFlybyScenario::makeOrientation(
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
    // local +X = right
    // local +Y = up
    // local -Z = forward
    m[0] = glm::vec4(r, 0.0f);
    m[1] = glm::vec4(u, 0.0f);
    m[2] = glm::vec4(-f, 0.0f);
    m[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

    return m;
}

void PromoFlybyScenario::applyShipPose(
    Ship& ship,
    const glm::vec3& position,
    const glm::vec3& forward,
    const glm::vec3& up,
    float speed
) const
{
    auto& tr = ship.core().transform();

    const glm::vec3 f =
        safeNormalize(forward, tr.forward());

    tr.position = position;
    tr.orientation = makeOrientation(f, up);

    tr.forwardVelocity = speed;
    tr.targetSpeed = speed;
    tr.localVelocity = glm::vec3(0.0f, 0.0f, -speed);

    tr.pitchRate = 0.0f;
    tr.yawRate = 0.0f;
    tr.rollRate = 0.0f;

    auto& control = ship.core().control();

    control.cruiseActive = false;
    control.jumpActive = false;

    control.pitchInput = 0.0f;
    control.yawInput = 0.0f;
    control.rollInput = 0.0f;

    control.forwardInput = 0.0f;
    control.strafeInput = 0.0f;
    control.liftInput = 0.0f;

    control.targetSpeedRate = 0.0f;
}

void PromoFlybyScenario::update(GameSimulation& sim, float dt)
{
    if (!m_active)
        return;

    m_time += dt;

    // Полный цикл:
    // approach -> split -> orbit -> rejoin
    const float approachDuration = 7.0f;
const float splitDuration    = 6.0f;
const float orbitDuration    = 18.0f;
const float rejoinDuration   = 6.0f;

    const float cycle =
        approachDuration +
        splitDuration +
        orbitDuration +
        rejoinDuration;

    const float t =
        std::fmod(m_time, cycle);

    enum class Phase
    {
        Approach,
        Split,
        Orbit,
        Rejoin
    };

    Phase phase = Phase::Approach;
    float phaseT = 0.0f;

    if (t < approachDuration)
    {
        phase = Phase::Approach;
        phaseT = t / approachDuration;
    }
    else if (t < approachDuration + splitDuration)
    {
        phase = Phase::Split;
        phaseT = (t - approachDuration) / splitDuration;
    }
    else if (t < approachDuration + splitDuration + orbitDuration)
    {
        phase = Phase::Orbit;
        phaseT =
            (t - approachDuration - splitDuration) /
            orbitDuration;
    }
    else
    {
        phase = Phase::Rejoin;
        phaseT =
            (t - approachDuration - splitDuration - orbitDuration) /
            rejoinDuration;
    }

    const float orbitStartAngle = 2.35f;

// Важно: угол крутится от общего времени, а не от t внутри fmod.
// Тогда корабли не "сбрасывают" орбиту при новом цикле.
const float baseAngle =
    orbitStartAngle + m_time * m_angularSpeed;

    auto& ships = sim.ships();

    for (const PromoShip& ps : m_ships)
    {
        auto it = ships.find(ps.id);

        if (it == ships.end() || !it->second)
            continue;

        Ship& ship = *it->second;

        const int group = ps.groupIndex;
        const int slot  = ps.slotIndex;

        const glm::vec3 compact =
            approachCenter(1.0f) +
            formationOffset(group, slot);

        // Фиксированная точка входа в орбиту.
        // Не используем baseAngle на split, иначе точка входа будет "убегать".
        const float entryAngle =
            orbitStartAngle + static_cast<float>(group - 1) * 0.25f;

        const glm::vec3 orbitEntry =
            orbitGroupCenter(group, entryAngle) +
            formationOffset(1, slot) * 0.55f;

        const glm::vec3 orbit =
            orbitGroupCenter(group, baseAngle) +
            formationOffset(1, slot) * 0.55f;

        glm::vec3 pos = compact;

        if (phase == Phase::Approach)
        {
            pos =
                approachCenter(phaseT) +
                formationOffset(group, slot);
        }
        else if (phase == Phase::Split)
        {
            pos =
                lerpVec3(
                    compact,
                    orbitEntry,
                    smootherStep(phaseT)
                );
        }
        else if (phase == Phase::Orbit)
        {
            pos = orbit;
        }
        else
{
    // Сборка не уводит корабли в даль.
    // Они сжимаются в строй на той же орбитальной дорожке.
    const glm::vec3 rejoin =
        orbitGroupCenter(1, baseAngle) +
        formationOffset(group, slot);

    pos =
        lerpVec3(
            orbit,
            rejoin,
            smootherStep(phaseT)
        );
}

        glm::vec3 lookPos =
            pos + glm::vec3(0.0f, 0.0f, -1.0f);

        if (phase == Phase::Orbit)
        {
            const float lookAngle =
                baseAngle + 0.08f * m_angularSpeed;

            lookPos =
                orbitGroupCenter(group, lookAngle) +
                formationOffset(1, slot) * 0.55f;
        }
        else if (phase == Phase::Split)
        {
            lookPos =
                lerpVec3(
                    compact,
                    orbitEntry,
                    smootherStep(std::min(1.0f, phaseT + 0.04f))
                );
        }
        else if (phase == Phase::Approach)
        {
            lookPos =
                approachCenter(std::min(1.0f, phaseT + 0.04f)) +
                formationOffset(group, slot);
        }
        else
        {
            const float lookAngle =
                baseAngle + 0.08f * m_angularSpeed;

            lookPos =
                orbitGroupCenter(1, lookAngle) +
                formationOffset(group, slot);
        }

        glm::vec3 forward =
            safeNormalize(
                lookPos - pos,
                {0.0f, 0.0f, -1.0f}
            );

        glm::vec3 up =
            {0.0f, 1.0f, 0.0f};

        if (phase == Phase::Orbit || phase == Phase::Split)
        {
            const glm::vec3 toStation =
                safeNormalize(
                    m_stationPos - pos,
                    {0.0f, -1.0f, 0.0f}
                );

            up =
                safeNormalize(
                    glm::mix(
                        glm::vec3(0.0f, 1.0f, 0.0f),
                        -toStation,
                        0.35f
                    ),
                    {0.0f, 1.0f, 0.0f}
                );
        }

        applyShipPose(
            ship,
            pos,
            forward,
            up,
            m_speed
        );
    }
}

} // namespace game::promo