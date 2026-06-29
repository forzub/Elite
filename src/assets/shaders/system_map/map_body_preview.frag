#version 330 core

in vec2 vUv;
in vec4 vColor;

uniform sampler2D uTexture;

out vec4 FragColor;

void main()
{
    vec4 texel = texture(uTexture, vUv);

    // System-map body textures are treated as opaque color maps.
    // Some generated preview/albedo images may contain garbage or mask-like
    // alpha rows. That alpha must not cut holes through planets/moons.
    FragColor = vec4(
        texel.rgb * vColor.rgb,
        vColor.a
    );
}