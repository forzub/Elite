#include "render/Font.h"

#include <glad/gl.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <stdexcept>
#include <iostream>

// ================================================================
// ctor
// ================================================================
Font::Font(const std::string& path, int pixelSize)
    : m_pixelSize(pixelSize)
{
    FT_Library ft;
    if (FT_Init_FreeType(&ft))
        throw std::runtime_error("FreeType init failed");

    FT_Face face;
    if (FT_New_Face(ft, path.c_str(), 0, &face))
    {
        FT_Done_FreeType(ft);
        throw std::runtime_error("Font load failed: " + path);
    }

    // задаём размер в ПИКСЕЛЯХ — это и есть основа всей математики
    FT_Set_Pixel_Sizes(face, 0, pixelSize);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    for (unsigned char c = 32; c < 128; ++c)
    {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER))
            continue;

        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);

        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RED,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            face->glyph->bitmap.buffer
        );

        // RED → ALPHA (как у тебя)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_ONE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_ONE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_ONE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        Character ch{
            tex,
            { face->glyph->bitmap.width,
              face->glyph->bitmap.rows },
            { face->glyph->bitmap_left,
              face->glyph->bitmap_top },
            static_cast<unsigned int>(face->glyph->advance.x)
        };

        m_chars[c] = ch;
    }

    FT_Done_Face(face);
    FT_Done_FreeType(ft);
}

// ================================================================
// glyph
// ================================================================
const Character& Font::glyph(char c) const
{
    auto it = m_chars.find(c);
    if (it == m_chars.end())
        return m_chars.at('?');

    return it->second;
}

// ================================================================
// ascent
// ================================================================
float Font::ascent() const
{
    float maxAscent = 0.0f;

    for (const auto& [c, ch] : m_chars)
    {
        if (ch.bearing.y > maxAscent)
            maxAscent = static_cast<float>(ch.bearing.y);
    }

    return maxAscent;
}

// ================================================================
// descent
// ================================================================
float Font::descent() const
{
    float maxDescent = 0.0f;

    for (const auto& [c, ch] : m_chars)
    {
        float descent = ch.size.y - ch.bearing.y;
        if (descent > maxDescent)
            maxDescent = descent;
    }

    return maxDescent;
}

// ================================================================
// lineHeight
// ================================================================
float Font::lineHeight() const
{
    return ascent() + descent();
}

// ================================================================
// measureText
// ================================================================
float Font::measureText(const std::string& text) const
{
    float width = 0.0f;

    for (char c : text)
    {
        auto it = m_chars.find(c);
        if (it == m_chars.end())
            continue;

        width += static_cast<float>(it->second.advance >> 6);
    }

    return width;
}
