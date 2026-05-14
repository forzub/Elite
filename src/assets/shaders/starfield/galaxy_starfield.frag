#version 330 core

in vec4 vColor;
in vec2 vUv;

out vec4 FragColor;

void main()
{
    vec2 p = vUv * 2.0 - 1.0;
    float r = length(p);

    if (r > 1.0)
        discard;

    float brightness = clamp(vColor.a, 0.0, 1.0);

    float core = pow(max(1.0 - r, 0.0), 5.5);
    float halo = pow(max(1.0 - r, 0.0), 2.0) * 0.35;

    float alpha = clamp((core * 2.25 + halo * 1.35) * brightness, 0.0, 1.0);
    vec3 color = vColor.rgb * (0.75 + brightness * 1.65);

    FragColor = vec4(color, alpha);
}