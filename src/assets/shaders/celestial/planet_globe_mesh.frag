#version 330 core

in vec2 vUv;
in vec3 vCameraNormal;
in float vNormalizedLatitude;

out vec4 outColor;

uniform sampler2D uTexture;

uniform vec4 uColor;
uniform float uOpacity;

uniform bool uUseHorizonFade;
uniform float uHorizonFadeStart;
uniform float uHorizonFadeEnd;

uniform bool uUsePolarFade;
uniform float uPolarFadeStart;
uniform float uPolarFadeEnd;

void main()
{
    vec3 cameraNormal =
        normalize(
            vCameraNormal
        );

    /*
        +Z — видимая полусфера в текущей DetailCamera.

        Это заменяет старый CPU-тест:

            if (midCamera.z < 0.0)
                continue;
    */
    if (cameraNormal.z <= 0.0)
        discard;

    vec2 textureUv =
        vec2(
            fract(
                vUv.x
            ),
            clamp(
                vUv.y,
                0.001,
                0.999
            )
        );

    vec4 sampleColor =
        texture(
            uTexture,
            textureUv
        );

    float alpha =
        sampleColor.a *
        uColor.a *
        uOpacity;

    if (uUseHorizonFade)
    {
        float horizonFade =
            smoothstep(
                uHorizonFadeStart,
                uHorizonFadeEnd,
                clamp(
                    cameraNormal.z,
                    0.0,
                    1.0
                )
            );

        alpha *=
            horizonFade;
    }

    if (uUsePolarFade)
    {
        float polarFade =
            1.0 -
            smoothstep(
                uPolarFadeStart,
                uPolarFadeEnd,
                vNormalizedLatitude
            );

        alpha *=
            polarFade;
    }

    if (alpha <= 0.0001)
        discard;

    outColor =
        vec4(
            sampleColor.rgb *
                uColor.rgb,
            alpha
        );
}