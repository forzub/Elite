#pragma once

#include <cstdint>
#include <vector>
#include <cstddef>

namespace render::celestial::clouds
{

struct WeatherMapPixel
{
    float coverage = 0.0f;
    float cloudType = 0.0f;
    float baseHeight = 0.0f;
    float thickness = 0.0f;
};

struct PlanetaryWeatherMap
{
    int width = 0;
    int height = 0;

    std::uint32_t seed = 0u;

    std::vector<WeatherMapPixel> pixels;

    bool valid() const
    {
        return
            width > 0 &&
            height > 0 &&
            pixels.size() ==
                static_cast<std::size_t>(width) *
                static_cast<std::size_t>(height);
    }

    const WeatherMapPixel& at(
        int x,
        int y
    ) const
    {
        return pixels[
            static_cast<std::size_t>(y) *
            static_cast<std::size_t>(width) +
            static_cast<std::size_t>(x)
        ];
    }
};

} // namespace render::celestial::clouds