#pragma once

#include "src/game/system_map/DetailMapRenderContext.h"

class SystemMapRenderer;

namespace game::system_map
{
class DetailMapBackend final : public DetailMapRenderContext
{
public:
    explicit DetailMapBackend(SystemMapRenderer& host) noexcept
        : m_host(host)
    {
    }

    void renderDetailMapPasses(
        const DetailMapPresentation& presentation,
        const Viewport& viewport,
        const world::celestial::DetailMapSnapshot& snapshot
    ) override;

private:
    SystemMapRenderer& m_host;
};
}
