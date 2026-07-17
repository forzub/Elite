#version 330 core

in vec2 vUv;

out vec4 outColor;

uniform sampler2D uOverlayTexture;

void main()
{
    /*
        Texture уже содержит premultiplied RGB/alpha.
    */
    outColor =
        texture(
            uOverlayTexture,
            vUv
        );
}
