#version 330 core

layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec3 bary;

uniform mat4 MVP;

out vec3 vBary;

void main()
{
    vBary = bary;
    gl_Position = MVP * vec4(position,1.0);
}