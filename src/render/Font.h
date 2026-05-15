#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include <glad/gl.h>
#include <glm/glm.hpp>

struct Character
{
    unsigned int textureID = 0;      // atlas texture id

    glm::ivec2 size    = {0, 0};     // glyph bitmap size in px
    glm::ivec2 bearing = {0, 0};     // offset from baseline
    unsigned int advance = 0;        // FreeType advance, 1/64 px

    glm::vec2 uv0 = {0.0f, 0.0f};    // atlas UV top-left
    glm::vec2 uv1 = {0.0f, 0.0f};    // atlas UV bottom-right

    uint32_t codepoint = 0;
};

class Font
{
public:
    Font(const std::string& path, int pixelSize);
    ~Font();

    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;

    Character& glyph(uint32_t codepoint);
    Character& glyph(char c)
    {
        return glyph(static_cast<uint32_t>(
            static_cast<unsigned char>(c)
        ));
    }

    int pixelSize() const { return m_pixelSize; }

    float ascent() const { return m_ascent; }
    float descent() const { return m_descent; }
    float lineHeight() const { return m_lineHeight; }

    float measureText(const std::string& text);

    unsigned int atlasTexture() const { return m_atlasTexture; }

private:
    Character& loadGlyph(uint32_t codepoint);

private:
    int m_pixelSize = 0;

    void* m_ftLibrary = nullptr;
    void* m_face = nullptr;

    unsigned int m_atlasTexture = 0;

    int m_atlasW = 2048;
    int m_atlasH = 2048;

    int m_padding = 1;
    int m_penX = 1;
    int m_penY = 1;
    int m_rowH = 0;

    float m_ascent = 0.0f;
    float m_descent = 0.0f;
    float m_lineHeight = 0.0f;

    std::unordered_map<uint32_t, Character> m_chars;
};