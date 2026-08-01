#pragma once

#include "src/game/system_map/DetailMapGeometryPass.h"
#include "src/game/system_map/DetailMapPlanetPass.h"
#include "src/game/system_map/DetailMapRenderContext.h"
#include "src/game/system_map/MapCelestialRenderResources.h"


namespace game::system_map
{
class DetailMapBackend final : public DetailMapRenderContext
{
public:
    explicit DetailMapBackend(MapCelestialRenderResources& resources) noexcept;

    void renderDetailMapPasses(
        const DetailMapPresentation& presentation,
        const Viewport& viewport,
        const world::celestial::DetailMapSnapshot& snapshot
    ) override;

private:
    MapCelestialRenderResources& m_resources;
    DetailMapPlanetPass m_planetPass;
    DetailMapGeometryPass m_geometryPass;
};
}
