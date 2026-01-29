#pragma once

#include <string>
#include <glm/glm.hpp>

#include "render/Font.h"
#include "render/types/Viewport.h"

class TextRenderer
{
public:
    static TextRenderer& instance();

    // void setViewportSize(int width, int height);
    // int viewportWidth()  const { return m_viewportW; }
    // int viewportHeight() const { return m_viewportH; }



    // ------------------------------------------------------------
    // lifecycle
    // ------------------------------------------------------------
    void init(); // ТОЛЬКО шейдеры и VAO/VBO


    // ------------------------------------------------------------
    // NEW API (ПРАВИЛЬНЫЙ)
    // baseline = y
    // ------------------------------------------------------------
    void textDraw(
        const Font& font,
        const std::string& text,
        float x,
        float baselineY,
        const glm::vec4& color   // ← RGBA

    );

    // --- Адаптер для старого кода (RGB → RGBA) ---
    inline void textDraw(
        const Font& font,
        const std::string& text,
        float x,
        float baselineY,
        const glm::vec3& color
    )
    {
        textDraw(font, text, x, baselineY, glm::vec4(color, 1.0f));
    }

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

    // int m_viewportW;
    // int m_viewportH;
};