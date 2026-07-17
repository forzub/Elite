#version 330 core

out vec4 outColor;

uniform vec2 uViewportOriginPx;
uniform vec2 uViewportSize;

uniform vec2 uPlanetCenterPx;
uniform float uPlanetRadiusPx;

uniform vec3 uLightDirection;

uniform float uShadowStrength;
uniform float uLimbDarkening;

void main()
{
    /*
        uPlanetCenterPx использует координаты карты:
        начало слева сверху.

        gl_FragCoord:
        начало слева снизу.
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

    float radiusSquared =
        dot(
            local,
            local
        );

    if (radiusSquared > 1.0)
        discard;

    /*
        Восстанавливаем приближённую нормаль сферы
        непосредственно по экранной координате.
    */
    float frontZ =
        sqrt(
            max(
                0.0,
                1.0 -
                radiusSquared
            )
        );

    vec3 sphereNormal =
        normalize(
            vec3(
                local.x,
                local.y,
                frontZ
            )
        );

    vec3 lightDirection =
        normalize(
            uLightDirection
        );

    /*
        Half-Lambert.

        В отличие от обычного max(dot(N,L), 0),
        здесь нет жёсткого терминатора дня и ночи.
    */
    float halfLambert =
        clamp(
            dot(
                sphereNormal,
                lightDirection
            ) *
                0.5 +
            0.5,
            0.0,
            1.0
        );

    /*
        Очень широкая мягкая теневая область.
    */
    
    

    float broadShadow =
    1.0 -
    smoothstep(
        0.42,
        0.88,
        halfLambert
    );

    /*
        Дополнительная средняя теневая масса:
        не делает терминатор, но лучше лепит форму.
    */
    float midShadow =
        1.0 -
        smoothstep(
            0.30,
            0.72,
            halfLambert
        );

    float limb =
        pow(
            clamp(
                1.0 -
                frontZ,
                0.0,
                1.0
            ),
            1.25
        );

    /*
        Очень слабый светлый подъём на освещённой стороне,
        чтобы шар читался не только через затемнение.
    */
    float softHighlight =
        smoothstep(
            0.62,
            0.96,
            halfLambert
        ) *
        0.035;

    float multiplier =
        1.0 -
        broadShadow *
            clamp(
                uShadowStrength,
                0.0,
                0.70
            ) -
        midShadow *
            clamp(
                uShadowStrength *
                    0.38,
                0.0,
                0.30
            ) -
        limb *
            clamp(
                uLimbDarkening,
                0.0,
                0.45
            ) +
        softHighlight;

    /*
        Пока делаем эффект специально заметным.
        Потом ослабим.
    */
    multiplier =
        clamp(
            multiplier,
            0.58,
            1.04
        );





    /*
        C++ использует multiplicative blending:

            result = sourceColor * destinationColor

        Поэтому здесь выводится не цвет тени,
        а коэффициент умножения существующего кадра.
    */
    outColor =
        vec4(
            vec3(
                multiplier
            ),
            1.0
        );
}