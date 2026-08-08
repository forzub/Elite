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

/*
    A map response is intentionally much slower than the render loop. At high
    universe-time scales a linear position blend turns an orbit into a visible
    polygon: position stays continuous, but its tangent changes at every map
    response. Cubic Hermite sampling uses only authoritative endpoint position
    and velocity and therefore keeps both position and first derivative smooth.
*/
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

glm::dvec3 hermitePosition(
    const glm::dvec3& p0,
    const glm::dvec3& v0,
    const glm::dvec3& p1,
    const glm::dvec3& v1,
    double universeIntervalSeconds,
    double alpha
)
{
    if (!std::isfinite(universeIntervalSeconds) ||
        std::abs(universeIntervalSeconds) <= 1.0e-9)
    {
        return lerpVector(p0, p1, alpha);
    }

    const double t = std::clamp(alpha, 0.0, 1.0);
    const double t2 = t * t;
    const double t3 = t2 * t;

    const double h00 = 2.0 * t3 - 3.0 * t2 + 1.0;
    const double h10 = t3 - 2.0 * t2 + t;
    const double h01 = -2.0 * t3 + 3.0 * t2;
    const double h11 = t3 - t2;

    return
        h00 * p0 +
        h10 * (v0 * universeIntervalSeconds) +
        h01 * p1 +
        h11 * (v1 * universeIntervalSeconds);
}

glm::dvec3 hermiteVelocity(
    const glm::dvec3& p0,
    const glm::dvec3& v0,
    const glm::dvec3& p1,
    const glm::dvec3& v1,
    double universeIntervalSeconds,
    double alpha
)
{
    if (!std::isfinite(universeIntervalSeconds) ||
        std::abs(universeIntervalSeconds) <= 1.0e-9)
    {
        return lerpVector(v0, v1, alpha);
    }

    const double t = std::clamp(alpha, 0.0, 1.0);
    const double t2 = t * t;

    const double dh00 = 6.0 * t2 - 6.0 * t;
    const double dh10 = 3.0 * t2 - 4.0 * t + 1.0;
    const double dh01 = -6.0 * t2 + 6.0 * t;
    const double dh11 = 3.0 * t2 - 2.0 * t;

    return
        (
            dh00 * p0 +
            dh10 * (v0 * universeIntervalSeconds) +
            dh01 * p1 +
            dh11 * (v1 * universeIntervalSeconds)
        ) /
        universeIntervalSeconds;
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

world::celestial::LocalSceneAxes hubAxesFromKinematics(
    const glm::dvec3& hubPositionMeters,
    const glm::dvec3& hubVelocityMps,
    const glm::dvec3& parentPositionMeters,
    const glm::dvec3& parentVelocityMps,
    const world::celestial::LocalSceneAxes& fallback
)
{
    world::celestial::LocalSceneAxes result = fallback;

    const glm::dvec3 radial = safeNormalize(
        hubPositionMeters - parentPositionMeters,
        fallback.y
    );

    const glm::dvec3 relativeVelocity =
        hubVelocityMps - parentVelocityMps;

    const glm::dvec3 tangentialVelocity =
        relativeVelocity -
        radial * glm::dot(relativeVelocity, radial);

    const glm::dvec3 prograde = safeNormalize(
        tangentialVelocity,
        fallback.x
    );

    const glm::dvec3 normal = safeNormalize(
        glm::cross(prograde, radial),
        fallback.z
    );

    result.x = safeNormalize(
        glm::cross(radial, normal),
        prograde
    );
    result.y = radial;
    result.z = normal;
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

void interpolateObjects(
    const std::vector<world::celestial::LocalSceneObject>& fromObjects,
    std::vector<world::celestial::LocalSceneObject>& toObjects,
    double universeIntervalSeconds,
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
        const glm::dvec3 destinationPosition = object.positionMeters;
        const glm::dvec3 destinationVelocity = object.velocityMps;

        object.positionMeters = hermitePosition(
            from.positionMeters,
            from.velocityMps,
            destinationPosition,
            destinationVelocity,
            universeIntervalSeconds,
            alpha
        );
        object.velocityMps = hermiteVelocity(
            from.positionMeters,
            from.velocityMps,
            destinationPosition,
            destinationVelocity,
            universeIntervalSeconds,
            alpha
        );
        object.axes = interpolateAxes(from.axes, object.axes, alpha);
    }
}

void interpolateOrbit(
    const world::celestial::DetailMapOrbit& from,
    world::celestial::DetailMapOrbit& to,
    const glm::dvec3& fromCenterVelocityMps,
    const glm::dvec3& toCenterVelocityMps,
    double universeIntervalSeconds,
    double alpha
)
{
    const glm::dvec3 destinationCenter = to.centerMeters;
    const glm::dvec3 destinationPosition = to.positionMeters;
    const glm::dvec3 destinationVelocity = to.velocityMps;

    to.centerMeters = hermitePosition(
        from.centerMeters,
        fromCenterVelocityMps,
        destinationCenter,
        toCenterVelocityMps,
        universeIntervalSeconds,
        alpha
    );
    to.positionMeters = hermitePosition(
        from.positionMeters,
        from.velocityMps,
        destinationPosition,
        destinationVelocity,
        universeIntervalSeconds,
        alpha
    );
    to.velocityMps = hermiteVelocity(
        from.positionMeters,
        from.velocityMps,
        destinationPosition,
        destinationVelocity,
        universeIntervalSeconds,
        alpha
    );
    to.radiusMeters = lerpScalar(from.radiusMeters, to.radiusMeters, alpha);
    to.altitudeMeters = lerpScalar(from.altitudeMeters, to.altitudeMeters, alpha);
    to.speedMps = lerpScalar(from.speedMps, to.speedMps, alpha);

    const glm::dvec3 centerVelocity =
        lerpVector(fromCenterVelocityMps, toCenterVelocityMps, alpha);
    const glm::dvec3 relativeVelocity = to.velocityMps - centerVelocity;

    to.radialAxis = safeNormalize(
        to.positionMeters - to.centerMeters,
        from.radialAxis
    );

    const glm::dvec3 tangentialVelocity =
        relativeVelocity -
        to.radialAxis * glm::dot(relativeVelocity, to.radialAxis);

    to.progradeAxis = safeNormalize(
        tangentialVelocity,
        from.progradeAxis
    );
    to.normalAxis = safeNormalize(
        glm::cross(to.progradeAxis, to.radialAxis),
        from.normalAxis
    );
    to.progradeAxis = safeNormalize(
        glm::cross(to.radialAxis, to.normalAxis),
        to.progradeAxis
    );
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
    const double universeIntervalSeconds =
        to.universeTimeSeconds - from.universeTimeSeconds;

    DetailMapSnapshot result = to;
    const glm::dvec3 destinationPlanetCenter = to.planetCenterMeters;
    const glm::dvec3 destinationPlanetVelocity = to.planetVelocityMps;

    result.universeTimeSeconds =
        lerpScalar(from.universeTimeSeconds, to.universeTimeSeconds, t);
    result.planetCenterMeters = hermitePosition(
        from.planetCenterMeters,
        from.planetVelocityMps,
        destinationPlanetCenter,
        destinationPlanetVelocity,
        universeIntervalSeconds,
        t
    );
    result.planetVelocityMps = hermiteVelocity(
        from.planetCenterMeters,
        from.planetVelocityMps,
        destinationPlanetCenter,
        destinationPlanetVelocity,
        universeIntervalSeconds,
        t
    );
    result.planetRadiusMeters =
        lerpScalar(from.planetRadiusMeters, to.planetRadiusMeters, t);
    result.planetRotationPhaseRad =
        lerpAngle(from.planetRotationPhaseRad, to.planetRotationPhaseRad, t);

    // Every Detail scene currently anchors originWorldMeters at planetCenter.
    result.scene.originWorldMeters = result.planetCenterMeters;

    std::unordered_map<std::string, const DetailMapOrbit*> fromHubOrbits;
    for (const auto& orbit : from.hubOrbits)
        fromHubOrbits[orbit.id] = &orbit;
    for (auto& orbit : result.hubOrbits)
    {
        const auto it = fromHubOrbits.find(orbit.id);
        if (it != fromHubOrbits.end())
        {
            interpolateOrbit(
                *it->second,
                orbit,
                from.planetVelocityMps,
                to.planetVelocityMps,
                universeIntervalSeconds,
                t
            );
        }
    }

    std::unordered_map<std::string, const DetailMapOrbit*> fromPlayerOrbits;
    for (const auto& orbit : from.playerOrbits)
        fromPlayerOrbits[orbit.id] = &orbit;
    for (auto& orbit : result.playerOrbits)
    {
        const auto it = fromPlayerOrbits.find(orbit.id);
        if (it != fromPlayerOrbits.end())
        {
            interpolateOrbit(
                *it->second,
                orbit,
                from.planetVelocityMps,
                to.planetVelocityMps,
                universeIntervalSeconds,
                t
            );
        }
    }

    interpolateObjects(
        from.scene.objects,
        result.scene.objects,
        universeIntervalSeconds,
        t
    );
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
    const double universeIntervalSeconds =
        to.universeTimeSeconds - from.universeTimeSeconds;

    HubMapSnapshot result = to;

    const glm::dvec3 destinationHubPosition = to.hubWorldPositionMeters;
    const glm::dvec3 destinationHubVelocity = to.hubWorldVelocityMps;
    const glm::dvec3 destinationParentPosition =
        to.parentPlanetWorldPositionMeters;
    const glm::dvec3 destinationParentVelocity =
        to.parentPlanetWorldVelocityMps;

    result.universeTimeSeconds =
        lerpScalar(from.universeTimeSeconds, to.universeTimeSeconds, t);
    result.hubWorldPositionMeters = hermitePosition(
        from.hubWorldPositionMeters,
        from.hubWorldVelocityMps,
        destinationHubPosition,
        destinationHubVelocity,
        universeIntervalSeconds,
        t
    );
    result.hubWorldVelocityMps = hermiteVelocity(
        from.hubWorldPositionMeters,
        from.hubWorldVelocityMps,
        destinationHubPosition,
        destinationHubVelocity,
        universeIntervalSeconds,
        t
    );
    result.parentPlanetWorldPositionMeters = hermitePosition(
        from.parentPlanetWorldPositionMeters,
        from.parentPlanetWorldVelocityMps,
        destinationParentPosition,
        destinationParentVelocity,
        universeIntervalSeconds,
        t
    );
    result.parentPlanetWorldVelocityMps = hermiteVelocity(
        from.parentPlanetWorldPositionMeters,
        from.parentPlanetWorldVelocityMps,
        destinationParentPosition,
        destinationParentVelocity,
        universeIntervalSeconds,
        t
    );

    const LocalSceneAxes fallbackAxes = interpolateAxes(
        from.hubWorldAxes,
        to.hubWorldAxes,
        t
    );
    result.hubWorldAxes = hubAxesFromKinematics(
        result.hubWorldPositionMeters,
        result.hubWorldVelocityMps,
        result.parentPlanetWorldPositionMeters,
        result.parentPlanetWorldVelocityMps,
        fallbackAxes
    );

    const glm::dvec3 parentRelative =
        result.parentPlanetWorldPositionMeters -
        result.hubWorldPositionMeters;
    result.parentPlanetCenterLocalMeters = glm::dvec3(
        glm::dot(parentRelative, result.hubWorldAxes.x),
        glm::dot(parentRelative, result.hubWorldAxes.y),
        glm::dot(parentRelative, result.hubWorldAxes.z)
    );

    result.parentPlanetRotationPhaseRad =
        lerpAngle(
            from.parentPlanetRotationPhaseRad,
            to.parentPlanetRotationPhaseRad,
            t
        );
    result.scene.originWorldMeters = result.hubWorldPositionMeters;

    interpolateObjects(
        from.scene.objects,
        result.scene.objects,
        universeIntervalSeconds,
        t
    );
    return result;
}

void extrapolateDetailToUniverseTime(
    world::celestial::DetailMapSnapshot& snapshot,
    double renderUniverseTimeSeconds
)
{
    constexpr double MaxExtrapolationUniverseSeconds = 30.0;

    if (!snapshot.valid || !std::isfinite(renderUniverseTimeSeconds))
        return;

    const double rawDelta =
        renderUniverseTimeSeconds - snapshot.universeTimeSeconds;
    if (!std::isfinite(rawDelta))
        return;

    const double dt = std::clamp(
        rawDelta,
        -MaxExtrapolationUniverseSeconds,
        MaxExtrapolationUniverseSeconds
    );
    if (std::abs(dt) <= 1.0e-9)
        return;

    snapshot.universeTimeSeconds += dt;
    snapshot.planetCenterMeters += snapshot.planetVelocityMps * dt;
    snapshot.scene.originWorldMeters = snapshot.planetCenterMeters;

    auto extrapolateOrbit =
        [&](world::celestial::DetailMapOrbit& orbit)
        {
            orbit.centerMeters += snapshot.planetVelocityMps * dt;
            orbit.positionMeters += orbit.velocityMps * dt;

            const glm::dvec3 relativeVelocity =
                orbit.velocityMps - snapshot.planetVelocityMps;
            orbit.radialAxis = safeNormalize(
                orbit.positionMeters - orbit.centerMeters,
                orbit.radialAxis
            );
            const glm::dvec3 tangentialVelocity =
                relativeVelocity -
                orbit.radialAxis * glm::dot(relativeVelocity, orbit.radialAxis);
            orbit.progradeAxis = safeNormalize(
                tangentialVelocity,
                orbit.progradeAxis
            );
            orbit.normalAxis = safeNormalize(
                glm::cross(orbit.progradeAxis, orbit.radialAxis),
                orbit.normalAxis
            );
            orbit.progradeAxis = safeNormalize(
                glm::cross(orbit.radialAxis, orbit.normalAxis),
                orbit.progradeAxis
            );
        };

    for (auto& orbit : snapshot.hubOrbits)
        extrapolateOrbit(orbit);
    for (auto& orbit : snapshot.playerOrbits)
        extrapolateOrbit(orbit);

    for (auto& object : snapshot.scene.objects)
        object.positionMeters += object.velocityMps * dt;
}

void extrapolateHubToUniverseTime(
    world::celestial::HubMapSnapshot& snapshot,
    double renderUniverseTimeSeconds
)
{
    constexpr double MaxExtrapolationUniverseSeconds = 30.0;

    if (!snapshot.valid || !std::isfinite(renderUniverseTimeSeconds))
        return;

    const double rawDelta =
        renderUniverseTimeSeconds - snapshot.universeTimeSeconds;
    if (!std::isfinite(rawDelta))
        return;

    const double dt = std::clamp(
        rawDelta,
        -MaxExtrapolationUniverseSeconds,
        MaxExtrapolationUniverseSeconds
    );
    if (std::abs(dt) <= 1.0e-9)
        return;

    snapshot.universeTimeSeconds += dt;
    snapshot.hubWorldPositionMeters += snapshot.hubWorldVelocityMps * dt;
    snapshot.parentPlanetWorldPositionMeters +=
        snapshot.parentPlanetWorldVelocityMps * dt;

    snapshot.hubWorldAxes = hubAxesFromKinematics(
        snapshot.hubWorldPositionMeters,
        snapshot.hubWorldVelocityMps,
        snapshot.parentPlanetWorldPositionMeters,
        snapshot.parentPlanetWorldVelocityMps,
        snapshot.hubWorldAxes
    );

    const glm::dvec3 parentRelative =
        snapshot.parentPlanetWorldPositionMeters -
        snapshot.hubWorldPositionMeters;
    snapshot.parentPlanetCenterLocalMeters = glm::dvec3(
        glm::dot(parentRelative, snapshot.hubWorldAxes.x),
        glm::dot(parentRelative, snapshot.hubWorldAxes.y),
        glm::dot(parentRelative, snapshot.hubWorldAxes.z)
    );

    snapshot.scene.originWorldMeters = snapshot.hubWorldPositionMeters;
    for (auto& object : snapshot.scene.objects)
        object.positionMeters += object.velocityMps * dt;
}

} // namespace

const char* AuthoritativeMapInterpolator::detailSampleModeName() const noexcept
{
    switch (m_detailSampleMode)
    {
        case SampleMode::Holding:       return "hold";
        case SampleMode::Interpolating: return "interp";
        case SampleMode::Extrapolating: return "extrap";
        case SampleMode::None:           return "none";
    }

    return "none";
}

const char* AuthoritativeMapInterpolator::hubSampleModeName() const noexcept
{
    switch (m_hubSampleMode)
    {
        case SampleMode::Holding:       return "hold";
        case SampleMode::Interpolating: return "interp";
        case SampleMode::Extrapolating: return "extrap";
        case SampleMode::None:           return "none";
    }

    return "none";
}

void AuthoritativeMapInterpolator::acceptDetail(
    const world::celestial::DetailMapSnapshot& snapshot,
    double serverTimeSeconds,
    std::uint64_t universeTimelineRevision
)
{
    const bool sameTimeline =
        m_detailHistory.empty() ||
        m_detailHistory.back().universeTimelineRevision ==
            universeTimelineRevision;

    const bool sameTarget =
        sameTimeline &&
        m_hasDetail &&
        !m_detailHistory.empty() &&
        m_detailHistory.back().snapshot.valid &&
        m_detailHistory.back().snapshot.detailTarget == snapshot.detailTarget;

    if (!sameTarget)
        m_detailHistory.clear();

    if (!m_detailHistory.empty() &&
        serverTimeSeconds <= m_detailHistory.back().serverTimeSeconds)
    {
        if (std::abs(
                serverTimeSeconds -
                m_detailHistory.back().serverTimeSeconds
            ) <= 1.0e-9)
        {
            m_detailHistory.back().snapshot = snapshot;
        }

        m_hasDetail = snapshot.valid;
        if (m_detailHistory.size() == 1u)
            m_detailVisual = snapshot;
        return;
    }

    m_detailHistory.push_back({
        serverTimeSeconds,
        universeTimelineRevision,
        snapshot
    });

    while (m_detailHistory.size() > MaxBufferedSnapshots)
        m_detailHistory.pop_front();

    m_hasDetail = snapshot.valid;
    if (m_detailHistory.size() == 1u)
        m_detailVisual = snapshot;
}

void AuthoritativeMapInterpolator::acceptHub(
    const world::celestial::HubMapSnapshot& snapshot,
    double serverTimeSeconds,
    std::uint64_t universeTimelineRevision
)
{
    const bool sameTimeline =
        m_hubHistory.empty() ||
        m_hubHistory.back().universeTimelineRevision ==
            universeTimelineRevision;

    const bool sameTarget =
        sameTimeline &&
        m_hasHub &&
        !m_hubHistory.empty() &&
        m_hubHistory.back().snapshot.valid &&
        m_hubHistory.back().snapshot.systemId == snapshot.systemId &&
        m_hubHistory.back().snapshot.hubId == snapshot.hubId;

    if (!sameTarget)
        m_hubHistory.clear();

    if (!m_hubHistory.empty() &&
        serverTimeSeconds <= m_hubHistory.back().serverTimeSeconds)
    {
        if (std::abs(
                serverTimeSeconds -
                m_hubHistory.back().serverTimeSeconds
            ) <= 1.0e-9)
        {
            m_hubHistory.back().snapshot = snapshot;
        }

        m_hasHub = snapshot.valid;
        if (m_hubHistory.size() == 1u)
            m_hubVisual = snapshot;
        return;
    }

    m_hubHistory.push_back({
        serverTimeSeconds,
        universeTimelineRevision,
        snapshot
    });

    while (m_hubHistory.size() > MaxBufferedSnapshots)
        m_hubHistory.pop_front();

    m_hasHub = snapshot.valid;
    if (m_hubHistory.size() == 1u)
        m_hubVisual = snapshot;
}

void AuthoritativeMapInterpolator::update(
    double renderServerTimeSeconds,
    double renderUniverseTimeSeconds
)
{
    if (m_hasDetail)
    {
        sampleDetail(
            renderServerTimeSeconds,
            renderUniverseTimeSeconds
        );
    }

    if (m_hasHub)
    {
        sampleHub(
            renderServerTimeSeconds,
            renderUniverseTimeSeconds
        );
    }
}

void AuthoritativeMapInterpolator::sampleDetail(
    double renderServerTimeSeconds,
    double renderUniverseTimeSeconds
)
{
    if (m_detailHistory.empty())
    {
        m_detailSampleMode = SampleMode::None;
        m_detailNewestGapSeconds = 0.0;
        return;
    }

    m_detailNewestGapSeconds =
        renderServerTimeSeconds - m_detailHistory.back().serverTimeSeconds;

    while (m_detailHistory.size() > 2u &&
           m_detailHistory[1].serverTimeSeconds <= renderServerTimeSeconds)
    {
        m_detailHistory.pop_front();
    }

    if (renderServerTimeSeconds <= m_detailHistory.front().serverTimeSeconds ||
        m_detailHistory.size() == 1u)
    {
        m_detailSampleMode = SampleMode::Holding;
        m_detailVisual = m_detailHistory.front().snapshot;
        return;
    }

    if (renderServerTimeSeconds >= m_detailHistory.back().serverTimeSeconds)
    {
        m_detailSampleMode = SampleMode::Extrapolating;
        m_detailVisual = m_detailHistory.back().snapshot;
        extrapolateDetailToUniverseTime(
            m_detailVisual,
            renderUniverseTimeSeconds
        );
        return;
    }

    for (std::size_t i = 0; i + 1u < m_detailHistory.size(); ++i)
    {
        const auto& older = m_detailHistory[i];
        const auto& newer = m_detailHistory[i + 1u];

        if (older.serverTimeSeconds <= renderServerTimeSeconds &&
            newer.serverTimeSeconds >= renderServerTimeSeconds)
        {
            const double span =
                newer.serverTimeSeconds - older.serverTimeSeconds;
            const double alpha =
                span > 1.0e-9
                    ? (renderServerTimeSeconds - older.serverTimeSeconds) / span
                    : 1.0;

            m_detailSampleMode = SampleMode::Interpolating;
            m_detailVisual = interpolateDetail(
                older.snapshot,
                newer.snapshot,
                alpha
            );
            return;
        }
    }

    m_detailSampleMode = SampleMode::Holding;
    m_detailVisual = m_detailHistory.back().snapshot;
}

void AuthoritativeMapInterpolator::sampleHub(
    double renderServerTimeSeconds,
    double renderUniverseTimeSeconds
)
{
    if (m_hubHistory.empty())
    {
        m_hubSampleMode = SampleMode::None;
        m_hubNewestGapSeconds = 0.0;
        return;
    }

    m_hubNewestGapSeconds =
        renderServerTimeSeconds - m_hubHistory.back().serverTimeSeconds;

    while (m_hubHistory.size() > 2u &&
           m_hubHistory[1].serverTimeSeconds <= renderServerTimeSeconds)
    {
        m_hubHistory.pop_front();
    }

    if (renderServerTimeSeconds <= m_hubHistory.front().serverTimeSeconds ||
        m_hubHistory.size() == 1u)
    {
        m_hubSampleMode = SampleMode::Holding;
        m_hubVisual = m_hubHistory.front().snapshot;
        return;
    }

    if (renderServerTimeSeconds >= m_hubHistory.back().serverTimeSeconds)
    {
        m_hubSampleMode = SampleMode::Extrapolating;
        m_hubVisual = m_hubHistory.back().snapshot;
        extrapolateHubToUniverseTime(
            m_hubVisual,
            renderUniverseTimeSeconds
        );
        return;
    }

    for (std::size_t i = 0; i + 1u < m_hubHistory.size(); ++i)
    {
        const auto& older = m_hubHistory[i];
        const auto& newer = m_hubHistory[i + 1u];

        if (older.serverTimeSeconds <= renderServerTimeSeconds &&
            newer.serverTimeSeconds >= renderServerTimeSeconds)
        {
            const double span =
                newer.serverTimeSeconds - older.serverTimeSeconds;
            const double alpha =
                span > 1.0e-9
                    ? (renderServerTimeSeconds - older.serverTimeSeconds) / span
                    : 1.0;

            m_hubSampleMode = SampleMode::Interpolating;
            m_hubVisual = interpolateHub(
                older.snapshot,
                newer.snapshot,
                alpha
            );
            return;
        }
    }

    m_hubSampleMode = SampleMode::Holding;
    m_hubVisual = m_hubHistory.back().snapshot;
}

} // namespace game::system_map
