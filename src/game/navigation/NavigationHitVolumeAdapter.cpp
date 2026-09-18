#include "NavigationHitVolumeAdapter.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace game::navigation
{
namespace
{

bool includeVolume(
    const game::damage::HitVolume& volume,
    const NavigationHitVolumeAdapter::Options& options
) noexcept
{
    return
        !volume.destroyed &&
        (options.includeSupportLinkVolumes || !volume.supportLinkVolume);
}

glm::dmat3 normalizedBasis(const glm::dmat3& basis) noexcept
{
    glm::dmat3 out(1.0);
    for (int axis = 0; axis < 3; ++axis)
    {
        const glm::dvec3 v = basis[axis];
        const double lengthSquared = glm::dot(v, v);
        if (std::isfinite(lengthSquared) && lengthSquared > 1.0e-18)
            out[axis] = v / std::sqrt(lengthSquared);
    }
    return out;
}

} // namespace

std::vector<world::navigation::NavigationObstacle>
NavigationHitVolumeAdapter::buildObstacles(
    const game::damage::HitComponent& hitComponent,
    std::uint32_t entityId,
    const glm::dvec3& objectWorldPositionMeters,
    const glm::dmat3& objectLocalToWorld,
    const std::string& idPrefix,
    const Options& options
)
{
    std::vector<world::navigation::NavigationObstacle> out;
    out.reserve(hitComponent.volumes.size());

    const glm::dmat3 ownerBasis = normalizedBasis(objectLocalToWorld);

    std::size_t volumeIndex = 0;
    for (const auto& volume : hitComponent.volumes)
    {
        if (!includeVolume(volume, options))
        {
            ++volumeIndex;
            continue;
        }

        world::navigation::NavigationObstacle obstacle;
        obstacle.id =
            idPrefix + "::hit::" + std::to_string(volumeIndex);
        obstacle.entityId = entityId;
        obstacle.shape =
            world::navigation::NavigationObstacleShape::Box;

        const glm::dvec3 localCenter(volume.center);
        obstacle.centerMeters =
            objectWorldPositionMeters +
            ownerBasis * localCenter;

        const glm::dmat3 localBasis(volume.orientation);
        obstacle.localToWorldBasis =
            normalizedBasis(ownerBasis * localBasis);
        obstacle.halfExtentsMeters =
            glm::max(glm::dvec3(volume.halfSize), glm::dvec3(0.0));
        obstacle.requiredClearanceMeters =
            std::max(0.0, options.requiredClearanceMeters);

        out.push_back(std::move(obstacle));
        ++volumeIndex;
    }

    return out;
}

std::vector<world::navigation::NavigationObstacle>
NavigationHitVolumeAdapter::buildObstacles(
    const game::damage::HitComponent& hitComponent,
    std::uint32_t entityId,
    const glm::dvec3& objectWorldPositionMeters,
    const glm::dmat3& objectLocalToWorld,
    const std::string& idPrefix
)
{
    return buildObstacles(
        hitComponent,
        entityId,
        objectWorldPositionMeters,
        objectLocalToWorld,
        idPrefix,
        Options{}
    );
}

double NavigationHitVolumeAdapter::conservativeRadiusFromOrigin(
    const game::damage::HitComponent& hitComponent,
    const Options& options
) noexcept
{
    double radius = 0.0;

    for (const auto& volume : hitComponent.volumes)
    {
        if (!includeVolume(volume, options))
            continue;

        const glm::dvec3 center(volume.center);
        const glm::dvec3 half =
            glm::max(glm::dvec3(volume.halfSize), glm::dvec3(0.0));

        const double candidate =
            glm::length(center) + glm::length(half);
        if (std::isfinite(candidate))
            radius = std::max(radius, candidate);
    }

    return radius;
}

double NavigationHitVolumeAdapter::conservativeRadiusFromOrigin(
    const game::damage::HitComponent& hitComponent
) noexcept
{
    return conservativeRadiusFromOrigin(hitComponent, Options{});
}

} // namespace game::navigation
