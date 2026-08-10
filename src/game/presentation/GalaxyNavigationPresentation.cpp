#include "src/game/presentation/GalaxyNavigationPresentation.h"

#include <algorithm>

#include "src/game/navigation/SystemNavigationGrid.h"
#include "src/world/celestial/CelestialTypes.h"
#include "src/world/celestial/SystemMapTypes.h"
#include "src/world/coordinates/WorldPosition.h"

namespace game::presentation
{
GalaxyPlayerMarkerPosition resolveGalaxyPlayerMarkerPosition(
    const world::celestial::GalaxyMapSnapshot& galaxy,
    const world::celestial::PlayerNavigationState& navigation
)
{
    GalaxyPlayerMarkerPosition result;

    const auto system =
        std::find_if(
            galaxy.systems.begin(),
            galaxy.systems.end(),
            [&](const auto& candidate)
            {
                return candidate.id == navigation.currentSystemId;
            }
        );

    result.insideKnownSystem =
        system != galaxy.systems.end();

    if (result.insideKnownSystem)
    {
        result.positionLy =
            system->positionLy +
            navigation.systemLocalAu /
                game::navigation::SystemNavigationGrid::AuPerLightYear;
    }
    else
    {
        result.positionLy =
            world::coordinates::toGalacticLy(
                navigation.worldPosition
            );
    }

    return result;
}
}
