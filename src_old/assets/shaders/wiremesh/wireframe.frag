#version 330 core

in vec3 vBary;
out vec4 FragColor;

float edgeFactor()
{
    vec3 d = fwidth(vBary);
    vec3 a3 = smoothstep(vec3(0.0), d * 1.0, vBary);
    return min(min(a3.x, a3.y), a3.z);
}

void main()
{
    float e = edgeFactor();

    vec3 edge = vec3(0.7,0.7,0.7);
    vec3 fill = vec3(0.2,0.2,0.2);

    vec3 color = mix(edge, fill, e);

    FragColor = vec4(color,1.0);
}