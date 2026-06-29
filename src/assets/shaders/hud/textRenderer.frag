#version 330 core

in vec2 TexCoords;
in vec4 GlyphColor;

out vec4 FragColor;

uniform sampler2D text;

void main()
{
    float glyphAlpha = texture(text, TexCoords).a;

    FragColor = vec4(
        GlyphColor.rgb,
        glyphAlpha * GlyphColor.a
    );
}