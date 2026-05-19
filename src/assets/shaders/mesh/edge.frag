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

    FragColor = vec4(finalColor, finalAlpha);
}