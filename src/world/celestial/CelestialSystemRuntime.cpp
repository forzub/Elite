#include "src/world/celestial/CelestialSystemRuntime.h"

#include <cmath>
#include <unordered_map>

namespace world::celestial
{

namespace
{
constexpr double TwoPi = 6.28318530717958647692;
}

void CelestialSystemRuntime::setSystem(
    const CelestialSystemDefinition* definition
)
{
    m_definition = definition;

    m_snapshot = CelestialSystemSnapshot{};

    if (!m_definition)
        return;

    m_snapshot.systemId = m_definition->systemId;
    m_snapshot.systemName = m_definition->name;
}

void CelestialSystemRuntime::update(double simTimeSeconds)
{
    m_snapshot.bodies.clear();
    m_snapshot.simTimeSeconds = simTimeSeconds;

    if (!m_definition)
        return;

    std::unordered_map<std::string, glm::dvec3> worldAuById;

    for (const auto& body : m_definition->bodies)
    {
        CelestialBodyState state;
        state.id = body.id;
        state.name = body.name;
        state.type = body.type;
        state.parentId = body.parentId;
        state.environmentPresetId = body.environmentPresetId;
        state.radiusKm = body.radiusKm;

        state.dayLengthHours =
            body.dayLengthHours;

        state.axialTiltDeg =
            body.axialTiltDeg;

        state.axisNodeDeg =
            body.axisNodeDeg;

        state.textureLongitudeOffsetDeg =
            body.textureLongitudeOffsetDeg;

        state.rings = body.rings;

        glm::dvec3 parentAu {0.0};

        if (!body.parentId.empty())
        {
            auto it = worldAuById.find(body.parentId);
            if (it != worldAuById.end())
                parentAu = it->second;
        }

        const glm::dvec3 relativeAu =
            computeRelativePositionAu(body, simTimeSeconds);

        state.positionAu = parentAu + relativeAu;
        state.worldMeters = state.positionAu * MetersPerAu;

        if (body.orbitalPeriodDays > 0.0)
        {
            state.orbitalPhaseRad =
                std::fmod(
                    simTimeSeconds / (body.orbitalPeriodDays * SecondsPerDay),
                    1.0
                ) * TwoPi;
        }

        if (body.dayLengthHours > 0.0)
        {
            state.rotationPhaseRad =
                std::fmod(
                    simTimeSeconds / (body.dayLengthHours * 3600.0),
                    1.0
                ) * TwoPi;

            state.rotationPhaseRad +=
                body.rotationOffsetDeg *
                TwoPi /
                360.0;
        }

        worldAuById[state.id] = state.positionAu;
        m_snapshot.bodies.push_back(std::move(state));
    }
}

glm::dvec3 CelestialSystemRuntime::computeRelativePositionAu(
    const CelestialBodyDefinition& body,
    double simTimeSeconds
) const
{
    if (body.type == BodyType::Star)
        return body.staticPositionAu;

    if (body.distanceAu <= 0.0)
        return body.staticPositionAu;

    double phase = 0.0;

    if (body.orbitalPeriodDays > 0.0)
    {
        phase =
            std::fmod(
                simTimeSeconds / (body.orbitalPeriodDays * SecondsPerDay),
                1.0
            ) * TwoPi;
    }

    // Пока все орбиты круговые в XZ.
    // Это не финальная астрономия, но это уже нормальный runtime-слой:
    // definition не меняется, меняется только snapshot.
    return glm::dvec3(
        std::cos(phase) * body.distanceAu,
        0.0,
        std::sin(phase) * body.distanceAu
    );
}

} // namespace world::celestial