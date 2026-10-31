#version 330 core

in vec3 fragWorldPos;
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
        // Anime: dark ink contours.
        finalColor = vec3(0.05, 0.05, 0.08);
        finalAlpha = clamp(finalAlpha * 1.4, 0.0, 1.0);
    }

    FragColor = vec4(finalColor, finalAlpha);
}