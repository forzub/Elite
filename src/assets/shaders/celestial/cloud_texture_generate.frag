#version 330 core

in vec2 vUv;

out vec4 outColor;

const float PI =
    3.14159265358979323846;

const int MAX_CLIMATE_ZONES = 8;

uniform float uSeed;
uniform int uPattern;

/*
    0 = terrestrial
    1 = gas giant banded
    2 = venusian streaks
    3 = ice giant banded
*/
uniform int uMorphology;

uniform bool uPrimaryOpaqueDeck;
uniform float uOpacityFloor;

uniform float uEvolutionTime;

uniform float uGlobalCoverage;
uniform float uLayerDensity;

uniform float uWeatherScale;
uniform float uMassScale;
uniform float uErosionScale;
uniform float uDetailScale;

uniform float uDomainWarpStrength;
uniform float uBillowStrength;
uniform float uWorleyErosionStrength;

uniform float uEdgeFragmentStrength;
uniform float uEdgeFragmentScale;
uniform float uDetachedFragmentProbability;
uniform float uSoftEdgeWidth;

uniform vec3 uCloudColor;
uniform vec3 uShadowColor;

uniform int uZoneCount;

/*
    zoneA:
        x = latitudeCenterDeg
        y = halfWidthDeg
        z = coverageMultiplier
        w = densityMultiplier

    zoneB:
        x = fragmentation
        y = zonalStretch
        z = driftSpeedMultiplier
        w = unused
*/
uniform vec4 uZoneA[MAX_CLIMATE_ZONES];
uniform vec4 uZoneB[MAX_CLIMATE_ZONES];

float saturateFloat(
    float value
)
{
    return
        clamp(
            value,
            0.0,
            1.0
        );
}

float hash31(
    vec3 p
)
{
    p =
        fract(
            p * 0.1031
        );

    p +=
        dot(
            p,
            p.yzx + 33.33
        );

    return
        fract(
            (p.x + p.y) *
            p.z
        );
}

float valueNoise3(
    vec3 position
)
{
    vec3 cell =
        floor(position);

    vec3 local =
        fract(position);

    vec3 blend =
        local *
        local *
        (
            3.0 -
            2.0 * local
        );

    float n000 =
        hash31(
            cell +
            vec3(0.0, 0.0, 0.0)
        );

    float n100 =
        hash31(
            cell +
            vec3(1.0, 0.0, 0.0)
        );

    float n010 =
        hash31(
            cell +
            vec3(0.0, 1.0, 0.0)
        );

    float n110 =
        hash31(
            cell +
            vec3(1.0, 1.0, 0.0)
        );

    float n001 =
        hash31(
            cell +
            vec3(0.0, 0.0, 1.0)
        );

    float n101 =
        hash31(
            cell +
            vec3(1.0, 0.0, 1.0)
        );

    float n011 =
        hash31(
            cell +
            vec3(0.0, 1.0, 1.0)
        );

    float n111 =
        hash31(
            cell +
            vec3(1.0, 1.0, 1.0)
        );

    float nx00 =
        mix(
            n000,
            n100,
            blend.x
        );

    float nx10 =
        mix(
            n010,
            n110,
            blend.x
        );

    float nx01 =
        mix(
            n001,
            n101,
            blend.x
        );

    float nx11 =
        mix(
            n011,
            n111,
            blend.x
        );

    float nxy0 =
        mix(
            nx00,
            nx10,
            blend.y
        );

    float nxy1 =
        mix(
            nx01,
            nx11,
            blend.y
        );

    return
        mix(
            nxy0,
            nxy1,
            blend.z
        );
}

float fbm3(
    vec3 position,
    int octaveCount
)
{
    float result = 0.0;
    float normalization = 0.0;

    float amplitude = 0.5;
    float frequency = 1.0;

    for (int octave = 0;
         octave < 6;
         ++octave)
    {
        if (octave >= octaveCount)
            break;

        result +=
            valueNoise3(
                position *
                frequency
            ) *
            amplitude;

        normalization +=
            amplitude;

        frequency *=
            2.03;

        amplitude *=
            0.5;
    }

    return
        normalization > 0.0
            ? result /
                normalization
            : 0.0;
}

float billowFbm3(
    vec3 position,
    int octaveCount
)
{
    float result = 0.0;
    float normalization = 0.0;

    float amplitude = 0.5;
    float frequency = 1.0;

    for (int octave = 0;
         octave < 6;
         ++octave)
    {
        if (octave >= octaveCount)
            break;

        float source =
            valueNoise3(
                position *
                frequency
            );

        float billow =
            1.0 -
            abs(
                source * 2.0 -
                1.0
            );

        result +=
            billow *
            amplitude;

        normalization +=
            amplitude;

        frequency *=
            2.07;

        amplitude *=
            0.52;
    }

    return
        normalization > 0.0
            ? result /
                normalization
            : 0.0;
}

/*
    Дешёвая cellular-подобная эрозия.

    Это не полный Worley с 27 соседями:
    для runtime texture generation нам важнее скорость.
*/
float pseudoCellular3(
    vec3 position
)
{
    vec3 cell =
        floor(position);

    vec3 local =
        fract(position);

    float center =
        hash31(
            cell
        );

    float xNeighbour =
        hash31(
            cell +
            vec3(1.0, 0.0, 0.0)
        );

    float yNeighbour =
        hash31(
            cell +
            vec3(0.0, 1.0, 0.0)
        );

    float zNeighbour =
        hash31(
            cell +
            vec3(0.0, 0.0, 1.0)
        );

    float blended =
        mix(
            center,
            xNeighbour,
            local.x
        );

    blended =
        mix(
            blended,
            yNeighbour,
            local.y
        );

    blended =
        mix(
            blended,
            zNeighbour,
            local.z
        );

    return
        1.0 -
        abs(
            blended * 2.0 -
            1.0
        );
}

vec3 spherePositionFromUv(
    vec2 uv
)
{
    float longitude =
        (
            uv.x -
            0.5
        ) *
        2.0 *
        PI;

    float latitude =
        (
            0.5 -
            uv.y
        ) *
        PI;

    float cosLatitude =
        cos(latitude);

    return
        vec3(
            cosLatitude *
                cos(longitude),
            sin(latitude),
            cosLatitude *
                sin(longitude)
        );
}

void evaluateClimate(
    float latitudeDeg,
    out float coverageMultiplier,
    out float densityMultiplier,
    out float fragmentation,
    out float zonalStretch
)
{
    float accumulatedCoverage = 0.0;
    float accumulatedDensity = 0.0;
    float accumulatedFragmentation = 0.0;
    float accumulatedStretch = 0.0;
    float accumulatedWeight = 0.0;

    for (int zoneIndex = 0;
         zoneIndex < MAX_CLIMATE_ZONES;
         ++zoneIndex)
    {
        if (zoneIndex >= uZoneCount)
            break;

        vec4 zoneA =
            uZoneA[zoneIndex];

        vec4 zoneB =
            uZoneB[zoneIndex];

        float center =
            zoneA.x;

        float halfWidth =
            max(
                0.1,
                zoneA.y
            );

        float distanceToZone =
            abs(
                latitudeDeg -
                center
            );

        float weight =
            1.0 -
            smoothstep(
                halfWidth,
                halfWidth * 1.65,
                distanceToZone
            );

        accumulatedCoverage +=
            zoneA.z *
            weight;

        accumulatedDensity +=
            zoneA.w *
            weight;

        accumulatedFragmentation +=
            zoneB.x *
            weight;

        accumulatedStretch +=
            zoneB.y *
            weight;

        accumulatedWeight +=
            weight;
    }

    if (accumulatedWeight <= 0.0001)
    {
        coverageMultiplier = 1.0;
        densityMultiplier = 1.0;
        fragmentation = 0.35;
        zonalStretch = 1.0;
        return;
    }

    coverageMultiplier =
        accumulatedCoverage /
        accumulatedWeight;

    densityMultiplier =
        accumulatedDensity /
        accumulatedWeight;

    fragmentation =
        accumulatedFragmentation /
        accumulatedWeight;

    zonalStretch =
        accumulatedStretch /
        accumulatedWeight;
}


float latitudeBandField(
    float latitudeDeg,
    float warpA,
    float warpB,
    float bandFrequency,
    float irregularity
)
{
    float narrowBands =
        sin(
            radians(latitudeDeg) *
                bandFrequency +
            warpA *
                irregularity
        );

    float broadBands =
        sin(
            radians(latitudeDeg) *
                bandFrequency *
                0.42 +
            warpB *
                irregularity *
                0.73
        );

    return
        0.5 +
        0.5 *
        (
            narrowBands * 0.64 +
            broadBands * 0.36
        );
}

float venusianStreakField(
    vec3 spherePosition,
    float latitudeDeg,
    float warpA,
    float warpB,
    float timeValue
)
{
    /*
        Венерианские streaks:
        длинные почти зональные структуры,
        слегка наклонённые и деформированные.
    */
    float longitude =
        atan(
            spherePosition.z,
            spherePosition.x
        );

    float latitude =
        radians(
            latitudeDeg
        );

    float diagonalCoordinate =
        longitude * 1.35 +
        latitude * 5.2 +
        warpA * 1.8 -
        warpB * 1.1 +
        timeValue * 0.025;

    float fineStreak =
        sin(
            diagonalCoordinate * 5.0
        );

    float broadStreak =
        sin(
            diagonalCoordinate * 1.65 +
            warpB * 4.0
        );

    return
        clamp(
            0.5 +
            fineStreak * 0.23 +
            broadStreak * 0.27,
            0.0,
            1.0
        );
}




void main()
{
    vec2 uv =
        vec2(
            fract(vUv.x),
            clamp(
                vUv.y,
                0.0,
                1.0
            )
        );

    vec3 spherePosition =
        spherePositionFromUv(
            uv
        );

    float latitudeDeg =
        asin(
            clamp(
                spherePosition.y,
                -1.0,
                1.0
            )
        ) *
        180.0 /
        PI;

    float climateCoverage = 1.0;
    float climateDensity = 1.0;
    float climateFragmentation = 0.35;
    float zonalStretch = 1.0;

    evaluateClimate(
        latitudeDeg,
        climateCoverage,
        climateDensity,
        climateFragmentation,
        zonalStretch
    );

    /*
        Два независимо движущихся поля деформации.

        Они меняют не только положение, но и форму облака.
    */
    vec3 evolutionA =
        vec3(
            uEvolutionTime * 0.17,
            uEvolutionTime * 0.09,
            -uEvolutionTime * 0.13
        );

    vec3 evolutionB =
        vec3(
            -uEvolutionTime * 0.11,
            uEvolutionTime * 0.15,
            uEvolutionTime * 0.07
        );

    float warpA =
        fbm3(
            spherePosition *
                max(
                    1.0,
                    uWeatherScale
                ) +
            evolutionA +
            vec3(
                uSeed * 0.001,
                0.0,
                0.0
            ),
            3
        );

    float warpB =
        fbm3(
            spherePosition *
                max(
                    1.0,
                    uWeatherScale * 1.31
                ) +
            evolutionB +
            vec3(
                0.0,
                uSeed * 0.0017,
                0.0
            ),
            3
        );

    vec3 warpedPosition =
        normalize(
            spherePosition +
            vec3(
                warpA - 0.5,
                warpB - 0.5,
                warpA - warpB
            ) *
            uDomainWarpStrength
        );

    float weather =
        fbm3(
            warpedPosition *
                max(
                    1.0,
                    uWeatherScale
                ) +
            evolutionA * 0.45,
            5
        );

    float coverageProbability =
        clamp(
            uGlobalCoverage *
            mix(
                1.0,
                climateCoverage,
                0.58
            ),
            0.01,
            0.98
        );

    float weatherThreshold =
        0.86 -
        coverageProbability *
        0.62;

    float weatherMask =
        smoothstep(
            weatherThreshold -
                0.09,
            weatherThreshold +
                0.10,
            weather
        );



        /*
            Морфология газовых гигантов и Венеры.
        */
        if (uMorphology == 1)
        {
            /*
                Юпитер / Сатурн:
                обязательные широтные полосы.

                Различия между планетами получаются:
                - цветом каждого слоя;
                - circulation zones;
                - opacity;
                - generation scale из конкретного JSON.
            */
            float bandField =
                latitudeBandField(
                    latitudeDeg,
                    warpA,
                    warpB,
                    18.0,
                    3.4
                );

            weatherMask =
                mix(
                    0.62,
                    1.0,
                    smoothstep(
                        0.18,
                        0.82,
                        bandField
                    )
                );
        }
        else if (uMorphology == 2)
        {
            /*
                Венера:
                основной deck непрозрачен,
                верхние overlays имеют длинные streaks.
            */
            float streakField =
                venusianStreakField(
                    spherePosition,
                    latitudeDeg,
                    warpA,
                    warpB,
                    uEvolutionTime
                );

            if (uPrimaryOpaqueDeck)
            {
                weatherMask =
                    mix(
                        0.86,
                        1.0,
                        streakField
                    );
            }
            else
            {
                weatherMask *=
                    smoothstep(
                        0.34,
                        0.78,
                        streakField
                    );
            }
        }
        else if (uMorphology == 3)
        {
            /*
                Ice giants:
                более мягкие широкие полосы.
            */
            float bandField =
                latitudeBandField(
                    latitudeDeg,
                    warpA,
                    warpB,
                    10.0,
                    2.0
                );

            weatherMask =
                mix(
                    0.76,
                    1.0,
                    smoothstep(
                        0.20,
                        0.84,
                        bandField
                    )
                );
        }



    if (uPattern == 3)
    {
        float bandA =
            0.5 +
            0.5 *
            sin(
                latitudeDeg * 0.22 +
                warpA * 4.0
            );

        float bandB =
            0.5 +
            0.5 *
            sin(
                latitudeDeg * 0.08 +
                warpB * 5.0
            );

        float bandMask =
            mix(
                bandA,
                bandB,
                0.45
            );

        weatherMask *=
            smoothstep(
                0.28,
                0.88,
                bandMask
            );

        zonalStretch =
            max(
                zonalStretch,
                3.0
            );
    }





    vec3 massPosition =
        warpedPosition;

    if (uMorphology == 1)
    {
        /*
            Gas giants:
            вытягиваем структуры вдоль параллелей.
        */
        massPosition.y *=
            0.24;
    }
    else if (uMorphology == 2)
    {
        /*
            Венера:
            ещё более длинные, тонкие streaks.
        */
        massPosition.y *=
            0.14;

        massPosition.x +=
            massPosition.z * 0.42;
    }
    else if (uMorphology == 3)
    {
        massPosition.y *=
            0.38;
    }

    massPosition =
        normalize(
            massPosition
        );





    float ordinaryMass =
        fbm3(
            massPosition *
                max(
                    2.0,
                    uMassScale
                ) +
            evolutionB * 0.82,
            5
        );

    float billowMass =
        billowFbm3(
            massPosition *
                max(
                    2.0,
                    uMassScale
                ) +
            evolutionA * 1.12,
            5
        );

    float mass =
        mix(
            ordinaryMass,
            billowMass,
            clamp(
                uBillowStrength,
                0.0,
                1.0
            )
        );

    float patternThresholdOffset = 0.0;

        if (uPattern == 0)
    {
        patternThresholdOffset =
            0.075;
    }
    else if (uPattern == 2)
    {
        patternThresholdOffset =
            -0.075;
    }
    else if (uPattern == 3)
    {
        patternThresholdOffset =
            -0.045;
    }

    float massThreshold =
        0.71 -
        weatherMask *
            0.35 +
        patternThresholdOffset;

    float edgeWidth =
        max(
            0.025,
            uSoftEdgeWidth
        );

    float density =
        smoothstep(
            massThreshold,
            massThreshold +
                edgeWidth,
            mass
        );

    density *=
        weatherMask;


    if (uPattern == 3)
    {
        density *=
            mix(
                0.88,
                1.18,
                climateDensity
            );
    }


    /*
        Эрозия края использует независимое поле,
        поэтому край непрерывно меняет форму.
    */
    float erosion =
        pseudoCellular3(
            warpedPosition *
                max(
                    3.0,
                    uErosionScale
                ) +
            evolutionB * 1.47
        );

    float edgeMask =
        1.0 -
        smoothstep(
            0.52,
            0.90,
            density
        );

    density -=
        (
            1.0 -
            erosion
        ) *
        uWorleyErosionStrength *
        edgeMask *
        0.72;

    /*
        Мелкие фрагменты разрешены только в зоне края.
    */
    float fragmentNoise =
        billowFbm3(
            warpedPosition *
                max(
                    4.0,
                    uEdgeFragmentScale
                ) +
            evolutionA * 1.83,
            3
        );

    float boundaryBand =
        smoothstep(
            0.015,
            0.34,
            density
        ) *
        (
            1.0 -
            smoothstep(
                0.42,
                0.76,
                density
            )
        );

    float fragmentThreshold =
        1.0 -
        clamp(
            uDetachedFragmentProbability,
            0.02,
            0.95
        );

    float detachedFragments =
        smoothstep(
            fragmentThreshold,
            1.0,
            fragmentNoise
        ) *
        boundaryBand *
        uEdgeFragmentStrength;

    density =
        max(
            density,
            detachedFragments
        );

    /*
        Мелкая внутренняя текстура.
    */
    float detail =
        fbm3(
            warpedPosition *
                max(
                    4.0,
                    uDetailScale
                ) +
            evolutionB * 2.13,
            4
        );

    float coreMask =
        smoothstep(
            0.28,
            0.88,
            density
        );

    density +=
        (
            detail -
            0.5
        ) *
        0.22 *
        coreMask;

    density *=
        clamp(
            uLayerDensity *
            climateDensity,
            0.05,
            1.35
        );

    density =
        saturateFloat(
            density
        );

    /*
        Освещение запекается псевдообъёмно.
        Оно меняется вместе с внутренней структурой.
    */
    float highlightNoise =
        fbm3(
            warpedPosition *
                max(
                    3.0,
                    uDetailScale * 0.55
                ) -
            evolutionA * 0.67,
            3
        );

    float core =
        smoothstep(
            0.42,
            0.92,
            density
        );

    float colorMix =
        clamp(
            0.18 +
            density * 0.42 +
            core * 0.22 +
            (
                highlightNoise -
                0.5
            ) *
            0.18,
            0.0,
            1.0
        );

    vec3 color =
        mix(
            uShadowColor,
            uCloudColor,
            colorMix
        );



        float alpha =
            smoothstep(
                0.045,
                0.70,
                density
            );

        if (uPrimaryOpaqueDeck)
        {
            /*
                У газовых гигантов и Венеры это не прозрачные облака
                над поверхностью — это сам видимый диск планеты.
            */
            alpha =
                max(
                    alpha,
                    uOpacityFloor
                );
        }

        

    outColor =
        vec4(
            clamp(
                color,
                vec3(0.0),
                vec3(1.0)
            ),
            alpha
        );
}