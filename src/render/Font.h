#pragma once

#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

struct Character
{
    unsigned int textureID;
    glm::ivec2 size;
    glm::ivec2 bearing;
    unsigned int advance;
};

class Font
{
public:
    Font(const std::string& path, int pixelSize);

    const Character& glyph(char c) const;

    int pixelSize() const { return m_pixelSize; }
    float ascent() const;
    float descent() const;
    float lineHeight() const;

    float measureText(const std::string& text) const;

private:
    int m_pixelSize;
    std::unordered_map<char, Character> m_chars;
};
