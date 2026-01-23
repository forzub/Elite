// #pragma once

// #include <string>
// #include <unordered_map>

// #include <glm/glm.hpp>

// struct Character
// {
//     unsigned int textureID;
//     glm::ivec2 size;
//     glm::ivec2 bearing;
//     unsigned int advance;
// };

// class TextRenderer
// {
// public:
//     static TextRenderer& instance();

//     void init(const std::string& fontPath);
//     void textDraw(const std::string& text,
//               float x, float y,
//               float scale,
//               const glm::vec3& color);
//     void setViewportSize(int width, int height);
//     void drawWorldLabel(
//         const std::string& text,
//         const glm::vec3& worldPos,
//         const glm::mat4& view,
//         const glm::mat4& projection,
//         float scale,
//         const glm::vec3& color
//     );
//     int viewportWidth() const  { return m_viewportW; }
//     int viewportHeight() const { return m_viewportH; }
//     float measureText(const std::string& text, float scale) const;
//     float lineHeight(float scale) const;
//     float ascent(float scale) const;

// private:
//     TextRenderer() = default;

//     std::unordered_map<char, Character> m_chars;

//     unsigned int m_vao = 0;
//     unsigned int m_vbo = 0;

//     int m_viewportW = 1;
//     int m_viewportH = 1;
//     int m_fontPixelSize = 48;

//     unsigned int m_shader = 0;
// };

#pragma once

#include <string>
#include <glm/glm.hpp>

#include "render/Font.h"

class TextRenderer
{
public:
    static TextRenderer& instance();

    // ------------------------------------------------------------
    // lifecycle
    // ------------------------------------------------------------
    void init(); // ТОЛЬКО шейдеры и VAO/VBO
    void setViewportSize(int width, int height);

    int viewportWidth()  const { return m_viewportW; }
    int viewportHeight() const { return m_viewportH; }

    // ------------------------------------------------------------
    // NEW API (ПРАВИЛЬНЫЙ)
    // baseline = y
    // ------------------------------------------------------------
    void textDraw(
        const Font& font,
        const std::string& text,
        float x,
        float baselineY,
        const glm::vec3& color
    );

    // ------------------------------------------------------------
    // LEGACY API (НЕ ЛОМАЕМ ПРОЕКТ)
    // scale — ВРЕМЕННО
    // ------------------------------------------------------------
    void textDraw(
        const std::string& text,
        float x,
        float y,
        float scale,
        const glm::vec3& color
    );

private:
    TextRenderer() = default;

private:
    unsigned int m_vao    = 0;
    unsigned int m_vbo    = 0;
    unsigned int m_shader = 0;

    int m_viewportW = 1;
    int m_viewportH = 1;
};
