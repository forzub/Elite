#version 330 core

in vec2 vUv;
out vec4 FragColor;

uniform int uPass;
uniform sampler2D uTransitionSnapshot;
uniform float uTransitionAlpha;

// Pass 2 is a map-only atmospheric veil drawn after the shared starfield
// and before map geometry, labels and UI.
uniform float uMapVeilCenterAlpha;
uniform float uMapVeilEdgeAlpha;
uniform float uMapVeilAquaStrength;

float hash(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

void main()
{
    if (uPass == 1)
    {
        vec4 oldFrame =
            texture(uTransitionSnapshot, vUv);

        FragColor = vec4(
            oldFrame.rgb,
            oldFrame.a * clamp(uTransitionAlpha, 0.0, 1.0)
        );

        return;
    }




if (uPass == 2)
{
    /*
        Этот проход выдаёт не "цвет тумана", а RGB-множитель.

        В C++ для него задан blending:

            src = GL_ZERO
            dst = GL_SRC_COLOR

        Значит итог:
            result.rgb = old.rgb * FragColor.rgb
    */

    vec2 p =
        (vUv - vec2(0.5)) *
        2.0;

    /*
        Слегка растягиваем по горизонтали, чтобы виньетка
        давила на края и углы, а не только кругом сжималась к центру.
    */
    p.x *= 0.78;

    float radialDistance =
        dot(
            p,
            p
        );

    float edge =
        smoothstep(
            0.18,
            1.16,
            radialDistance
        );

    /*
        Углы поддавливаем сильнее.
    */
    float cornerDistance =
        abs(p.x) +
        abs(p.y);

    float corners =
        smoothstep(
            1.05,
            1.72,
            cornerDistance
        );

    edge =
        max(
            edge,
            corners * 0.72
        );

    float centerDarkening =
        clamp(
            uMapVeilCenterAlpha,
            0.0,
            0.95
        );

    float edgeDarkening =
        clamp(
            uMapVeilEdgeAlpha,
            centerDarkening,
            0.95
        );

    float darkening =
        mix(
            centerDarkening,
            edgeDarkening,
            edge
        );

    /*
        Aqua Blue не добавляет света.
        Он лишь оставляет чуть больше синего,
        чтобы фон был холодным, а не просто серо-чёрным.
    */
    float aquaGlow =
        1.0 - smoothstep(
            0.08,
            0.92,
            distance(
                vUv,
                vec2(0.18, 0.30)
            )
        );

    float aquaAmount =
        aquaGlow *
        clamp(
            uMapVeilAquaStrength,
            0.0,
            1.0
        );

    vec3 coldRetention =
        mix(
            vec3(1.0),
            vec3(0.88, 0.96, 1.0),
            aquaAmount
        );

    vec3 multiplier =
        vec3(1.0 - darkening) *
        coldRetention;

    FragColor =
        vec4(
            clamp(
                multiplier,
                0.0,
                1.0
            ),
            1.0
        );

    return;
}


    vec2 uv = vUv;

    vec3 base    = vec3(0.002, 0.006, 0.014);
    vec3 upper   = vec3(0.010, 0.024, 0.048);
    vec3 cyanFog = vec3(0.020, 0.110, 0.170);

    vec3 col =
        mix(base, upper, smoothstep(0.0, 1.0, uv.y));

    float leftGlow =
    1.0 - smoothstep(
        0.05,
        0.85,
        distance(
            uv,
            vec2(0.12, 0.24)
        )
    );

    float topGlow =
    1.0 - smoothstep(
        0.10,
        0.95,
        distance(
            uv,
            vec2(0.62, 0.98)
        )
    );

    col += cyanFog * leftGlow * 0.28;
    col += cyanFog * topGlow * 0.12;

    float vignette =
    1.0 - smoothstep(
        0.24,
        0.95,
        distance(
            uv,
            vec2(0.50, 0.52)
        )
    );

    col *= 0.38 + 0.78 * vignette;

    float n =
        hash(floor(uv * vec2(420.0, 240.0)));

    col += (n - 0.5) * 0.010;

    FragColor = vec4(col, 1.0);
}