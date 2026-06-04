#version 330 core

in vec3 vNormal;
in vec3 vWorldPos;

out vec4 FragColor;

uniform vec3 uCameraPos;
uniform vec3 uLightDir;

uniform vec3 uBaseColor;
uniform vec3 uLitColor;
uniform float uEmission;

void main()
{
    vec3 n = normalize(vNormal);
    vec3 v = normalize(uCameraPos - vWorldPos);
    vec3 l = normalize(-uLightDir);

    float ndl = max(dot(n, l), 0.0);
    float rim = pow(1.0 - max(dot(n, v), 0.0), 2.8);

    vec3 color = mix(uBaseColor, uLitColor, ndl);
    color += vec3(1.0) * rim * 0.08;

    color += uLitColor * uEmission;

    FragColor = vec4(color, 1.0);
}