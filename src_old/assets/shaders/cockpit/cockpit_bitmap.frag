#version 330 core

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uTex;
uniform float uAlphaMul;


void main()
{
    vec4 c = texture(uTex, vUV);
    c.a *= uAlphaMul;
    FragColor = c;
}
