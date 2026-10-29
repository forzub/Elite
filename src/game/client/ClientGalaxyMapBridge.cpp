#include "src/game/client/ClientGalaxyMapBridge.h"

#include <algorithm>
#include <cmath>

#include "src/world/interstellar/DensityLattice.h"

namespace game::client
{

/*
    Map presentation of an interstellar-medium object: a structure (relative
    density lattice, one phase) or a background cell (log10 density and a
    phase per voxel). Background opacity follows log density so the hot
    Local Bubble cavity stays clear and the neutral wall / warm medium read
    faintly; structures use their relative density.
*/
std::shared_ptr<const world::celestial::GalaxyMapMediumVolume> galaxyMapMediumVolume(
    const world::celestial::GalaxyObjectDefinition& source)
{
    const bool background = source.objectType == "interstellar_medium_background";
    if (!background && source.objectType != "interstellar_medium_region")
        return nullptr;
    const auto& props = source.properties;
    const nlohmann::json* latticeJson = nullptr;
    if (background && props.contains("lattice"))
        latticeJson = &props["lattice"];
    else if (props.contains("geometry") && props["geometry"].contains("lattice"))
        latticeJson = &props["geometry"]["lattice"];
    world::interstellar::DensityLattice lattice;
    if (!latticeJson || !world::interstellar::latticeFromJson(*latticeJson, lattice))
        return nullptr;

    auto volume = std::make_shared<world::celestial::GalaxyMapMediumVolume>();
    volume->originLy = lattice.originLy;
    volume->voxelLy = lattice.voxelLy;
    volume->dims = lattice.dims;
    volume->background = background;
    volume->opacity.resize(lattice.size());
    if (background)
    {
        volume->phases = lattice.phases;
        for (std::size_t k = 0; k < lattice.size(); ++k)
        {
            const char phase = k < lattice.phases.size() ? lattice.phases[k] : 'h';
            const double u = phase == 'h'
                ? 0.0
                : std::clamp((std::log10(std::max(lattice.densityCm3(k), 1.0e-12)) + 1.5) / 3.5, 0.0, 1.0);
            volume->opacity[k] = static_cast<std::uint8_t>(std::lround(255.0 * u));
        }
    }
    else
    {
        const char phase = world::interstellar::mediumPhaseCode(props.value("phase", std::string()));
        volume->phases.assign(lattice.size(), phase ? phase : 'p');
        volume->opacity = lattice.values;
    }
    return volume;
}

} // namespace game::client
