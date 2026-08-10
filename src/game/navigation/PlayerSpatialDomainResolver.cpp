#include "src/game/navigation/PlayerSpatialDomainResolver.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace game::navigation
{
namespace
{
bool finiteVector(const glm::dvec3& value)
{
    return
        std::isfinite(value.x) &&
        std::isfinite(value.y) &&
        std::isfinite(value.z);
}
}

PlayerSpatialDomainResolution resolvePlayerSpatialDomain(
    const std::vector<world::celestial::StarSystemSummary>& systems,
    int sourceSystemId,
    const world::coordinates::WorldPosition& sourceWorldPosition,
    double systemMembershipRadiusAu
)
{
    PlayerSpatialDomainResolution result;

    if (systems.empty() ||
        !std::isfinite(systemMembershipRadiusAu) ||
        systemMembershipRadiusAu <= 0.0)
    {
        return result;
    }

    glm::dvec3 galacticPositionLy {0.0};

    if (sourceSystemId >= 0)
    {
        const auto sourceSystem =
            std::find_if(
                systems.begin(),
                systems.end(),
                [sourceSystemId](const auto& candidate)
                {
                    return candidate.id == sourceSystemId;
                }
            );

        if (sourceSystem == systems.end())
            return result;

        galacticPositionLy =
            sourceSystem->positionLy +
            world::coordinates::fullMeters(sourceWorldPosition) *
                world::coordinates::LightYearsPerMeter;
    }
    else
    {
        galacticPositionLy =
            world::coordinates::toGalacticLy(sourceWorldPosition);
    }

    if (!finiteVector(galacticPositionLy))
        return result;

    const world::celestial::StarSystemSummary* nearestSystem = nullptr;
    double nearestDistanceLy =
        std::numeric_limits<double>::infinity();

    for (const auto& system : systems)
    {
        const double distanceLy =
            glm::length(system.positionLy - galacticPositionLy);

        if (!std::isfinite(distanceLy))
            continue;

        if (distanceLy < nearestDistanceLy)
        {
            nearestDistanceLy = distanceLy;
            nearestSystem = &system;
        }
    }

    const double membershipRadiusLy =
        systemMembershipRadiusAu *
        world::celestial::MetersPerAu *
        world::coordinates::LightYearsPerMeter;

    result.valid = true;
    result.galacticPositionLy = galacticPositionLy;

    if (nearestSystem && nearestDistanceLy <= membershipRadiusLy)
    {
        result.currentSystemId = nearestSystem->id;
        result.systemLocalMeters =
            (galacticPositionLy - nearestSystem->positionLy) *
            world::coordinates::MetersPerLightYear;
        result.systemLocalAu =
            result.systemLocalMeters /
            world::celestial::MetersPerAu;
        result.worldPosition =
            world::coordinates::makeWorldPositionFromMeters(
                result.systemLocalMeters
            );
        return result;
    }

    result.currentSystemId = -1;
    result.worldPosition =
        world::coordinates::makeWorldPositionFromMeters(
            galacticPositionLy *
            world::coordinates::MetersPerLightYear
        );
    result.systemLocalMeters = glm::dvec3(0.0);
    result.systemLocalAu = glm::dvec3(0.0);
    return result;
}

} // namespace game::navigation
