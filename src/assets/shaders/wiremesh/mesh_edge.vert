#version 330 core

layout(location = 0) in vec3 inPos;

uniform mat4 MVP;
uniform vec2 viewport;
uniform float thickness;

void main()
{
    gl_Position = MVP * vec4(inPos,1.0);
}
