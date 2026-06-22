#version 330 core

in vec2 vUv;
in vec4 vColor;

uniform sampler2D uTexture;

out vec4 FragColor;

void main()
{
    vec4 texel = texture(uTexture, vUv);

    if (texel.a < 0.02)
        discard;

    FragColor = texel * vColor;
}