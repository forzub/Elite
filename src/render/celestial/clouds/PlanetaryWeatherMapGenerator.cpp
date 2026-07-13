#include "src/render/celestial/clouds/PlanetaryWeatherMapGenerator.h"

#include <algorithm>
#include <cmath>

namespace render::celestial::clouds
{

namespace
{

float smoothStep(
    float edge0,
    float edge1,
    float value
)
{
    const float t =
        std::clamp(
            (value - edge0) /
            std::max(0.000001f, edge1 - edge0),
            0.0f,
            1.0f
        );

    return t * t * (3.0f - 2.0f * t);
}

float hash01(
    int x,
    int y,
    std::uint32_t seed
)
{
    std::uint32_t h =
        static_cast<std::uint32_t>(x) * 374761393u +
        static_cast<std::uint32_t>(y) * 668265263u +
        seed * 2246822519u;

    h =
        (h ^ (h >> 13u)) *
        1274126177u;

    h ^= h >> 16u;

    return
        static_cast<float>(h & 0x00ffffffu) /
        static_cast<float>(0x01000000u);
}

int wrapX(
    int x,
    int width
)
{
    int result = x % width;

    if (result < 0)
        result += width;

    return result;
}

float valueNoisePeriodicX(
    float x,
    float y,
    int periodX,
    std::uint32_t seed
)
{
    const int x0 =
        static_cast<int>(std::floor(x));

    const int y0 =
        static_cast<int>(std::floor(y));

    const float tx = x - std::floor(x);
    const float ty = y - std::floor(y);

    const float sx =
        tx * tx * (3.0f - 2.0f * tx);

    const float sy =
        ty * ty * (3.0f - 2.0f * ty);

    const float a =
        hash01(wrapX(x0, periodX), y0, seed);

    const float b =
        hash01(wrapX(x0 + 1, periodX), y0, seed);

    const float c =
        hash01(wrapX(x0, periodX), y0 + 1, seed);

    const float d =
        hash01(wrapX(x0 + 1, periodX), y0 + 1, seed);

    const float ab = a + (b - a) * sx;
    const float cd = c + (d - c) * sx;

    return ab + (cd - ab) * sy;
}

float fbmPeriodicX(
    float x,
    float y,
    int basePeriodX,
    std::uint32_t seed,
    int octaveCount
)
{
    float sum = 0.0f;
    float normalization = 0.0f;

    float amplitude = 0.5f;
    float frequency = 1.0f;

    for (int octave = 0;
         octave < octaveCount;
         ++octave)
    {
        const int period =
            std::max(
                1,
                basePeriodX << octave
            );

        sum +=
            valueNoisePeriodicX(
                x * frequency,
                y * frequency,
                period,
                seed +
                    static_cast<std::uint32_t>(
                        octave * 101
                    )
            ) *
            amplitude;

        normalization += amplitude;

        amplitude *= 0.5f;
        frequency *= 2.0f;
    }

    return normalization > 0.0f
        ? sum / normalization
        : 0.0f;
}

} // namespace

float
PlanetaryWeatherMapGenerator::climateValueAtLatitude(
    float latitudeDeg,
    const world::celestial::visual::
        EnvironmentCloudCirculationProfile& circulation,
    float& outDensityMultiplier,
    float& outFragmentation,
    float& outZonalStretch
)
{
    float coverageAccumulator = 0.0f;
    float densityAccumulator = 0.0f;
    float fragmentationAccumulator = 0.0f;
    float stretchAccumulator = 0.0f;
    float weightAccumulator = 0.0f;

    for (const auto& zone : circulation.zones)
    {
        const float distance =
            std::abs(
                latitudeDeg -
                zone.latitudeCenterDeg
            );

        const float outerWidth =
            zone.halfWidthDeg * 1.6f;

        const float weight =
            1.0f -
            smoothStep(
                zone.halfWidthDeg,
                outerWidth,
                distance
            );

        if (weight <= 0.0f)
            continue;

        coverageAccumulator +=
            zone.coverageMultiplier * weight;

        densityAccumulator +=
            zone.densityMultiplier * weight;

        fragmentationAccumulator +=
            zone.fragmentation * weight;

        stretchAccumulator +=
            zone.zonalStretch * weight;

        weightAccumulator += weight;
    }

    if (weightAccumulator <= 0.000001f)
    {
        outDensityMultiplier = 1.0f;
        outFragmentation = 0.35f;
        outZonalStretch = 1.0f;

        return 1.0f;
    }

    outDensityMultiplier =
        densityAccumulator /
        weightAccumulator;

    outFragmentation =
        fragmentationAccumulator /
        weightAccumulator;

    outZonalStretch =
        stretchAccumulator /
        weightAccumulator;

    return
        coverageAccumulator /
        weightAccumulator;
}

PlanetaryWeatherMap
PlanetaryWeatherMapGenerator::generate(
    const world::celestial::visual::
        EnvironmentCloudProfile& profile,
    std::uint32_t runtimeSeed,
    int width,
    int height
) const
{
    PlanetaryWeatherMap result;

    result.width = std::max(width, 16);
    result.height = std::max(height, 8);
    result.seed =
        profile.generation.seedBase ^
        runtimeSeed;

    result.pixels.resize(
        static_cast<std::size_t>(result.width) *
        static_cast<std::size_t>(result.height)
    );

    const int weatherPeriod =
        std::max(
            3,
            static_cast<int>(
                std::round(
                    profile.generation.weatherScale
                )
            )
        );

    for (int y = 0; y < result.height; ++y)
    {
        const float v =
            (
                static_cast<float>(y) +
                0.5f
            ) /
            static_cast<float>(result.height);

        const float latitudeDeg =
            90.0f - v * 180.0f;

        float densityMultiplier = 1.0f;
        float fragmentation = 0.35f;
        float zonalStretch = 1.0f;

        const float climateCoverage =
            climateValueAtLatitude(
                latitudeDeg,
                profile.circulation,
                densityMultiplier,
                fragmentation,
                zonalStretch
            );

        for (int x = 0; x < result.width; ++x)
        {
            const float u =
                (
                    static_cast<float>(x) +
                    0.5f
                ) /
                static_cast<float>(result.width);

            const float warpU =
                (
                    fbmPeriodicX(
                        u * weatherPeriod,
                        v * 3.0f,
                        weatherPeriod,
                        result.seed + 11u,
                        3
                    ) -
                    0.5f
                ) *
                profile.generation.domainWarpStrength;

            const float warpV =
                (
                    fbmPeriodicX(
                        u * weatherPeriod + 7.0f,
                        v * 2.5f,
                        weatherPeriod,
                        result.seed + 47u,
                        3
                    ) -
                    0.5f
                ) *
                profile.generation.domainWarpStrength *
                0.65f;

            const float weather =
                fbmPeriodicX(
                    (u + warpU) *
                        static_cast<float>(weatherPeriod),
                    (v + warpV) *
                        3.5f /
                        std::max(0.2f, zonalStretch),
                    weatherPeriod,
                    result.seed + 101u,
                    5
                );

        const float longitudinalVariation =
            0.82f +
            0.36f *
            fbmPeriodicX(
                u * 5.0f,
                v * 2.2f,
                5,
                result.seed + 211u,
                3
            );

        /*
            Климатические зоны не должны напрямую превращаться
            в жёсткие полосы. Они должны только смещать вероятность.
        */
        const float climateBias =
            0.75f +
            0.25f *
            climateCoverage;

        const float baseCoverage =
            std::clamp(
                profile.globalCoverage *
                climateBias *
                longitudinalVariation,
                0.0f,
                1.0f
            );

        /*
            Порог делаем мягче. Тогда weather map остаётся
            планетарной, но не превращается в "широтную ленту".
        */
        const float threshold =
            0.92f -
            baseCoverage * 0.78f;

            const float coverage =
                smoothStep(
                    threshold - 0.14f,
                    threshold + 0.10f,
                    weather
                );

            WeatherMapPixel& destination =
                result.pixels[
                    static_cast<std::size_t>(y) *
                    static_cast<std::size_t>(result.width) +
                    static_cast<std::size_t>(x)
                ];

            destination.coverage =
                std::clamp(
                    coverage *
                    densityMultiplier,
                    0.0f,
                    1.0f
                );

            destination.cloudType =
                fragmentation;

            if (!profile.layers.empty())
            {
                float baseHeight = 0.0f;
                float topHeight = 0.0f;
                float totalWeight = 0.0f;

                for (const auto& layer : profile.layers)
                {
                    const float weight =
                        std::max(
                            0.001f,
                            layer.coverage *
                            layer.density
                        );

                    baseHeight +=
                        layer.baseHeightKm *
                        weight;

                    topHeight +=
                        layer.topHeightKm *
                        weight;

                    totalWeight += weight;
                }

                if (totalWeight > 0.0f)
                {
                    baseHeight /= totalWeight;
                    topHeight /= totalWeight;
                }

                destination.baseHeight =
                    baseHeight;

                destination.thickness =
                    std::max(
                        0.0f,
                        topHeight -
                        baseHeight
                    );
            }
        }
    }

    return result;
}

} // namespace render::celestial::clouds