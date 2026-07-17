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

float valueNoise1D(
    float coordinate,
    float seed
)
{
    float cell =
        floor(
            coordinate
        );

    float local =
        fract(
            coordinate
        );

    /*
        Smooth interpolation вместо прямого перехода
        между случайными значениями.
    */
    float blend =
        local *
        local *
        (
            3.0 -
            2.0 *
            local
        );

    float first =
        hash11(
            cell +
            seed *
                17.137
        );

    float second =
        hash11(
            cell +
            1.0 +
            seed *
                17.137
        );

    return
        mix(
            first,
            second,
            blend
        );
}


float layeredValueNoise1D(
    float coordinate,
    float seed
)
{
    float broad =
        valueNoise1D(
            coordinate,
            seed
        );

    float medium =
        valueNoise1D(
            coordinate *
                2.17 +
            13.1,
            seed +
                7.3
        );

    float fine =
        valueNoise1D(
            coordinate *
                5.03 +
            41.7,
            seed +
                19.1
        );

    return
        broad *
            0.55 +
        medium *
            0.30 +
        fine *
            0.15;
}


float periodicRingEdgeNoise(
    float azimuth,
    float seed
)
{
    /*
        Бесшовный шум по азимуту.

        Частоты целочисленные, поэтому при переходе atan()
        от +PI к -PI значение остаётся непрерывным и на
        границе кольца не возникает отдельного шва.
    */
    float broad =
        sin(
            azimuth *
                3.0 +
            seed *
                1.71
        );

    float medium =
        sin(
            azimuth *
                7.0 -
            seed *
                2.13
        );

    float fine =
        sin(
            azimuth *
                13.0 +
            seed *
                0.73
        );

    return
        clamp(
            broad *
                0.50 +
            medium *
                0.31 +
            fine *
                0.19,
            -1.0,
            1.0
        );
}



/*
    Художественная псевдотень от планеты на кольцах.

    Координаты ringCoordinate измеряются в радиусах планеты,
    поэтому радиус цилиндра тени можно задавать непосредственно
    около 1.0 без дополнительных uniforms.

    Функция возвращает маску 0..1. Сила затемнения применяется
    отдельно ниже, чтобы Saturn-like и Jupiter-like кольца могли
    иметь разную плотность тени.
*/
float planetCastRingShadowMask(
    vec2 ringCoordinate,
    vec2 shadowDirectionRing,
    float ringRadius,
    float azimuth
)
{
    vec2 shadowDirection =
        normalize(
            shadowDirectionRing
        );

    vec2 shadowSide =
        vec2(
            -shadowDirection.y,
            shadowDirection.x
        );

    /*
        along > 0 означает область в направлении тени,
        то есть от источника света за планетой.
    */
    float along =
        dot(
            ringCoordinate,
            shadowDirection
        );

    float across =
        abs(
            dot(
                ringCoordinate,
                shadowSide
            )
        );

    /*
        Тень близка по ширине к диаметру планеты,
        но слегка сужается вдали и имеет небольшой
        бесшовный шум, чтобы не выглядеть идеальным
        геометрическим прямоугольником.
    */
    float distanceAlong =
        max(
            0.0,
            along
        );

    float widthNoise =
        periodicRingEdgeNoise(
            azimuth,
            193.7
        );

    float shadowHalfWidth =
        mix(
            0.96,
            0.72,
            smoothstep(
                1.0,
                4.5,
                distanceAlong
            )
        );

    shadowHalfWidth *=
        1.0 +
        widthNoise *
            0.055;

    float penumbraWidth =
        0.14 +
        0.05 *
        smoothstep(
            1.0,
            4.0,
            distanceAlong
        );

    float crossMask =
        1.0 -
        smoothstep(
            shadowHalfWidth -
                penumbraWidth,
            shadowHalfWidth +
                penumbraWidth,
            across
        );

    /*
        У самой планеты тень появляется плавно,
        без жёсткой линии, а затем медленно теряет
        плотность к дальним участкам кольца.
    */
    float startMask =
        smoothstep(
            -0.20,
            0.42,
            along
        );

    float distanceFade =
        mix(
            1.0,
            0.68,
            smoothstep(
                1.15,
                5.0,
                distanceAlong
            )
        );

    /*
        За пределами типичных размеров системы колец
        тень постепенно исчезает. В обычных Details это
        почти не влияет, но защищает от длинного тёмного
        луча у экстремально больших систем.
    */
    float radialFade =
        1.0 -
        smoothstep(
            5.0,
            7.0,
            ringRadius
        );

    return
        clamp(
            crossMask *
            startMask *
            distanceFade *
            radialFade,
            0.0,
            1.0
        );
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
    float populationScale,
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
    populationScale уменьшает именно вероятность
    существования частицы. Он применяется после
    минимального clamp 0.10, поэтому возле края
    населённость действительно может уйти почти к нулю.
*/
particleProbability *=
    clamp(
        populationScale,
        0.0,
        1.0
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
            0.040,
            0.125,
            clamp(
                uVisualParticleShape.w,
                0.0,
                1.0
            )
        );

    /*
        Экранное anti-aliasing.

        Особенно важно на фоне крупной мягкой планеты:
        без него частицы выглядят белыми иглами.
    */
    float pixelSoftness =
        max(
            0.012,
            fwidth(
                distanceToParticle
            ) *
                1.50
        );

    float finalSoftness =
        softness +
        pixelSoftness;

    float dotMask =
        1.0 -
        smoothstep(
            normalizedRadius -
                finalSoftness,
            normalizedRadius +
                finalSoftness,
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

    /*
        Направление псевдотени задаётся в экранном пространстве.

        Свет условно приходит сверху-слева, поэтому тень идёт
        вниз-вправо. Затем экранный вектор переводится обратно
        в локальные координаты плоскости кольца тем же обратным
        преобразованием, которое используется для ringCoordinate.

        Благодаря этому художественный свет остаётся стабильным
        в кадре при наклоне кольца.
    */
    vec2 shadowDirectionScreenGl =
        normalize(
            vec2(
                0.72,
                -0.38
            )
        );

    vec2 shadowDirectionRing;

    shadowDirectionRing.x =
        (
            shadowDirectionScreenGl.x *
                ringAxisYGl.y -
            shadowDirectionScreenGl.y *
                ringAxisYGl.x
        ) /
        determinant;

    shadowDirectionRing.y =
        (
            ringAxisXGl.x *
                shadowDirectionScreenGl.y -
            ringAxisXGl.y *
                shadowDirectionScreenGl.x
        ) /
        determinant;

    if (length(shadowDirectionRing) <
        0.00001)
    {
        shadowDirectionRing =
            vec2(
                0.86,
                -0.51
            );
    }
    else
    {
        shadowDirectionRing =
            normalize(
                shadowDirectionRing
            );
    }





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

        float bandWidth =
            max(
                0.00001,
                outerRadius -
                    innerRadius
            );

        /*
            Для particle-cloud оставляем физические размеры
            band-а неизменными, но разрешаем небольшой
            художественный запас с обеих сторон. В этой зоне
            могут появляться редкие отдельные камни.
        */
        float particleVisualMargin =
            uVisualMode == 1
                ? max(
                    bandWidth *
                        0.20,
                    1.75 /
                        pixelsPerPlanetRadius
                )
                : 0.0;

        float renderInnerRadius =
            max(
                0.0,
                innerRadius -
                    particleVisualMargin
            );

        float renderOuterRadius =
            outerRadius +
            particleVisualMargin;

        if (ringRadius <
                renderInnerRadius ||
            ringRadius >
                renderOuterRadius)
        {
            continue;
        }

        /*
            bandU остаётся привязан к физическим границам.
            В художественной внешней зоне он может быть меньше
            0.0 или больше 1.0 — это используется для выбросов.
        */
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



        /*
            Минимальная мягкость в один экранный пиксель.

            Без этого узкие rings превращаются в слишком
            резкие концентрические проволоки.
        */
        edgeSoftness =
            max(
                edgeSoftness,
                fwidth(
                    bandU
                ) *
                    1.35
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
                Saturn-like rings.

                Азимутальный warp очень слабый: кольца остаются
                круглыми, но радиальная структура не выглядит
                как набор полос, нарезанных идеальным циркулем.
            */
            float azimuthWarp =
                sin(
                    azimuth *
                        2.0 +
                    seed *
                        1.7
                ) *
                    0.012 +
                sin(
                    azimuth *
                        5.0 -
                    seed *
                        0.9
                ) *
                    0.006;

            float irregularBandU =
                clamp(
                    bandU +
                    azimuthWarp *
                        (
                            0.35 +
                            appearance.w *
                                0.65
                        ),
                    0.0,
                    1.0
                );

            /*
                Три масштаба структуры:

                broad  — большие зоны плотности;
                medium — средние разрывы и пояса;
                fine   — тонкие детали без одинакового периода.
            */
            float broadStructure =
                layeredValueNoise1D(
                    irregularBandU *
                        (
                            4.0 +
                            uVisualStructure.z *
                                5.0
                        ),
                    seed
                );

            float mediumStructure =
                layeredValueNoise1D(
                    irregularBandU *
                        (
                            13.0 +
                            uVisualStructure.z *
                                17.0
                        ),
                    seed +
                        23.0
                );

            float fineStructure =
                layeredValueNoise1D(
                    irregularBandU *
                        (
                            48.0 +
                            uVisualStructure.w *
                                80.0
                        ),
                    seed +
                        61.0
                );

            float structure =
                broadStructure *
                    0.50 +
                mediumStructure *
                    0.32 +
                fineStructure *
                    0.18;

            /*
                Немного расширяем светлые и тёмные области,
                чтобы структура была похожа на слои вещества,
                а не на линейный градиент шума.
            */
            structure =
                smoothstep(
                    0.18,
                    0.86,
                    structure
                );

            float structureAmount =
                clamp(
                    structureScale *
                        uVisualStructure.z,
                    0.0,
                    1.0
                );

            float shapedDensity =
                mix(
                    0.72,
                    1.24,
                    structure
                );

            float density =
                appearance.x *
                opacityScale *
                edgeFade *
                mix(
                    1.0,
                    shapedDensity,
                    structureAmount
                );

            /*
                Старый radialNoise оставляем лишь как
                слабую микровариацию, а не как главный узор.
            */
            density *=
                1.0 -
                appearance.y *
                (
                    0.10 +
                    noiseValue *
                        0.08
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
                    0.22;
        }
        else
        {
            /*
                Jupiter-like particle cloud.

                Физические размеры band-а остаются прежними,
                но художественные inner/outer boundaries немного
                колеблются вдоль азимута.
            */
            float innerEdgeNoise =
                periodicRingEdgeNoise(
                    azimuth,
                    seed +
                        11.0
                );

            float outerEdgeNoise =
                periodicRingEdgeNoise(
                    azimuth,
                    seed +
                        47.0
                );

            /*
                bandU == 0.0 — физический внутренний край.
                bandU == 1.0 — физический внешний край.

                Иногда художественная граница уходит немного
                наружу, иногда отступает внутрь.
            */
            float innerBoundaryU =
                0.025 +
                innerEdgeNoise *
                    0.055;

            float outerBoundaryU =
                0.975 +
                outerEdgeNoise *
                    0.075;

            float irregularSpan =
                max(
                    0.25,
                    outerBoundaryU -
                        innerBoundaryU
                );

            float irregularU =
                (
                    bandU -
                        innerBoundaryU
                ) /
                irregularSpan;

            /*
                Экранно-сглаженная маска нерегулярного края.
            */
            float particleEdgeSoftness =
                max(
                    0.018,
                    fwidth(
                        bandU
                    ) *
                        1.80
                );

            float innerPresence =
                smoothstep(
                    -particleEdgeSoftness,
                    particleEdgeSoftness *
                        2.50,
                    bandU -
                        innerBoundaryU
                );

            float outerPresence =
                smoothstep(
                    -particleEdgeSoftness,
                    particleEdgeSoftness *
                        2.50,
                    outerBoundaryU -
                        bandU
                );

            float coreEnvelope =
                innerPresence *
                outerPresence;

            /*
                В центре band-а population близка к единице.
                К обеим нерегулярным границам количество камней
                постепенно уменьшается.
            */
            float edgePopulation =
                smoothstep(
                    0.0,
                    0.22,
                    irregularU
                ) *
                (
                    1.0 -
                    smoothstep(
                        0.78,
                        1.0,
                        irregularU
                    )
                );

            edgePopulation =
                pow(
                    clamp(
                        edgePopulation,
                        0.0,
                        1.0
                    ),
                    0.72
                );

            /*
                Редкие локальные выбросы за текущую
                художественную границу. Азимут разделён на
                крупные сектора; примерно 12% секторов получают
                шанс оставить отдельные камни снаружи кольца.
            */
            float angular01 =
                fract(
                    azimuth /
                        (
                            2.0 *
                            PI
                        ) +
                    0.5
                );

            float outlierCell =
                floor(
                    angular01 *
                        96.0
                );

            float outlierRandom =
                hash11(
                    outlierCell +
                    seed *
                        83.17
                );

            float outsideDistance =
                max(
                    innerBoundaryU -
                        bandU,
                    bandU -
                        outerBoundaryU
                );

            float outsideMask =
                step(
                    0.0,
                    outsideDistance
                );

            float outlierTail =
                outsideMask *
                step(
                    0.88,
                    outlierRandom
                ) *
                (
                    1.0 -
                    smoothstep(
                        0.0,
                        0.11,
                        outsideDistance
                    )
                );

            float particleEnvelope =
                max(
                    coreEnvelope,
                    outlierTail *
                        0.72
                );

            /*
                Этот множитель влияет на вероятность создания
                procedural particle. Поэтому у края камней
                становится действительно меньше, а не просто
                меняется их прозрачность.
            */
            float particlePopulationScale =
                clamp(
                    edgePopulation +
                    outlierTail *
                        0.24,
                    0.0,
                    1.0
                );

            float particles =
                particleCloudMask(
                    ringRadius,
                    bandWidth,
                    bandU,
                    azimuth,
                    pixelsPerPlanetRadius,
                    seed,
                    densityScale,
                    particlePopulationScale,
                    bandParticle
                );

            /*
                Непрерывная слабая пыль остаётся только внутри
                основной структуры. За границу выпускаются лишь
                редкие отдельные procedural particles.
            */
            float faintHaze =
                uVisualLod.w *
                uVisualParticleShape.w *
                uVisualParticle.x *
                uVisualParticle.y *
                uVisualParticleShape.z *
                0.08 *
                coreEnvelope *
                mix(
                    0.10,
                    1.0,
                    edgePopulation
                );

            float rawParticleOpacity =
                clamp(
                    appearance.x *
                    opacityScale *
                    uVisualParticle.y,
                    0.0,
                    1.0
                );

            float artisticParticleGain =
                visibilityClass == 0
                    ? 10.0
                    : (
                        visibilityClass == 1
                            ? 8.0
                            : 6.5
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
                particleEnvelope;

            /*
                Минимальная видимость отдельной точки также
                ослабевает у границы. Пограничные камни реже и
                в среднем слабее центральных.
            */
            if (particles >
                0.05)
            {
                float minimumDotAlpha =
                    visibilityClass == 0
                        ? 0.12
                        : (
                            visibilityClass == 1
                                ? 0.08
                                : (
                                    visibilityClass == 2
                                        ? 0.05
                                        : 0.035
                                )
                        );

                float edgeAlphaScale =
                    mix(
                        0.24,
                        1.0,
                        edgePopulation
                    );

                alpha =
                    max(
                        alpha,
                        particles *
                        minimumDotAlpha *
                        particleEnvelope *
                        edgeAlphaScale
                    );
            }

            alpha +=
                faintHaze;

            brightness =
                0.84 +
                hash11(
                    seed +
                    clamp(
                        irregularU,
                        0.0,
                        1.0
                    ) *
                        91.0
                ) *
                0.20;
        }

                /*
            Плавное затемнение задней области.

            uRenderPart по-прежнему нужен для правильного порядка:
            back rings -> planet -> front rings.

            Но яркость больше не переключается скачком ровно
            на линии depth == 0.
        */
        float projectedDepthScale =
            max(
                0.0001,
                ringRadius *
                length(
                    uRingDepthCoefficients
                )
            );

        float normalizedDepth =
            depth /
            projectedDepthScale;

        /*
            На самой границе backAmount равен нулю.
            Затемнение постепенно набирается глубже за планетой.
        */
        float backAmount =
            smoothstep(
                0.0,
                0.42,
                -normalizedDepth
            );

        if (uVisualMode == 0)
        {
            float targetBackBrightness =
                uVisualOcclusion.x > 0.5
                    ? uVisualOcclusion.y
                    : 0.92;

            brightness *=
                mix(
                    1.0,
                    targetBackBrightness,
                    backAmount
                );
        }
        else
        {
            brightness *=
                mix(
                    1.0,
                    0.97,
                    backAmount
                );
        }

        /*
            Локальная псевдотень от планеты на кольцах.

            Это отдельный эффект, не связанный с общим
            back-half dimming выше. Маска строит мягкий
            теневой след за диском планеты в направлении,
            противоположном условному экранному свету.
        */
        float planetShadowMask =
            planetCastRingShadowMask(
                ringCoordinate,
                shadowDirectionRing,
                ringRadius,
                azimuth
            );

        /*
            Плотные Saturn-like кольца принимают более
            выраженную тень. Пылевые Jupiter-like кольца
            затемняются слабее, иначе редкие частицы
            полностью исчезнут.
        */
        float planetShadowStrength =
            uVisualMode == 0
                ? 0.46
                : 0.27;

        brightness *=
            1.0 -
            planetShadowMask *
                planetShadowStrength;

        /*
            Дополнительная мягкая контактная окклюзия
            только на заднем проходе рядом с планетой.

            Она подчёркивает, что задние кольца уходят
            за шар, но не создаёт чёрной линии по краю.
        */
        float nearPlanetMask =
            1.0 -
            smoothstep(
                1.02,
                1.72,
                ringRadius
            );

        float backContactMask =
            uRenderPart == 0
                ? nearPlanetMask *
                    smoothstep(
                        0.02,
                        0.34,
                        -normalizedDepth
                    )
                : 0.0;

        float contactStrength =
            uVisualMode == 0
                ? 0.16
                : 0.09;

        brightness *=
            1.0 -
            backContactMask *
                contactStrength;












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
        ? 1.35
        : (
            visibilityClass == 1
                ? 1.22
                : 1.12
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
            0.28
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