#version 330 core

in vec4 vColor;
in vec2 vUv;

out vec4 FragColor;

uniform float uBrightnessScale;
uniform int uSceneStyle; // 0 Elite Classic, 1 Modern, 2 Anime

void main()
{
    vec2 p = vUv * 2.0 - 1.0;
    float r = length(p);

    if (r > 1.0)
        discard;

    float brightness =
        clamp(
            vColor.a * clamp(uBrightnessScale, 0.0, 2.0),
            0.0,
            1.0
        );

    float core = pow(max(1.0 - r, 0.0), 5.5);
    float halo = pow(max(1.0 - r, 0.0), 2.0) * 0.35;

    float alpha = clamp((core * 2.25 + halo * 1.35) * brightness, 0.0, 1.0);
    vec3 color = vColor.rgb * (0.75 + brightness * 1.65);

    if (uSceneStyle == 0)
    {
        // Elite Classic: crisp white points, no halo, no colour.
        alpha = r < 0.35 ? clamp(brightness * 1.6, 0.0, 1.0) : 0.0;
        color = vec3(0.92);
    }
    else if (uSceneStyle == 2)
    {
        // Anime: small flat discs, two brightness steps, soft tint.
        float l = dot(vColor.rgb, vec3(0.3, 0.5, 0.2));
        color = mix(vec3(l), vColor.rgb, 0.5) * 1.3;
        alpha = r < 0.45 ? (brightness > 0.5 ? 1.0 : 0.55) * min(1.0, brightness * 2.0) : 0.0;
    }
    FragColor = vec4(color, alpha);
}