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
        // Elite Classic (author 2026-11-21): dark far away, lighter near (the station preset's far / near edge
        // colours), no alpha floor - the near-white 0.86 lines with a fixed 0.7 floor made a whole city's edges pop in
        // at once where the CPU starts drawing them
        c = mix(vec3(0.18, 0.20, 0.26), vec3(0.58, 0.60, 0.66), vEdgeAlpha);
    }
    else if (uSceneStyle == 2)
    {
        // Anime: quiet interior detail lines (silhouette = composite ink).
        c = vec3(0.10, 0.12, 0.18);
        alpha *= 0.42;
    }
    FragColor = vec4(c, alpha);
}