#version 330 core

in vec3 fragWorldPos;
out vec4 FragColor;

uniform vec3 color;
uniform float alpha;
uniform float thickness;
uniform vec2 viewport;
uniform vec3 cameraPos;
uniform float fadeStart;
uniform float fadeEnd;

void main()
{
    float dist = distance(fragWorldPos, cameraPos);
    
    // Затухание ребер с расстоянием
    float finalAlpha = alpha;
    if (dist > fadeStart) {
        if (dist < fadeEnd) {
            float t = (dist - fadeStart) / (fadeEnd - fadeStart);
            finalAlpha = alpha * (1.0 - t);
        } else {
            finalAlpha = 0.0;
        }
    }
    
    FragColor = vec4(color, finalAlpha);
}