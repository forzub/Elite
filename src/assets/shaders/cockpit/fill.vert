#version 330 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aLocal;
layout (location = 2) in vec4 aColor;

out vec2 vLocal;
out vec4 vColor;

void main()
{
    vLocal = aLocal;
    vColor = aColor;
    gl_Position = vec4(aPos, 0.0, 1.0);
}

