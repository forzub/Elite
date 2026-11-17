#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 3) in vec3 materialColor; // material base colour (model data)

uniform mat4 MVP;
uniform mat4 M;
uniform mat3 normalMat;
uniform vec3 cameraPos;  // ← ДОБАВЬТЕ ЭТУ СТРОКУ

out vec3 vNormal;
out vec3 vColor;
out vec3 vWorldPos;
out vec3 vViewPos;
out vec3 vObjectPos;
out vec3 vObjectNormal;
flat out float vInstSeed;

void main()
{
    // the facade seed rides in the unused [0][3] slot of M (SceneRenderer fillModel); MVP is built from the clean model
    vInstSeed = M[0].w;
    mat4 Mc = M;
    Mc[0].w = 0.0;
    vec4 worldPos = Mc * vec4(position, 1.0);
    vWorldPos = worldPos.xyz;
    vViewPos = (MVP * vec4(position, 1.0)).xyz;
    vObjectPos = position;
    vObjectNormal = normal;
    
    gl_Position = MVP * vec4(position, 1.0);

    vNormal = normalize(normalMat * normal);
    vColor = materialColor;
}