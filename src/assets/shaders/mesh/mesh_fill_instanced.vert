#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 3) in vec3 debugColor;

// mat4 занимает 4 attribute location: 4,5,6,7
layout(location = 4) in mat4 instanceModel;

uniform mat4 VP;

out vec3 vNormal;
out vec3 vColor;
out vec3 vWorldPos;
out vec3 vViewPos;

void main()
{
    vec4 worldPos = instanceModel * vec4(position, 1.0);

    vWorldPos = worldPos.xyz;
    vViewPos = (VP * worldPos).xyz;

    gl_Position = VP * worldPos;

    mat3 normalMat =
        transpose(inverse(mat3(instanceModel)));

    vNormal = normalize(normalMat * normal);
    vColor = debugColor;
}