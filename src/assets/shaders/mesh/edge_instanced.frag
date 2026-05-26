#version 330 core

in float vEdgeAlpha;

out vec4 FragColor;

uniform vec3 edgeColor;
uniform float edgeIntensity;

void main()
{
    float alpha = clamp(vEdgeAlpha * edgeIntensity, 0.0, 1.0);

    if (alpha <= 0.01)
        discard;

    FragColor = vec4(edgeColor, alpha);
}