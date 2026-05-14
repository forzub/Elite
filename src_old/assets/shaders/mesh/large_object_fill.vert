#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;

uniform mat4 MVP;
uniform mat4 M;
uniform mat3 normalMat;
uniform vec3 cameraPos;  // ← ДОБАВЬТЕ ЭТУ СТРОКУ

out vec3 vNormal;
out vec3 vColor;
out vec3 vWorldPos;
out vec3 vViewPos;
out vec3 vObjectPos;

void main()
{
    vec4 worldPos = M * vec4(position, 1.0);
    vWorldPos = worldPos.xyz;
    vViewPos = (MVP * vec4(position, 1.0)).xyz;
    vObjectPos = position;
    
    gl_Position = MVP * vec4(position, 1.0);

    vNormal = normalize(normalMat * normal);
    vColor = vec3(1.0);
}