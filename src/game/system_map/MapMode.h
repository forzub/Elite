#pragma once

namespace game::system_map
{
    enum class MapMode
    {
        Galaxy,
        System,
        Detail,
        Hub,

        // Source compatibility for code outside the map subsystem.
        Planet = Detail
    };
}
