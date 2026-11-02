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
        // Anime: delicate soft points - a small round core and a faint
        // halo, pale (tint kept gentle), the brightest slightly warm.
        float l = dot(vColor.rgb, vec3(0.3, 0.5, 0.2));
        color = mix(vec3(l), vColor.rgb, 0.40) * 1.15;
        color = mix(color, color * vec3(1.06, 1.0, 0.88), smoothstep(0.6, 1.0, brightness));
        float softCore = 1.0 - smoothstep(0.12, 0.42, r);
        float softHalo = pow(max(1.0 - r, 0.0), 2.5) * 0.22;
        alpha = clamp((softCore + softHalo) * min(1.0, brightness * 1.8), 0.0, 1.0);
    }
    FragColor = vec4(color, alpha);
}