#version 330 core

in vec4 vColor;

out vec4 outColor;

void main()
{
    if (vColor.a <= 0.001)
        discard;

    outColor =
        vColor;
}
