#version 330 core

layout(location=0) in vec3 inPos;
layout(location=1) in vec3 inNormal;
layout(location=2) in vec3 inBary;
layout(location=3) in vec3 inColor;

uniform mat4 MVP;

out vec3 bary;
out vec3 color;

void main()
{
    bary = inBary;
    color = inColor;

    gl_Position = MVP * vec4(inPos,1.0);
}
