#version 330 core

in vec2 vUv;

out vec4 FragColor;

uniform sampler2D uHazeTexture;

void main()
{
    FragColor =
        texture(
            uHazeTexture,
            vUv
        );
}
