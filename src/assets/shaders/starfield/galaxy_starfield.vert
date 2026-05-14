#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;
layout(location = 2) in float aSize;
layout(location = 3) in vec2 aUv;

uniform mat4 uMVP;

out vec4 vColor;
out vec2 vUv;

void main()
{
    gl_Position = uMVP * vec4(aPos, 1.0);
    vColor = aColor;
    vUv = aUv;
}