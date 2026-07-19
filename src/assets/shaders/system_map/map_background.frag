#version 330 core

in vec2 vUv;
out vec4 FragColor;

uniform int uPass;
uniform sampler2D uTransitionSnapshot;
uniform float uTransitionAlpha;

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

    vec2 uv = vUv;

    vec3 base    = vec3(0.002, 0.006, 0.014);
    vec3 upper   = vec3(0.010, 0.024, 0.048);
    vec3 cyanFog = vec3(0.020, 0.110, 0.170);

    vec3 col =
        mix(base, upper, smoothstep(0.0, 1.0, uv.y));

    float leftGlow =
        smoothstep(
            0.85,
            0.05,
            distance(uv, vec2(0.12, 0.24))
        );

    float topGlow =
        smoothstep(
            0.95,
            0.10,
            distance(uv, vec2(0.62, 0.98))
        );

    col += cyanFog * leftGlow * 0.28;
    col += cyanFog * topGlow * 0.12;

    float vignette =
        smoothstep(
            0.95,
            0.24,
            distance(uv, vec2(0.50, 0.52))
        );

    col *= 0.38 + 0.78 * vignette;

    float n =
        hash(floor(uv * vec2(420.0, 240.0)));

    col += (n - 0.5) * 0.010;

    FragColor = vec4(col, 1.0);
}