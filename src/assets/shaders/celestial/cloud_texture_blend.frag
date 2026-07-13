#version 330 core

in vec2 vUv;

out vec4 outColor;

uniform sampler2D uPreviousTexture;
uniform sampler2D uNextTexture;

uniform float uTransition;

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

    vec4 previousValue =
        texture(
            uPreviousTexture,
            uv
        );

    vec4 nextValue =
        texture(
            uNextTexture,
            uv
        );

    /*
        Hermite smoothstep вместо линейного перехода.
        Скорость изменения формы не будет резко меняться
        в начале и в конце интервала.
    */
    float t =
        clamp(
            uTransition,
            0.0,
            1.0
        );

    t =
        t *
        t *
        (
            3.0 -
            2.0 * t
        );

    outColor =
        mix(
            previousValue,
            nextValue,
            t
        );
}