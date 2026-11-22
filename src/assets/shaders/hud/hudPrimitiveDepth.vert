#version 330 core

// DepthLineBatch: clip-space positions (the depth tested against the scene)
layout (location = 0) in vec4 aClip;
layout (location = 1) in vec4 aColor;

out vec4 vColor;

void main()
{
    gl_Position = aClip;
    vColor = aColor;
}
