#version 330 core

out vec4 outColor;

const float PI =
    3.14159265358979323846;

const int MAX_RING_BANDS =
    16;

uniform vec2 uViewportOriginPx;
uniform vec2 uViewportSize;
uniform vec2 uPlanetCenterPx;

uniform vec2 uRingAxisXPx;
uniform vec2 uRingAxisYPx;
uniform vec2 uRingDepthCoefficients;

uniform int uRenderPart;
uniform int uBandCount;

uniform vec4 uBandGeometry[MAX_RING_BANDS];
uniform vec4 uBandColorOptical[MAX_RING_BANDS];
uniform vec4 uBandAppearance[MAX_RING_BANDS];
uniform vec4 uBandExtra[MAX_RING_BANDS];

/*
    x = particle clumpiness
    y = radial jitter
    z = minimum particle size px
    w = maximum particle size px
*/
uniform vec4 uBandParticle[MAX_RING_BANDS];

uniform float uRotationPhase;

/*
    0 = layered_bands
    1 = particle_cloud
*/
uniform int uVisualMode;

/*
    main, secondary, faint, diffuse.
*/
uniform vec4 uVisualBandEmphasis;

/*
    x = artistic width scale
    y = gap contrast
    z = radial structure strength
    w = fine structure strength
*/
uniform vec4 uVisualStructure;

/*
    x = particle density
    y = particle opacity scale
    z = minimum particle size
    w = maximum particle size
*/
uniform vec4 uVisualParticle;

/*
    x = radial jitter
    y = azimuthal clumping
    z = cluster scale
    w = softness
*/
uniform vec4 uVisualParticleShape;

/*
    x = artistic occlusion enabled
    y = back-half brightness
    z = inner-edge darkening
    w = global brightness variation
*/
uniform vec4 uVisualOcclusion;

/*
    x = edge softness scale
    y = minimum visible width in pixels
    z = minimum main band width in pixels
    w = continuous fill for particle mode
*/
uniform vec4 uVisualLod;

float hash11(
    float value
)
{
    return
        fract(
            sin(
                value *
                    127.1
            ) *
            43758.5453123
        );
}

float hash21(
    vec2 value
)
{
    return
        fract(
            sin(
                dot(
                    value,
                    vec2(
                        127.1,
                        311.7
                    )
                )
            ) *
            43758.5453123
        );
}

vec2 hash22(
    vec2 value
)
{
    return
        fract(
            sin(
                vec2(
                    dot(
                        value,
                        vec2(
                            127.1,
                            311.7
                        )
                    ),
                    dot(
                        value,
                        vec2(
                            269.5,
                            183.3
                        )
                    )
                )
            ) *
            43758.5453123
        );
}

float radialNoise(
    float radius,
    float seed
)
{
    float first =
        sin(
            radius *
                96.0 +
            seed *
                13.0
        );

    float second =
        sin(
            radius *
                287.0 +
            seed *
                29.0
        );

    float third =
        sin(
            radius *
                811.0 +
            seed *
                47.0
        );

    return
        first * 0.52 +
        second * 0.31 +
        third * 0.17;
}

float visibilityEmphasis(
    int visibilityClass
)
{
    if (visibilityClass == 0)
        return uVisualBandEmphasis.x;

    if (visibilityClass == 1)
        return uVisualBandEmphasis.y;

    if (visibilityClass == 3)
        return uVisualBandEmphasis.w;

    return uVisualBandEmphasis.z;
}

float particleCloudMask(
    float ringRadius,
    float bandWidth,
    float bandU,
    float azimuth,
    float pixelsPerPlanetRadius,
    float seed,
    float densityScale,
    vec4 bandParticle
)
{
    

/*
    Размер procedural-cell выбирается в экранных
    пикселях. Это принципиально важно: при прежних
    72–180 radial cells узкое кольцо делилось на
    субпиксельные полосы, и частицы исчезали.
*/
float targetCellSizePx =
    mix(
        5.5,
        3.5,
        clamp(
            densityScale,
            0.0,
            1.0
        )
    );

float radialWidthPx =
    max(
        1.0,
        bandWidth *
            pixelsPerPlanetRadius
    );

float circumferencePx =
    max(
        1.0,
        2.0 *
            PI *
            ringRadius *
            pixelsPerPlanetRadius
    );

/*
    Ячейки остаются приблизительно квадратными
    в экранном пространстве.
*/
float radialCells =
    max(
        1.0,
        floor(
            radialWidthPx /
                targetCellSizePx
        )
    );

float angularCells =
    max(
        24.0,
        floor(
            circumferencePx /
                targetCellSizePx
        )
    );



    vec2 particleCoordinate =
        vec2(
            bandU *
                radialCells,

            (
                azimuth /
                (
                    2.0 *
                    PI
                ) +
                0.5
            ) *
                angularCells
        );

    vec2 cell =
        floor(
            particleCoordinate
        );

    vec2 local =
        fract(
            particleCoordinate
        );

    vec2 randomPoint =
    0.10 +
    hash22(
        cell +
        seed
    ) *
    0.80;

/*
    Дополнительный хаотический сдвиг разрушает
    визуально правильные радиальные и угловые ряды.
*/
randomPoint +=
    (
        hash22(
            cell.yx +
            seed *
                3.17
        ) -
        0.5
    ) *
    vec2(
        0.18,
        0.24
    );



    randomPoint =
    clamp(
        randomPoint,
        vec2(0.04),
        vec2(0.96)
    );

    /*
        Радиальный разброс разрушает идеальные окружности.
    */
    float effectiveRadialJitter =
    mix(
        uVisualParticleShape.x,
        bandParticle.y,
        0.65
    );

    randomPoint.x +=
        (
            hash21(
                cell +
                    seed *
                        2.7
            ) -
            0.5
        ) *
        effectiveRadialJitter;

    /*
    Линейное произведение давало слишком малую
    вероятность для слабых пылевых колец.

    Корень усиливает редкие системы художественно,
    не превращая их в сплошную поверхность.
*/
float particleProbability =
    clamp(
        sqrt(
            max(
                0.0,
                uVisualParticle.x *
                    densityScale
            )
        ) *
            1.05,
        0.10,
        0.88
    );

    /*
        Крупные неоднородные кластеры.
    */
    float cluster =
        0.5 +
        0.5 *
        sin(
            azimuth *
                (
                    2.0 +
                    uVisualParticleShape.z *
                        8.0
                ) +
            seed *
                9.0 +
            ringRadius *
                11.0
        );

    float effectiveClumpiness =
    mix(
        uVisualParticleShape.y,
        bandParticle.x,
        0.65
    );

    particleProbability *=
        mix(
            1.0,
            smoothstep(
                0.15,
                0.92,
                cluster
            ),
            effectiveClumpiness
        );

    float existence =
        step(
            hash21(
                cell +
                seed *
                    5.3
            ),
            particleProbability
        );

    float distanceToParticle =
        length(
            local -
            randomPoint
        );

    float minimumParticleSize =
        max(
            0.05,
            mix(
                uVisualParticle.z,
                bandParticle.z,
                0.65
            )
        );

    float maximumParticleSize =
        max(
            minimumParticleSize,
            mix(
                uVisualParticle.w,
                bandParticle.w,
                0.65
            )
        );

    float sizeVariation =
        mix(
            minimumParticleSize,
            maximumParticleSize,
            hash21(
                cell +
                    seed *
                        11.7
            )
        );

    /*
        sizeVariation приходит в пикселях, но здесь переводится
        в относительный размер частицы внутри polar-cell.
    */
    /*
    Это не физический размер отдельного камня.
    Это размер визуального пылевого пятна внутри
    процедурной polar-cell.
*/
/*
    sizeVariation хранится в пикселях.

    Переводим радиус частицы в координаты текущей
    экранной ячейки, размер которой targetCellSizePx.
*/
float normalizedRadius =
    clamp(
        (
            sizeVariation *
                0.5
        ) /
            max(
                1.0,
                targetCellSizePx
            ),
        0.075,
        0.34
    );

    float softness =
    mix(
        0.025,
        0.09,
        clamp(
            uVisualParticleShape.w,
            0.0,
            1.0
        )
    );

    float dotMask =
        1.0 -
        smoothstep(
            normalizedRadius -
                softness,
            normalizedRadius +
                softness,
            distanceToParticle
        );

    return
        dotMask *
        existence;
}

void main()
{
    vec2 localFragmentPx =
        gl_FragCoord.xy -
        uViewportOriginPx;

    vec2 planetCenterGlPx =
        vec2(
            uPlanetCenterPx.x,
            uViewportSize.y -
                uPlanetCenterPx.y
        );

    vec2 screenOffset =
        localFragmentPx -
        planetCenterGlPx;

    
    
    /*
    uRingAxisXPx и uRingAxisYPx рассчитаны
    в экранных координатах карты:

        X направлен вправо,
        Y направлен вниз.

    gl_FragCoord использует OpenGL-координаты:

        X направлен вправо,
        Y направлен вверх.

    Центр планеты мы уже перевернули по Y.
    Поэтому проекционные оси кольца тоже должны
    быть переведены в OpenGL-систему.
*/
vec2 ringAxisXGl =
    vec2(
        uRingAxisXPx.x,
        -uRingAxisXPx.y
    );

vec2 ringAxisYGl =
    vec2(
        uRingAxisYPx.x,
        -uRingAxisYPx.y
    );

float determinant =
    ringAxisXGl.x *
        ringAxisYGl.y -
    ringAxisXGl.y *
        ringAxisYGl.x;

if (abs(determinant) <
    0.00001)
{
    discard;
}

vec2 ringCoordinate;

ringCoordinate.x =
    (
        screenOffset.x *
            ringAxisYGl.y -
        screenOffset.y *
            ringAxisYGl.x
    ) /
    determinant;

ringCoordinate.y =
    (
        ringAxisXGl.x *
            screenOffset.y -
        ringAxisXGl.y *
            screenOffset.x
    ) /
    determinant;





    float ringRadius =
        length(
            ringCoordinate
        );

    float depth =
        dot(
            ringCoordinate,
            uRingDepthCoefficients
        );

    if (uRenderPart == 0 &&
        depth >= 0.0)
    {
        discard;
    }

    if (uRenderPart == 1 &&
        depth < 0.0)
    {
        discard;
    }

    float azimuth =
        atan(
            ringCoordinate.y,
            ringCoordinate.x
        ) +
        uRotationPhase;

    vec4 accumulated =
        vec4(0.0);

    for (int bandIndex = 0;
         bandIndex < MAX_RING_BANDS;
         ++bandIndex)
    {
        if (bandIndex >=
            uBandCount)
        {
            break;
        }

        vec4 geometry =
            uBandGeometry[
                bandIndex
            ];

        vec4 colorOptical =
            uBandColorOptical[
                bandIndex
            ];

        vec4 appearance =
            uBandAppearance[
                bandIndex
            ];

        vec4 extra =
            uBandExtra[
                bandIndex
            ];

        vec4 bandParticle =
            uBandParticle[
                bandIndex
            ];

        float originalInner =
            geometry.x;

        float originalOuter =
            geometry.y;



        float centerRadius =
    (
        originalInner +
        originalOuter
    ) *
    0.5;

int visibilityClass =
    int(
        geometry.w +
        0.5
    );

float halfWidth =
    (
        originalOuter -
        originalInner
    ) *
    0.5;

    /*
        Приблизительный экранный масштаб одного радиуса планеты.
    */
    float pixelsPerPlanetRadius =
        max(
            0.0001,
            (
                length(
                    uRingAxisXPx
                ) +
                length(
                    uRingAxisYPx
                )
            ) *
            0.5
        );

    float minimumWidthPx =
        visibilityClass == 0
            ? uVisualLod.z
            : uVisualLod.y;

    float minimumHalfWidth =
        (
            minimumWidthPx /
            pixelsPerPlanetRadius
        ) *
        0.5;

    halfWidth =
        max(
            halfWidth,
            minimumHalfWidth
        );


        /*
            Приблизительное количество экранных пикселей
            на одну planet-radius unit.

            Берём среднюю длину двух проекционных осей.
        */
        

        




        float classEmphasis =
            visibilityEmphasis(
                visibilityClass
            );

        /*
            Сатурноподобные системы получают чуть более
            широкие визуальные bands без изменения физики.
        */
        float widthScale =
            mix(
                1.0,
                uVisualStructure.x,
                clamp(
                    classEmphasis,
                    0.0,
                    1.5
                )
            );

        float innerRadius =
            centerRadius -
            halfWidth *
                widthScale;

        float outerRadius =
            centerRadius +
            halfWidth *
                widthScale;

        if (ringRadius <
                innerRadius ||
            ringRadius >
                outerRadius)
        {
            continue;
        }

        float bandWidth =
            max(
                0.00001,
                outerRadius -
                    innerRadius
            );

        float bandU =
            (
                ringRadius -
                    innerRadius
            ) /
            bandWidth;

        float edgeSoftness =
            max(
                0.001,
                geometry.z *
                    max(
                        0.05,
                        uVisualLod.x
                    )
            );

        /*
            gapContrast делает края главных bands
            более отчётливыми.
        */
        edgeSoftness /=
            max(
                0.25,
                uVisualStructure.y
            );

        float edgeFade =
            smoothstep(
                0.0,
                edgeSoftness,
                bandU
            ) *
            (
                1.0 -
                smoothstep(
                    1.0 -
                        edgeSoftness,
                    1.0,
                    bandU
                )
            );

        float seed =
            extra.x;

        /*
            extra.y уже содержит visual_opacity_scale
            конкретного band-а.

            В подготовленном JSON он уже рассчитан
            с учётом visibility class. Повторно умножать
            его на classEmphasis нельзя, иначе faint и
            diffuse rings подавляются дважды.
        */
        float opacityScale =
            max(
                0.0,
                extra.y
            );

        float structureScale =
            extra.z;

        float densityScale =
            extra.w;

        float noiseValue =
            radialNoise(
                ringRadius,
                seed
            );

        float alpha = 0.0;
        float brightness = 1.0;

        if (uVisualMode == 0)
        {
            /*
                Layered bands:
                крупная слоистость + тонкая структура.
            */
            float broadStructure =
                0.5 +
                0.5 *
                sin(
                    bandU *
                        (
                            8.0 +
                            uVisualStructure.z *
                                22.0
                        ) *
                        PI +
                    seed *
                        4.0
                );

            float fineStructure =
                0.5 +
                0.5 *
                sin(
                    bandU *
                        (
                            80.0 +
                            uVisualStructure.w *
                                260.0
                        ) *
                        PI +
                    seed *
                        17.0
                );

            float structure =
                mix(
                    1.0,
                    mix(
                        broadStructure,
                        fineStructure,
                        0.28
                    ),
                    clamp(
                        structureScale *
                            uVisualStructure.z,
                        0.0,
                        1.0
                    )
                );

            float density =
                appearance.x *
                opacityScale *
                edgeFade *
                mix(
                    0.38,
                    1.28,
                    structure
                );

            density *=
                1.0 -
                appearance.y *
                (
                    0.20 +
                    noiseValue *
                        0.16
                );

            alpha =
                1.0 -
                exp(
                    -density *
                    max(
                        0.001,
                        colorOptical.a
                    )
                );

            brightness =
                1.0 +
                noiseValue *
                    (
                        appearance.z +
                        uVisualOcclusion.w
                    ) *
                    0.45;
        }
        else
        {
            /*
                Particle cloud:
                непрерывной поверхности больше нет.
            */
            float particles =
    particleCloudMask(
        ringRadius,
        bandWidth,
        bandU,
        azimuth,
        pixelsPerPlanetRadius,
        seed,
        densityScale,
        bandParticle
    );

            float faintHaze =
                uVisualLod.w *
                uVisualParticleShape.w *
                uVisualParticle.x *
                uVisualParticle.y *
                uVisualParticleShape.z *
                0.08;

            
            


            /*
    Физические opacity пылевых колец очень малы,
    но при прямом линейном использовании отдельные
    частицы полностью исчезают.

    Здесь используется художественное усиление
    только попавшей в пиксель частицы. Пространство
    между частицами остаётся прозрачным.
*/
float rawParticleOpacity =
    clamp(
        appearance.x *
        opacityScale *
        uVisualParticle.y,
        0.0,
        1.0
    );

/*
    Экспоненциальное преобразование делает слабые
    частицы видимыми, не превращая их в непрозрачные.
*/
float artisticParticleGain =
    visibilityClass == 0
        ? 22.0
        : (
            visibilityClass == 1
                ? 18.0
                : 14.0
        );

float visibleParticleOpacity =
    1.0 -
    exp(
        -rawParticleOpacity *
            artisticParticleGain
    );
alpha =
    particles *
    visibleParticleOpacity *
    edgeFade;



    /*
    Если procedural particle существует, он не должен
    полностью исчезать из-за очень малой физической
    opacity. Минимум действует только внутри точки,
    а не на всю поверхность кольца.
*/
if (particles >
    0.05)
{
    float minimumDotAlpha =
    visibilityClass == 0
        ? 0.24
        : (
            visibilityClass == 1
                ? 0.16
                : (
                    visibilityClass == 2
                        ? 0.10
                        : 0.065
                )
        );

    alpha =
        max(
            alpha,
            particles *
                minimumDotAlpha *
                edgeFade
        );
}

alpha +=
    faintHaze *
    edgeFade;







            brightness =
                0.76 +
                hash11(
                    seed +
                    bandU *
                        91.0
                ) *
                0.38;
        }

        /*
            Нефизическое художественное затемнение задней части.
        */
if (uRenderPart == 0)
{
    if (uVisualMode == 0)
    {
        /*
            Layered Saturn-like rings могут иметь
            лёгкое нефизическое затемнение сзади.
        */
        if (uVisualOcclusion.x >
            0.5)
        {
            brightness *=
                uVisualOcclusion.y;
        }
        else
        {
            brightness *=
                0.92;
        }
    }
    else
    {
        /*
            Разреженное пылевое облако почти одинаково
            читается на передней и задней половинах.
        */
        brightness *=
            0.98;
    }
}

        /*
            Небольшое затемнение внутреннего края
            без расчёта положения звезды.
        */
        brightness *=
            1.0 -
            uVisualOcclusion.z *
            (
                1.0 -
                smoothstep(
                    0.0,
                    0.35,
                    bandU
                )
            );

        float azimuthVariation =
            1.0 +
            appearance.w *
            (
                sin(
                    azimuth *
                        3.0 +
                    seed *
                        11.0
                ) *
                    0.54 +
                sin(
                    azimuth *
                        7.0 +
                    seed *
                        19.0
                ) *
                    0.24
            );

        alpha *=
            clamp(
                azimuthVariation,
                0.0,
                2.0
            );

        vec3 bandColor =
    colorOptical.rgb *
    brightness;

if (uVisualMode == 1)
{
    /*
        Пылевые кольца в реальности очень тёмные, но при
        буквальном цвете они исчезают на космическом фоне.

        Усиливаем только визуальную яркость частиц,
        не создавая сплошную поверхность.
    */
    float particleColorBoost =
        visibilityClass == 0
            ? 2.15
            : (
                visibilityClass == 1
                    ? 1.75
                    : 1.45
            );

    bandColor =
        mix(
            bandColor,
            vec3(
                dot(
                    bandColor,
                    vec3(
                        0.299,
                        0.587,
                        0.114
                    )
                )
            ),
            0.18
        );

    bandColor *=
        particleColorBoost;
}

        accumulated.rgb +=
            bandColor *
            alpha *
            (
                1.0 -
                accumulated.a
            );

        accumulated.a +=
            alpha *
            (
                1.0 -
                accumulated.a
            );
    }

    if (accumulated.a <=
        0.0001)
    {
        discard;
    }

    vec3 straightColor =
        accumulated.rgb /
        max(
            0.0001,
            accumulated.a
        );

    outColor =
        vec4(
            clamp(
                straightColor,
                vec3(0.0),
                vec3(1.0)
            ),
            clamp(
                accumulated.a,
                0.0,
                1.0
            )
        );
}