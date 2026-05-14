#version 330 core

in vec3 bary;
in vec3 color;

out vec4 FragColor;

uniform float thickness = 0.02;

void main()
{
    float d = min(min(bary.x, bary.y), bary.z);

    float edge = smoothstep(0.0, thickness, d);

    vec3 fillColor = color;
    vec3 lineColor = vec3(1.0);

    vec3 final = mix(lineColor, fillColor, edge);

    FragColor = vec4(final,1.0);
}
