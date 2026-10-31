#version 330 core

in float vEdgeAlpha;

out vec4 FragColor;

uniform vec3 edgeColor;
uniform float edgeIntensity;
uniform int uSceneStyle; // 0 Elite Classic, 1 Modern, 2 Anime

void main()
{
    float alpha = clamp(vEdgeAlpha * edgeIntensity, 0.0, 1.0);

    if (alpha <= 0.01)
        discard;

    vec3 c = edgeColor;
    if (uSceneStyle == 0)
    {
        c = vec3(0.86, 0.87, 0.88);
        alpha = max(alpha, 0.7);
    }
    else if (uSceneStyle == 2)
        c = vec3(0.05, 0.05, 0.08);
    FragColor = vec4(c, alpha);
}