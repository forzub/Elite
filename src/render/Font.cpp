#include "render/Font.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
uint32_t nextUtf8Codepoint(
    const std::string& text,
    size_t& i
)
{
    if (i >= text.size())
        return 0;

    const unsigned char c0 =
        static_cast<unsigned char>(text[i]);

    if (c0 < 0x80)
    {
        ++i;
        return c0;
    }

    if ((c0 & 0xE0) == 0xC0 && i + 1 < text.size())
    {
        const unsigned char c1 =
            static_cast<unsigned char>(text[i + 1]);

        i += 2;

        return
            ((c0 & 0x1F) << 6) |
            (c1 & 0x3F);
    }

    if ((c0 & 0xF0) == 0xE0 && i + 2 < text.size())
    {
        const unsigned char c1 =
            static_cast<unsigned char>(text[i + 1]);

        const unsigned char c2 =
            static_cast<unsigned char>(text[i + 2]);

        i += 3;

        return
            ((c0 & 0x0F) << 12) |
            ((c1 & 0x3F) << 6) |
            (c2 & 0x3F);
    }

    if ((c0 & 0xF8) == 0xF0 && i + 3 < text.size())
    {
        const unsigned char c1 =
            static_cast<unsigned char>(text[i + 1]);

        const unsigned char c2 =
            static_cast<unsigned char>(text[i + 2]);

        const unsigned char c3 =
            static_cast<unsigned char>(text[i + 3]);

        i += 4;

        return
            ((c0 & 0x07) << 18) |
            ((c1 & 0x3F) << 12) |
            ((c2 & 0x3F) << 6) |
            (c3 & 0x3F);
    }

    ++i;
    return static_cast<uint32_t>('?');
}

std::vector<std::string> fallbackFontCandidates()
{
    std::vector<std::string> result = {
        "assets/fonts/NotoSansCJK-Regular.otf",
        "src/assets/fonts/NotoSansCJK-Regular.otf"
    };

#ifdef _WIN32
    std::filesystem::path windowsDir = "C:/Windows";
    if (const char* env = std::getenv("WINDIR"))
    {
        if (*env)
            windowsDir = env;
    }

    const std::filesystem::path fontsDir = windowsDir / "Fonts";
    result.push_back((fontsDir / "msyh.ttc").string());
    result.push_back((fontsDir / "msyhbd.ttc").string());
    result.push_back((fontsDir / "simhei.ttf").string());
    result.push_back((fontsDir / "simsun.ttc").string());
    result.push_back((fontsDir / "YuGothR.ttc").string());
    result.push_back((fontsDir / "YuGothM.ttc").string());
    result.push_back((fontsDir / "meiryo.ttc").string());
    result.push_back((fontsDir / "msgothic.ttc").string());
#else
    result.push_back("/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc");
    result.push_back("/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.otf");
#endif

    return result;
}
}

Font::Font(const std::string& path, int pixelSize)
    : m_pixelSize(pixelSize)
{
    FT_Library ft = nullptr;

    if (FT_Init_FreeType(&ft))
        throw std::runtime_error("FreeType init failed");

    FT_Face face = nullptr;

    if (FT_New_Face(ft, path.c_str(), 0, &face))
    {
        FT_Done_FreeType(ft);
        throw std::runtime_error("Font load failed: " + path);
    }

    FT_Select_Charmap(face, FT_ENCODING_UNICODE);
    FT_Set_Pixel_Sizes(face, 0, pixelSize);

    m_ftLibrary = ft;
    m_face = face;

    // Roboto remains the requested visual face for Latin/Cyrillic. CJK
    // glyphs are resolved lazily from an installed/bundled Unicode fallback
    // face instead of collapsing to '?'. No player-facing renderer has to
    // know which script a particular string uses.
    for (const std::string& fallbackPath : fallbackFontCandidates())
    {
        if (fallbackPath == path || !std::filesystem::exists(fallbackPath))
            continue;

        FT_Face fallback = nullptr;
        if (FT_New_Face(ft, fallbackPath.c_str(), 0, &fallback) != 0)
            continue;

        if (FT_Select_Charmap(fallback, FT_ENCODING_UNICODE) != 0)
        {
            FT_Done_Face(fallback);
            continue;
        }

        FT_Set_Pixel_Sizes(fallback, 0, pixelSize);
        m_fallbackFaces.push_back(fallback);
    }

    if (face->size)
    {
        m_ascent =
            static_cast<float>(face->size->metrics.ascender >> 6);

        m_descent =
            static_cast<float>(
                -(face->size->metrics.descender >> 6)
            );

        m_lineHeight =
            static_cast<float>(face->size->metrics.height >> 6);
    }
    else
    {
        m_ascent = static_cast<float>(pixelSize);
        m_descent = static_cast<float>(pixelSize) * 0.25f;
        m_lineHeight = static_cast<float>(pixelSize) * 1.25f;
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glGenTextures(1, &m_atlasTexture);
    glBindTexture(GL_TEXTURE_2D, m_atlasTexture);

    std::vector<unsigned char> empty(
        static_cast<size_t>(m_atlasW * m_atlasH),
        0
    );

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RED,
        m_atlasW,
        m_atlasH,
        0,
        GL_RED,
        GL_UNSIGNED_BYTE,
        empty.data()
    );

    // RED -> ALPHA. RGB остаётся белым.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_ONE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_ONE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_ONE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);

    // Загружаем базовые символы заранее.
    // Остальные — динамически по мере появления в строках.
    for (uint32_t cp = 32; cp < 128; ++cp)
        glyph(cp);
}

Font::~Font()
{
    if (m_atlasTexture != 0)
    {
        glDeleteTextures(1, &m_atlasTexture);
        m_atlasTexture = 0;
    }

    for (void* fallbackFace : m_fallbackFaces)
    {
        if (fallbackFace)
            FT_Done_Face(static_cast<FT_Face>(fallbackFace));
    }
    m_fallbackFaces.clear();

    if (m_face)
    {
        FT_Done_Face(static_cast<FT_Face>(m_face));
        m_face = nullptr;
    }

    if (m_ftLibrary)
    {
        FT_Done_FreeType(static_cast<FT_Library>(m_ftLibrary));
        m_ftLibrary = nullptr;
    }
}

Character& Font::glyph(uint32_t codepoint)
{
    auto it = m_chars.find(codepoint);

    if (it != m_chars.end())
        return it->second;

    return loadGlyph(codepoint);
}

Character& Font::loadGlyph(uint32_t codepoint)
{
    FT_Face primaryFace = static_cast<FT_Face>(m_face);
    FT_Face face = primaryFace;

    if (!face)
        throw std::runtime_error("Font face is null");

    uint32_t actualCodepoint = codepoint;

    FT_UInt glyphIndex = FT_Get_Char_Index(face, actualCodepoint);

    if (glyphIndex == 0)
    {
        for (void* fallbackPtr : m_fallbackFaces)
        {
            FT_Face fallback = static_cast<FT_Face>(fallbackPtr);
            const FT_UInt fallbackGlyph =
                FT_Get_Char_Index(fallback, actualCodepoint);

            if (fallbackGlyph == 0)
                continue;

            face = fallback;
            glyphIndex = fallbackGlyph;
            break;
        }
    }

    if (glyphIndex == 0 && actualCodepoint != static_cast<uint32_t>('?'))
    {
        actualCodepoint = static_cast<uint32_t>('?');
        face = primaryFace;
        glyphIndex = FT_Get_Char_Index(primaryFace, actualCodepoint);
    }

    if (FT_Load_Char(face, actualCodepoint, FT_LOAD_RENDER))
    {
        actualCodepoint = static_cast<uint32_t>('?');
        face = primaryFace;
        FT_Load_Char(primaryFace, actualCodepoint, FT_LOAD_RENDER);
    }

    FT_GlyphSlot g = face->glyph;

    const int glyphW = static_cast<int>(g->bitmap.width);
    const int glyphH = static_cast<int>(g->bitmap.rows);

    Character ch;
    ch.textureID = m_atlasTexture;
    ch.size = {glyphW, glyphH};
    ch.bearing = {
        g->bitmap_left,
        g->bitmap_top
    };
    ch.advance = static_cast<unsigned int>(g->advance.x);
    ch.codepoint = codepoint;

    if (glyphW > 0 && glyphH > 0)
    {
        if (m_penX + glyphW + m_padding >= m_atlasW)
        {
            m_penX = m_padding;
            m_penY += m_rowH + m_padding;
            m_rowH = 0;
        }

        if (m_penY + glyphH + m_padding >= m_atlasH)
        {
            std::cerr
                << "[Font] Atlas overflow for codepoint "
                << codepoint
                << ". Increase atlas size.\n";

            if (codepoint != static_cast<uint32_t>('?'))
                return glyph(static_cast<uint32_t>('?'));
        }

        glBindTexture(GL_TEXTURE_2D, m_atlasTexture);

        glTexSubImage2D(
            GL_TEXTURE_2D,
            0,
            m_penX,
            m_penY,
            glyphW,
            glyphH,
            GL_RED,
            GL_UNSIGNED_BYTE,
            g->bitmap.buffer
        );

        glBindTexture(GL_TEXTURE_2D, 0);

        ch.uv0 = {
            static_cast<float>(m_penX) / static_cast<float>(m_atlasW),
            static_cast<float>(m_penY) / static_cast<float>(m_atlasH)
        };

        ch.uv1 = {
            static_cast<float>(m_penX + glyphW) / static_cast<float>(m_atlasW),
            static_cast<float>(m_penY + glyphH) / static_cast<float>(m_atlasH)
        };

        m_penX += glyphW + m_padding;
        m_rowH = std::max(m_rowH, glyphH);
    }

    auto [it, inserted] = m_chars.emplace(codepoint, ch);
    return it->second;
}

float Font::measureText(const std::string& text)
{
    float width = 0.0f;

    size_t i = 0;

    while (i < text.size())
    {
        const uint32_t cp = nextUtf8Codepoint(text, i);
        Character& ch = glyph(cp);

        width += static_cast<float>(ch.advance >> 6);
    }

    return width;
}