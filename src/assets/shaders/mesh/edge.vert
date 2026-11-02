#version 330 core

layout(location = 0) in vec3 inPos;
layout(location = 1) in float inWeight; // structural edge weight

uniform mat4 MVP;
uniform mat4 M;  // матрица модели

out vec3 worldPos;
out vec3 viewPos;
out float edgeWeight;

void main()
{
    vec4 worldPos4 = M * vec4(inPos, 1.0);
    worldPos = worldPos4.xyz;
    gl_Position = MVP * vec4(inPos, 1.0);
    viewPos = gl_Position.xyz;
    edgeWeight = inWeight;
}