#version 330 core

in vec2 vUv;

out vec4 outColor;

uniform vec2 uViewportOriginPx;
uniform vec2 uViewportSize;

uniform vec2 uPlanetCenterPx;
uniform float uPlanetRadiusPx;

uniform float uRadiusScale;
uniform float uVisualIntensity;

uniform vec4 uSurfaceHaze;
uniform vec4 uLimbCore;
uniform vec4 uNearAtmosphere;
uniform vec4 uOuterAtmosphere;

float smoothBand(
    float radius,
    float startRadius,
    float peakRadius,
    float endRadius
)
{
    float rise =
        smoothstep(
            startRadius,
            peakRadius,
            radius
        );

    float fall =
        1.0 -
        smoothstep(
            peakRadius,
            endRadius,
            radius
        );

    return
        clamp(
            rise * fall,
            0.0,
            1.0
        );
}

void main()
{
    
    
/*
    gl_FragCoord использует OpenGL-координаты:
    начало находится слева снизу.

    uPlanetCenterPx приходит из map renderer:
    начало находится слева сверху.

    Поэтому переворачиваем только Y координату центра.
*/
vec2 planetCenterWindowPx =
    vec2(
        uViewportOriginPx.x +
            uPlanetCenterPx.x,
        uViewportOriginPx.y +
            (
                uViewportSize.y -
                uPlanetCenterPx.y
            )
    );

vec2 local =
    (
        gl_FragCoord.xy -
        planetCenterWindowPx
    ) /
    max(
        1.0,
        uPlanetRadiusPx
    );




    float radialDistance =
        length(local);

    float atmosphereOuterRadius =
        max(
            1.001,
            uRadiusScale + 0.075
        );

    if (radialDistance >
        atmosphereOuterRadius)
    {
        discard;
    }

    float intensity =
        max(
            0.0,
            uVisualIntensity
        );

    /*
        Внутренняя часть диска.

        viewNormal:
            1 в центре;
            0 у горизонта.
    */
    float insidePlanet =
        1.0 -
        smoothstep(
            0.998,
            1.003,
            radialDistance
        );

    float viewNormal =
        sqrt(
            max(
                0.0,
                1.0 -
                    radialDistance *
                    radialDistance
            )
        );

    /*
        Приближённая оптическая глубина:
        у края луч проходит через более длинный путь атмосферы.
    */
    float opticalDepth =
        1.0 /
        max(
            0.075,
            viewNormal
        );

    float interiorHazeStrength =
        (
            1.0 -
            exp(
                -0.055 *
                opticalDepth
            )
        ) *
        insidePlanet;

    /*
        В центре haze слабее.
        У горизонта — сильнее.
    */
    interiorHazeStrength *=
        smoothstep(
            0.35,
            0.98,
            radialDistance
        );

    vec4 accumulated =
        vec4(0.0);

    vec4 interiorHaze =
        uSurfaceHaze;

    interiorHaze.a *=
        interiorHazeStrength *
        intensity;

    accumulated.rgb +=
        interiorHaze.rgb *
        interiorHaze.a;

    accumulated.a +=
        interiorHaze.a;

    /*
        Ядро лимба.
    */
    float limbCoreStrength =
        smoothBand(
            radialDistance,
            0.975,
            1.002,
            1.025
        );

    vec4 limbCore =
        uLimbCore;

    limbCore.a *=
        limbCoreStrength *
        intensity;

    accumulated.rgb =
        accumulated.rgb +
        limbCore.rgb *
        limbCore.a *
        (
            1.0 -
            accumulated.a
        );

    accumulated.a =
        accumulated.a +
        limbCore.a *
        (
            1.0 -
            accumulated.a
        );

    /*
        Плотная нижняя атмосфера.
    */
    float nearStrength =
        smoothBand(
            radialDistance,
            0.995,
            1.025,
            max(
                1.05,
                uRadiusScale + 0.035
            )
        );

    vec4 nearAtmosphere =
        uNearAtmosphere;

    nearAtmosphere.a *=
        nearStrength *
        intensity;

    accumulated.rgb =
        accumulated.rgb +
        nearAtmosphere.rgb *
        nearAtmosphere.a *
        (
            1.0 -
            accumulated.a
        );

    accumulated.a =
        accumulated.a +
        nearAtmosphere.a *
        (
            1.0 -
            accumulated.a
        );

    /*
        Широкое внешнее растворение.
    */
    float outerStrength =
        smoothBand(
            radialDistance,
            1.015,
            max(
                1.04,
                uRadiusScale + 0.025
            ),
            atmosphereOuterRadius
        );

    vec4 outerAtmosphere =
        uOuterAtmosphere;

    outerAtmosphere.a *=
        outerStrength *
        intensity;

    accumulated.rgb =
        accumulated.rgb +
        outerAtmosphere.rgb *
        outerAtmosphere.a *
        (
            1.0 -
            accumulated.a
        );

    accumulated.a =
        accumulated.a +
        outerAtmosphere.a *
        (
            1.0 -
            accumulated.a
        );

    if (accumulated.a <= 0.0001)
        discard;

    /*
        RGB уже накоплен как premultiplied color.
        Но текущий renderer использует обычный SRC_ALPHA.
        Переводим обратно в straight alpha.
    */
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