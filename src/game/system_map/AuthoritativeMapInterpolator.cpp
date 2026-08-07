#include "src/game/system_map/AuthoritativeMapInterpolator.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>

#include <glm/geometric.hpp>

namespace game::system_map
{
namespace
{
constexpr double TwoPi = 6.28318530717958647692;
constexpr double Pi = 3.14159265358979323846;

glm::dvec3 lerpVector(
    const glm::dvec3& a,
    const glm::dvec3& b,
    double alpha
)
{
    return a + (b - a) * alpha;
}

double lerpScalar(double a, double b, double alpha)
{
    return a + (b - a) * alpha;
}

double lerpAngle(double a, double b, double alpha)
{
    double delta = std::fmod(b - a + Pi, TwoPi);
    if (delta < 0.0)
        delta += TwoPi;
    delta -= Pi;
    return a + delta * alpha;
}

glm::dvec3 safeNormalize(
    const glm::dvec3& value,
    const glm::dvec3& fallback
)
{
    const double length = glm::length(value);
    return length > 0.000000001 ? value / length : fallback;
}

world::celestial::LocalSceneAxes interpolateAxes(
    const world::celestial::LocalSceneAxes& a,
    const world::celestial::LocalSceneAxes& b,
    double alpha
)
{
    world::celestial::LocalSceneAxes result;

    result.x = safeNormalize(lerpVector(a.x, b.x, alpha), a.x);
    result.y = safeNormalize(lerpVector(a.y, b.y, alpha), a.y);
    result.z = safeNormalize(
        glm::cross(result.x, result.y),
        safeNormalize(lerpVector(a.z, b.z, alpha), a.z)
    );
    result.y = safeNormalize(glm::cross(result.z, result.x), result.y);

    return result;
}

std::string objectKey(
    const world::celestial::LocalSceneObject& object
)
{
    if (!object.stableId.empty())
        return "stable:" + object.stableId;

    return "entity:" + std::to_string(object.id.value);
}

void interpolateOrbit(
    const world::celestial::DetailMapOrbit& from,
    world::celestial::DetailMapOrbit& to,
    double alpha
)
{
    to.centerMeters = lerpVector(from.centerMeters, to.centerMeters, alpha);
    to.positionMeters = lerpVector(from.positionMeters, to.positionMeters, alpha);
    to.velocityMps = lerpVector(from.velocityMps, to.velocityMps, alpha);
    to.radiusMeters = lerpScalar(from.radiusMeters, to.radiusMeters, alpha);
    to.altitudeMeters = lerpScalar(from.altitudeMeters, to.altitudeMeters, alpha);
    to.speedMps = lerpScalar(from.speedMps, to.speedMps, alpha);
    to.radialAxis = safeNormalize(
        lerpVector(from.radialAxis, to.radialAxis, alpha),
        from.radialAxis
    );
    to.progradeAxis = safeNormalize(
        lerpVector(from.progradeAxis, to.progradeAxis, alpha),
        from.progradeAxis
    );
    to.normalAxis = safeNormalize(
        glm::cross(to.radialAxis, to.progradeAxis),
        safeNormalize(
            lerpVector(from.normalAxis, to.normalAxis, alpha),
            from.normalAxis
        )
    );
    to.progradeAxis = safeNormalize(
        glm::cross(to.normalAxis, to.radialAxis),
        to.progradeAxis
    );
}

void interpolateObjects(
    const std::vector<world::celestial::LocalSceneObject>& fromObjects,
    std::vector<world::celestial::LocalSceneObject>& toObjects,
    double alpha
)
{
    std::unordered_map<
        std::string,
        const world::celestial::LocalSceneObject*
    > fromById;

    fromById.reserve(fromObjects.size());
    for (const auto& object : fromObjects)
        fromById[objectKey(object)] = &object;

    for (auto& object : toObjects)
    {
        const auto it = fromById.find(objectKey(object));
        if (it == fromById.end())
            continue;

        const auto& from = *it->second;
        object.positionMeters =
            lerpVector(from.positionMeters, object.positionMeters, alpha);
        object.velocityMps =
            lerpVector(from.velocityMps, object.velocityMps, alpha);
        object.axes = interpolateAxes(from.axes, object.axes, alpha);
    }
}

world::celestial::DetailMapSnapshot interpolateDetail(
    const world::celestial::DetailMapSnapshot& from,
    const world::celestial::DetailMapSnapshot& to,
    double alpha
)
{
    using namespace world::celestial;

    if (!from.valid || from.detailTarget != to.detailTarget)
        return to;

    const double t = std::clamp(alpha, 0.0, 1.0);
    DetailMapSnapshot result = to;

    result.universeTimeSeconds =
        lerpScalar(from.universeTimeSeconds, to.universeTimeSeconds, t);
    result.planetCenterMeters =
        lerpVector(from.planetCenterMeters, to.planetCenterMeters, t);
    result.planetVelocityMps =
        lerpVector(from.planetVelocityMps, to.planetVelocityMps, t);
    result.planetRadiusMeters =
        lerpScalar(from.planetRadiusMeters, to.planetRadiusMeters, t);
    result.planetRotationPhaseRad =
        lerpAngle(from.planetRotationPhaseRad, to.planetRotationPhaseRad, t);
    result.scene.originWorldMeters =
        lerpVector(from.scene.originWorldMeters, to.scene.originWorldMeters, t);

    std::unordered_map<std::string, const DetailMapOrbit*> fromHubOrbits;
    for (const auto& orbit : from.hubOrbits)
        fromHubOrbits[orbit.id] = &orbit;
    for (auto& orbit : result.hubOrbits)
    {
        const auto it = fromHubOrbits.find(orbit.id);
        if (it != fromHubOrbits.end())
            interpolateOrbit(*it->second, orbit, t);
    }

    std::unordered_map<std::string, const DetailMapOrbit*> fromPlayerOrbits;
    for (const auto& orbit : from.playerOrbits)
        fromPlayerOrbits[orbit.id] = &orbit;
    for (auto& orbit : result.playerOrbits)
    {
        const auto it = fromPlayerOrbits.find(orbit.id);
        if (it != fromPlayerOrbits.end())
            interpolateOrbit(*it->second, orbit, t);
    }

    interpolateObjects(from.scene.objects, result.scene.objects, t);
    return result;
}

world::celestial::HubMapSnapshot interpolateHub(
    const world::celestial::HubMapSnapshot& from,
    const world::celestial::HubMapSnapshot& to,
    double alpha
)
{
    using namespace world::celestial;

    if (!from.valid ||
        from.systemId != to.systemId ||
        from.hubId != to.hubId)
    {
        return to;
    }

    const double t = std::clamp(alpha, 0.0, 1.0);
    HubMapSnapshot result = to;

    result.universeTimeSeconds =
        lerpScalar(from.universeTimeSeconds, to.universeTimeSeconds, t);
    result.hubWorldPositionMeters =
        lerpVector(from.hubWorldPositionMeters, to.hubWorldPositionMeters, t);
    result.hubWorldVelocityMps =
        lerpVector(from.hubWorldVelocityMps, to.hubWorldVelocityMps, t);
    result.hubWorldAxes =
        interpolateAxes(from.hubWorldAxes, to.hubWorldAxes, t);
    result.parentPlanetWorldPositionMeters =
        lerpVector(
            from.parentPlanetWorldPositionMeters,
            to.parentPlanetWorldPositionMeters,
            t
        );
    result.parentPlanetCenterLocalMeters =
        lerpVector(
            from.parentPlanetCenterLocalMeters,
            to.parentPlanetCenterLocalMeters,
            t
        );
    result.parentPlanetRotationPhaseRad =
        lerpAngle(
            from.parentPlanetRotationPhaseRad,
            to.parentPlanetRotationPhaseRad,
            t
        );
    result.scene.originWorldMeters =
        lerpVector(from.scene.originWorldMeters, to.scene.originWorldMeters, t);

    interpolateObjects(from.scene.objects, result.scene.objects, t);
    return result;
}

} // namespace

void AuthoritativeMapInterpolator::acceptDetail(
    const world::celestial::DetailMapSnapshot& snapshot,
    double blendDurationSeconds
)
{
    const bool sameTarget =
        m_hasDetail &&
        m_detailVisual.valid &&
        m_detailVisual.detailTarget == snapshot.detailTarget;

    if (!sameTarget || blendDurationSeconds <= 0.0)
    {
        m_detailFrom = snapshot;
        m_detailTo = snapshot;
        m_detailVisual = snapshot;
        m_detailElapsedSeconds = 0.0;
        m_detailDurationSeconds = 0.0;
        m_hasDetail = snapshot.valid;
        return;
    }

    // Both interpolation endpoints are confirmed server frames.
    // Never seed a new blend from a presentation-only intermediate pose.
    m_detailFrom = m_detailTo;
    m_detailTo = snapshot;
    m_detailElapsedSeconds = 0.0;
    m_detailDurationSeconds =
        std::clamp(blendDurationSeconds, 0.02, 1.0);
    m_hasDetail = true;
}

void AuthoritativeMapInterpolator::acceptHub(
    const world::celestial::HubMapSnapshot& snapshot,
    double blendDurationSeconds
)
{
    const bool sameTarget =
        m_hasHub &&
        m_hubVisual.valid &&
        m_hubVisual.systemId == snapshot.systemId &&
        m_hubVisual.hubId == snapshot.hubId;

    if (!sameTarget || blendDurationSeconds <= 0.0)
    {
        m_hubFrom = snapshot;
        m_hubTo = snapshot;
        m_hubVisual = snapshot;
        m_hubElapsedSeconds = 0.0;
        m_hubDurationSeconds = 0.0;
        m_hasHub = snapshot.valid;
        return;
    }

    // Both interpolation endpoints are confirmed server frames.
    // Never seed a new blend from a presentation-only intermediate pose.
    m_hubFrom = m_hubTo;
    m_hubTo = snapshot;
    m_hubElapsedSeconds = 0.0;
    m_hubDurationSeconds =
        std::clamp(blendDurationSeconds, 0.02, 1.0);
    m_hasHub = true;
}

void AuthoritativeMapInterpolator::update(double wallDeltaSeconds)
{
    const double safeDelta = std::clamp(wallDeltaSeconds, 0.0, 0.10);

    if (m_hasDetail && m_detailDurationSeconds > 0.0)
    {
        m_detailElapsedSeconds = std::min(
            m_detailElapsedSeconds + safeDelta,
            m_detailDurationSeconds
        );
        updateDetail();
    }

    if (m_hasHub && m_hubDurationSeconds > 0.0)
    {
        m_hubElapsedSeconds = std::min(
            m_hubElapsedSeconds + safeDelta,
            m_hubDurationSeconds
        );
        updateHub();
    }
}

void AuthoritativeMapInterpolator::updateDetail()
{
    const double alpha =
        m_detailDurationSeconds > 0.0
            ? m_detailElapsedSeconds / m_detailDurationSeconds
            : 1.0;

    m_detailVisual = interpolateDetail(m_detailFrom, m_detailTo, alpha);

    if (alpha >= 1.0)
    {
        m_detailVisual = m_detailTo;
        m_detailFrom = m_detailTo;
        m_detailDurationSeconds = 0.0;
    }
}

void AuthoritativeMapInterpolator::updateHub()
{
    const double alpha =
        m_hubDurationSeconds > 0.0
            ? m_hubElapsedSeconds / m_hubDurationSeconds
            : 1.0;

    m_hubVisual = interpolateHub(m_hubFrom, m_hubTo, alpha);

    if (alpha >= 1.0)
    {
        m_hubVisual = m_hubTo;
        m_hubFrom = m_hubTo;
        m_hubDurationSeconds = 0.0;
    }
}

} // namespace game::system_map
