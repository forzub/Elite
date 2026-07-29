#pragma once

namespace game::system_map
{

struct LocalMapControlSettings
{
    double rotateSensitivity = 0.008;
    double zoomStep = 1.08;
    double minZoom = 0.15;
    double maxZoom = 16.0;
};

} // namespace game::system_map
