#version 330 core

in vec2 vUv;

out vec4 FragColor;

uniform sampler2D uHazeTexture;
uniform int uSceneStyle; // 0 Elite Classic, 1 Modern, 2 Anime

void main()
{
    vec4 haze = texture(uHazeTexture, vUv);
    if (uSceneStyle == 0)
    {
        // Elite Classic: a black sky, the Milky Way only as a faint grey.
        float l = dot(haze.rgb, vec3(0.3, 0.5, 0.2));
        haze = vec4(vec3(l), haze.a * 0.25);
    }
    else if (uSceneStyle == 2)
    {
        // Anime: a soft painted wash, not hard-edged stripes: gently
        // desaturated toward cool blue-violet, two soft density tones, and
        // kept faint so it never competes with ships and stations.
        float l = dot(haze.rgb, vec3(0.3, 0.5, 0.2));
        vec3 c = mix(vec3(l), haze.rgb, 0.45) * vec3(0.92, 0.95, 1.08);
        float a = haze.a;
        float tone = smoothstep(0.05, 0.40, a) * 0.6 + smoothstep(0.35, 0.80, a) * 0.4;
        haze = vec4(c, tone * a * 0.65 + a * 0.10);
    }
    FragColor = haze;
}
