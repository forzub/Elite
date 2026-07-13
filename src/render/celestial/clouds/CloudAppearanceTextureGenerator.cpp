#include "src/render/celestial/clouds/CloudAppearanceTextureGenerator.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

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
    const float denominator =
        std::max(
            0.000001f,
            edge1 - edge0
        );

    const float t =
        std::clamp(
            (value - edge0) /
                denominator,
            0.0f,
            1.0f
        );

    return
        t *
        t *
        (3.0f - 2.0f * t);
}

float wrap01(
    float value
)
{
    value =
        std::fmod(
            value,
            1.0f
        );

    if (value < 0.0f)
        value += 1.0f;

    return value;
}

int wrapInteger(
    int value,
    int period
)
{
    if (period <= 0)
        return value;

    int result =
        value % period;

    if (result < 0)
        result += period;

    return result;
}

float hash01(
    int x,
    int y,
    std::uint32_t seed
)
{
    std::uint32_t hash =
        static_cast<std::uint32_t>(x) *
            374761393u +
        static_cast<std::uint32_t>(y) *
            668265263u +
        seed *
            2246822519u;

    hash =
        (hash ^ (hash >> 13u)) *
        1274126177u;

    hash ^=
        hash >> 16u;

    return
        static_cast<float>(
            hash & 0x00ffffffu
        ) /
        static_cast<float>(
            0x01000000u
        );
}

float valueNoisePeriodicX(
    float x,
    float y,
    int periodX,
    std::uint32_t seed
)
{
    const int x0 =
        static_cast<int>(
            std::floor(x)
        );

    const int y0 =
        static_cast<int>(
            std::floor(y)
        );

    const float fractionX =
        x - std::floor(x);

    const float fractionY =
        y - std::floor(y);

    const float smoothX =
        fractionX *
        fractionX *
        (
            3.0f -
            2.0f * fractionX
        );

    const float smoothY =
        fractionY *
        fractionY *
        (
            3.0f -
            2.0f * fractionY
        );

    const float a =
        hash01(
            wrapInteger(
                x0,
                periodX
            ),
            y0,
            seed
        );

    const float b =
        hash01(
            wrapInteger(
                x0 + 1,
                periodX
            ),
            y0,
            seed
        );

    const float c =
        hash01(
            wrapInteger(
                x0,
                periodX
            ),
            y0 + 1,
            seed
        );

    const float d =
        hash01(
            wrapInteger(
                x0 + 1,
                periodX
            ),
            y0 + 1,
            seed
        );

    const float horizontal0 =
        a +
        (b - a) *
        smoothX;

    const float horizontal1 =
        c +
        (d - c) *
        smoothX;

    return
        horizontal0 +
        (
            horizontal1 -
            horizontal0
        ) *
        smoothY;
}

float fbmPeriodicX(
    float x,
    float y,
    int periodX,
    std::uint32_t seed,
    int octaveCount
)
{
    float result = 0.0f;
    float normalization = 0.0f;

    float amplitude = 0.5f;
    float frequency = 1.0f;

    for (int octave = 0;
         octave < octaveCount;
         ++octave)
    {
        const int octavePeriod =
            std::max(
                1,
                periodX << octave
            );

        result +=
            valueNoisePeriodicX(
                x * frequency,
                y * frequency,
                octavePeriod,
                seed +
                    static_cast<std::uint32_t>(
                        octave * 137
                    )
            ) *
            amplitude;

        normalization +=
            amplitude;

        amplitude *= 0.5f;
        frequency *= 2.0f;
    }

    return
        normalization > 0.0f
            ? result /
                normalization
            : 0.0f;
}

float billowFbmPeriodicX(
    float x,
    float y,
    int periodX,
    std::uint32_t seed,
    int octaveCount
)
{
    float result = 0.0f;
    float normalization = 0.0f;

    float amplitude = 0.5f;
    float frequency = 1.0f;

    for (int octave = 0;
         octave < octaveCount;
         ++octave)
    {
        const int octavePeriod =
            std::max(
                1,
                periodX << octave
            );

        const float source =
            valueNoisePeriodicX(
                x * frequency,
                y * frequency,
                octavePeriod,
                seed +
                    static_cast<std::uint32_t>(
                        octave * 173
                    )
            );

        /*
            Ridge/billow field.

            Значения около середины noise становятся
            плотными округлыми массами, а край дробится
            следующими октавами.
        */
        const float billow =
            1.0f -
            std::abs(
                source * 2.0f -
                1.0f
            );

        result +=
            billow *
            amplitude;

        normalization +=
            amplitude;

        amplitude *= 0.52f;
        frequency *= 2.0f;
    }

    return
        normalization > 0.0f
            ? result /
                normalization
            : 0.0f;
}

float worleyPeriodicX(
    float x,
    float y,
    int periodX,
    std::uint32_t seed
)
{
    const int cellX =
        static_cast<int>(
            std::floor(x)
        );

    const int cellY =
        static_cast<int>(
            std::floor(y)
        );

    float nearestDistanceSquared =
        99999.0f;

    for (int offsetY = -1;
         offsetY <= 1;
         ++offsetY)
    {
        for (int offsetX = -1;
             offsetX <= 1;
             ++offsetX)
        {
            const int candidateX =
                cellX +
                offsetX;

            const int candidateY =
                cellY +
                offsetY;

            const int wrappedX =
                wrapInteger(
                    candidateX,
                    periodX
                );

            const float pointX =
                static_cast<float>(
                    candidateX
                ) +
                hash01(
                    wrappedX,
                    candidateY,
                    seed + 31u
                );

            const float pointY =
                static_cast<float>(
                    candidateY
                ) +
                hash01(
                    wrappedX,
                    candidateY,
                    seed + 83u
                );

            const float distanceX =
                x -
                pointX;

            const float distanceY =
                y -
                pointY;

            const float distanceSquared =
                distanceX *
                    distanceX +
                distanceY *
                    distanceY;

            nearestDistanceSquared =
                std::min(
                    nearestDistanceSquared,
                    distanceSquared
                );
        }
    }

    return
        std::clamp(
            std::sqrt(
                nearestDistanceSquared
            ),
            0.0f,
            1.0f
        );
}

float sampleWeatherCoverage(
    const PlanetaryWeatherMap& map,
    float u,
    float v
)
{
    if (!map.valid())
        return 0.0f;

    u = wrap01(u);

    v =
        std::clamp(
            v,
            0.0f,
            1.0f
        );

    const float pixelX =
        u *
        static_cast<float>(
            map.width
        ) -
        0.5f;

    const float pixelY =
        v *
        static_cast<float>(
            map.height
        ) -
        0.5f;

    const int x0 =
        static_cast<int>(
            std::floor(pixelX)
        );

    const int y0 =
        std::clamp(
            static_cast<int>(
                std::floor(pixelY)
            ),
            0,
            map.height - 1
        );

    const int x1 =
        x0 + 1;

    const int y1 =
        std::min(
            y0 + 1,
            map.height - 1
        );

    const float fractionX =
        pixelX -
        std::floor(pixelX);

    const float fractionY =
        pixelY -
        std::floor(pixelY);

    const float a =
        map.at(
            wrapInteger(
                x0,
                map.width
            ),
            y0
        ).coverage;

    const float b =
        map.at(
            wrapInteger(
                x1,
                map.width
            ),
            y0
        ).coverage;

    const float c =
        map.at(
            wrapInteger(
                x0,
                map.width
            ),
            y1
        ).coverage;

    const float d =
        map.at(
            wrapInteger(
                x1,
                map.width
            ),
            y1
        ).coverage;

    const float horizontal0 =
        a +
        (b - a) *
        fractionX;

    const float horizontal1 =
        c +
        (d - c) *
        fractionX;

    return
        horizontal0 +
        (
            horizontal1 -
            horizontal0
        ) *
        fractionY;
}

std::size_t fieldIndex(
    int x,
    int y,
    int width
)
{
    return
        static_cast<std::size_t>(y) *
            static_cast<std::size_t>(width) +
        static_cast<std::size_t>(x);
}

float fieldAt(
    const std::vector<float>& field,
    int x,
    int y,
    int width,
    int height
)
{
    x =
        wrapInteger(
            x,
            width
        );

    y =
        std::clamp(
            y,
            0,
            height - 1
        );

    return
        field[
            fieldIndex(
                x,
                y,
                width
            )
        ];
}

std::uint8_t toByte(
    float value
)
{
    return
        static_cast<std::uint8_t>(
            std::clamp(
                value,
                0.0f,
                1.0f
            ) *
                255.0f +
            0.5f
        );
}

} // namespace

CloudAppearanceImage
CloudAppearanceTextureGenerator::generate(
    const PlanetaryWeatherMap& weatherMap,
    const world::celestial::visual::
        EnvironmentCloudProfile& profile,
    std::uint32_t seed
) const
{
    CloudAppearanceImage result;

    if (!weatherMap.valid() ||
        !profile.enabled)
    {
        return result;
    }

    result.width =
        weatherMap.width;

    result.height =
        weatherMap.height;

    const int width =
        result.width;

    const int height =
        result.height;

    const std::size_t pixelCount =
        static_cast<std::size_t>(width) *
        static_cast<std::size_t>(height);

    std::vector<float> rawDensity(
        pixelCount,
        0.0f
    );

    std::vector<float> finalDensity(
        pixelCount,
        0.0f
    );

    const int massPeriod =
        std::max(
            8,
            static_cast<int>(
                std::round(
                    profile.generation.massScale
                )
            )
        );

    const int erosionPeriod =
        std::max(
            12,
            static_cast<int>(
                std::round(
                    profile.generation.erosionScale
                )
            )
        );

    const int fragmentPeriod =
        std::max(
            18,
            static_cast<int>(
                std::round(
                    profile.generation.edgeFragmentScale
                )
            )
        );

    const int detailPeriod =
        std::max(
            24,
            static_cast<int>(
                std::round(
                    profile.generation.detailScale
                )
            )
        );

    /*
        Первый проход:
        глобальная weather coverage + billow cloud mass
        + направленная Worley-эрозия.
    */
    for (int y = 0;
         y < height;
         ++y)
    {
        const float v =
            (
                static_cast<float>(y) +
                0.5f
            ) /
            static_cast<float>(height);

        const float latitude =
            (
                v -
                0.5f
            ) *
            2.0f;

        for (int x = 0;
             x < width;
             ++x)
        {
            const float u =
                (
                    static_cast<float>(x) +
                    0.5f
                ) /
                static_cast<float>(width);

            const float coverage =
                sampleWeatherCoverage(
                    weatherMap,
                    u,
                    v
                );

            if (coverage <= 0.0001f)
                continue;

            const float warpX =
                (
                    fbmPeriodicX(
                        u * 7.0f,
                        v * 4.0f,
                        7,
                        seed + 301u,
                        3
                    ) -
                    0.5f
                ) *
                profile.generation.
                    domainWarpStrength *
                1.4f;

            const float warpY =
                (
                    fbmPeriodicX(
                        u * 9.0f + 13.0f,
                        v * 5.0f - 7.0f,
                        9,
                        seed + 367u,
                        3
                    ) -
                    0.5f
                ) *
                profile.generation.
                    domainWarpStrength;

            const float warpedU =
                wrap01(
                    u +
                    warpX
                );

            const float warpedV =
                std::clamp(
                    v +
                    warpY,
                    0.0f,
                    1.0f
                );

            const float billow =
                billowFbmPeriodicX(
                    warpedU *
                        static_cast<float>(
                            massPeriod
                        ),
                    warpedV * 8.0f,
                    massPeriod,
                    seed + 503u,
                    5
                );

            const float supportingMass =
                fbmPeriodicX(
                    warpedU *
                        static_cast<float>(
                            massPeriod
                        ) *
                        0.72f,
                    warpedV * 5.5f,
                    massPeriod,
                    seed + 571u,
                    4
                );

            const float shape =
                billow *
                    profile.generation.
                        billowStrength +
                supportingMass *
                    (
                        1.0f -
                        profile.generation.
                            billowStrength
                    );

            /*
                Чем выше weather coverage, тем ниже порог
                появления облачной массы.
            */
            const float massThreshold =
                0.74f -
                coverage *
                    0.38f;

            float density =
                smoothStep(
                    massThreshold,
                    massThreshold +
                        std::max(
                            0.035f,
                            profile.generation.
                                softEdgeWidth
                        ),
                    shape
                );

            /*
                Worley применяется не как самостоятельный
                рисунок, а как эрозия слабых частей массы.
            */
            const float worleyDistance =
                worleyPeriodicX(
                    warpedU *
                        static_cast<float>(
                            erosionPeriod
                        ),
                    warpedV * 15.0f,
                    erosionPeriod,
                    seed + 809u
                );

            const float erosionField =
                smoothStep(
                    0.18f,
                    0.72f,
                    worleyDistance
                );

            const float edgeSensitivity =
                1.0f -
                smoothStep(
                    0.58f,
                    0.92f,
                    density
                );

            density -=
                erosionField *
                profile.generation.
                    worleyErosionStrength *
                edgeSensitivity *
                0.72f;

            /*
                Полюса equirectangular texture сходятся
                в точки. Не обнуляем облачность, но мягко
                уменьшаем агрессивную фрагментацию.
            */
            const float polarStability =
                1.0f -
                smoothStep(
                    0.78f,
                    0.99f,
                    std::abs(latitude)
                ) *
                    0.42f;

            density *=
                polarStability;



            const float fineBillow =
                billowFbmPeriodicX(
                    warpedU *
                        static_cast<float>(detailPeriod) *
                        1.25f,
                    warpedV * 20.0f,
                    detailPeriod,
                    seed + 1601u,
                    4
                );

            const float wispyNoise =
                fbmPeriodicX(
                    warpedU *
                        static_cast<float>(detailPeriod) *
                        2.1f,
                    warpedV * 34.0f,
                    detailPeriod,
                    seed + 1709u,
                    4
                );

            const float coreMask =
                smoothStep(
                    0.42f,
                    0.86f,
                    density
                );

            const float fringeMask =
                smoothStep(
                    0.04f,
                    0.36f,
                    density
                ) *
                (
                    1.0f -
                    smoothStep(
                        0.52f,
                        0.88f,
                        density
                    )
                );

            /*
                Внутренняя масса облака.
            */
            density +=
                (fineBillow - 0.55f) *
                0.22f *
                coreMask;

            /*
                Полупрозрачная рваная кромка.
            */
            density +=
                (wispyNoise - 0.50f) *
                0.26f *
                fringeMask;

            density =
                std::clamp(
                    density,
                    0.0f,
                    1.0f
                );

                

            rawDensity[
                fieldIndex(
                    x,
                    y,
                    width
                )
            ] =
                std::clamp(
                    density,
                    0.0f,
                    1.0f
                );
        }
    }

    /*
        Второй проход:
        морфологическая граница основной массы.

        boundary = dilation - erosion.
        Именно возле неё разрешены отдельные мелкие облака.
    */
    for (int y = 0;
         y < height;
         ++y)
    {
        const float v =
            (
                static_cast<float>(y) +
                0.5f
            ) /
            static_cast<float>(height);

        for (int x = 0;
             x < width;
             ++x)
        {
            const float u =
                (
                    static_cast<float>(x) +
                    0.5f
                ) /
                static_cast<float>(width);

            float localMinimum =
                1.0f;

            float localMaximum =
                0.0f;

            for (int offsetY = -1;
                 offsetY <= 1;
                 ++offsetY)
            {
                for (int offsetX = -1;
                     offsetX <= 1;
                     ++offsetX)
                {
                    const float sample =
                        fieldAt(
                            rawDensity,
                            x + offsetX,
                            y + offsetY,
                            width,
                            height
                        );

                    localMinimum =
                        std::min(
                            localMinimum,
                            sample
                        );

                    localMaximum =
                        std::max(
                            localMaximum,
                            sample
                        );
                }
            }

            const float boundary =
                std::clamp(
                    localMaximum -
                    localMinimum,
                    0.0f,
                    1.0f
                );

            const float fragmentNoise =
                billowFbmPeriodicX(
                    u *
                        static_cast<float>(
                            fragmentPeriod
                        ),
                    v * 24.0f,
                    fragmentPeriod,
                    seed + 1201u,
                    3
                );

            const float fragmentGate =
                smoothStep(
                    1.0f -
                        std::clamp(
                            profile.generation.
                                detachedFragmentProbability,
                            0.02f,
                            0.95f
                        ),
                    1.0f,
                    fragmentNoise
                );

            const float fragments =
                boundary *
                fragmentGate *
                profile.generation.
                    edgeFragmentStrength;

            /*
                Дополнительные маленькие пустоты непосредственно
                внутри границы. Это ломает гладкую линию.
            */
            const float cavityNoise =
                fbmPeriodicX(
                    u *
                        static_cast<float>(
                            detailPeriod
                        ),
                    v * 30.0f,
                    detailPeriod,
                    seed + 1427u,
                    3
                );

            const float cavities =
                boundary *
                smoothStep(
                    0.67f,
                    0.88f,
                    cavityNoise
                ) *
                profile.generation.
                    worleyErosionStrength *
                0.38f;

            const float source =
                rawDensity[
                    fieldIndex(
                        x,
                        y,
                        width
                    )
                ];

            finalDensity[
                fieldIndex(
                    x,
                    y,
                    width
                )
            ] =
                std::clamp(
                    std::max(
                        source -
                            cavities,
                        fragments
                    ),
                    0.0f,
                    1.0f
                );
        }
    }

    result.rgba.resize(
        pixelCount *
        4u,
        0u
    );

    /*
        Третий проход:
        цвет, псевдорельеф и alpha.
    */
    for (int y = 0;
         y < height;
         ++y)
    {
        const float v =
            (
                static_cast<float>(y) +
                0.5f
            ) /
            static_cast<float>(height);

        for (int x = 0;
             x < width;
             ++x)
        {
            const float u =
                (
                    static_cast<float>(x) +
                    0.5f
                ) /
                static_cast<float>(width);

            const float density =
                fieldAt(
                    finalDensity,
                    x,
                    y,
                    width,
                    height
                );

            const float gradientX =
                fieldAt(
                    finalDensity,
                    x + 1,
                    y,
                    width,
                    height
                ) -
                fieldAt(
                    finalDensity,
                    x - 1,
                    y,
                    width,
                    height
                );

            const float gradientY =
                fieldAt(
                    finalDensity,
                    x,
                    y + 1,
                    width,
                    height
                ) -
                fieldAt(
                    finalDensity,
                    x,
                    y - 1,
                    width,
                    height
                );

            /*
                Условный свет сверху-слева.
                Это не физическое освещение, но даёт внутреннюю
                структуру вместо плоской серой заливки.
            */
            const float gradientLighting =
                std::clamp(
                    0.64f +
                    gradientX *
                        1.55f -
                    gradientY *
                        1.20f,
                    0.25f,
                    1.15f
                );

            const float interiorDetail =
                fbmPeriodicX(
                    u *
                        static_cast<float>(
                            detailPeriod
                        ),
                    v * 28.0f,
                    detailPeriod,
                    seed + 1801u,
                    3
                );

            const float core =
                smoothStep(
                    0.48f,
                    0.92f,
                    density
                );

            float colorMix =
                0.18f +
                density * 0.28f +
                core * 0.30f +
                gradientLighting * 0.18f +
                (
                    interiorDetail -
                    0.5f
                ) *
                    0.14f;

            colorMix =
                std::clamp(
                    colorMix,
                    0.0f,
                    1.0f
                );

            glm::vec3 color =
                profile.appearance.shadowColor *
                    (1.0f - colorMix) +
                profile.appearance.baseColor *
                    colorMix;

            const float luminance =
                color.r * 0.2126f +
                color.g * 0.7152f +
                color.b * 0.0722f;

            color =
                glm::mix(
                    glm::vec3(
                        luminance
                    ),
                    color,
                    std::clamp(
                        profile.appearance.saturation,
                        0.0f,
                        1.0f
                    )
                );

            color =
                (
                    color -
                    glm::vec3(0.5f)
                ) *
                    std::max(
                        0.01f,
                        profile.appearance.contrast
                    ) +
                glm::vec3(0.5f);

            color *=
                std::clamp(
                    0.66f +
                    gradientLighting * 0.28f +
                    core * 0.14f,
                    0.0f,
                    1.18f
                );

            const float coreAlpha =
                smoothStep(
                    0.08f,
                    0.70f,
                    density
                );

            const float fringeAlpha =
                smoothStep(
                    0.03f,
                    0.28f,
                    density
                ) *
                (
                    1.0f -
                    smoothStep(
                        0.46f,
                        0.84f,
                        density
                    )
                );

            const float alpha =
                std::clamp(
                    coreAlpha * 0.86f +
                    fringeAlpha * 0.22f,
                    0.0f,
                    1.0f
                );

            const std::size_t destinationIndex =
                fieldIndex(
                    x,
                    y,
                    width
                ) *
                4u;

            result.rgba[
                destinationIndex + 0u
            ] =
                toByte(color.r);

            result.rgba[
                destinationIndex + 1u
            ] =
                toByte(color.g);

            result.rgba[
                destinationIndex + 2u
            ] =
                toByte(color.b);

            result.rgba[
                destinationIndex + 3u
            ] =
                toByte(alpha);
        }
    }

    return result;
}

} // namespace render::celestial::clouds