#version 330 core

in vec3 fragWorldPos;
in float fragEdgeWeight;
out vec4 FragColor;

uniform vec3 color;
uniform vec3 farColor;
uniform float alpha;
uniform float thickness;
uniform vec2 viewport;
uniform vec3 cameraPos;
uniform float fadeStart;
uniform float fadeEnd;
uniform float fadePower;
uniform int uSceneStyle; // 0 Elite Classic, 1 Modern, 2 Anime

void main()
{
    float dist = distance(fragWorldPos, cameraPos);

    float t = 0.0;
    if (dist > fadeStart) {
        t = clamp((dist - fadeStart) / max(fadeEnd - fadeStart, 0.001), 0.0, 1.0);
        t = t * t * (3.0 - 2.0 * t);
        t = pow(t, fadePower);
    }

    float finalAlpha = alpha * (1.0 - t);

    // Дальние ребра не просто исчезают, а становятся светлее
    vec3 finalColor = mix(color, farColor, t);

    if (uSceneStyle == 0)
    {
        // Elite Classic: light grey vector edges, fading to grey far away.
        finalColor = mix(vec3(0.92, 0.93, 0.94), vec3(0.62), t);
        finalAlpha = max(finalAlpha, 0.95 * (1.0 - t * 0.4));
    }
    else if (uSceneStyle == 2)
    {
        // Anime: a line hierarchy by structural weight (the silhouette is
        // the composite's depth ink). Major breaks / material boundaries:
        // near-black, strong, kept far; medium creases: weaker, gone at
        // mid range; minor ones: faint and only close. Far away an object
        // keeps its silhouette and big masses, not a wireframe.
        float major = smoothstep(0.55, 0.95, fragEdgeWeight);
        float fadeEnd = mix(300.0, 2200.0, major);
        float strength = mix(0.12, 0.80, major);
        finalColor = vec3(0.025, 0.028, 0.040);
        finalAlpha = strength * (1.0 - smoothstep(fadeEnd * 0.35, fadeEnd, dist));
    }

    FragColor = vec4(finalColor, finalAlpha);
}